-- Opening the door from the web UI is logged as an entry of the admin who gave the command.
--
-- Two parties know about a web open: the browser (who asked, and that the door confirmed) and the
-- door (that it actually opened, journaled with metadata.forced and uploaded later). They are
-- merged into one row, whichever arrives first:
--   * log_web_open (browser, after the door's acknowledgement) inserts the row with opened_by,
--     or claims the door's forced row if that was uploaded first;
--   * door_log skips a forced event that a web row already accounts for, marking it confirmed.
-- A forced open nobody claimed (the browser closed before logging) still appears, without an actor.

alter table public.entries
	add column opened_by uuid references public.profiles (id) on delete set null;
create index entries_opened_by_idx on public.entries (opened_by, created_at desc)
	where opened_by is not null;

create function public.log_web_open(door text, command_id text, latency_ms integer)
	returns bigint
	language plpgsql security definer set search_path = ''
as $$
declare
	meta jsonb := jsonb_build_object('source', 'web', 'command_id', command_id, 'latency_ms', latency_ms);
	entry_id bigint;
begin
	if not (select private.is_admin()) then
		raise exception 'only admins open the door' using errcode = '42501';
	end if;
	if not exists (select 1 from public.doors where id = door) then
		raise exception 'unknown door' using errcode = '22023';
	end if;

	-- The same command logged twice (a retried request) is one entry.
	select id into entry_id from public.entries
	where door_id = door and metadata->>'command_id' = command_id;
	if found then return entry_id; end if;

	-- The door uploaded its forced event first: claim the nearest unclaimed one.
	update public.entries set opened_by = (select auth.uid()),
		metadata = metadata || meta || '{"door_confirmed":true}'::jsonb
	where id = (
		select id from public.entries
		where door_id = door and opened_by is null and (metadata->>'forced')::boolean
		  and created_at between now() - interval '2 minutes' and now() + interval '2 minutes'
		order by abs(extract(epoch from created_at - now()))
		limit 1
	)
	returning id into entry_id;
	if found then return entry_id; end if;

	insert into public.entries (door_id, type, opened_by, metadata)
	values (door, 'successful', (select auth.uid()), meta)
	returning id into entry_id;
	return entry_id;
end
$$;
revoke execute on function public.log_web_open(text, text, integer) from public, anon;
grant execute on function public.log_web_open(text, text, integer) to authenticated;

create or replace function public.door_log(entries jsonb) returns integer
	language plpgsql security definer set search_path = ''
as $$
declare
	door text := (select private.door_id());
	inserted integer;
	forced record;
	claimed bigint;
	skip bigint[] := '{}';
begin
	if door is null then
		raise exception 'not a door' using errcode = '42501';
	end if;
	if jsonb_typeof(entries) is distinct from 'array' or jsonb_array_length(entries) > 100 then
		raise exception 'entries must be an array of at most 100 events' using errcode = '22023';
	end if;

	-- A forced open the browser already logged confirms that row instead of adding another.
	for forced in
		select a.ord, coalesce(e.created_at, now()) as at, e.metadata
		from jsonb_array_elements(entries) with ordinality as a(value, ord),
		     jsonb_to_record(a.value) as e(created_at timestamptz, metadata jsonb)
		where (e.metadata->>'forced')::boolean
	loop
		-- A retried batch: this event was merged on an earlier upload.
		select id into claimed from public.entries
		where door_id = door and metadata->>'source' = 'web'
		  and (metadata->>'door_at')::timestamptz = forced.at;
		if claimed is null then
			update public.entries set metadata = metadata || jsonb_build_object(
					'door_confirmed', true, 'forced', true, 'door_at', forced.at, 'clock', forced.metadata->'clock')
			where id = (
				select id from public.entries
				where door_id = door and metadata->>'source' = 'web' and not metadata ? 'door_confirmed'
				  and created_at between forced.at - interval '2 minutes' and forced.at + interval '2 minutes'
				order by abs(extract(epoch from created_at - forced.at))
				limit 1
			)
			returning id into claimed;
		end if;
		if claimed is not null then skip := skip || forced.ord; end if;
	end loop;

	insert into public.entries (door_id, type, reason, key_id, serial_number, metadata, created_at)
	select door, e.type, e.reason, e.key_id, e.serial_number, coalesce(e.metadata, '{}'::jsonb),
	       -- A clock that is wrong in the future must not poison the feed; the past is kept as is.
	       case when e.created_at > now() + interval '1 hour' then now() else coalesce(e.created_at, now()) end
	from jsonb_array_elements(entries) with ordinality as a(value, ord),
	     jsonb_to_record(a.value) as e(
		type text, reason text, key_id text, serial_number text, metadata jsonb, created_at timestamptz)
	where a.ord <> all (skip)
	on conflict (door_id, created_at, key_id, serial_number) do nothing;

	get diagnostics inserted = row_count;
	return inserted + coalesce(array_length(skip, 1), 0);
end
$$;
