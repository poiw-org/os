-- Who published a build, now that `make release` acts on a person's behalf, and who may do so:
-- admins and members with an active key. Approving stays with admins (approve_firmware).
alter table public.firmware_releases
	add column uploaded_by uuid references public.profiles (id) on delete set null;
create index firmware_releases_uploaded_by_idx on public.firmware_releases (uploaded_by);

create function public.may_release_firmware(subject uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (select 1 from public.admins a where a.user_id = subject)
		or exists (
			select 1 from public.keys k
			join public.profiles p on p.id = k.user_id
			where k.user_id = subject and not p.blocked and not k.disabled
				and now() >= k.not_before and now() < k.expires_at
		)
$$;
revoke execute on function public.may_release_firmware(uuid) from public, anon, authenticated;
grant execute on function public.may_release_firmware(uuid) to service_role;
