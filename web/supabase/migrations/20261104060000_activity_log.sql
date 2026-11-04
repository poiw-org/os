-- Audit trail for the user timeline: who changed what about a user, and when.
--
-- Rows are written only by triggers on the tables that change (never by the browser), so every
-- change is logged no matter which path made it: the backoffice, a member, an edge function or
-- the sync from auth.users. The actor is the signed-in caller; calls through the service role
-- have none, so the keys edge function stamps issued_by / renewed_by on the row and the trigger
-- reads them from there. Door opens stay in public.entries; the timeline merges both.

create table public.activity (
	id bigint generated always as identity primary key,
	created_at timestamptz not null default now(),
	-- The member the event is about.
	user_id uuid not null references public.profiles (id) on delete cascade,
	-- Who did it; null for the member's own sign-up or automatic changes.
	actor_id uuid references public.profiles (id) on delete set null,
	kind text not null check (kind in (
		'registered', 'onboarded', 'answers_edited', 'user_blocked', 'user_unblocked',
		'admin_granted', 'admin_revoked',
		'key_issued', 'key_renewed', 'key_disabled', 'key_enabled', 'key_tag_released', 'key_deleted'
	)),
	-- Text and no FK: the log must survive key deletion.
	key_id text,
	serial_number text,
	detail jsonb not null default '{}'::jsonb
);
create index activity_user_idx on public.activity (user_id, created_at desc);
create index activity_actor_idx on public.activity (actor_id);

alter table public.activity enable row level security;
create policy "admins read activity" on public.activity
	for select to authenticated using ((select private.is_admin()));
revoke all on public.activity from anon, authenticated;
grant select on public.activity to authenticated;

alter publication supabase_realtime add table public.activity;

alter table public.keys add column renewed_by uuid references public.profiles (id) on delete set null;
create index keys_renewed_by_idx on public.keys (renewed_by);

create function private.log_activity(
	subject uuid, actor uuid, kind text,
	key_id text default null, serial_number text default null, detail jsonb default '{}'::jsonb)
	returns void
	language sql security definer set search_path = ''
as $$
	insert into public.activity (user_id, actor_id, kind, key_id, serial_number, detail)
	select subject, actor, kind, key_id, serial_number, detail
	where exists (select 1 from public.profiles where id = subject)
$$;
revoke execute on function private.log_activity(uuid, uuid, text, text, text, jsonb)
	from public, anon, authenticated;

-- ---------------------------------------------------------------- keys

create function private.log_key_change() returns trigger
	language plpgsql security definer set search_path = ''
as $$
declare
	actor uuid := (select auth.uid());
begin
	if tg_op = 'INSERT' then
		perform private.log_activity(new.user_id, new.issued_by, 'key_issued', new.id::text,
			new.serial_number, jsonb_build_object('expires_at', new.expires_at));
	elsif tg_op = 'DELETE' then
		perform private.log_activity(old.user_id, actor, 'key_deleted', old.id::text, old.serial_number);
	else
		if new.disabled is distinct from old.disabled then
			perform private.log_activity(new.user_id, actor,
				case when new.disabled then 'key_disabled' else 'key_enabled' end,
				new.id::text, new.serial_number);
		end if;
		if new.serial_released and not old.serial_released then
			perform private.log_activity(new.user_id, actor, 'key_tag_released', new.id::text,
				new.serial_number);
		end if;
		if new.expires_at is distinct from old.expires_at or new.not_before is distinct from old.not_before then
			perform private.log_activity(new.user_id, coalesce(actor, new.renewed_by), 'key_renewed',
				new.id::text, new.serial_number, jsonb_build_object('expires_at', new.expires_at));
		end if;
	end if;
	return null;
end
$$;
create trigger log_key_change
	after insert or update or delete on public.keys
	for each row execute function private.log_key_change();

-- ---------------------------------------------------------------- admins

create function private.log_admin_change() returns trigger
	language plpgsql security definer set search_path = ''
as $$
begin
	if tg_op = 'INSERT' then
		perform private.log_activity(new.user_id, (select auth.uid()), 'admin_granted');
	else
		perform private.log_activity(old.user_id, (select auth.uid()), 'admin_revoked');
	end if;
	return null;
end
$$;
create trigger log_admin_change
	after insert or delete on public.admins
	for each row execute function private.log_admin_change();

-- ---------------------------------------------------------------- profiles

create function private.log_profile_change() returns trigger
	language plpgsql security definer set search_path = ''
as $$
declare
	actor uuid := (select auth.uid());
begin
	if tg_op = 'INSERT' then
		perform private.log_activity(new.id, null, 'registered');
		return null;
	end if;
	if new.blocked is distinct from old.blocked then
		perform private.log_activity(new.id, actor,
			case when new.blocked then 'user_blocked' else 'user_unblocked' end);
	end if;
	if old.onboarded_at is null and new.onboarded_at is not null then
		perform private.log_activity(new.id, null, 'onboarded');
	elsif actor is not null and actor <> new.id and (
		(new.first_name, new.last_name, new.studies, new.department, new.referral_source, new.diy_familiar)
		is distinct from
		(old.first_name, old.last_name, old.studies, old.department, old.referral_source, old.diy_familiar)
	) then
		perform private.log_activity(new.id, actor, 'answers_edited');
	end if;
	return null;
end
$$;
create trigger log_profile_change
	after insert or update on public.profiles
	for each row execute function private.log_profile_change();

-- ---------------------------------------------------------------- history so far

insert into public.activity (created_at, user_id, kind)
select created_at, id, 'registered' from public.profiles;

insert into public.activity (created_at, user_id, actor_id, kind, key_id, serial_number, detail)
select issued_at, user_id, issued_by, 'key_issued', id::text, serial_number,
	jsonb_build_object('expires_at', expires_at)
from public.keys;
