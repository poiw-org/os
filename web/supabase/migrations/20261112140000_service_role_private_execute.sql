-- The triggers on public.keys call private.is_admin() and friends; service_role (edge functions)
-- needs EXECUTE on them as well as USAGE on the schema.
grant execute on all functions in schema private to service_role;
alter default privileges in schema private grant execute on functions to service_role;
