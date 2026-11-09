-- For edge functions acting on a caller's session: true only when the caller has a second factor and
-- this session passed it, however it did (authenticator, passkey or email code).
create function public.session_has_second_factor() returns boolean
	language sql stable security definer set search_path = ''
as $$
	select private.has_second_factor((select auth.uid())) and private.session_passed_second_factor()
$$;
revoke execute on function public.session_has_second_factor() from public, anon;
grant execute on function public.session_has_second_factor() to authenticated;
