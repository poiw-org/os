import 'package:flutter/services.dart';
import 'package:material_ui/material_ui.dart';

import '../keys/keys_screen.dart' show WebsiteNote;
import '../theme/theme.dart';
import '../theme/widgets.dart';
import 'auth_backend.dart';
import 'auth_controller.dart';

/// The web's login: one column, a panel with the form.
class SignInScreen extends StatefulWidget {
  const SignInScreen({super.key, required this.auth});
  final AuthController auth;

  @override
  State<SignInScreen> createState() => _SignInScreenState();
}

class _SignInScreenState extends State<SignInScreen> {
  final _email = TextEditingController();
  final _password = TextEditingController();

  @override
  void dispose() {
    _email.dispose();
    _password.dispose();
    super.dispose();
  }

  void _submit() {
    if (_email.text.trim().isEmpty || _password.text.isEmpty) return;
    TextInput.finishAutofillContext();
    widget.auth.signIn(_email.text, _password.text);
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.auth,
      builder: (context, _) {
        final auth = widget.auth;
        final type = context.type;
        return Screen(
          children: [
            Panel(
              padding: const EdgeInsets.all(24),
              children: [
                AutofillGroup(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      const Eyebrow('po/iw members'),
                      const SizedBox(height: 10),
                      Semantics(
                        header: true,
                        child: Text('Sign in', style: type.display),
                      ),
                      const SizedBox(height: 28),
                      DooraTextField(
                        label: 'Email',
                        controller: _email,
                        enabled: !auth.busy,
                        keyboardType: TextInputType.emailAddress,
                        textInputAction: TextInputAction.next,
                        autofillHints: const [AutofillHints.username],
                        hint: 'you@example.com',
                      ),
                      const SizedBox(height: 16),
                      DooraTextField(
                        label: 'Password',
                        controller: _password,
                        enabled: !auth.busy,
                        obscure: true,
                        textInputAction: TextInputAction.done,
                        autofillHints: const [AutofillHints.password],
                        errorText: auth.passwordError,
                        onSubmitted: (_) => _submit(),
                      ),
                      const SizedBox(height: 20),
                      DooraButton(
                        label: auth.busy ? 'Signing in…' : 'Sign in',
                        kind: ButtonKind.primary,
                        expand: true,
                        tall: true,
                        onPressed: auth.busy ? null : _submit,
                      ),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            // Passkey sign-in is not in this version; say so plainly.
            const WebsiteNote(
              'Accounts that sign in only with a passkey need a password first. Set one on',
            ),
          ],
        );
      },
    );
  }
}

/// The second step: the authenticator's code or the emailed one.
class SecondStepScreen extends StatefulWidget {
  const SecondStepScreen({super.key, required this.auth});
  final AuthController auth;

  @override
  State<SecondStepScreen> createState() => _SecondStepScreenState();
}

class _SecondStepScreenState extends State<SecondStepScreen> {
  final _code = TextEditingController();

  @override
  void dispose() {
    _code.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.auth,
      builder: (context, _) {
        final auth = widget.auth;
        final type = context.type;
        final email = auth.secondStep == SecondStep.email;
        return Screen(
          children: [
            Panel(
              padding: const EdgeInsets.all(24),
              children: [
                Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const Eyebrow('Second step'),
                    const SizedBox(height: 10),
                    Semantics(
                      header: true,
                      child: Text('Enter your code', style: type.display),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      email
                          ? (auth.notice ?? 'We are emailing you a code.')
                          : 'From your authenticator app.',
                      style: type.muted,
                    ),
                    const SizedBox(height: 24),
                    DooraTextField(
                      label: 'Verification code',
                      controller: _code,
                      enabled: !auth.busy,
                      autofocus: true,
                      monospace: true,
                      maxLength: 10,
                      keyboardType: TextInputType.number,
                      autofillHints: const [AutofillHints.oneTimeCode],
                      hint: '123456',
                      errorText: auth.codeError,
                      onSubmitted: (_) => auth.submitCode(_code.text),
                    ),
                    const SizedBox(height: 20),
                    DooraButton(
                      label: auth.busy ? 'Verifying…' : 'Verify',
                      kind: ButtonKind.primary,
                      expand: true,
                      tall: true,
                      onPressed: auth.busy
                          ? null
                          : () => auth.submitCode(_code.text),
                    ),
                    if (email) ...[
                      const SizedBox(height: 12),
                      DooraButton(
                        label: 'Send a new code',
                        expand: true,
                        onPressed: auth.busy ? null : auth.sendCode,
                      ),
                    ],
                  ],
                ),
              ],
            ),
            const SizedBox(height: 8),
            Align(
              alignment: Alignment.centerLeft,
              child: DooraTextButton(
                label: 'Sign out',
                onPressed: auth.signOut,
              ),
            ),
          ],
        );
      },
    );
  }
}

/// Signed in, but no usable virtual key.
class NoAccessScreen extends StatelessWidget {
  const NoAccessScreen({super.key, required this.onSignOut});
  final VoidCallback onSignOut;

  @override
  Widget build(BuildContext context) {
    return Screen(
      children: [
        Panel(
          children: [
            PanelRow(
              child: Semantics(
                liveRegion: true,
                container: true,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const Row(
                      children: [
                        Lamp(LampState.redOff),
                        SizedBox(width: 10),
                        Eyebrow('No access'),
                      ],
                    ),
                    const SizedBox(height: 14),
                    Text(
                      'No active key',
                      style: context.type.display.copyWith(fontSize: 26),
                    ),
                    const SizedBox(height: 6),
                    Text(
                      'You need an active virtual key to use the app. Ask an admin.',
                      style: context.type.muted,
                    ),
                    const SizedBox(height: 20),
                    DooraButton(
                      label: 'Sign out',
                      expand: true,
                      onPressed: onSignOut,
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ],
    );
  }
}

/// Shown until the first session check finishes, and when the build has no Supabase values.
class StartingScreen extends StatelessWidget {
  const StartingScreen({super.key, this.message});
  final String? message;

  @override
  Widget build(BuildContext context) => Screen(
    children: [
      message == null
          ? const LoadingText()
          : Text(message!, style: context.type.body),
    ],
  );
}
