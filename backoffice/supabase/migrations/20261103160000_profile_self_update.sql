-- Users may edit their own nickname (the display name in the backoffice header). Nothing else.
create policy "update own nickname" on public.profiles
	for update to authenticated
	using (id = (select auth.uid())) with check (id = (select auth.uid()));
grant update (nickname) on public.profiles to authenticated;
