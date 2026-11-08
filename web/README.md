# Doora web

This is the operating system of po/iw, the students' makerspace at HMU. It has two jobs. It is the public front page of the hackerspace, and behind the login (`/os`) it is where members and admins run the space. Today that means access control for the door. The plan is to bring future IoT devices into the same place, so the door is the first thing it manages and not the only one.

It is a static SvelteKit app (Svelte 5, Tailwind 4, TypeScript) that talks straight to Supabase, and it is deployed to Cloudflare Workers as static assets.

## Front page

The home page (`/`) is in Greek and is what visitors see at the hackerspace's address. It says what po/iw is, who runs it, how to find the lab, how to join, how to get in touch, and which projects the group builds (Saffron and Doora). A link in the top bar leads to the OS, or to the login if you are signed out. The mascot, Manousos Mikrotsipakis, also appears on the login screen.

## The OS

- Sign in with email and password, reset a forgotten password, and fill in a short onboarding form on first login.
- Users, keys and per-key pages for admins, with search, date filters and a chart of unlocks over the last 30 days.
- Issue, renew and disable keys. A key is either tied to a tag serial number or virtual (no tag).
- Live entries feed. New taps and key changes arrive over Supabase Realtime.
- Write a key to a tag, either with the door's own reader ("Write at door") or with Web NFC on Android Chrome.
- Force open: an admin sends the open command to the door from the browser. Members use the `unlock` function instead.
- Quick unlock for members from the login screen.
- Activity log of admin changes.

## Layout

```
src/
  routes/       pages: home, login, onboarding, reset-password, and /os (users, keys, issuing)
  lib/          api and auth helpers, realtime, NFC and serial parsing, types
    auth/       login shell with the mascot and the dithered background
    header/     top bar, menus and the door, tag and write-card modals
    ui/         small shared components (avatar, badges, charts, pickers)
static/         logo and other static files
supabase/
  migrations/   the database schema, one file per change, applied in order
  functions/    edge functions: keys, unlock, door-key (and shared helpers)
  templates/    auth email templates (confirmation, recovery)
e2e/            Playwright smoke and visual tests
design.md       the visual system: colours, type, layout rules
```

## Supabase

The schema is in `supabase/migrations`. Tables are `profiles`, `admins`, `keys`, `entries`, `doors`, `door_users` and `activity`, all with row level security. The door has its own role and calls two functions, `door_sync()` and `door_log()`, to fetch the disabled keys and upload its journal.

Edge functions:

| Function | What it does |
| --- | --- |
| `keys` | Admin only. Issues and renews keys by signing an RS256 token. The key pair is derived from the `RSA_KEY_SEED` secret. |
| `door-key` | Gives a signed-in door the public key it needs to check v2 tokens. |
| `unlock` | Lets a member with an active key open the door. Sends the command, waits for the door to confirm, logs it. |

Set the `RSA_KEY_SEED` secret on the Supabase project before deploying the functions. The `security-reminders` function also needs `SECURITY_CRON_SECRET`, `RESEND_API_KEY`, `EMAIL_FROM` and `APP_URL`, and deploys with `--no-verify-jwt`. Its daily schedule reads the Vault secrets `project_url` and `security_cron_secret`.

## Running it

You need Node 22.12 or newer and pnpm. Put `PUBLIC_SUPABASE_URL` and `PUBLIC_SUPABASE_PUBLISHABLE_KEY` in `.env.development` (and `.env.production` for builds). `PUBLIC_SENTRY_DSN` is optional.

```bash
pnpm install
pnpm dev          # http://localhost:3000
```

Other scripts:

```bash
pnpm check        # svelte-check and TypeScript
pnpm lint         # prettier and eslint
pnpm test:e2e     # Playwright smoke tests (desktop)
pnpm test:visual  # Playwright screenshot comparison
pnpm build        # static build into build/
pnpm deploy       # build and deploy with wrangler
```

`MODERNIZATION_PLAN.md` is an old plan for moving off the 2021 stack. It is kept for history.
