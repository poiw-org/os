-- Admins can promote other users to admin and demote other admins.
--
-- An admin cannot demote themselves, so the caller always remains an admin and the backoffice can
-- never be left without one. Two admins demoting each other at once would break that, so the
-- caller's own row is locked first: the second call waits, then finds its caller gone and fails.

create policy "admins read admins" on public.admins
	for select to authenticated using ((select private.is_admin()));

create function public.set_admin(target uuid, admin boolean) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	perform 1 from public.admins where user_id = (select auth.uid()) for update;
	if not found then
		raise exception 'only admins manage admins' using errcode = '42501';
	end if;
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
revoke execute on function public.set_admin(uuid, boolean) from public, anon;
grant execute on function public.set_admin(uuid, boolean) to authenticated;
