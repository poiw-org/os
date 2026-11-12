-- Edge functions write as service_role; the triggers on public.keys call private.* helpers, and
-- resolving those names needs USAGE on the schema. Without it every service-side key update fails
-- (ble-credential could not bind a phone: "permission denied for schema private").
grant usage on schema private to service_role;
