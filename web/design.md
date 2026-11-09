# Design — doora backoffice

Locked system for the backoffice. Extend this file instead of restyling pages one by one.

Direction: the original po/iw look. Midnight navy with a faint dot grid, Space Mono everywhere, tight 4px corners, one electric purple primary action. Hairline-divided panels, no shouting badges.

## Genre

hacker-terminal minimal on midnight navy. Dark only.

## Layout

- Full-width app shell: sticky translucent top bar, content up to 1500px, one-line footer.
- Phones (below `md`): the bar is the brand plus the word "Menu". It unfolds a sheet from under the bar (`MobileMenu`) with the desktop header's actions; "Menu" becomes "Close" in place. It leaves the way it came: Close, Escape, a tap on the scrim, or a swipe up (spring with momentum projection, rubber-band past open; a fade under reduced motion).
- Header (signed in): brand, then Firmware for admins, on the left. A Devices dropdown (each device with its status and Connect) on the right, on every width. Log out lives in the profile modal's footer.
- Home: two columns from `lg` — users on the left, live entries feed on the right (sticky, own scroll). Stacked on mobile.
- Hierarchy pages: users → user → key, each with a breadcrumb. Data streams in over Realtime with no visible indicator.

## Tokens (see `src/app.css`)

- Paper `--color-void` / `--color-paper` / `--color-paper-2` (navy, hue 285), ink `--color-ink`, hairline `gray-200`.
- Brand: `--color-brand` purple for the primary button, chips and focus; `--color-brand-text` for links. Mint (`green-700`) still means ok/success.
- Status is a dot plus text. Buttons are ghost (`.btn`), purple for the one primary (`.btn-primary`), red only for destructive (`.tone-red`).
- Type: Space Mono for everything; `.eyebrow` labels are uppercase and tracked. Roman only.
- Logo: `static/poiw.png`, recoloured with white lettering so it sits straight on the navy (`Brand`).
- Avatars: Gravatar (SHA-256 of the email, `d=404`) over a generated gradient fallback.

## Components

`.panel` + `.row` (hairline-divided lists), `.btn`, `.eyebrow`, `Avatar`, `KeyStatusBadge`, `Breadcrumb`, `ActivityRow`.

## Rules against template tells

- No icon library. Actions are words; status is a dot plus text. No pills, no tinted alert boxes, no side stripes.
- No glow, blur, gradient fills, drop shadows or decorative 3D. Surfaces are flat, separated by hairlines.
- Buttons and labels are sentence case, not uppercase. Headings are bold Space Mono, no subtitle under every h1.
- Auth screens are a single left-biased column on the dot grid. No split art panel, no segmented tabs.
- Avatars are a flat hue colour (or Gravatar). Loading is the word "Loading…", not a spinner.

## Motion

New live rows slide in 240ms; presses scale to 0.97; no pulses, pings or staggered entrances; everything collapses under `prefers-reduced-motion`.

Motion reports an event, never decorates. Status text swaps in place (`swapIn`/`swapOut` in `src/lib/motion.ts`). Outside the two door scenes below, nothing loops.

The door commands are the two sanctioned 3D scenes, sharing one language in `src/lib/three.ts` (token colours, the same key/fill/rim lighting, springs in Apple's response/damping terms, a loop that sleeps when nothing moves). Travel is critically damped; bounce is kept for physical moments (the door's swing, the success check). Loops run only while the door is actually doing something; reduced motion shows each state as a still frame.

- Remote unlock (`DoorStage`): a door with its NFC terminal on the wall, one cable to the internet (a wireframe globe) and one to the lock. The globe lights and spins while connecting, packets run down to the terminal while the command goes out, the terminal beats like a speaker with sound rings while the warning tone plays, then a packet runs to the lock, the bolt slides back and the door swings open onto a lit doorway. A failure knocks the side that failed.
- Writer mode (`TagStage`): the tag drifts toward the PN532 reader while the door waits (waves rise off the coil), docks with a sweep while writing, is knocked back for the wrong tag and pops a check when done.

Command progress is shown as a headline plus one detail line, with a three-figure summary and the per-step breakdown behind a "Steps" disclosure. No terminal transcripts.
