-- Admins can correct a member's sign-in email. The keys edge function changes auth.users and
-- private.sync_profile copies the address to the profile, so the change reaches the log through the
-- service role, which has no auth.uid(). As with keys.renewed_by, the function stamps the admin on
-- the row first and the trigger reads it from there. Column grants keep the stamp server-only.

alter table public.profiles
	add column email_changed_by uuid references public.profiles (id) on delete set null;
create index profiles_email_changed_by_idx on public.profiles (email_changed_by);

alter table public.activity drop constraint activity_kind_check;
alter table public.activity add constraint activity_kind_check check (kind in (
	'registered', 'onboarded', 'answers_edited', 'email_changed', 'user_blocked', 'user_unblocked',
	'admin_granted', 'admin_revoked',
	'key_issued', 'key_renewed', 'key_disabled', 'key_enabled', 'key_tag_released', 'key_deleted'
));

create or replace function private.log_profile_change() returns trigger
	language plpgsql security definer set search_path = ''
as $$
declare
	actor uuid := (select auth.uid());
begin
	if tg_op = 'INSERT' then
		perform private.log_activity(new.id, null, 'registered');
		return null;
	end if;
	if new.email is distinct from old.email then
		perform private.log_activity(new.id, coalesce(actor, new.email_changed_by), 'email_changed',
			detail => jsonb_build_object('from', old.email, 'to', new.email));
	end if;
	if new.blocked is distinct from old.blocked then
		perform private.log_activity(new.id, actor,
			case when new.blocked then 'user_blocked' else 'user_unblocked' end);
	end if;
	if old.onboarded_at is null and new.onboarded_at is not null then
		perform private.log_activity(new.id, null, 'onboarded');
	elsif actor is not null and actor <> new.id and (
		(new.first_name, new.last_name, new.studies, new.department, new.referral_source, new.diy_familiar)
		is distinct from
		(old.first_name, old.last_name, old.studies, old.department, old.referral_source, old.diy_familiar)
	) then
		perform private.log_activity(new.id, actor, 'answers_edited');
	end if;
	return null;
end
$$;
