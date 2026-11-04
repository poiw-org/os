import { resolve } from '$app/paths';

/** The page of one key. */
export const keyPath = (id: string) => resolve('/os/keys/[id]', { id });
/** The page of one user. */
export const userPath = (id: string) => resolve('/os/users/[id]', { id });
