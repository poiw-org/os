-- The onboarding wizard and the second-factor requirement.
--
-- New members answer the questionnaire before they have an account, so the answers arrive with the
-- sign-up. Anyone who holds a key must register a passkey or an authenticator app: a daily job
-- (security-reminders) reminds them every 3 days and suspends their keys after 14. A suspended key
-- is an ordinary disabled key, because `disabled` is what reaches the door's blacklist;
-- `suspended_at` only records that this job did it, so registering a factor restores those keys
-- and never one that a person disabled.

-- ---------------------------------------------------------------- questionnaire

alter table public.profiles drop constraint profiles_referral_source_check;
alter table public.profiles add constraint profiles_referral_source_check
	check (referral_source in (
		'friend', 'welcome_day', 'newsletter', 'campaign', 'existing member before questionaire'
	));

-- Same sync as before, plus the wizard's answers on sign-up. Writing them with a separate update
-- keeps the 'onboarded' entry in the activity log. Bad answers fail the sign-up: the columns'
-- constraints still decide what counts as complete.
create or replace function private.sync_profile() returns trigger
	language plpgsql security definer set search_path = ''
as $$
declare
	answers jsonb := new.raw_user_meta_data -> 'onboarding';
begin
	insert into public.profiles (id, email, name, nickname, username, auth0_id, blocked)
	values (
		new.id,
		new.email,
		new.raw_user_meta_data ->> 'name',
		new.raw_user_meta_data ->> 'nickname',
		new.raw_user_meta_data ->> 'username',
		new.raw_user_meta_data ->> 'auth0_id',
		coalesce(new.banned_until > now(), false)
	)
	on conflict (id) do update
		set email = excluded.email, blocked = excluded.blocked;

	if tg_op = 'INSERT' and answers is not null then
		if coalesce(answers ->> 'referral_source', '') not in ('friend', 'welcome_day', 'newsletter', 'campaign') then
			raise exception 'invalid referral source' using errcode = '22023';
		end if;
		update public.profiles set
			first_name = btrim(answers ->> 'first_name'),
			last_name = btrim(answers ->> 'last_name'),
			studies = (answers ->> 'studies')::boolean,
			department = case when (answers ->> 'studies')::boolean then btrim(answers ->> 'department') end,
			referral_source = answers ->> 'referral_source',
			diy_familiar = (answers ->> 'diy_familiar')::boolean,
			onboarded_at = now()
		where id = new.id;
	end if;
	return new;
end
$$;

-- ---------------------------------------------------------------- state

-- When the 14 days started, and the last reminder. Only this migration's functions and the
-- service role write them.
alter table public.profiles
	add column security_required_since timestamptz,
	add column security_reminded_at timestamptz;

alter table public.keys
	add column suspended_at timestamptz,
	add constraint keys_suspended_is_disabled check (suspended_at is null or disabled);

-- However a key comes back (a factor registered, an admin re-enabling it), it is no longer suspended.
create function private.clear_key_suspension() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if old.disabled and not new.disabled then new.suspended_at := null; end if;
	return new;
end
$$;
create trigger clear_key_suspension
	before update of disabled on public.keys
	for each row execute function private.clear_key_suspension();

-- ---------------------------------------------------------------- rules

create function private.has_second_factor(target uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (select 1 from auth.webauthn_credentials c where c.user_id = target)
		or exists (select 1 from auth.mfa_factors f
		           where f.user_id = target and f.status = 'verified' and f.factor_type = 'totp')
$$;
revoke execute on function private.has_second_factor(uuid) from public, anon, authenticated;

-- A key that can open the door, or could again once its suspension is lifted.
create function private.holds_live_key(target uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$
	select exists (select 1 from public.keys k
	               where k.user_id = target and k.expires_at > now()
	                 and (not k.disabled or k.suspended_at is not null))
$$;
revoke execute on function private.holds_live_key(uuid) from public, anon, authenticated;

-- A passkey sign-in is aal1 to Supabase but is already a strong factor, so only a password session
-- of a user with an authenticator app still owes the code.
create function private.session_passed_second_factor() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select not exists (select 1 from auth.mfa_factors f
	                   where f.user_id = (select auth.uid()) and f.status = 'verified' and f.factor_type = 'totp')
		or (select auth.jwt() ->> 'aal') = 'aal2'
		or exists (select 1 from jsonb_array_elements(coalesce((select auth.jwt() -> 'amr'), '[]'::jsonb)) method
		           where coalesce(method ->> 'method', method #>> '{}') = 'passkey')
$$;
revoke execute on function private.session_passed_second_factor() from public, anon, authenticated;

create or replace function private.can_unlock() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select (select private.session_passed_second_factor()) and ((select private.is_admin()) or exists (
		select 1 from public.keys k
		join public.profiles p on p.id = k.user_id
		where k.user_id = (select auth.uid()) and not p.blocked and not k.disabled
		  and now() >= k.not_before and now() < k.expires_at
	))
$$;

-- Members may only disable their keys, except to bring back a suspended key once they comply.
create or replace function private.guard_member_key_update() returns trigger
	language plpgsql set search_path = ''
as $$
begin
	if (select auth.uid()) is null or (select private.is_admin()) then return new; end if;
	if old.suspended_at is not null and not new.disabled
		and new.serial_released is not distinct from old.serial_released
		and (select private.member_may_restore((select auth.uid()))) then
		return new;
	end if;
	if not new.disabled or new.serial_released is distinct from old.serial_released then
		raise exception 'You can only disable your own keys' using errcode = '42501';
	end if;
	return new;
end
$$;

-- The guard runs as the caller, who cannot execute has_second_factor directly.
create function private.member_may_restore(target uuid) returns boolean
	language sql stable security definer set search_path = ''
as $$ select private.has_second_factor(target) $$;
revoke execute on function private.member_may_restore(uuid) from public, anon;
grant execute on function private.member_may_restore(uuid) to authenticated;

-- ---------------------------------------------------------------- what the app asks

create function public.security_status() returns jsonb
	language sql stable security definer set search_path = ''
as $$
	select jsonb_build_object(
		'has_factor', private.has_second_factor(me),
		'suspends_at', (select p.security_required_since + interval '14 days' from public.profiles p where p.id = me),
		'required', not private.has_second_factor(me) and (
			exists (select 1 from public.admins a where a.user_id = me)
			or exists (select 1 from public.keys k where k.user_id = me and k.expires_at > now())
		)
	)
	from (select auth.uid() as me) caller
$$;
revoke execute on function public.security_status() from public, anon;
grant execute on function public.security_status() to authenticated;

-- For whoever issues a key: is this member's email verified, do they have a factor, and when would
-- their keys be suspended?
create function public.second_factor_status(target uuid) returns jsonb
	language plpgsql stable security definer set search_path = ''
as $$
begin
	if not ((select auth.role()) = 'service_role' or (select private.is_admin())) then
		raise exception 'Not an admin' using errcode = '42501';
	end if;
	return (
		select jsonb_build_object(
			'email_verified', u.email_confirmed_at is not null,
			'has_factor', private.has_second_factor(target),
			'suspends_at', p.security_required_since + interval '14 days'
		)
		from public.profiles p join auth.users u on u.id = p.id where p.id = target
	);
end
$$;
revoke execute on function public.second_factor_status(uuid) from public, anon;
grant execute on function public.second_factor_status(uuid) to authenticated, service_role;

-- ---------------------------------------------------------------- suspending and restoring

create function private.restore_suspended_keys(target uuid) returns integer
	language plpgsql security definer set search_path = ''
as $$
declare
	restored integer;
begin
	if not private.has_second_factor(target) then return 0; end if;
	update public.keys k set disabled = false
	from public.profiles p
	where p.id = k.user_id and k.user_id = target and k.suspended_at is not null
	  and not k.serial_released and not p.blocked;
	get diagnostics restored = row_count;
	update public.profiles set security_required_since = null, security_reminded_at = null
	where id = target and security_required_since is not null;
	return restored;
end
$$;
revoke execute on function private.restore_suspended_keys(uuid) from public, anon, authenticated;

-- Called right after a member registers a factor; the function checks that they really have one.
create function public.restore_my_keys() returns integer
	language sql security definer set search_path = ''
as $$ select private.restore_suspended_keys((select auth.uid())) $$;
revoke execute on function public.restore_my_keys() from public, anon;
grant execute on function public.restore_my_keys() to authenticated;

-- One pass of the daily job. Returns who to email; the caller stamps security_reminded_at only for
-- reminders that were actually sent. Days are counted on the Athens calendar.
create function public.security_sweep()
	returns table (user_id uuid, email text, kind text, days_left integer)
	language plpgsql security definer set search_path = ''
as $$
declare
	today date := (now() at time zone 'Europe/Athens')::date;
	member uuid;
begin
	-- Catches anyone whose browser never made the restore call.
	for member in
		select distinct k.user_id from public.keys k
		where k.suspended_at is not null and private.has_second_factor(k.user_id)
	loop
		perform private.restore_suspended_keys(member);
	end loop;

	-- Compliant, or nothing left to protect: the clock stops.
	update public.profiles p
	set security_required_since = null, security_reminded_at = null
	where p.security_required_since is not null
	  and (private.has_second_factor(p.id) or not private.holds_live_key(p.id));

	update public.profiles p set security_required_since = now()
	where p.security_required_since is null and not p.blocked
	  and not private.has_second_factor(p.id)
	  and exists (select 1 from public.keys k
	              where k.user_id = p.id and not k.disabled and k.expires_at > now());

	return query
	with overdue as (
		update public.keys k set disabled = true, suspended_at = now()
		from public.profiles p
		where p.id = k.user_id and not k.disabled and k.expires_at > now() and not p.blocked
		  and p.security_required_since is not null
		  and today - (p.security_required_since at time zone 'Europe/Athens')::date >= 14
		returning k.user_id as id
	),
	suspended as (
		select distinct p.id, p.email from public.profiles p join overdue o on o.id = p.id
	),
	reminders as (
		select p.id, p.email,
			14 - (today - (p.security_required_since at time zone 'Europe/Athens')::date) as left_days
		from public.profiles p
		where p.security_required_since is not null and not p.blocked
		  and p.id not in (select o.id from overdue o)
		  and exists (select 1 from public.keys k
		              where k.user_id = p.id and not k.disabled and k.expires_at > now())
		  and (p.security_reminded_at is null
		       or today - (p.security_reminded_at at time zone 'Europe/Athens')::date >= 3)
	)
	select s.id, s.email, 'suspended'::text, 0 from suspended s
	union all
	select r.id, r.email, 'reminder'::text, r.left_days from reminders r;
end
$$;
revoke execute on function public.security_sweep() from public, anon, authenticated;
grant execute on function public.security_sweep() to service_role;

-- ---------------------------------------------------------------- the schedule

create extension if not exists pg_cron;
create extension if not exists pg_net;

-- pg_cron runs in UTC and noon in Athens is 09:00 or 10:00 UTC depending on the season, so it fires
-- at both and the function acts only at 12:00 Athens time. Needs two Vault secrets: project_url and
-- security_cron_secret (the same value as the function's SECURITY_CRON_SECRET).
select cron.schedule('security-reminders', '0 9,10 * * *', $job$
	select net.http_post(
		url := (select decrypted_secret from vault.decrypted_secrets where name = 'project_url')
			|| '/functions/v1/security-reminders',
		headers := jsonb_build_object(
			'Content-Type', 'application/json',
			'x-cron-secret', (select decrypted_secret from vault.decrypted_secrets where name = 'security_cron_secret')
		),
		body := '{}'::jsonb
	)
$job$);
