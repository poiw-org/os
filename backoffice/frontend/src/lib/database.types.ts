// Generated from the Supabase schema (generate_typescript_types), trimmed to the Database type.
// Regenerate after changing backoffice/supabase/migrations.
export type Json = string | number | boolean | null | { [key: string]: Json | undefined } | Json[];

export type Database = {
	__InternalSupabase: {
		PostgrestVersion: '14.18';
	};
	public: {
		Tables: {
			admins: {
				Row: { user_id: string };
				Insert: { user_id: string };
				Update: { user_id?: string };
				Relationships: [];
			};
			doors: {
				Row: { force_open: boolean; id: string; name: string | null };
				Insert: { force_open?: boolean; id: string; name?: string | null };
				Update: { force_open?: boolean; id?: string; name?: string | null };
				Relationships: [];
			};
			entries: {
				Row: {
					created_at: string;
					door_id: string;
					id: number;
					key_id: string | null;
					metadata: Json;
					opened_by: string | null;
					reason: string | null;
					serial_number: string | null;
					type: string;
				};
				Insert: {
					created_at?: string;
					door_id: string;
					id?: never;
					key_id?: string | null;
					metadata?: Json;
					opened_by?: string | null;
					reason?: string | null;
					serial_number?: string | null;
					type: string;
				};
				Update: {
					created_at?: string;
					door_id?: string;
					id?: never;
					key_id?: string | null;
					metadata?: Json;
					opened_by?: string | null;
					reason?: string | null;
					serial_number?: string | null;
					type?: string;
				};
				Relationships: [
					{
						foreignKeyName: 'entries_opened_by_fkey';
						columns: ['opened_by'];
						isOneToOne: false;
						referencedRelation: 'profiles';
						referencedColumns: ['id'];
					}
				];
			};
			keys: {
				Row: {
					disabled: boolean;
					expires_at: string;
					id: string;
					issued_at: string;
					issued_by: string | null;
					jwt: string | null;
					legacy_id: string | null;
					not_before: string;
					serial_number: string | null;
					serial_released: boolean;
					user_id: string;
				};
				Insert: {
					disabled?: boolean;
					expires_at: string;
					id?: string;
					issued_at?: string;
					issued_by?: string | null;
					jwt?: string | null;
					legacy_id?: string | null;
					not_before: string;
					serial_number?: string | null;
					serial_released?: boolean;
					user_id: string;
				};
				Update: {
					disabled?: boolean;
					expires_at?: string;
					id?: string;
					issued_at?: string;
					issued_by?: string | null;
					jwt?: string | null;
					legacy_id?: string | null;
					not_before?: string;
					serial_number?: string | null;
					serial_released?: boolean;
					user_id?: string;
				};
				Relationships: [
					{
						foreignKeyName: 'keys_issued_by_fkey';
						columns: ['issued_by'];
						isOneToOne: false;
						referencedRelation: 'profiles';
						referencedColumns: ['id'];
					},
					{
						foreignKeyName: 'keys_user_id_fkey';
						columns: ['user_id'];
						isOneToOne: false;
						referencedRelation: 'profiles';
						referencedColumns: ['id'];
					}
				];
			};
			profiles: {
				Row: {
					auth0_id: string | null;
					blocked: boolean;
					created_at: string;
					email: string | null;
					id: string;
					name: string | null;
					nickname: string | null;
					username: string | null;
				};
				Insert: {
					auth0_id?: string | null;
					blocked?: boolean;
					created_at?: string;
					email?: string | null;
					id: string;
					name?: string | null;
					nickname?: string | null;
					username?: string | null;
				};
				Update: {
					auth0_id?: string | null;
					blocked?: boolean;
					created_at?: string;
					email?: string | null;
					id?: string;
					name?: string | null;
					nickname?: string | null;
					username?: string | null;
				};
				Relationships: [];
			};
		};
		Views: { [_ in never]: never };
		Functions: {
			can_unlock: {
				Args: Record<string, never>;
				Returns: boolean;
			};
			log_web_open: {
				Args: { door: string; command_id: string; latency_ms: number };
				Returns: number;
			};
			log_web_open_as: {
				Args: { actor: string; door: string; command_id: string; latency_ms: number };
				Returns: number;
			};
			set_admin: {
				Args: { target: string; admin: boolean };
				Returns: undefined;
			};
		};
		Enums: { [_ in never]: never };
		CompositeTypes: { [_ in never]: never };
	};
};
