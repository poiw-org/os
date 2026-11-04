-- Security hardening for keys and the web-open log.
--
-- 1. A key's row is the door's only source for its blacklist entry (door_sync lists rows that are
--    disabled and not yet expired). The token on the tag is verified offline and keeps the exp it
--    was signed with, so:
--      * a key may not be deleted while that token can still be valid (deleting a disabled key
--        would drop it from the blacklist and un-revoke the tag);
--      * a key's expires_at may never move earlier (a shortened date never reaches the tag, and
--        a disabled key would leave the blacklist before its token stops working).
--    Ending access early is done by disabling the key.
-- 2. log_web_open is for the admin browser path only. Members are logged by the unlock edge
--    function, through log_web_open_as with the service role, only after the door's ack.

-- ---------------------------------------------------------------- keys: no un-revoking

create or replace function private.guard_key_delete() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if old.expires_at > now() then
		raise exception 'A key can only be deleted after it expires; disable it to revoke access'
			using errcode = '42501';
	end if;
	return old;
end
$$;
drop trigger if exists guard_key_delete on public.keys;
create trigger guard_key_delete
	before delete on public.keys
	for each row execute function private.guard_key_delete();

create or replace function private.guard_key_expiry() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if new.expires_at < old.expires_at then
		raise exception 'A key''s expiry cannot be moved earlier; disable the key instead'
			using errcode = '42501';
	end if;
	return new;
end
$$;
drop trigger if exists guard_key_expiry on public.keys;
create trigger guard_key_expiry
	before update of expires_at on public.keys
	for each row execute function private.guard_key_expiry();

-- ---------------------------------------------------------------- web opens

-- can_unlock for a given user (the edge function acts for a caller it has already authenticated).
create or replace function private.can_unlock_as(actor uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (select 1 from public.admins a where a.user_id = actor) or exists (
		select 1 from public.keys k
		join public.profiles p on p.id = k.user_id
		where k.user_id = actor and not p.blocked and not k.disabled
		  and now() >= k.not_before and now() < k.expires_at
	)
$$;
revoke execute on function private.can_unlock_as(uuid) from public, anon, authenticated;

-- The shared logging logic; callers check permission first.
create or replace function private.record_web_open(
	actor uuid, door text, command_id text, latency_ms integer)
	returns bigint
	language plpgsql security definer set search_path = ''
as $$
declare
	meta jsonb;
	entry_id bigint;
begin
	if actor is null then
		raise exception 'no actor' using errcode = '42501';
	end if;
	if command_id is null or command_id !~* '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$' then
		raise exception 'command_id must be a uuid' using errcode = '22023';
	end if;
	if not exists (select 1 from public.doors where id = door) then
		raise exception 'unknown door' using errcode = '22023';
	end if;
	meta := jsonb_build_object('source', 'web', 'command_id', lower(command_id),
		'latency_ms', least(greatest(coalesce(latency_ms, 0), 0), 60000));

	-- The same command logged twice (a retried request) is one entry.
	select id into entry_id from public.entries
	where public.entries.door_id = door and metadata->>'command_id' = lower(command_id);
	if found then return entry_id; end if;

	-- The door uploaded its forced event first: claim the nearest unclaimed one.
	update public.entries set opened_by = actor,
		metadata = metadata || meta || '{"door_confirmed":true}'::jsonb
	where id = (
		select id from public.entries
		where public.entries.door_id = door and opened_by is null and (metadata->>'forced')::boolean
		  and not metadata ? 'door_confirmed'
		  and created_at between now() - interval '2 minutes' and now() + interval '2 minutes'
		order by abs(extract(epoch from created_at - now()))
		limit 1
	)
	returning id into entry_id;
	if found then return entry_id; end if;

	insert into public.entries (door_id, type, opened_by, metadata)
	values (door, 'successful', actor, meta)
	returning id into entry_id;
	return entry_id;
end
$$;
revoke execute on function private.record_web_open(uuid, text, text, integer)
	from public, anon, authenticated;

-- Browser path: admins only (they open the door over realtime themselves).
create or replace function public.log_web_open(door text, command_id text, latency_ms integer)
	returns bigint
	language plpgsql security definer set search_path = ''
as $$
begin
	if not (select private.is_admin()) then
		raise exception 'only admins log web opens directly' using errcode = '42501';
	end if;
	return private.record_web_open((select auth.uid()), door, command_id, latency_ms);
end
$$;
revoke execute on function public.log_web_open(text, text, integer) from public, anon;
grant execute on function public.log_web_open(text, text, integer) to authenticated;

-- Edge-function path: service role only, for the caller it authenticated, after the door's ack.
create or replace function public.log_web_open_as(
	actor uuid, door text, command_id text, latency_ms integer)
	returns bigint
	language plpgsql security definer set search_path = ''
as $$
begin
	if not private.can_unlock_as(actor) then
		raise exception 'you need an active key to unlock the door' using errcode = '42501';
	end if;
	return private.record_web_open(actor, door, command_id, latency_ms);
end
$$;
revoke execute on function public.log_web_open_as(uuid, text, text, integer)
	from public, anon, authenticated;
grant execute on function public.log_web_open_as(uuid, text, text, integer) to service_role;
