# Doora backoffice: audit and modernization plan

Scope: `backoffice/frontend` (~960 lines: 7 routes, Header, 2 entities, api/auth helpers).
Goal: current dependencies, sound patterns, shadcn-style components. **Visual design language stays as is.**

---

## 1. Audit

### 1.1 Dependencies

The stack is a pre-1.0 SvelteKit snapshot from mid-2021. Nothing here upgrades in place; the SvelteKit routing and hooks APIs it uses no longer exist.

| Area       | Now                                                       | Problem                                                                                                           | Target                                                   |
| ---------- | --------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| Framework  | `svelte ^3`, `@sveltejs/kit: "next"` (unpinned tag)       | `svelte-kit dev/build`, `__layout.svelte`, `target`, `request`-style hooks and `$page.query` are all removed APIs | Svelte 5 (runes), SvelteKit 2, Vite                      |
| CSS        | `tailwindcss ^2` + `purge` + postcss/cssnano/autoprefixer | EOL; `purge` renamed; Google Fonts via CSS `@import`                                                              | Tailwind 4 (`@tailwindcss/vite`, CSS-first `@theme`)     |
| TS         | `typescript ^4`, non-strict, `importsNotUsedAsValues`     | Flag removed; `any` everywhere                                                                                    | TS 5, `strict: true`                                     |
| HTTP       | `axios ^0.21.1`                                           | Known CVEs (SSRF/ReDoS/XSRF-token leak), global-instance interceptors                                             | native `fetch` wrapper                                   |
| Auth       | `@auth0/auth0-spa-js ^1`, plus unused `auth0-js`          | v1 uses `client_id`/`redirect_uri` at top level; v2 changed the API                                               | `@auth0/auth0-spa-js` v2                                 |
| Monitoring | `@sentry/browser ^6`, `@sentry/tracing`                   | `Integrations.BrowserTracing` removed in v8                                                                       | `@sentry/svelte` current                                 |
| Tooling    | `prettier ~2.2`, no ESLint, no tests, no CI               |                                                                                                                   | Prettier 3, ESLint 9 flat config, Vitest, Playwright, CI |

**Dead or misplaced dependencies** (no imports found): `firebase`, `npm`, `install`, `auth0-js`, `jsbarcode`, `svelte-barcode`, `cookie`, `@lukeed/uuid` (only used by the dead `hooks.ts`), `svelte-simple-modal`, `@fontsource/fira-mono`, `@types/cookie`, `adapter-node`, `adapter-netlify`. Tooling (`prettier`, `svelte-check`, `autoprefixer`, ...) sits in `dependencies` instead of `devDependencies`. `package.json` name is `~TODO~`. **Three lockfiles** (`package-lock.json`, `pnpm-lock.yaml`, `yarn.lock`) mean nobody knows what is actually installed.

### 1.2 Bugs and risks found while reading (fix during the port, not later)

1. **Auth header is probably broken.** `src/lib/api.ts` calls async `getToken()` inside a sync interceptor without `await`, so the header becomes `Bearer [object Promise]`.
2. **`createAuth0Client()` runs on every call** (`authService.js`): every API request, `isAuthenticated()`, `getUser()` builds a new client and may hit Auth0 again. Needs one lazily created singleton.
3. **Response interceptor crashes on network errors** (`error.response.status` when `response` is undefined). It also shows `alert()` and then logs out.
4. **`Key.create()` never resolves** (`key.ts`): `new Promise(async () => ...)` has no `resolve`. Currently unused (the create page calls `API.post` directly), which is itself duplication.
5. **`__layout.svelte` discards its content**: `<main><slot/></main>` sits inside `<Header>`, which has no slot, so only the trailing `<slot>` renders. The layout also initializes Sentry at `tracesSampleRate: 1.0` with the DSN hardcoded.
6. **`renew.svelte` prints `[object Object]`** for the owner (`key.user` is an object; should be `key.user.email`).
7. **Search is a stub** (`search = async () => {}`); the box on the home page does nothing.
8. **`items/index.svelte`**: `export const prerender/hydrate/router` inside the instance script (invalid, ignored), and a `setTimeout(..., 500)` wrapper around load to dodge the auth race.
9. **`hooks.ts` is dead and legacy**: sets a `userid` cookie nothing reads; with `ssr: false` + adapter-static it never runs.
10. **Auth guard is copy-pasted into each page's `onMount`**, so protected content flashes and the guard is easy to forget. `forceOpen.svelte` has no guard at all.
11. **Destructive actions are guarded by native `confirm()`**, and "force open the door" triggers on a plain GET (`API.get('/forceOpen')`; backend contract, flag for the API owner).
12. Config in source: Auth0 domain/client id/audience, API URLs, Sentry DSN, hardcoded `doora.poiw.org` logout URL. These should be env (`PUBLIC_*`).

### 1.3 Patterns

- **Navigation by `window.location =` and `location.reload()`** after nearly every mutation: full reloads, lost state, re-auth round trips. Use `goto`/`invalidate`.
- **`<a on:click>` without `href`, `<span on:click>`, `<div on:click>`** for nav and actions: not focusable, no keyboard, no semantics. Use real `<a href>` / `<button>`.
- **Feedback is `alert()` / `confirm()`** everywhere; no inline errors, no toasts, no disabled/pending button states.
- **NFC code is triplicated** (index, create, items/index) with no abort/cleanup and `//@ts-ignore`. One `$lib/nfc.ts` helper.
- **Entities as classes with `fromJSON` + `declare` fields + active-record methods** (`key.disable()`) that call a global axios. `any` at the boundary, dates hand-parsed. Replace with plain types parsed at the API boundary and a small `keys` API module.
- **Almost no loading/empty/error states**: one spinner (from a spinner library) and one "no keys" message.
- **Duplicated markup**: the key detail label/value block x7, outline-button color variants x6, date range fields x2 (create, renew).
- Inline `<style>` after `</html>` in `app.html` (invalid); print-hiding done with ids.
- `Header` implements the responsive layout twice (desktop and mobile copies of the user block).
- Hardcoded `el-GR` locale in several places.

### 1.4 Design language to preserve (extracted)

- Font: **JetBrains Mono** everywhere.
- Palette: **green-700** primary (teal-700 on forms), red/purple/blue/yellow-500 per-action outlines, gray-50 list rows, gray-400 muted.
- Shape: square-ish **outline buttons** (1px border + same-color text; filled variant for the primary "ISSUE KEY" on mobile), `rounded` rows, small radii.
- Typography: **UPPERCASE tracked labels**, `tracking-widest` titles, light labels over bold values.
- Layout: single column card, `max-width: 1200px`, shadow + radius only at `xl`, footer below.

---

## 2. Target architecture

```
src/
  app.html            # clean shell, font via @fontsource-variable/jetbrains-mono (self-hosted)
  app.css             # Tailwind 4 @import + @theme tokens (the design language, in one place)
  hooks.client.ts     # Sentry init
  lib/
    config.ts         # PUBLIC_* env, typed
    auth.svelte.ts    # Auth0 singleton + reactive { user, ready } state, login/logout/getToken
    api/client.ts     # fetch wrapper: base URL, awaited token, ApiError, 401 handling (redirect, no alert)
    api/keys.ts       # list/get/create/renew/enable/disable/delete, entries, users, forceOpen
    schemas.ts        # Valibot (or Zod) schemas: Key, Entry, User -> inferred types, dates parsed once
    nfc.ts            # isSupported(), readTag(signal), writeTag(jwt) with AbortSignal + typed errors
    components/ui/    # shadcn-svelte components (owned source)
    components/       # KeyRow, Field (label/value), DateRangeFields, UserMenu, AppHeader
  routes/
    +layout.ts        # ssr=false; auth guard here, once (redirect to /login)
    +layout.svelte    # AppHeader, <main>, footer, <Toaster/>
    +page.svelte      # key list + search
    login/+page.svelte
    forceOpen/        # keep old URL working (redirect) if bookmarked
    items/+page.svelte        # keep ?id= URLs for compatibility
    items/create/+page.svelte
    items/renew/+page.svelte
```

Decisions and why:

- **Fresh scaffold, port files in.** The Kit 1-beta to 2 gap is a rewrite of routing/hooks/config; `sv create` + copy is less work and less risk than chained migrations. Use `sv migrate svelte-5` only for component syntax.
- **Plain `fetch`, no HTTP library, no TanStack Query.** About 8 endpoints and no caching needs. SvelteKit `load` with `invalidate()` after mutations covers refetching.
- **Auth guard in `+layout.ts`** removes the copy-pasted `onMount` checks and the content flash (and the 500 ms hack).
- **Schemas at the boundary** instead of entity classes: `Key`/`Entry` become inferred types; `getDateTimeString` becomes a `formatDateTime` util; locale lives in one constant.
- **Keep `?id=` routes** so existing links and bookmarks keep working. Moving to `/items/[id]` is a later, separate decision.

---

## 3. UI kit: shadcn-svelte (on bits-ui, Melt UI's successor)

You mentioned Melt UI. Recommendation: use **shadcn-svelte**. It is the shadcn model you asked for (components copied into `$lib/components/ui`, owned and themeable, Tailwind-based), and it is built on **bits-ui**, which in turn builds on Melt UI's primitives. Raw Melt UI is headless builders and would mean hand-writing every component's styling and a11y wiring. Check current versions when starting; Melt UI and bits-ui have been reworked for Svelte 5.

Component mapping:

| Today                                                            | Replace with                                                                                                                                       |
| ---------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `confirm()` x4 (enable/disable/delete, double-confirm on delete) | `AlertDialog`; keep the "deleting does NOT disable the key; offline keys can still unlock" warning verbatim, add type-the-key-id to confirm delete |
| Force-open page + `alert`                                        | `AlertDialog` from the header (no separate page needed), toast result                                                                              |
| `alert()` feedback                                               | `Sonner` toasts (success/error), inline `Alert` for page-level failures                                                                            |
| `svelte-select` user picker                                      | `Combobox` (Popover + Command), searchable                                                                                                         |
| `date-picker-svelte` x4                                          | `Popover` + `Calendar` (or `RangeCalendar` for notBefore -> expires) in one `DateRangeFields`                                                      |
| bare `<input>` / `<label>`                                       | `Input`, `Label`, with field-level error text                                                                                                      |
| outline `<button>` x10                                           | `Button` variants (`outline-primary`, `outline-destructive`, ...) mapped to the current colors                                                     |
| ENABLED/DISABLED boxes, success/failure icons                    | `Badge`                                                                                                                                            |
| Spinner (`svelte-loading-spinners`)                              | `Skeleton` rows for list/detail; small spinner only for button pending                                                                             |
| Header user block x2                                             | `DropdownMenu` (avatar -> log out), `Sheet` for the mobile menu                                                                                    |
| Log rows with manual expand                                      | `Collapsible` / `Accordion` (keyboard + aria for free)                                                                                             |
| Home search stub                                                 | `Input` + client-side filter (see section 4)                                                                                                       |
| FontAwesome                                                      | `lucide` (shadcn default). **One visual deviation:** outline vs solid glyphs. Decide before merging; the alternative is a solid icon set           |

Theming: Tailwind 4 `@theme` in `app.css` carries the extracted design language: JetBrains Mono, `--primary` = green-700, destructive red, small radius, uppercase/tracking as component defaults. Components get restyled once in `components/ui`, so screens do not need per-page color classes. Verify with before/after screenshots, not by eye.

---

## 4. UX improvements (per screen, design unchanged)

- **Home**: make search work (client-side filter on email / key id / serial number over the fetched list; if the list is ever paginated server-side, ask for a `q` param), skeleton rows, empty/error states, rows as real links with `focus-visible`. Keep NFC "Scan tag", but show unsupported browsers as a disabled button with an explanation instead of an alert after the click.
- **Key detail**: `Field` component for label/value, copy buttons for id/S/N/JWT, status `Badge`, action buttons disabled while pending, `AlertDialog`s, toast on success and stay on the page (`invalidate`) instead of `location.reload()`. NFC write gets a progress dialog (approach tag -> validating -> writing) with Cancel (AbortController).
- **Create**: validation (S/N format, user required, `notBefore <= expiresAt`), Combobox, pending state, error toast with retry, submit disabled until valid. Today submitting with no user selected sends an empty user.
- **Renew**: fix the `[object Object]` owner, prefill current dates, same date fields component. Confirm the intended `min={today}` rule on notBefore for an already-active key.
- **Force open**: confirm dialog reachable from the header on any page; result toast.
- **Header/shell**: one responsive implementation, user `DropdownMenu`, mobile `Sheet`, active-route state, `<a href>` everywhere, `print:hidden`.
- **Global**: light theme only (dark mode not requested; tokens make it cheap later). Page titles, focus management after dialogs, `prefers-reduced-motion`.

---

## 5. Phased plan

Each phase ends green (`check`, `lint`, build, smoke) and is its own PR.

**Phase 0: safety net**

- **Done.** Kept `package-lock.json` (the only lockfile in sync with `package.json`; `pnpm-lock.yaml` was stale, with no `firebase`), deleted `pnpm-lock.yaml` and `yarn.lock`. Switch to pnpm with the Phase 1 scaffold. `.nvmrc` added (`26`, the version this was verified on).
- **Done.** Removed the 13 dead dependencies and the dead `hooks.ts`, moved tooling to `devDependencies`, pinned `@sveltejs/kit` to `1.0.0-next.154` (the `next` tag would drift), named the package. Clean `npm ci` + build verified: 401 -> 255 packages, same build output files, on Node 26.
- **Done.** `e2e/visual.spec.ts`: baselines for 5 screens x desktop/mobile in `e2e/baseline/` (fixed clock and timezone, mocked API). Update intentionally with `npm run test:visual -- --update-snapshots`. The token list from 1.4 is still prose only.
- **Done.** `e2e/smoke.spec.ts`: 7 flows (list/open, access log, disable/enable, delete, create, renew, force open) with the API mocked via `page.route`. Auth0 is stubbed by seeding the `auth0-spa-js` v1.17 localStorage cache entry (no signature check is made on cached entries, so no JWT signing is needed). The `api` fixture is `auto`, so no test can reach real Auth0/API/Sentry. Run `npm run test:e2e`; on a machine without the matching browser revision set `PW_CHROMIUM_PATH`.
- Caveat: the auth seed depends on `authService.js` config (client id, audience, scope) and on SDK v1 internals. Phase 2 (auth singleton) and the v2 SDK upgrade will need `seedSession` updated or replaced by a test seam.

**Phase 1: toolchain port (no UI changes)**

**Done.** Result: 7 smoke tests + 10 visual comparisons pass against the _unchanged Phase 0 baselines_ (tolerance tightened to 0.2%; the real differences are 7-278 px per page, bold-text anti-aliasing from the variable font). `svelte-check`: 0 errors; ESLint + Prettier clean. Dev server (HTTPS :3000) and prod build verified.

What changed vs. this plan:

- **SvelteKit 3, not 2** (Kit 3.1 / Vite 8 / Svelte 5.57 / Tailwind 4.3 / TS 6). Scaffolded with `sv create`; the adapter and plugins now live in `vite.config.ts` (no `svelte.config.js`). `$lib` is gone: imports use `#lib/...` (package.json `imports`) with explicit `.ts` extensions. Env vars are declared in `src/env.ts` (`defineEnvVars`) and read from `$app/env/public`; values live in `.env.production` / `.env.development` (public values only, un-ignored).
- **`ssr = false` + `prerender = true`**, no SPA fallback: output is one `.html` per route (`items.html`, `forceOpen.html`, ...). The old build emitted `items/index.html`; check that the host serves extensionless URLs (Netlify does).
- **pnpm** adopted; dev server is `vite dev --host --port 3000` with `@vitejs/plugin-basic-ssl` (dev only) to keep the `https://<host>:3000/login` Auth0 callback. Not exercised against real Auth0.
- **Auth0 SDK v2**, Sentry on `@sentry/svelte` (sample rate 1.0 -> 0.1, disabled when no DSN), font self-hosted (`@fontsource-variable/jetbrains-mono`), Svelte 5 runes mode everywhere.
- Tailwind 4 compatibility, all in `app.css`: v2 colour shades pinned in `@theme` (v2's `green` was emerald, `yellow` amber, `purple` violet, so the v4 defaults would have changed the palette), v2 preflight defaults restored (default border colour, pointer cursor, placeholder colour).
- **Finding: teal never rendered.** Tailwind 2 had no `teal`, so `text-teal-700` / `border-teal-700` (Scan tag, S/N input text, CREATE) displayed black text with a default border. To keep the design unchanged the teal namespace is unset. **Decide in Phase 3** whether those become green-700 (consistent with the other primary actions) or teal.
- `@fortawesome/free-solid-svg-icons` is **pinned to 5.15.4** on purpose: v7's solid glyphs are lighter and shift the header by ~2px. Goes away with the Phase 3 icon swap.
- Behaviour deliberately left as-is (Phase 2 items): `getToken()` not awaited, a new Auth0 client per call, `[object Object]` on the renew page, the 500 ms auth hack, full-page `window.location` navigation, triplicated NFC code, and `/login` never redirecting after a successful callback.
- Small cleanups made while porting: dead `entryPopup.svelte`, `hooks.ts`, unused `Key.create()` and the empty search handler removed; forms now call `preventDefault`; `<img alt>` added; `{key.user}` on renew left untouched on purpose.
- Remaining svelte-check warnings (15): the Header's `<a onclick>` elements without `href`; fixed by the Phase 3 header rebuild.

**Supabase migration (done; supersedes the auth/API half of Phase 2)**

Auth0 and `api.poiw.org` are replaced by Supabase (project `yshwchmbsuafribrttlt`) in the web app. The ESP firmware is untouched (it will move to ESP32 first).

- **Source of truth:** `backoffice/supabase/` (`migrations/*.sql`, `functions/keys/index.ts`), applied to the project through the Supabase MCP.
- **Schema:** `profiles` (mirrors `auth.users` via a trigger; keeps `auth0_id`), `admins`, `keys`, `entries`, `doors`. RLS on everything, admins only. The browser can only change `keys.disabled`, delete keys and set `doors.force_open`; dates and the JWT never change from the browser.
- **Edge function `keys`** (`issue` / `renew`): checks the caller is in `admins`, signs the HS256 key JWT (`sub` = user, `jti` = key id, `aud` = tag serial, `iat`/`nbf`/`exp`, the claims the firmware reads) with the `JWT_SIGNING_KEY` secret, writes the row. `verify_jwt` is off at the gateway; the function validates the token itself.
- **Users:** 51 Auth0 users imported (emails, names, usernames, verified flag, 2 blocked users banned, original ids kept in `profiles.auth0_id`). No passwords: everyone uses "Forgot password?". The `donfn` account is the only admin.
- **Web app:** `src/lib/api.ts` (all data access), `auth.ts`, `supabase.ts`; one auth guard in `+layout.ts` replaces the per-page checks and the 500 ms hack; new `/login` and `/reset-password`. Fixed on the way: `Authorization` header bug, the per-call Auth0 client, `[object Object]` on renew, create without a user, hooks cleanup. Key ids are uuids, shown as their first 8 characters.
- **Tests:** Playwright now mocks Supabase (auth, REST, function): 19 smoke tests x 2 viewports + 12 visual comparisons. Not exercised against the live project: sign-in, and the function with the real signing secret.

Manual steps outside the repo (Supabase dashboard): disable public sign-ups, add the redirect URLs (`https://doora.poiw.org/reset-password`, `https://localhost:3000/reset-password`), configure custom SMTP before sending ~50 reset emails, enable leaked-password protection, and make sure `JWT_SIGNING_KEY` equals the ESP's `JWTDecryptioPass` so already-written tags stay valid.

Still open: existing keys and entry logs were not imported (no export provided), and the `serial_number` format stored today is whatever the form sends.

**Phase 2: core patterns**

- `config.ts`, `auth.svelte.ts` singleton (bugs 1, 2), `api/client.ts` (1, 3), `api/keys.ts`, schemas (bug 4 disappears with the entity classes), `+layout.ts` guard (8, 10), `nfc.ts`.
- Replace `window.location`/`reload` with `goto`/`invalidate`; fix bugs 5, 6, 9.
- Unit tests: api client (401, network error, token awaited), schemas (date parsing), nfc helper with a fake `NDEFReader`.

**Phase 3: UI kit and screens**

- `shadcn-svelte init`; configure tokens (section 3); add components as screens need them: Button, Input, Label, Badge, Skeleton, Sonner, AlertDialog, Popover + Calendar, Combobox, DropdownMenu, Sheet, Collapsible.
- Rebuild one screen per PR in this order: Shell/Header, Home, Key detail, Create, Renew (force-open folds into the header). Each includes its section 4 items.
- Remove `svelte-select`, `date-picker-svelte`, `svelte-fa`, `svelte-loading-spinners`, FontAwesome as each is replaced.

**Phase 4: hardening**

- CI (check, lint, unit, e2e), Renovate/Dependabot, a11y pass (axe in Playwright, keyboard-only run of the main flow), real-device NFC test on Chrome Android, README (dev/run/deploy, env vars).

---

## 6. Open questions

1. **Is the API contract changeable?** Wanted: `POST /forceOpen`, a search param or pagination on `/keys`; and confirm what `Authorization` format the backend accepts today (bug 1).
2. **Keep existing URLs (`/items?id=`)?** The plan assumes yes.
3. **Icons:** accept lucide (outline) in place of FontAwesome solid, or keep a solid set?
4. **Deployment target:** `adapter-static` on `doora.poiw.org`, presumably Netlify (an adapter is listed); confirm so the SPA fallback and env are set up correctly.
5. **Is the rest of `backoffice/` (Firebase config, `functions/`) still in use?** Out of scope here, but the frontend's `firebase` dependency suggests leftover coupling.
