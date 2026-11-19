// The sign-in, onboarding and password reset screens.
export const auth: Record<string, string> = {
	// Sign in
	'Sign in to continue': 'Συνδέσου για να συνεχίσεις',
	'An app wants you to sign in with your po/iw account. Next you choose whether to allow it.':
		'Μια εφαρμογή ζητά να συνδεθείς με τον λογαριασμό σου στο po/iw. Στη συνέχεια θα επιλέξεις αν θα της το επιτρέψεις.',
	'Reset password': 'Επαναφορά κωδικού',
	'We will email you a link to choose a new one.':
		'Θα σου στείλουμε με email έναν σύνδεσμο για να ορίσεις νέο.',
	'Send reset link': 'Αποστολή συνδέσμου',
	'Sign in with your password to finish. Email links can’t be used on their own.':
		'Συνδέσου με τον κωδικό σου για να ολοκληρώσεις. Οι σύνδεσμοι στο email δεν αρκούν από μόνοι τους.',
	'If an account exists for this email, a reset link is on its way.':
		'Αν υπάρχει λογαριασμός με αυτό το email, ο σύνδεσμος επαναφοράς είναι καθ’ οδόν.',
	'Not you? Use another account': 'Δεν είσαι εσύ; Χρησιμοποίησε άλλον λογαριασμό',
	Password: 'Κωδικός',
	'Forgot password?': 'Ξέχασες τον κωδικό;',
	'Show password': 'Εμφάνιση κωδικού',
	'Hide password': 'Απόκρυψη κωδικού',
	Show: 'Εμφάνιση',
	Hide: 'Απόκρυψη',
	'One moment…': 'Μια στιγμή…',
	'Sign in with a passkey': 'Σύνδεση με passkey',
	'No account?': 'Δεν έχεις λογαριασμό;',
	Register: 'Εγγραφή',
	'Back to sign in': 'Πίσω στη σύνδεση',

	// Lab refusal
	'We’d love to have you in {app}, but we can’t let you in just yet: it’s only for people who have access to the lab, and this account doesn’t.':
		'Θα θέλαμε να σε έχουμε στο {app}, αλλά προς το παρόν δεν μπορούμε να σε αφήσουμε να μπεις: είναι μόνο για όσους έχουν πρόσβαση στο εργαστήριο, και αυτός ο λογαριασμός δεν έχει.',
	'Come say hi on Discord and the team will gladly sort you out. Once you have lab access, just come back and sign in again.':
		'Πέρνα να πεις γεια στο Discord και η ομάδα θα το τακτοποιήσει με χαρά. Μόλις αποκτήσεις πρόσβαση στο εργαστήριο, γύρνα και συνδέσου ξανά.',
	'Say hi on Discord ♥': 'Πες γεια στο Discord ♥',

	// Quick unlock
	'Quick unlock': 'Γρήγορο ξεκλείδωμα',
	'Open the door with a passkey': 'Άνοιξε την πόρτα με ένα passkey',
	'Waiting for your passkey': 'Περιμένουμε το passkey σου',
	'Follow the prompt on your device': 'Ακολούθησε τις οδηγίες στη συσκευή σου',
	'Contacting the door': 'Επικοινωνία με την πόρτα',
	'This takes a moment': 'Θα πάρει λίγο',
	Unlocking: 'Ξεκλείδωμα',
	'Unlocking in {n} s': 'Ξεκλείδωμα σε {n} δ',
	'Unlocking now': 'Ξεκλειδώνει τώρα',
	'You can come in': 'Μπορείς να περάσεις',

	// Shell
	'Manousos Mikrotsipakis, the po/iw mascot': 'Μανούσος Μικροτσιπάκης, Mascot του po/iw',
	'Open source by po/iw ·': 'Ανοιχτό λογισμικό από το po/iw ·',

	// Reset password and verify
	'Could not set the password.': 'Δεν μπορέσαμε να ορίσουμε τον κωδικό.',
	'Link expired': 'Ο σύνδεσμος έληξε',
	'This reset link is invalid or has already been used.':
		'Ο σύνδεσμος επαναφοράς δεν ισχύει ή έχει ήδη χρησιμοποιηθεί.',
	'Request a new link': 'Ζήτα νέο σύνδεσμο',
	'8+ characters': '8+ χαρακτήρες',
	'Save password': 'Αποθήκευση κωδικού',
	'Could not send the code.': 'Δεν μπορέσαμε να στείλουμε τον κωδικό.',
	'Could not verify the code.': 'Δεν μπορέσαμε να επαληθεύσουμε τον κωδικό.',
	'No authenticator is registered.': 'Δεν έχει καταχωρηθεί εφαρμογή επαλήθευσης.',
	'Enter your code': 'Βάλε τον κωδικό σου',
	'We emailed it to {email}.': 'Τον στείλαμε στο {email}.',
	'From your authenticator app.': 'Από την εφαρμογή επαλήθευσης.',
	'Verification code': 'Κωδικός επαλήθευσης',
	'Send a new code': 'Στείλε νέο κωδικό',

	// OAuth consent
	'Confirm that it is you': 'Να επιβεβαιώνει ότι είσαι εσύ',
	'See your email address': 'Να βλέπει τη διεύθυνση email σου',
	'See your name and picture': 'Να βλέπει το όνομα και τη φωτογραφία σου',
	'See your phone number': 'Να βλέπει τον αριθμό τηλεφώνου σου',
	'Stay signed in to it while you are away': 'Να σε κρατά συνδεδεμένο/η όσο λείπεις',
	'This link is missing its sign-in request. Start again from the app.':
		'Σε αυτόν τον σύνδεσμο λείπει το αίτημα σύνδεσης. Ξεκίνα ξανά από την εφαρμογή.',
	'{message}. Start again from the app.': '{message}. Ξεκίνα ξανά από την εφαρμογή.',
	'Sign in to {app}': 'Σύνδεση στο {app}',
	'Sign in to an app': 'Σύνδεση σε μια εφαρμογή',
	'as {email}': 'ως {email}',
	'Taking you back to the app…': 'Σε γυρνάμε πίσω στην εφαρμογή…',
	'Checking the request…': 'Ελέγχουμε το αίτημα…',
	'{app} wants to use your po/iw account to sign you in. It will be able to:':
		'Το {app} θέλει να χρησιμοποιήσει τον λογαριασμό σου στο po/iw για να σε συνδέσει. Θα μπορεί:',
	'It cannot open the door, see your keys or use the OS. Afterwards you go to {destination}. You can disconnect it any time in Settings.':
		'Δεν μπορεί να ανοίξει την πόρτα, να δει τα κλειδιά σου ή να χρησιμοποιήσει το OS. Μετά θα πας στο {destination}. Μπορείς να την αποσυνδέσεις όποτε θέλεις από τις Ρυθμίσεις.',
	'Back to the app': 'Πίσω στην εφαρμογή',

	// Small bits
	'Manousos Mikrotsipakis': 'Μανούσος Μικροτσιπάκης',
	'Opening Doora…': 'Άνοιγμα του Doora…',
	'Loading…': 'Φόρτωση…',
	'Operating System': 'Λειτουργικό Σύστημα',
	'3D model of the lab’s 3D printer printing Manousos Mikrotsipakis':
		'Τρισδιάστατο μοντέλο του 3D printer του εργαστηρίου, που τυπώνει τον Μανούσο Μικροτσιπάκη',

	// Onboarding
	'Welcome · po/iw': 'Καλώς ήρθες · po/iw',
	'Check your email.': 'Έλεγξε το email σου.',
	'We sent a link to {email}. Open it to finish signing up.':
		'Στείλαμε έναν σύνδεσμο στο {email}. Άνοιξέ τον για να ολοκληρώσεις την εγγραφή σου.',
	'Step {n} of {total}': 'Βήμα {n} από {total}',
	'Welcome. What should we call you?': 'Καλώς ήρθες. Πώς σε λένε;',
	'Are you studying?': 'Σπουδάζεις;',
	'Field of study': 'Τμήμα σπουδών',
	'How did you find us?': 'Πώς μας βρήκες;',
	'Through a friend': 'Μέσω φίλου',
	'First-year welcome day': 'Ημέρα υποδοχής πρωτοετών',
	'Advertising campaign': 'Διαφημιστική καμπάνια',
	'Are you familiar with DIY hardware?': 'Είσαι εξοικειωμένος/η με DIY εξοπλισμό;',
	'Create your account.': 'Φτιάξε τον λογαριασμό σου.',
	'Confirm password': 'Επιβεβαίωση κωδικού',
	'At least 8 characters': 'Τουλάχιστον 8 χαρακτήρες',
	'Your password doesn’t meet all the requirements.': 'Ο κωδικός δεν καλύπτει όλες τις απαιτήσεις.',
	'The passwords don’t match.': 'Οι κωδικοί δεν ταιριάζουν.',
	'An account with this email already exists.': 'Υπάρχει ήδη λογαριασμός με αυτό το email.',
	'Create account': 'Δημιουργία λογαριασμού',
	Back: 'Πίσω',
	Continue: 'Συνέχεια'
};
