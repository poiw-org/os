-- The portal's second-factor wall was only in the browser: a password alone gave an admin session the
-- API accepted. Being an admin now means this session passed a second factor, so every policy, RPC and
-- door channel gated on is_admin() enforces it. Every admin already has a factor enrolled.
create or replace function private.is_admin() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (select 1 from public.admins a where a.user_id = (select auth.uid()))
		and private.has_second_factor((select auth.uid()))
		and private.session_passed_second_factor()
$$;

create or replace function public.set_admin(target uuid, admin boolean) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	if not (select private.is_admin()) then
		raise exception 'only admins manage admins' using errcode = '42501';
	end if;
	perform 1 from public.admins where user_id = (select auth.uid()) for update;
	if target = (select auth.uid()) then
		raise exception 'ask another admin to change your own role' using errcode = 'P0001';
	end if;
	if not exists (select 1 from public.profiles where id = target) then
		raise exception 'unknown user' using errcode = '22023';
	end if;

	if admin then
		insert into public.admins (user_id) values (target) on conflict do nothing;
	else
		delete from public.admins where user_id = target;
	end if;
end
$$;

-- For firmware-cli-auth, as the caller: anyone with a second factor must have passed it, and admins
-- must have one.
create function public.session_can_release() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select private.session_passed_second_factor()
		and (private.has_second_factor((select auth.uid()))
			or not exists (select 1 from public.admins a where a.user_id = (select auth.uid())))
$$;
revoke execute on function public.session_can_release() from public, anon;
grant execute on function public.session_can_release() to authenticated;

-- A blocked account loses release access even if it is an admin.
create or replace function public.may_release_firmware(subject uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select not exists (select 1 from public.profiles p where p.id = subject and p.blocked)
		and (exists (select 1 from public.admins a where a.user_id = subject)
			or exists (
				select 1 from public.keys k
				where k.user_id = subject and not k.disabled
					and now() >= k.not_before and now() < k.expires_at
			))
$$;

-- An old approval link stops working: a build must be approved within a day of being uploaded.
create or replace function public.approve_firmware(release uuid, expected_sha256 text) returns void
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
