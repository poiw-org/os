-- Remote unlock asks public.user_roles (admin or lab-access) instead of repeating the active-key rule,
-- so the door, the ID tokens and the Apps restriction all share one definition.
create or replace function private.can_unlock_as(actor uuid)
returns boolean
language sql
stable security definer
set search_path to ''
as $$
	select exists (
		select 1 from public.user_roles r
		where r.user_id = actor and r.role in ('admin', 'lab-access')
	)
$$;

create or replace function private.can_unlock()
returns boolean
language sql
stable security definer
set search_path to ''
as $$
	select (select private.session_passed_second_factor())
		and private.can_unlock_as((select auth.uid()))
$$;
