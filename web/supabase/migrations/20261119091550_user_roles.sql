-- The roles each user holds, worked out from what admins assign, so they are never out of date:
--   admin       a row in public.admins
--   lab-access  a key that opens the door right now: not disabled (suspension disables it), inside
--               its validity window, and the user is not blocked
-- Apps that sign people in with their po/iw account get these as the ID token's `roles` claim
-- (the `oidc` edge function). security_invoker keeps the tables' row level security: members see
-- their own roles, admins everyone's.
create view public.user_roles with (security_invoker = true) as
	select user_id, 'admin'::text as role from public.admins
	union
	select k.user_id, 'lab-access'
	from public.keys k
	join public.profiles p on p.id = k.user_id
	where not k.disabled and not p.blocked and now() >= k.not_before and now() < k.expires_at;

revoke all on public.user_roles from public, anon;
grant select on public.user_roles to authenticated, service_role;
