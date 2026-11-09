-- Firmware updates. `firmware-upload` stores a signed image and creates a pending release, an admin
-- approves it in the portal, and the door then downloads it from Storage over its own connection.
-- The door still checks the signature itself, so nothing here can make it run unsigned firmware.

create table public.firmware_releases (
	id uuid primary key default gen_random_uuid(),
	created_at timestamptz not null default now(),
	-- A label from the uploader. It is not part of the signed image; sha256 identifies the build.
	version text not null,
	sha256 text not null check (sha256 ~ '^[0-9a-f]{64}$'),
	size integer not null check (size > 0),
	status text not null default 'pending' check (status in ('pending', 'approved', 'rejected')),
	approved_by uuid references public.profiles (id) on delete set null,
	approved_at timestamptz,
	-- Reported by the door once a release is approved.
	door_state text check (door_state in ('downloading', 'rebooting', 'installed', 'rolled_back', 'failed')),
	door_error text,
	door_updated_at timestamptz
);
create index firmware_releases_created_idx on public.firmware_releases (created_at desc);
create index firmware_releases_approved_by_idx on public.firmware_releases (approved_by);

alter table public.firmware_releases enable row level security;
create policy "admins read firmware releases" on public.firmware_releases
	for select to authenticated using ((select private.is_admin()));
revoke all on public.firmware_releases from anon, authenticated;
grant select on public.firmware_releases to authenticated;

alter publication supabase_realtime add table public.firmware_releases;

-- What the door may still fetch: approved in the last 30 minutes and not finished.
create function private.firmware_is_active(status text, approved_at timestamptz, door_state text)
	returns boolean
	language sql stable set search_path = ''
as $$
	select status = 'approved' and approved_at > now() - interval '30 minutes'
		and (door_state is null or door_state = 'downloading')
$$;
revoke execute on function private.firmware_is_active(text, timestamptz, text) from public, anon, authenticated;

-- ---------------------------------------------------------------- storage

insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values ('firmware', 'firmware', false, 1966080, array['application/octet-stream']);

create function private.firmware_readable(object_name text) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select (select private.door_id()) is not null and exists (
		select 1 from public.firmware_releases r
		where r.sha256 || '.bin' = object_name
			and private.firmware_is_active(r.status, r.approved_at, r.door_state)
	)
$$;
revoke execute on function private.firmware_readable(text) from public, anon;
grant execute on function private.firmware_readable(text) to authenticated;

create policy "doors read the approved firmware" on storage.objects
	for select to authenticated
	using (bucket_id = 'firmware' and (select private.firmware_readable(name)));

-- ---------------------------------------------------------------- admins

create function public.approve_firmware(release uuid) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	if not (select private.is_admin()) then
		raise exception 'not an admin' using errcode = '42501';
	end if;
	if exists (
		select 1 from public.firmware_releases r
		where private.firmware_is_active(r.status, r.approved_at, r.door_state)
	) then
		raise exception 'another update is in progress' using errcode = 'P0001';
	end if;
	update public.firmware_releases
		set status = 'approved', approved_by = (select auth.uid()), approved_at = now()
		where id = release and status = 'pending';
	if not found then
		raise exception 'this release is no longer pending' using errcode = 'P0001';
	end if;
	-- Only a wake-up: the door asks door_firmware() what is really approved.
	perform realtime.send(
		jsonb_build_object('id', release, 'ts', extract(epoch from now()) * 1000),
		'firmware_update', 'door:' || d.id, true)
	from public.doors d;
end
$$;
revoke execute on function public.approve_firmware(uuid) from public, anon;
grant execute on function public.approve_firmware(uuid) to authenticated;

create function public.reject_firmware(release uuid) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	if not (select private.is_admin()) then
		raise exception 'not an admin' using errcode = '42501';
	end if;
	update public.firmware_releases set status = 'rejected' where id = release and status = 'pending';
	if not found then
		raise exception 'this release is no longer pending' using errcode = 'P0001';
	end if;
end
$$;
revoke execute on function public.reject_firmware(uuid) from public, anon;
grant execute on function public.reject_firmware(uuid) to authenticated;

-- ---------------------------------------------------------------- the door

create function public.door_firmware() returns jsonb
	language plpgsql stable security definer set search_path = ''
as $$
begin
	if (select private.door_id()) is null then
		raise exception 'not a door' using errcode = '42501';
	end if;
	return (
		select jsonb_build_object('id', r.id, 'size', r.size, 'sha256', r.sha256)
		from public.firmware_releases r
		where private.firmware_is_active(r.status, r.approved_at, r.door_state)
		order by r.approved_at desc limit 1
	);
end
$$;
revoke execute on function public.door_firmware() from public, anon;
grant execute on function public.door_firmware() to authenticated;

create function public.firmware_report(release uuid, state text, error text default null) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	if (select private.door_id()) is null then
		raise exception 'not a door' using errcode = '42501';
	end if;
	update public.firmware_releases
		set door_state = state, door_error = left(error, 500), door_updated_at = now()
		where id = release and status = 'approved';
end
$$;
revoke execute on function public.firmware_report(uuid, text, text) from public, anon;
grant execute on function public.firmware_report(uuid, text, text) to authenticated;
