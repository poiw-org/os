-- Approving a build someone else uploaded takes a typed acknowledgement, so the portal's warning
-- cannot be skipped by calling the function directly. The uploader's own approval is unchanged.
-- A build whose uploader is unknown counts as someone else's.

drop function public.approve_firmware(uuid, text);

create function public.approve_firmware(
	release uuid,
	expected_sha256 text,
	acknowledgement text default null
) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	if not (select private.is_admin()) then
		raise exception 'not an admin' using errcode = '42501';
	end if;
	if acknowledgement is distinct from 'this is dangerous' and exists (
		select 1 from public.firmware_releases r
		where r.id = release and r.uploaded_by is distinct from (select auth.uid())
	) then
		raise exception 'someone else uploaded this firmware; type the warning to approve it'
			using errcode = 'P0001';
	end if;
	if exists (
		select 1 from public.firmware_releases r
		where private.firmware_is_active(r.status, r.approved_at, r.door_state)
	) then
		raise exception 'another update is in progress' using errcode = 'P0001';
	end if;
	update public.firmware_releases
		set status = 'approved', approved_by = (select auth.uid()), approved_at = now()
		where id = release and status = 'pending' and sha256 = expected_sha256
			and created_at > now() - interval '24 hours';
	if not found then
		raise exception 'this exact firmware is no longer waiting for approval' using errcode = 'P0001';
	end if;
	-- Only a wake-up: the door asks door_firmware() what is really approved.
	perform realtime.send(
		jsonb_build_object('id', release, 'ts', extract(epoch from now()) * 1000),
		'firmware_update', 'door:' || d.id, true)
	from public.doors d;
end
$$;
revoke execute on function public.approve_firmware(uuid, text, text) from public, anon;
grant execute on function public.approve_firmware(uuid, text, text) to authenticated;
