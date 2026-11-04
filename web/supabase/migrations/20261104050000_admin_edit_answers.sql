-- Admins may correct a member's onboarding answers from the member's profile page. Members can still
-- only update their own row ("update own nickname"); this policy adds the admin case. Which columns
-- are writable stays governed by the column grants from member_onboarding, and the
-- profiles_onboarded_complete_check constraint still refuses an onboarded profile with a missing answer.
create policy "admins update profiles" on public.profiles
	for update to authenticated
	using ((select private.is_admin())) with check ((select private.is_admin()));
