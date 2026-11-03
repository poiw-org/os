import { createClient } from '@supabase/supabase-js';
import { config } from '#lib/config.ts';
import type { Database } from '#lib/database.types.ts';

// The password-recovery email link carries #...&type=recovery. The client strips that hash while
// it restores the session, so note it before the client exists.
export const openedRecoveryLink =
	typeof window !== 'undefined' && window.location.hash.includes('type=recovery');

export const supabase = createClient<Database>(config.supabaseUrl, config.supabasePublishableKey);
