-- v1 tokens carry the key's legacy id as their jti: such a key still accounts for its tag.
create or replace function private.record_unidentified_tag() returns trigger
language plpgsql security definer set search_path = '' as $$
declare
  ident text := private.tag_identity(new);
begin
  if new.opened_by is not null or ident is null
     or (new.key_id is not null and exists (
           select 1 from public.keys k where k.id::text = new.key_id or k.legacy_id = new.key_id)) then
    return null;
  end if;
  insert into public.unidentified_tags as t (id, serial_number, claims, first_seen, last_seen, last_reason)
  values (ident, new.serial_number, new.metadata -> 'claims', new.created_at, new.created_at, new.reason)
  on conflict (id) do update set
    sightings = t.sightings + 1,
    serial_number = coalesce(excluded.serial_number, t.serial_number),
    claims = coalesce(excluded.claims, t.claims),
    last_seen = greatest(t.last_seen, excluded.last_seen),
    last_reason = excluded.last_reason;
  return null;
end $$;

delete from public.unidentified_tags t
 where t.user_id is null and exists (select 1 from public.keys k where k.legacy_id = t.id);
