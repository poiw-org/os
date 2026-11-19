-- OAuth apps that only people with the lab-access role (public.user_roles: an active key) may sign
-- in to. The `oidc` edge function refuses their tokens to anyone else; the consent screen says so
-- first. Admins set it on the Apps page, through the `oauth-clients` edge function (service role).
-- Not a foreign key to auth.oauth_clients: that table is Supabase's; deleting an app removes its row.
create table public.lab_access_apps (
	client_id uuid primary key,
	created_at timestamptz not null default now()
);

alter table public.lab_access_apps enable row level security;
-- App ids are not secret; the consent screen reads whether the app it shows is restricted.
create policy "signed-in users read" on public.lab_access_apps
	for select to authenticated using (true);

revoke all on public.lab_access_apps from public, anon, authenticated;
grant select on public.lab_access_apps to authenticated;
grant all on public.lab_access_apps to service_role;
