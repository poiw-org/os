-- The door console can also set the speaker volume (`volume 60`, percent, persisted on the door).
alter table public.door_console_commands drop constraint door_console_commands_command_check;
alter table public.door_console_commands add constraint door_console_commands_command_check check (
	command in ('help', 'status', 'show', 'reboot', 'pitch', 'volume', 'play')
	or command ~ '^pitch [0-9]{1,2}(\.[0-9]{1,3})?$'
	or command ~ '^volume [0-9]{1,3}$'
	or command ~ '^play (unlock|deny|alert|write)$'
);
