-- Stream changes to the backoffice. Realtime applies each table's select policy per subscriber,
-- so only admins receive these rows.
alter publication supabase_realtime add table public.profiles, public.keys, public.entries, public.doors;

-- Newest-first activity feed across all keys.
create index if not exists entries_created_at_idx on public.entries (created_at desc);
