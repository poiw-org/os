-- A member enrolled with a passkey who can't use it signs in with their password and a code sent to
-- their email. Supabase has no email factor, so confirm-email-code records the password session
-- that also proved the inbox, and that record is what counts as having passed.
create table private.email_verified_sessions (
	session_id uuid primary key references auth.sessions (id) on delete cascade,
	user_id uuid not null references auth.users (id) on delete cascade
);

create function public.mark_session_email_verified(session uuid, target uuid) returns void
	language sql security definer set search_path = ''
as $$
	insert into private.email_verified_sessions (session_id, user_id)
	values (session, target)
	on conflict (session_id) do nothing
$$;
revoke execute on function public.mark_session_email_verified(uuid, uuid) from public, anon, authenticated;
grant execute on function public.mark_session_email_verified(uuid, uuid) to service_role;

create or replace function private.session_passed_second_factor() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select not private.has_second_factor((select auth.uid()))
		or (select auth.jwt() ->> 'aal') = 'aal2'
		or exists (select 1 from jsonb_array_elements(coalesce((select auth.jwt() -> 'amr'), '[]'::jsonb)) method
		           where coalesce(method ->> 'method', method #>> '{}') = 'passkey')
		or exists (select 1 from private.email_verified_sessions v
		           where v.user_id = (select auth.uid())
		             and v.session_id::text = (select auth.jwt() ->> 'session_id'))
$$;

-- Adds `needs_email_code`: a passkey account, without an authenticator to use instead, whose session
-- has not passed.
create or replace function public.security_status() returns jsonb
	language sql stable security definer set search_path = ''
as $$
	with me as (
		select (select auth.uid()) as id
	), state as (
		select
			private.has_second_factor(me.id) as has_factor,
			exists (select 1 from public.admins a where a.user_id = me.id)
				or private.holds_live_key(me.id) as protects_something,
			(select p.security_required_since + interval '14 days'
			 from public.profiles p where p.id = me.id) as suspends_at,
			exists (select 1 from public.keys k
			        where k.user_id = me.id and k.suspended_at is not null
			          and k.expires_at > now()) as suspended,
			exists (select 1 from auth.mfa_factors f
			        where f.user_id = me.id and f.status = 'verified' and f.factor_type = 'totp') as has_totp
		from me
	)
	select jsonb_build_object(
		'has_factor', has_factor,
		'suspends_at', suspends_at,
		'required', not has_factor and protects_something,
		'enforced', not has_factor and protects_something
			and (suspended or coalesce(suspends_at <= now(), false)),
		'needs_email_code', has_factor and not has_totp
			and not (select private.session_passed_second_factor())
	)
	from state
$$;
