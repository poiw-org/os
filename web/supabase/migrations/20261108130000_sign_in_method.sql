-- The email-first login, and the rules that make it more than a form: what the page hides, an
-- attacker can still call directly, so the second factor is enforced here.

-- Anyone with a passkey signs in with it; everyone else uses a password. Only the sign-in-method
-- edge function may ask, after checking the captcha.
create function public.should_offer_passkey(address text) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (
		select 1 from auth.users u
		join auth.webauthn_credentials c on c.user_id = u.id
		where lower(u.email) = lower(btrim(address))
	)
$$;
revoke execute on function public.should_offer_passkey(text) from public, anon, authenticated;
grant execute on function public.should_offer_passkey(text) to service_role;

-- Once someone has a factor, a session counts only if it used one. Before, a passkey-only member's
-- password or email link alone could open the door. Nobody without a factor is affected: that is
-- the grace period, and security_sweep ends it.
create or replace function private.session_passed_second_factor() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select not private.has_second_factor((select auth.uid()))
		or (select auth.jwt() ->> 'aal') = 'aal2'
		or exists (select 1 from jsonb_array_elements(coalesce((select auth.jwt() -> 'amr'), '[]'::jsonb)) method
		           where coalesce(method ->> 'method', method #>> '{}') = 'passkey')
$$;

-- `required` now ignores keys a person disabled and `enforced` ends the grace period: the app stops
-- being usable until a factor is registered. It doesn't wait for the daily job to suspend the keys.
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
			          and k.expires_at > now()) as suspended
		from me
	)
	select jsonb_build_object(
		'has_factor', has_factor,
		'suspends_at', suspends_at,
		'required', not has_factor and protects_something,
		'enforced', not has_factor and protects_something
			and (suspended or coalesce(suspends_at <= now(), false))
	)
	from state
$$;
