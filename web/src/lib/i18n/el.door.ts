// Remote unlock, writing tags, and firmware: the parts that talk to the door.
export const door: Record<string, string> = {
	// Remote unlock
	'Unlock the door?': 'Να ξεκλειδώσει η πόρτα;',
	'Sends a one-time unlock to door:{door}. It is logged under your name.':
		'Στέλνει ένα εφάπαξ ξεκλείδωμα στην πόρτα door:{door}. Καταγράφεται στο όνομά σου.',
	Unlock: 'Ξεκλείδωμα',
	'Unlocking…': 'Ξεκλείδωμα…',
	'Try again': 'Δοκίμασε ξανά',
	Connecting: 'Σύνδεση',
	Sending: 'Αποστολή',
	'Waiting for the door': 'Περιμένουμε την πόρτα',
	Opening: 'Άνοιγμα',
	Logging: 'Καταγραφή',
	'Door unlocked': 'Η πόρτα ξεκλείδωσε',
	'Could not unlock': 'Δεν ξεκλείδωσε',
	'Recorded under your name': 'Καταγράφηκε στο όνομά σου',
	'It opened, but the entry was not logged': 'Άνοιξε, αλλά η είσοδος δεν καταγράφηκε',
	'The door opened, but the entry could not be logged.':
		'Η πόρτα άνοιξε, αλλά η είσοδος δεν καταγράφηκε.',
	'Could not send the remote unlock command.':
		'Δεν στάλθηκε η εντολή απομακρυσμένου ξεκλειδώματος.',
	'Unlock progress': 'Πρόοδος ξεκλειδώματος',
	'Door reply': 'Απάντηση πόρτας',
	Elapsed: 'Χρόνος',
	Logged: 'Καταγράφηκε',
	Steps: 'Βήματα',
	Auth: 'Σύνδεση',
	Channel: 'Κανάλι',
	Read: 'Ανάγνωση',
	Write: 'Εγγραφή',

	// Step texts the door flow reports
	'Authorising the realtime socket': 'Εξουσιοδότηση του realtime socket',
	'Access token attached': 'Το access token επισυνάφθηκε',
	'Could not join {topic} ({reason}).': 'Δεν έγινε σύνδεση στο {topic} ({reason}).',
	'Could not reach the realtime service.': 'Δεν βρέθηκε η υπηρεσία realtime.',
	'Signed in': 'Συνδέθηκες',
	'The server relays to the door': 'Ο server προωθεί την εντολή στην πόρτα',
	Failed: 'Απέτυχε',
	'Command on the wire': 'Η εντολή στάλθηκε',
	'Warning tone playing at the door': 'Η πόρτα παίζει προειδοποιητικό ήχο',
	'Door unlocking': 'Η πόρτα ξεκλειδώνει',
	'Unlocking in {left} s': 'Ξεκλείδωμα σε {left} δ',
	'Entry recorded under your name': 'Η είσοδος καταγράφηκε στο όνομά σου',
	'Not logged': 'Δεν καταγράφηκε',
	'Not logged: {error}': 'Δεν καταγράφηκε: {error}',
	'Commands available here: status, show, reboot, help.':
		'Διαθέσιμες εντολές εδώ: status, show, reboot, help.',
	'The command could not be sent ({sent}).': 'Η εντολή δεν στάλθηκε ({sent}).',
	'Waiting for the door to confirm': 'Περιμένουμε την επιβεβαίωση της πόρτας',
	'No answer yet, resending (copy {copies})':
		'Καμία απάντηση ακόμα, ξαναστέλνουμε (αντίγραφο {copies})',
	'The door did not answer, so nothing was unlocked. It may be offline.':
		'Η πόρτα δεν απάντησε, οπότε δεν ξεκλείδωσε τίποτα. Ίσως είναι εκτός σύνδεσης.',
	'The door did not answer, so nothing was written. It may be offline.':
		'Η πόρτα δεν απάντησε, οπότε δεν γράφτηκε τίποτα. Ίσως είναι εκτός σύνδεσης.',
	'The door stopped answering.': 'Η πόρτα σταμάτησε να απαντά.',
	'Door is in writer mode': 'Η πόρτα είναι σε λειτουργία εγγραφής',
	'Waiting for the tag': 'Περιμένουμε το tag',
	'Tag detected': 'Εντοπίστηκε tag',
	'Writing and verifying the tag': 'Εγγραφή και έλεγχος του tag',
	'Written and read back': 'Γράφτηκε και διαβάστηκε ξανά',
	'Tag {serial}': 'Tag {serial}',
	'The door reported “{state}”.': 'Η πόρτα ανέφερε «{state}».',
	'The tag is too small to hold the key. Use an NTAG215/216 or a MIFARE Classic 1K.':
		'Το tag είναι πολύ μικρό για το κλειδί. Χρησιμοποίησε NTAG215/216 ή MIFARE Classic 1K.',
	'The door can only write NTAG tags and MIFARE Classic cards that are already formatted for NDEF.':
		'Η πόρτα γράφει μόνο tags NTAG και κάρτες MIFARE Classic που είναι ήδη φορμαρισμένες για NDEF.',
	'The tag moved away or refused the write. Nothing was changed on the key.':
		'Το tag απομακρύνθηκε ή αρνήθηκε την εγγραφή. Το κλειδί δεν άλλαξε.',
	'No tag was presented in time.': 'Δεν πλησίασε κανένα tag εγκαίρως.',
	'Cancelled.': 'Ακυρώθηκε.',
	'The door refused to write this key.': 'Η πόρτα αρνήθηκε να γράψει αυτό το κλειδί.',

	// Writing a tag
	'Scan a tag': 'Σάρωση tag',
	'Write to tag': 'Εγγραφή στο tag',
	'Write at door': 'Εγγραφή στην πόρτα',
	'Uses the reader on': 'Χρησιμοποιεί τον αναγνώστη της',
	'as a scanner.': 'ως σαρωτή.',
	'as a writer.': 'ως εγγραφέα.',
	'Read a tag with the door': 'Διάβασε ένα tag με την πόρτα',
	'The reader at the door reads the serial number of the next tag you hold to it. Stand at the door with the tag in your hand, then start.':
		'Ο αναγνώστης στην πόρτα διαβάζει τον σειριακό αριθμό του επόμενου tag που θα του πλησιάσεις. Στάσου στην πόρτα με το tag στο χέρι και ξεκίνα.',
	'Write this key with the door': 'Γράψε αυτό το κλειδί με την πόρτα',
	'The reader at the door writes the key to the tag with S/N {serial}. Stand at the door with that tag in your hand, then start.':
		'Ο αναγνώστης στην πόρτα γράφει το κλειδί στο tag με S/N {serial}. Στάσου στην πόρτα με εκείνο το tag στο χέρι και ξεκίνα.',
	'Putting the door in writer mode': 'Η πόρτα μπαίνει σε λειτουργία εγγραφής',
	'Reaching the door over its private channel.':
		'Επικοινωνούμε με την πόρτα από το ιδιωτικό της κανάλι.',
	'Hold the tag to the reader': 'Πλησίασε το tag στον αναγνώστη',
	'Lay it flat on the reader. The door beeps while it waits.':
		'Ακούμπα το ίσιο πάνω στον αναγνώστη. Η πόρτα κάνει μπιπ όσο περιμένει.',
	'Lay it flat on the reader and keep it there. The door beeps while it waits.':
		'Ακούμπα το ίσιο πάνω στον αναγνώστη και άφησέ το εκεί. Η πόρτα κάνει μπιπ όσο περιμένει.',
	'That is a different tag': 'Αυτό είναι άλλο tag',
	'This key belongs to the tag ending {tail}. Swap it for that one.':
		'Αυτό το κλειδί ανήκει στο tag που τελειώνει σε {tail}. Βάλε εκείνο.',
	'Writing. Keep it still': 'Γράφουμε. Κράτα το ακίνητο',
	'Do not lift the tag until the door plays the happy chord.':
		'Μην το σηκώσεις μέχρι να παίξει η πόρτα τον χαρούμενο ήχο.',
	'Tag read': 'Το tag διαβάστηκε',
	'Serial number {serial} is now in the form.': 'Ο σειριακός αριθμός {serial} μπήκε στη φόρμα.',
	'Key written': 'Το κλειδί γράφτηκε',
	'The door read it back and it matches. Tap the tag on the door to try it.':
		'Η πόρτα το διάβασε ξανά και ταιριάζει. Ακούμπα το tag στην πόρτα για να το δοκιμάσεις.',
	'The tag was not read': 'Το tag δεν διαβάστηκε',
	'The tag was not written': 'Το tag δεν γράφτηκε',
	'Could not write the tag.': 'Δεν έγινε εγγραφή στο tag.',
	'Writer mode ends in {remaining} s': 'Η λειτουργία εγγραφής λήγει σε {remaining} δ',
	'Waiting…': 'Αναμονή…',
	'Start scanning': 'Έναρξη σάρωσης',
	'Start writer mode': 'Έναρξη λειτουργίας εγγραφής',

	// Renewing a key
	'Renew {id}': 'Ανανέωση {id}',
	'The key must expire after it activates.': 'Το κλειδί πρέπει να λήγει μετά την ενεργοποίησή του.',
	'Key renewed.': 'Το κλειδί ανανεώθηκε.',
	'Could not renew the key.': 'Δεν ανανεώθηκε το κλειδί.',
	'Reflash the tag.': 'Ξαναγράψε το tag.',
	'The tag still carries the old token, with the old dates. The door only honours the new dates once the renewed key is written to the tag again.':
		'Το tag κουβαλά ακόμα το παλιό token με τις παλιές ημερομηνίες. Η πόρτα δέχεται τις νέες μόνο όταν το ανανεωμένο κλειδί γραφτεί ξανά στο tag.',
	'Renewed. This is a virtual key, so there is no tag to reflash.':
		'Ανανεώθηκε. Είναι εικονικό κλειδί, οπότε δεν υπάρχει tag να ξαναγραφτεί.',
	'The key is bound to a tag. After renewing, write it to the tag again so the tag carries the new dates.':
		'Το κλειδί είναι δεμένο με ένα tag. Μετά την ανανέωση, γράψε το ξανά στο tag για να πάρει τις νέες ημερομηνίες.',
	'Renewing…': 'Ανανέωση…',
	'Activate not before': 'Ενεργό από',
	'Expires at': 'Λήγει στις',

	// Firmware list and approval
	'No firmware uploaded yet. Publish a build with':
		'Δεν έχει ανέβει firmware ακόμα. Δημοσίευσε ένα build με',
	'Door is downloading': 'Η πόρτα το κατεβάζει',
	'Door is restarting into it': 'Η πόρτα κάνει επανεκκίνηση με αυτό',
	Installed: 'Εγκαταστάθηκε',
	'Failed its trial; the door went back to the previous firmware':
		'Απέτυχε στη δοκιμή· η πόρτα γύρισε στο προηγούμενο firmware',
	'Door could not install it': 'Η πόρτα δεν μπόρεσε να το εγκαταστήσει',
	'Waiting for approval from the link make release opens':
		'Περιμένει έγκριση από τον σύνδεσμο που ανοίγει το make release',
	Rejected: 'Απορρίφθηκε',
	'Approved; waiting for the door': 'Εγκρίθηκε· περιμένει την πόρτα',
	'Approve firmware': 'Έγκριση firmware',
	'Allow firmware uploads': 'Άδεια για ανέβασμα firmware',
	'Install this firmware?': 'Να εγκατασταθεί αυτό το firmware;',
	'This code will run on the door. Only approve it if you know what changed and the uploader confirms this hash: {hash}':
		'Αυτός ο κώδικας θα τρέξει στην πόρτα. Έγκρινέ τον μόνο αν ξέρεις τι άλλαξε και το άτομο που τον ανέβασε επιβεβαιώνει αυτό το hash: {hash}',
	'Approved. The door will fetch it now.': 'Εγκρίθηκε. Η πόρτα θα το κατεβάσει τώρα.',
	'Rejected.': 'Απορρίφθηκε.',
	'Your build is uploaded. Contact an admin to approve it; the door will not install it until they do. Approval has to happen within 24 hours.':
		'Το build σου ανέβηκε. Ζήτα από κάποιον admin να το εγκρίνει· η πόρτα δεν θα το εγκαταστήσει μέχρι τότε. Η έγκριση πρέπει να γίνει μέσα σε 24 ώρες.',
	'Send them this link': 'Στείλε τους αυτόν τον σύνδεσμο',
	'This release does not exist.': 'Αυτή η έκδοση δεν υπάρχει.',
	Version: 'Έκδοση',
	'Uploaded by': 'Ανέβηκε από',
	unknown: 'άγνωστο',
	Build: 'Build',
	"Compare this hash with the one printed in the uploader's terminal. If someone else uploaded it, confirm the hash with them directly.":
		'Σύγκρινε αυτό το hash με αυτό που τυπώθηκε στο τερματικό αυτού που το ανέβασε. Αν το ανέβασε άλλο άτομο, επιβεβαίωσε το hash απευθείας μαζί του.',
	'This label says the build has uncommitted changes, so its code may not be in the repository.':
		'Η ετικέτα λέει ότι το build έχει αλλαγές που δεν έγιναν commit, οπότε ο κώδικάς του μπορεί να μην υπάρχει στο repository.',
	'This link does not carry the hash of this release, so it cannot be approved. Use the link opened by':
		'Αυτός ο σύνδεσμος δεν έχει το hash αυτής της έκδοσης, οπότε δεν μπορεί να εγκριθεί. Χρησιμοποίησε τον σύνδεσμο που άνοιξε το',
	'No longer waiting for approval ({state}).': 'Δεν περιμένει πια έγκριση ({state}).',
	'You do not know what is in this binary.': 'Δεν ξέρεις τι κρύβει αυτό το binary.',
	'{who} uploaded it, not you. Once approved it runs on the door and can do anything the door can do, including letting anyone in. A label and a hash cannot tell you what the code does.':
		'Το ανέβασε {who}, όχι εσύ. Μόλις εγκριθεί, τρέχει στην πόρτα και μπορεί να κάνει ό,τι κάνει η πόρτα, ακόμα και να αφήσει οποιονδήποτε να μπει. Μια ετικέτα και ένα hash δεν σου λένε τι κάνει ο κώδικας.',
	'Someone else': 'Κάποιο άλλο άτομο',
	'You may want to check the code yourself before approving. If you cannot vouch for it, reject it.':
		'Ίσως θες να δεις τον κώδικα μόνος ή μόνη σου πριν εγκρίνεις. Αν δεν μπορείς να εγγυηθείς γι’ αυτόν, απόρριψέ τον.',
	'To approve anyway, type': 'Για να εγκρίνεις παρ’ όλα αυτά, γράψε',
	'Only admins and members with an active key can upload firmware.':
		'Μόνο οι admins και τα μέλη με ενεργό κλειδί μπορούν να ανεβάσουν firmware.',
	'This link is not valid. Run': 'Αυτός ο σύνδεσμος δεν ισχύει. Τρέξε ξανά το',
	' again.': '.',
	'The release tool on this computer is asking to upload firmware builds as':
		'Το εργαλείο release σε αυτόν τον υπολογιστή ζητά να ανεβάζει builds firmware ως',
	'for the next 30 days.': 'για τις επόμενες 30 ημέρες.',
	'It can only queue builds. An admin still has to approve each one before the door installs it.':
		'Μπορεί μόνο να βάζει builds σε αναμονή. Κάποιος admin πρέπει να εγκρίνει το καθένα πριν το εγκαταστήσει η πόρτα.',
	'It cannot open the door, change keys or use the door console.':
		'Δεν μπορεί να ανοίξει την πόρτα, να αλλάξει κλειδιά ή να χρησιμοποιήσει την κονσόλα της πόρτας.',
	'Only continue if you just ran': 'Συνέχισε μόνο αν μόλις έτρεξες εσύ το',
	' yourself.': '.'
};
