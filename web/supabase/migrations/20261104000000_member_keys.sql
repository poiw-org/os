-- Members (any signed-in user who is not an admin) can see their own keys and disable them, and
-- unlock the door remotely while they hold an active key. Only admins create or change keys.
--
-- Virtual keys: a key without a tag (serial_number is null). It has the same validity window,
-- status and audit trail as a physical key, and it is what gives a member remote unlock without a tag.

alter table public.keys alter column serial_number drop not null;

-- ---------------------------------------------------------------- who may unlock remotely

create function private.can_unlock() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select (select private.is_admin()) or exists (
		select 1 from public.keys k
		join public.profiles p on p.id = k.user_id
		where k.user_id = (select auth.uid()) and not p.blocked and not k.disabled
		  and now() >= k.not_before and now() < k.expires_at
	)
$$;
revoke execute on function private.can_unlock() from public, anon;
grant execute on function private.can_unlock() to authenticated;

create function public.can_unlock() returns boolean
	language sql stable security definer set search_path = ''
as $$ select private.can_unlock() $$;
revoke execute on function public.can_unlock() from public, anon;
grant execute on function public.can_unlock() to authenticated;

-- ---------------------------------------------------------------- members: own keys

create policy "members read own keys" on public.keys
	for select to authenticated using (user_id = (select auth.uid()));
create policy "members disable own keys" on public.keys
	for update to authenticated
	using (user_id = (select auth.uid())) with check (user_id = (select auth.uid()));

create policy "members read own entries" on public.entries
	for select to authenticated
	using (opened_by = (select auth.uid())
	       or key_id in (select k.id::text from public.keys k where k.user_id = (select auth.uid())));

-- The policy above lets a member update their own rows; this limits them to disabling.
-- Calls without a user (the edge function's service role) are not members.
create function private.guard_member_key_update() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if (select auth.uid()) is null or (select private.is_admin()) then return new; end if;
	if not new.disabled or new.serial_released is distinct from old.serial_released then
		raise exception 'You can only disable your own keys' using errcode = '42501';
	end if;
	return new;
end
$$;
create trigger guard_member_key_update
	before update on public.keys
	for each row execute function private.guard_member_key_update();

-- ---------------------------------------------------------------- logging a remote unlock

create or replace function public.log_web_open(door text, command_id text, latency_ms integer)
	returns bigint
	language plpgsql security definer set search_path = ''
as $$
declare
	meta jsonb := jsonb_build_object('source', 'web', 'command_id', command_id, 'latency_ms', latency_ms);
	entry_id bigint;
begin
	if not (select private.can_unlock()) then
		raise exception 'you need an active key to unlock the door' using errcode = '42501';
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
