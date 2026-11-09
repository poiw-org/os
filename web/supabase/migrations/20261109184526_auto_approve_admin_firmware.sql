-- An admin's upload is approved by the upload itself. Only firmware-upload calls this, after checking
-- the release token, so there is no session to ask: `uploader` is the token's subject. Returns false
-- when the build has to wait for the portal instead: the uploader is not an admin, or another update
-- is still running.
create function public.auto_approve_firmware(release uuid, uploader uuid) returns boolean
	language plpgsql security definer set search_path = ''
as $$
begin
	if not exists (select 1 from public.admins a where a.user_id = uploader)
		or not private.has_second_factor(uploader)
		or exists (
			select 1 from public.firmware_releases r
			where private.firmware_is_active(r.status, r.approved_at, r.door_state)
		)
	then
		return false;
	end if;
	update public.firmware_releases
		set status = 'approved', approved_by = uploader, approved_at = now()
		where id = release and status = 'pending' and uploaded_by = uploader;
	if not found then
		return false;
	end if;
	-- Only a wake-up: the door asks door_firmware() what is really approved.
	perform realtime.send(
		jsonb_build_object('id', release, 'ts', extract(epoch from now()) * 1000),
		'firmware_update', 'door:' || d.id, true)
	from public.doors d;
	return true;
end
$$;
revoke execute on function public.auto_approve_firmware(uuid, uuid) from public, anon, authenticated;
grant execute on function public.auto_approve_firmware(uuid, uuid) to service_role;
