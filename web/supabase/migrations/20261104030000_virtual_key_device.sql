-- The phone app uses virtual keys (a key with no tag: serial_number is null). A virtual key can be
-- bound to ONE phone: the public half of a P-256 key pair that lives in the phone's Secure Enclave /
-- Keystore. The key's credential JWT carries that public key, so the door can check, offline, that
-- whoever presents the credential also holds the private key.
--
-- Only users who hold a virtual key can use the app. Disabling the key revokes the phone through the
-- same blacklist as any other key. Binding is done by the `ble-credential` edge function (service
-- role); no browser role has a column grant on the new columns.
--
-- Expand only: existing keys, tokens and doors are untouched, and every existing virtual key simply
-- has no device yet.
-- Rollback: drop the trigger, functions, index, constraints and columns below (nothing else uses them).

-- ---------------------------------------------------------------- device columns

alter table public.keys
	add column device_pubkey text,       -- base64url of the 65-byte uncompressed P-256 point (04 || X || Y)
	add column device_name text,
	add column device_bound_at timestamptz;

-- 65 bytes -> 87 base64url characters, no padding. Whether the point is on the curve is checked
-- by the edge function (WebCrypto) before it is stored.
alter table public.keys
	add constraint keys_device_pubkey_format
		check (device_pubkey is null or device_pubkey ~ '^[A-Za-z0-9_-]{87}$'),
	add constraint keys_device_only_virtual
		check (device_pubkey is null or serial_number is null),
	add constraint keys_device_fields_together
		check ((device_pubkey is null) = (device_bound_at is null)),
	add constraint keys_device_name_length
		check (device_name is null or char_length(device_name) between 1 and 64);

-- One phone key belongs to one key row, even after that key is disabled.
create unique index keys_device_pubkey_key on public.keys (device_pubkey) where device_pubkey is not null;

-- ---------------------------------------------------------------- a binding never changes

-- The credential already issued names this public key (`cnf`). Swapping or clearing it would let a
-- different phone ride on a credential the door still accepts. To move to a new phone, issue a new
-- virtual key and disable the old one.
create function private.guard_device_binding() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if old.device_pubkey is not null
	   and (new.device_pubkey is distinct from old.device_pubkey
	        or new.device_bound_at is distinct from old.device_bound_at) then
		raise exception 'A key''s device cannot be changed; issue a new virtual key instead'
			using errcode = '42501';
	end if;
	return new;
end
$$;
create trigger guard_device_binding
	before update of device_pubkey, device_bound_at on public.keys
	for each row execute function private.guard_device_binding();

-- ---------------------------------------------------------------- who may use the app

-- The app works only for users who hold a usable virtual key: not disabled, inside its validity
-- window, and the user is not blocked. Checked when the app signs in, enrols and refreshes; once a
-- phone holds a credential it keeps unlocking offline until that credential expires.
create function private.can_use_app_as(actor uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (
		select 1 from public.keys k
		join public.profiles p on p.id = k.user_id
		where k.user_id = actor and k.serial_number is null
		  and not p.blocked and not k.disabled
		  and now() >= k.not_before and now() < k.expires_at
	)
$$;
revoke execute on function private.can_use_app_as(uuid) from public, anon, authenticated;

-- For the app: the caller asks about themself.
create function public.can_use_app() returns boolean
	language sql stable security definer set search_path = ''
as $$ select private.can_use_app_as((select auth.uid())) $$;
revoke execute on function public.can_use_app() from public, anon;
grant execute on function public.can_use_app() to authenticated;

-- For the edge function, which has already authenticated the caller.
create function public.can_use_app_as(actor uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$ select private.can_use_app_as(actor) $$;
revoke execute on function public.can_use_app_as(uuid) from public, anon, authenticated;
grant execute on function public.can_use_app_as(uuid) to service_role;
