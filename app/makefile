# The Supabase URL and publishable key default to the project's (lib/config.dart); override with
# SUPABASE_URL=... SUPABASE_PUBLISHABLE_KEY=... when pointing at another project.
DEFINES = $(if $(SUPABASE_URL),--dart-define=SUPABASE_URL=$(SUPABASE_URL)) \
          $(if $(SUPABASE_PUBLISHABLE_KEY),--dart-define=SUPABASE_PUBLISHABLE_KEY=$(SUPABASE_PUBLISHABLE_KEY))

.PHONY: run test check tokens ios

run:
	flutter run $(DEFINES)

ios:
	flutter build ios --simulator --debug $(DEFINES)

test:
	flutter test

check:
	dart format --set-exit-if-changed lib test && flutter analyze && flutter test

# Regenerates lib/theme/tokens.g.dart and ios/DoorActivity/Tokens.g.swift from the web CSS.
tokens:
	node tool/gen_tokens.mjs
