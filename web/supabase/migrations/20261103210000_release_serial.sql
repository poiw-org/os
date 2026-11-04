-- A disabled key can release its tag's serial number so another key can be issued on that tag.
-- A released key keeps its row and its place in the door's blacklist, but no longer owns the serial.
-- It can be enabled again only if nobody has taken the serial in the meantime; enabling then
-- reclaims it.

alter table public.keys add column serial_released boolean not null default false;
alter table public.keys add constraint keys_released_is_disabled check (not serial_released or disabled);

alter table public.keys drop constraint keys_serial_number_key;
create unique index keys_serial_number_key on public.keys (serial_number) where not serial_released;

create function private.guard_serial_release() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if new.serial_released and not new.disabled and not old.serial_released then
		raise exception 'Disable the key before releasing its tag' using errcode = 'P0001';
	end if;
	if old.serial_released and new.serial_released and old.disabled and not new.disabled then
		if exists (select 1 from public.keys k
		           where k.serial_number = new.serial_number and not k.serial_released and k.id <> new.id) then
			raise exception 'This key''s tag now belongs to another key. Remove that key first.'
				using errcode = 'P0001';
		end if;
		new.serial_released := false;
	end if;
	return new;
end
$$;
create trigger guard_serial_release
	before update of disabled, serial_released on public.keys
	for each row execute function private.guard_serial_release();

grant update (serial_released) on public.keys to authenticated;
