-- The door console can also set the speaker's pitch multiplier (`pitch 1.25`, persisted on the
-- door) and play any chime (`play unlock`). The door validates the value again.
alter table public.door_console_commands drop constraint door_console_commands_command_check;
alter table public.door_console_commands add constraint door_console_commands_command_check check (
	command in ('help', 'status', 'show', 'reboot', 'pitch', 'play')
	or command ~ '^pitch [0-9]{1,2}(\.[0-9]{1,3})?$'
	or command ~ '^play (unlock|deny|alert|write)$'
);
