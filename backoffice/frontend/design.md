# Design — doora backoffice

Locked system for the backoffice. Extend this file instead of restyling pages one by one.

Direction: quiet and premium. Hairline-divided panels instead of boxed cards, a white primary action, one muted accent that only means ok/live, no status badges shouting, no live indicators.

## Genre

modern-minimal with atmospheric dark (Linear / Vercel). Dark only.

## Layout

- Full-width app shell: sticky translucent top bar, content up to 1500px, one-line footer.
- Home: two columns from `lg` — users on the left, live entries feed on the right (sticky, own scroll). Stacked on mobile.
- Hierarchy pages: users → user → key, each with a breadcrumb. Data streams in over Realtime with no visible indicator.

## Tokens (see `src/app.css`)

- Paper `--color-void` / `--color-paper` / `--color-paper-2`, ink `--color-ink`, hairline `gray-200`.
- Accent: muted mint `oklch(80% 0.15 162)` (the old `green-700` name still points at it).
- Status is a dot plus text. Buttons are ghost (`.btn`), white for the one primary (`.btn-primary`), red only for destructive (`.tone-red`).
- Type: Geist (UI), Geist Mono (data, `.eyebrow` labels). Roman only, no italics.
- Avatars: Gravatar (SHA-256 of the email, `d=404`) over a generated gradient fallback.

## Components

`.panel` + `.row` (hairline-divided lists), `.btn`, `.eyebrow`, `Avatar`, `KeyStatusBadge`, `Breadcrumb`, `ActivityRow`.

## Motion

New live rows slide in 240ms; presses scale to 0.97; everything collapses under `prefers-reduced-motion`.
