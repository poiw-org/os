-- Doora initial schema.
-- Access model: only users listed in public.admins can use the backoffice. Everything is
-- protected by row-level security; the browser can only toggle keys.disabled and
-- doors.force_open. Issuing/renewing keys (which signs the JWT) is done by the `keys`
-- edge function with the service role.

create schema if not exists private;
revoke all on schema private from public, anon, authenticated;
grant usage on schema private to authenticated;

-- ---------------------------------------------------------------- profiles / admins

create table public.profiles (
	id uuid primary key references auth.users (id) on delete cascade,
	email text,
	name text,
	nickname text,
	username text,
	auth0_id text unique,
	blocked boolean not null default false,
	created_at timestamptz not null default now()
);
create unique index profiles_email_key on public.profiles (lower(email));

create table public.admins (
	user_id uuid primary key references auth.users (id) on delete cascade
);

create function private.is_admin() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (select 1 from public.admins a where a.user_id = (select auth.uid()))
$$;
revoke execute on function private.is_admin() from public, anon;
grant execute on function private.is_admin() to authenticated;

-- Mirror auth.users into public.profiles (the only thing the app lists when issuing keys).
create function private.sync_profile() returns trigger
	language plpgsql security definer set search_path = ''
as $$
begin
	insert into public.profiles (id, email, name, nickname, username, auth0_id, blocked)
	values (
		new.id,
		new.email,
		new.raw_user_meta_data ->> 'name',
		new.raw_user_meta_data ->> 'nickname',
		new.raw_user_meta_data ->> 'username',
		new.raw_user_meta_data ->> 'auth0_id',
		coalesce(new.banned_until > now(), false)
	)
	on conflict (id) do update
		set email = excluded.email, blocked = excluded.blocked;
	return new;
end
$$;
create trigger sync_profile
	after insert or update of email, banned_until on auth.users
	for each row execute function private.sync_profile();

-- ---------------------------------------------------------------- keys

create table public.keys (
	id uuid primary key default gen_random_uuid(),
	user_id uuid not null references public.profiles (id),
	serial_number text not null unique,
	not_before timestamptz not null,
	expires_at timestamptz not null,
	issued_at timestamptz not null default now(),
	issued_by uuid references public.profiles (id) on delete set null,
	disabled boolean not null default false,
	jwt text,
	check (expires_at > not_before)
);
create index keys_user_id_idx on public.keys (user_id);
create index keys_issued_by_idx on public.keys (issued_by);

-- ---------------------------------------------------------------- door log / doors

create table public.entries (
	id bigint generated always as identity primary key,
	door_id text not null,
	type text not null check (type in ('successful', 'failed')),
	reason text check (
		reason in (
			'Unknown', 'KeyExpired', 'KeyBlacklisted', 'KeyNotYetActivated',
			'IssuingDateInTheFuture', 'SerialNumberMismatch'
		)
	),
	-- The JWT's jti. Text and no FK: the log must survive key deletion, and old tags may carry
	-- ids that are not uuids.
	key_id text,
	serial_number text,
	metadata jsonb not null default '{}'::jsonb,
	created_at timestamptz not null default now()
);
create index entries_key_id_idx on public.entries (key_id, created_at desc);

create table public.doors (
	id text primary key,
	name text,
	force_open boolean not null default false
);
insert into public.doors (id, name) values ('main', 'Main door');

-- ---------------------------------------------------------------- RLS

alter table public.profiles enable row level security;
alter table public.admins enable row level security;
alter table public.keys enable row level security;
alter table public.entries enable row level security;
alter table public.doors enable row level security;

create policy "own profile or admin" on public.profiles
	for select to authenticated
	using (id = (select auth.uid()) or (select private.is_admin()));

create policy "read own admin row" on public.admins
	for select to authenticated
	using (user_id = (select auth.uid()));

create policy "admins read keys" on public.keys
	for select to authenticated using ((select private.is_admin()));
create policy "admins toggle keys" on public.keys
	for update to authenticated
	using ((select private.is_admin())) with check ((select private.is_admin()));
create policy "admins delete keys" on public.keys
	for delete to authenticated using ((select private.is_admin()));

create policy "admins read entries" on public.entries
	for select to authenticated using ((select private.is_admin()));

create policy "admins read doors" on public.doors
	for select to authenticated using ((select private.is_admin()));
create policy "admins update doors" on public.doors
	for update to authenticated
	using ((select private.is_admin())) with check ((select private.is_admin()));

-- ---------------------------------------------------------------- grants

revoke all on all tables in schema public from anon, authenticated;
grant select on public.profiles, public.admins, public.keys, public.entries, public.doors
	to authenticated;
-- Dates and the JWT only change through the edge function, so the browser cannot touch them.
grant update (disabled) on public.keys to authenticated;
grant delete on public.keys to authenticated;
grant update (force_open) on public.doors to authenticated;
