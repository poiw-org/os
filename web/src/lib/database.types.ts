// Generated from the Supabase schema (generate_typescript_types), trimmed to the Database type.
// Regenerate after changing backoffice/supabase/migrations.
export type Json = string | number | boolean | null | { [key: string]: Json | undefined } | Json[];

export type Database = {
	__InternalSupabase: {
		PostgrestVersion: '14.18';
	};
	public: {
		Tables: {
			unidentified_tags: {
				Row: {
					id: string;
					serial_number: string | null;
					claims: Json | null;
					first_seen: string;
					last_seen: string;
					sightings: number;
					last_reason: string | null;
					user_id: string | null;
					attached_by: string | null;
					attached_at: string | null;
				};
				Insert: never;
				Update: never;
				Relationships: [];
			};
			firmware_releases: {
				Row: {
					id: string;
					created_at: string;
					version: string;
					sha256: string;
					size: number;
					status: string;
					approved_by: string | null;
					approved_at: string | null;
					door_state: string | null;
					door_error: string | null;
					door_updated_at: string | null;
				};
				Insert: never;
				Update: never;
				Relationships: [
					{
						foreignKeyName: 'firmware_releases_uploaded_by_fkey';
						columns: ['uploaded_by'];
						isOneToOne: false;
						referencedRelation: 'profiles';
						referencedColumns: ['id'];
					}
				];
			};
			admins: {
				Row: { user_id: string };
				Insert: { user_id: string };
				Update: { user_id?: string };
				Relationships: [];
			};
			activity: {
				Row: {
					actor_id: string | null;
					created_at: string;
					detail: Json;
					id: number;
					key_id: string | null;
					kind: string;
					serial_number: string | null;
					user_id: string;
				};
				Insert: never;
				Update: never;
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
					device_bound_at: string | null;
					device_name: string | null;
					device_pubkey: string | null;
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
					renewed_by: string | null;
					user_id: string;
				};
				Insert: {
					device_bound_at?: string | null;
					device_name?: string | null;
					device_pubkey?: string | null;
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
					renewed_by?: string | null;
					user_id: string;
				};
				Update: {
					device_bound_at?: string | null;
					device_name?: string | null;
					device_pubkey?: string | null;
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
					renewed_by?: string | null;
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
					department: string | null;
					diy_familiar: boolean | null;
					first_name: string | null;
					last_name: string | null;
					onboarded_at: string | null;
					referral_source: string | null;
					studies: boolean | null;
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
					department?: string | null;
					diy_familiar?: boolean | null;
					first_name?: string | null;
					last_name?: string | null;
					onboarded_at?: string | null;
					referral_source?: string | null;
					studies?: boolean | null;
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
					department?: string | null;
					diy_familiar?: boolean | null;
					first_name?: string | null;
					last_name?: string | null;
					onboarded_at?: string | null;
					referral_source?: string | null;
					studies?: boolean | null;
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
			attach_unidentified_tag: {
				Args: { tag: string; target: string | null };
				Returns: undefined;
			};
			send_door_command: {
				Args: { door: string; command: string };
				Returns: undefined;
			};
			approve_firmware: {
				Args: { release: string; expected_sha256: string; acknowledgement?: string };
				Returns: undefined;
			};
			reject_firmware: {
				Args: { release: string };
				Returns: undefined;
			};
			can_unlock: {
				Args: Record<string, never>;
				Returns: boolean;
			};
			security_status: {
				Args: Record<string, never>;
				Returns: Json;
			};
			second_factor_status: {
				Args: { target: string };
				Returns: Json;
			};
			restore_my_keys: {
				Args: Record<string, never>;
				Returns: number;
			};
			can_use_app: {
				Args: Record<string, never>;
				Returns: boolean;
			};
			can_use_app_as: {
				Args: { actor: string };
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
