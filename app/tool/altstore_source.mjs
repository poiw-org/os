// Writes the AltStore source (https://faq.altstore.io/developers/make-a-source) for one release.
// The source lives in each release as altstore.json; poiw.org/altstore.json redirects to the
// latest one, so adding the source in AltStore always yields the newest build.
//
//   node tool/altstore_source.mjs --repo o/r --tag v0.1.0-build.7 --version 0.1.0 --build 7 \
//        --ipa doora.ipa --out altstore.json
import { statSync, writeFileSync } from 'node:fs';
import { parseArgs } from 'node:util';

const { values: a } = parseArgs({
  options: {
    repo: { type: 'string' },
    tag: { type: 'string' },
    version: { type: 'string' },
    build: { type: 'string' },
    ipa: { type: 'string' },
    out: { type: 'string' },
  },
});
for (const k of ['repo', 'tag', 'version', 'build', 'ipa', 'out']) {
  if (!a[k]) throw new Error(`missing --${k}`);
}

const site = 'https://poiw.org';
const tint = '653EAE'; // brand, from lib/theme/tokens.g.dart

const source = {
  name: 'po/iw',
  identifier: 'org.poiw.altstore',
  subtitle: 'Apps from the po/iw hackerspace',
  description: 'Doora, the door lock of the po/iw hackerspace.',
  iconURL: `${site}/poiw.png`,
  website: site,
  tintColor: tint,
  apps: [
    {
      name: 'Doora',
      bundleIdentifier: 'org.poiw.doora',
      developerName: 'po/iw',
      subtitle: 'The door lock of the hackerspace',
      localizedDescription:
        'Your virtual keys, and the unlock. Walk up to the door and open it from your phone over Bluetooth.',
      iconURL: `${site}/poiw.png`,
      tintColor: tint,
      category: 'utilities',
      versions: [
        {
          version: a.version,
          buildVersion: a.build,
          date: new Date().toISOString(),
          downloadURL: `https://github.com/${a.repo}/releases/download/${a.tag}/doora.ipa`,
          size: statSync(a.ipa).size,
          minOSVersion: '16.1',
          localizedDescription: `Build ${a.build}`,
        },
      ],
      appPermissions: {
        entitlements: [],
        privacy: {
          NSBluetoothAlwaysUsageDescription: 'Doora uses Bluetooth to find the door and unlock it.',
        },
      },
    },
  ],
  news: [],
};

writeFileSync(a.out, JSON.stringify(source, null, 2) + '\n');
