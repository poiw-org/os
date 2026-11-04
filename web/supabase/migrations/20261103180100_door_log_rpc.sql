-- Doors upload their event journal through this function instead of inserting into entries.
--
-- Why: uploads are retried after outages, so the door must be able to say "ignore duplicates", and
-- Postgres only allows ON CONFLICT DO NOTHING when the caller can also SELECT the conflicting row.
-- Granting doors SELECT on entries would let a stolen door credential read the whole access log.
-- The function keeps doors strictly write-only: it takes the door id from the caller's identity
-- (never from the payload) and returns only a count.

drop policy "doors log their own entries" on public.entries;
revoke insert on public.entries from authenticated;

create function public.door_log(entries jsonb) returns integer
	language plpgsql security definer set search_path = ''
as $$
declare
	door text := (select private.door_id());
	inserted integer;
begin
	if door is null then
		raise exception 'not a door' using errcode = '42501';
	end if;
	if jsonb_typeof(entries) is distinct from 'array' or jsonb_array_length(entries) > 100 then
		raise exception 'entries must be an array of at most 100 events' using errcode = '22023';
	end if;

	insert into public.entries (door_id, type, reason, key_id, serial_number, metadata, created_at)
	select door, e.type, e.reason, e.key_id, e.serial_number, coalesce(e.metadata, '{}'::jsonb),
	       -- A clock that is wrong in the future must not poison the feed; the past is kept as is.
	       case when e.created_at > now() + interval '1 hour' then now() else coalesce(e.created_at, now()) end
	from jsonb_to_recordset(entries) as e(
		type text, reason text, key_id text, serial_number text, metadata jsonb, created_at timestamptz)
	on conflict (door_id, created_at, key_id, serial_number) do nothing;

	get diagnostics inserted = row_count;
	return inserted;
end
$$;
revoke execute on function public.door_log(jsonb) from public, anon;
grant execute on function public.door_log(jsonb) to authenticated;
