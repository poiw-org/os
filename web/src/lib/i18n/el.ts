import { door } from './el.door.ts';
import { install } from './el.install.ts';
import { landing } from './el.landing.ts';
import { pages } from './el.pages.ts';
import { shell } from './el.shell.ts';

export const el: Record<string, string> = { ...landing, ...shell, ...door, ...pages, ...install };
