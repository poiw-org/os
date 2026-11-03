-- v1 keys: keys imported from the Firestore setup. Their cards still carry a token whose jti is the
-- old Firestore document id, so the door reports (and blacklists) them by that id until the card
-- is rewritten. legacy_id ties those ids to the key.

alter table public.keys add column legacy_id text unique;
comment on column public.keys.legacy_id is 'Firestore document id of a v1 key; the jti on cards not yet rewritten.';

update public.keys k set legacy_id = d.legacy_id
from (values
	('e2:55:e6:24', '1kPLmtSMb8wIH7QI'), ('124578963', '2RTBb59qWavSBM9X'),
	('53:bd:57:a1', '4CuMxY4Gy1VivWw9'), ('93:c1:01:11', '7IqgqBt5E4FCJose'),
	('2a:a7:87:80', 'AxaK7lqU72nGuo8n'), ('13:b4:64:f7', 'B2pO1djU404-7bdQ'),
	('13:e8:1a:11', 'C-lDM7B2wGY7nURY'), ('0a:af:1a:80', 'DKRk1NiGMEGkSEkB'),
	('9a:4c:f8:a5', 'EQMAeqiIjC8H9C-T'), ('3a:14:86:80', 'Hm43uqtkpIOqYbFN'),
	('2a:4b:50:80', 'JtoVPZLYREjKfiNb'), ('fc:49:dc:35', 'LS97Ndw4USk-qsMZ'),
	('6a:ef:61:7f', 'LWAVpWv7MWHRqRE_'), ('a3:b5:26:83', 'MY--s1dyAjLW7Ct1'),
	('1a:d2:f0:80', 'MZmKhLw4UBo4_v6n'), ('2a:62:4e:80', 'Mxfd1uGaQREHwGHm'),
	('3a:15:a3:80', 'R2g0qhuKYPQ-bFQh'), ('53:fb:7b:f7', 'RrFUvTroXgdDAqYf'),
	('2a:45:0a:80', 'SbhatCqZHCjY7N74'), ('13:2d:16:11', 'cAvPn5YhHFUUBlbu'),
	('a3:12:5a:f7', 'cQ6ZcooXgQoQah3I'), ('13:7c:4c:f7', 'cqjEqhfnLaxY7mwZ'),
	('3a:31:01:80', 'd8vyErE8WCeb8TfL'), ('c9:5c:26:83', 'fe_oHhutCj_hwjOn'),
	('11111111', 'hYnr6iobtUOzJIWe'), ('1a:ea:73:80', 'kZidicDCWNdJFJKB'),
	('2a:bf:a7:80', 'ltS1hl76g-Dsdqs6'), ('93:6f:12:11', 'nURrUnGibDgvSVgC'),
	('b3:3e:4f:f7', 'oNZwod9wtE_css77'), ('73:90:58:f7', 'oZamYyXlWtUnuuXS'),
	('1a:eb:fe:80', 'pGTUXe04SqHB_qZO'), ('d3:cf:d2:1c', 'r8pwF0U9wZtXMnV7'),
	('1a:b3:9b:80', 'x8VWL-eFZBMol-vK'), ('23:5a:6c:f7', 'yKeFCM0uHqgxikCY'),
	('53:99:0d:11', 'yvUQ4_-5rpZkIZpo'), ('5c:bf:e2:35', 'zM3n16L4QK_BRouT'),
	('0a:78:6e:80', 'zfbm7MTIpBFqXIMw')
) as d(serial_number, legacy_id)
where k.serial_number = d.serial_number;

-- Entries always name a key by its current id; ids of keys deleted before the import stay as they are.
update public.entries e set key_id = k.id::text
from public.keys k where e.key_id = k.legacy_id;

-- ---------------------------------------------------------------- door_log: translate v1 ids

create or replace function public.door_log(entries jsonb) returns integer
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
	select door, e.type, e.reason, coalesce(k.id::text, e.key_id), e.serial_number, coalesce(e.metadata, '{}'::jsonb),
	       -- A clock that is wrong in the future must not poison the feed; the past is kept as is.
	       case when e.created_at > now() + interval '1 hour' then now() else coalesce(e.created_at, now()) end
	from jsonb_to_recordset(entries) as e(
		type text, reason text, key_id text, serial_number text, metadata jsonb, created_at timestamptz)
	left join public.keys k on k.legacy_id = e.key_id
	on conflict (door_id, created_at, key_id, serial_number) do nothing;

	get diagnostics inserted = row_count;
	return inserted;
end
$$;

-- ---------------------------------------------------------------- door_sync: blacklist v1 ids too

-- A disabled v1 key is listed under both ids, since its card may carry either token.
create or replace function public.door_sync() returns jsonb
	language plpgsql stable set search_path = ''
as $$
begin
	if (select private.door_id()) is null then
		raise exception 'not a door' using errcode = '42501';
	end if;
	return (
		with blocked as (
			select id::text as id, expires_at from public.keys where disabled and expires_at > now()
			union all
			select legacy_id, expires_at from public.keys
			where disabled and expires_at > now() and legacy_id is not null
		)
		select jsonb_build_object(
			'now', extract(epoch from now())::bigint,
			'total', (select count(*) from blocked),
			'blocked', coalesce(
				(select jsonb_agg(jsonb_build_array(b.id, extract(epoch from b.expires_at)::bigint)
					order by b.expires_at desc)
				 from (select * from blocked order by expires_at desc limit 200) b),
				'[]'::jsonb))
	);
end
$$;
