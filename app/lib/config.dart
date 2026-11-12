/// The project's public values. The owner chose to hardcode them (the publishable key is meant to
/// ship in clients; row-level security does the protecting); `--dart-define` still overrides them.
abstract final class Config {
  static const String supabaseUrl = String.fromEnvironment(
    'SUPABASE_URL',
    defaultValue: 'https://yshwchmbsuafribrttlt.supabase.co',
  );
  static const String supabasePublishableKey = String.fromEnvironment(
    'SUPABASE_PUBLISHABLE_KEY',
    defaultValue: 'sb_publishable_Yqe-dl6usU3GEGPUbAIPTA_olGKWuMG',
  );

  static bool get isComplete =>
      supabaseUrl.isNotEmpty && supabasePublishableKey.isNotEmpty;

  /// Where members do what the app does not: passkeys, authenticator apps, new keys.
  static const String websiteUrl = 'https://poiw.org';
  static const String websiteHost = 'poiw.org';
}
