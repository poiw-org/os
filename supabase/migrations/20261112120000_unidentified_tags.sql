-- Tags tapped at the door that no existing key accounts for: the key was deleted, the token did not
-- verify, or the tag carried no token. Admins attach each to the person it belongs to.
create table public.unidentified_tags (
  id text primary key,                 -- the token's jti, else 'sn:' || serial number
  serial_number text,
  claims jsonb,                        -- what the token said about itself (entries.metadata.claims)
  first_seen timestamptz not null,
  last_seen timestamptz not null,
  sightings integer not null default 1,
  last_reason text,
  user_id uuid references public.profiles(id) on delete set null,
  attached_by uuid references public.profiles(id) on delete set null,
  attached_at timestamptz
);
create index unidentified_tags_open on public.unidentified_tags (last_seen desc) where user_id is null;

alter table public.unidentified_tags enable row level security;
create policy "admins read unidentified tags" on public.unidentified_tags
  for select using ((select private.is_admin()));
grant select on public.unidentified_tags to authenticated;
alter publication supabase_realtime add table public.unidentified_tags;

-- How a door entry names its tag; the web app derives the same id (api.ts tagIdentity).
create function private.tag_identity(e public.entries) returns text
language sql immutable as $$
  select coalesce(e.key_id, nullif(e.metadata #>> '{claims,jti}', ''), 'sn:' || e.serial_number)
$$;

create function private.record_unidentified_tag() returns trigger
language plpgsql security definer set search_path = '' as $$
declare
  ident text := private.tag_identity(new);
begin
  if new.opened_by is not null or ident is null
     or (new.key_id is not null and exists (select 1 from public.keys k where k.id::text = new.key_id)) then
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

create trigger entries_unidentified_tag after insert on public.entries
  for each row execute function private.record_unidentified_tag();

-- Attaching only says whose tag it is; it grants no access. A null target detaches it again.
create function public.attach_unidentified_tag(tag text, target uuid) returns void
language plpgsql security definer set search_path = '' as $$
begin
  if not private.is_admin() then raise exception 'Only admins can attach tags.' using errcode = '42501'; end if;
  update public.unidentified_tags
     set user_id = target,
         attached_by = case when target is null then null else auth.uid() end,
         attached_at = case when target is null then null else now() end
   where id = tag;
  if not found then raise exception 'No such tag.' using errcode = 'P0002'; end if;
end $$;
revoke all on function public.attach_unidentified_tag(text, uuid) from public, anon;
grant execute on function public.attach_unidentified_tag(text, uuid) to authenticated;

-- Backfill: tags seen in the last 30 days that no key accounts for.
insert into public.unidentified_tags (id, serial_number, claims, first_seen, last_seen, sightings, last_reason)
select private.tag_identity(e),
       (array_agg(e.serial_number order by e.created_at desc) filter (where e.serial_number is not null))[1],
       (array_agg(e.metadata -> 'claims' order by e.created_at desc) filter (where e.metadata ? 'claims'))[1],
       min(e.created_at), max(e.created_at), count(*), (array_agg(e.reason order by e.created_at desc))[1]
  from public.entries e
 where e.opened_by is null and e.created_at > now() - interval '30 days'
   and private.tag_identity(e) is not null
   and (e.key_id is null or not exists (select 1 from public.keys k where k.id::text = e.key_id))
 group by 1;
