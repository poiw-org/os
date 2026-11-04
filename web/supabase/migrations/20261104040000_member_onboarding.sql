-- Member onboarding: a few questions every member answers once, on the first login after
-- registering (existing members get the same prompt on their next login, because onboarded_at
-- starts out null for everyone).

alter table public.profiles
	add column first_name text,
	add column last_name text,
	add column studies boolean,
	add column department text,
	add column referral_source text,
	add column diy_familiar boolean,
	add column onboarded_at timestamptz;

alter table public.profiles
	add constraint profiles_referral_source_check
		check (referral_source in ('friend', 'welcome_day', 'newsletter', 'campaign')),
	-- A profile only counts as onboarded when every answer is in. The server enforces it, so the
	-- form cannot be bypassed by calling the API directly.
	add constraint profiles_onboarded_complete_check
		check (
			onboarded_at is null
			or (
				length(btrim(first_name)) > 0
				and length(btrim(last_name)) > 0
				and referral_source is not null
				and diy_familiar is not null
				and studies is not null
				and (studies = false or length(btrim(department)) > 0)
			)
		);

-- Members answer for themselves; the existing "update own nickname" policy already limits
-- updates to the member's own row. Admins can read everything through "own profile or admin".
grant update (
	first_name, last_name, studies, department, referral_source, diy_familiar, onboarded_at
) on public.profiles to authenticated;
