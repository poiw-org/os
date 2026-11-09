-- Commands for the door's console, from the portal. Admins can only add one through
-- send_door_command, so every command is recorded with who sent it; the door hears new rows over
-- realtime. The output comes back over the private door channel, which only admins can hear.

create table public.door_console_commands (
	id uuid primary key default gen_random_uuid(),
	created_at timestamptz not null default now(),
	door_id text not null references public.doors (id) on delete cascade,
	-- The door has its own allowlist too. `config` and `erase` are left out on purpose: they ask for
	-- secrets, and a wrong answer would cut the door off from the network with nobody at its USB port.
	command text not null check (command in ('help', 'status', 'show', 'reboot')),
	created_by uuid references public.profiles (id) on delete set null
);
create index door_console_commands_created_idx on public.door_console_commands (created_at desc);
create index door_console_commands_door_idx on public.door_console_commands (door_id);
create index door_console_commands_created_by_idx on public.door_console_commands (created_by);

alter table public.door_console_commands enable row level security;
create policy "admins read console commands" on public.door_console_commands
	for select to authenticated using ((select private.is_admin()));
create policy "doors read their own console commands" on public.door_console_commands
	for select to authenticated using (door_id = (select private.door_id()));
revoke all on public.door_console_commands from anon, authenticated;
grant select on public.door_console_commands to authenticated;

alter publication supabase_realtime add table public.door_console_commands;

create function public.send_door_command(door text, command text) returns void
	language plpgsql security definer set search_path = ''
as $$
begin
	if not (select private.is_admin()) then
		raise exception 'not an admin' using errcode = '42501';
	end if;
	if (select count(*) from public.door_console_commands c
	    where c.created_at > now() - interval '1 minute') >= 30 then
		raise exception 'too many commands; wait a minute' using errcode = 'P0001';
	end if;
	insert into public.door_console_commands (door_id, command, created_by)
	values (door, command, (select auth.uid()));
end
$$;
revoke execute on function public.send_door_command(text, text) from public, anon;
grant execute on function public.send_door_command(text, text) to authenticated;
