-- The door controller (ESP8266) signs in as an ordinary Supabase user that is mapped to a door in
-- public.door_users. Like admins, "being a door" is a row, so revoking a device is one delete (or
-- banning the user), effective immediately.
--
-- A door can: read keys (to keep its blacklist), insert entries for its own door, listen on its
-- realtime channel `door:<id>` for force-open commands and answer there. Nothing else.

-- ---------------------------------------------------------------- door accounts

create table public.door_users (
	user_id uuid primary key references auth.users (id) on delete cascade,
	door_id text not null references public.doors (id) on delete cascade
);
alter table public.door_users enable row level security;
create policy "door reads own mapping" on public.door_users
	for select to authenticated using (user_id = (select auth.uid()));
grant select on public.door_users to authenticated;

create function private.door_id() returns text
	language sql stable security definer set search_path = ''
as $$
	select door_id from public.door_users where user_id = (select auth.uid())
$$;
revoke execute on function private.door_id() from public, anon;
grant execute on function private.door_id() to authenticated;

-- ---------------------------------------------------------------- read-only access to keys

-- Doors need rows (not columns) of keys for the blacklist and for realtime key changes. They hold
-- the signing secret already, so the stored tokens reveal nothing new.
create policy "doors read keys" on public.keys
	for select to authenticated using ((select private.door_id()) is not null);

create policy "doors read their own door" on public.doors
	for select to authenticated using (id = (select private.door_id()));

-- ---------------------------------------------------------------- write access to entries

-- The door retries uploads after outages. This index makes a repeated upload a no-op
-- (the door posts with on_conflict + ignore-duplicates). Two events of the same key, tag and door
-- within one second are the same event.
create unique index entries_event_unique
	on public.entries (door_id, created_at, key_id, serial_number) nulls not distinct;

create policy "doors log their own entries" on public.entries
	for insert to authenticated with check (door_id = (select private.door_id()));
grant insert (door_id, type, reason, key_id, serial_number, metadata, created_at)
	on public.entries to authenticated;

-- ---------------------------------------------------------------- blacklist snapshot

-- One compact document: server time (a clock source when NTP is blocked) and the disabled keys
-- that have not expired yet, longest-lived first. The door stores at most 200.
create function public.door_sync() returns jsonb
	language plpgsql stable set search_path = ''
as $$
begin
	if (select private.door_id()) is null then
		raise exception 'not a door' using errcode = '42501';
	end if;
	return jsonb_build_object(
		'now', extract(epoch from now())::bigint,
		'total', (select count(*) from public.keys where disabled and expires_at > now()),
		'blocked', coalesce(
			(select jsonb_agg(jsonb_build_array(k.id, extract(epoch from k.expires_at)::bigint)
				order by k.expires_at desc)
			 from (select id, expires_at from public.keys
			       where disabled and expires_at > now()
			       order by expires_at desc limit 200) k),
			'[]'::jsonb)
	);
end
$$;
revoke execute on function public.door_sync() from public, anon;
grant execute on function public.door_sync() to authenticated;

-- ---------------------------------------------------------------- realtime channel `door:<id>`

-- Private channels are authorised through realtime.messages. Admins may talk on any door channel
-- (send force_open, hear the acknowledgement); a door only on its own.
create policy "admins listen on door channels" on realtime.messages
	for select to authenticated
	using ((select private.is_admin()) and (select realtime.topic()) like 'door:%'
	       and realtime.messages.extension in ('broadcast', 'presence'));
create policy "admins send on door channels" on realtime.messages
	for insert to authenticated
	with check ((select private.is_admin()) and (select realtime.topic()) like 'door:%'
	            and realtime.messages.extension = 'broadcast');

create policy "doors listen on their channel" on realtime.messages
	for select to authenticated
	using ((select realtime.topic()) = 'door:' || (select private.door_id())
	       and realtime.messages.extension in ('broadcast', 'presence'));
create policy "doors send on their channel" on realtime.messages
	for insert to authenticated
	with check ((select realtime.topic()) = 'door:' || (select private.door_id())
	            and realtime.messages.extension = 'broadcast');

-- Force-open is now a realtime broadcast, which is never stored, so a command can never be
-- replayed after an outage. The old flag column is no longer read by anything.
comment on column public.doors.force_open is 'Deprecated: force open is sent over realtime (door:<id>).';
