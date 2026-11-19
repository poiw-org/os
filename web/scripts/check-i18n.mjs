// Lists every message passed to t()/tn() that has no Greek translation. Run: node scripts/check-i18n.mjs
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { join } from 'node:path';
import { el } from '../src/lib/i18n/el.ts';

const files = [];
const walk = (dir) => {
	for (const name of readdirSync(dir)) {
		const path = join(dir, name);
		if (statSync(path).isDirectory()) walk(path);
		else if (/\.(svelte|ts)$/.test(name) && !path.includes('/i18n/')) files.push(path);
	}
};
walk('src');

const literal = String.raw`('(?:[^'\\\n]|\\.)*'|"(?:[^"\\\n]|\\.)*")`;
const calls = [
	new RegExp(String.raw`(?<![\w.$])t\(\s*${literal}`, 'g'),
	new RegExp(String.raw`(?<![\w.$])tn\([^,]+,\s*${literal}\s*,\s*${literal}`, 'g')
];
const missing = new Map();
for (const file of files) {
	const source = readFileSync(file, 'utf8');
	for (const re of calls)
		for (const match of source.matchAll(re))
			for (const raw of match.slice(1)) {
				const text = JSON.parse(
					raw.startsWith("'")
						? `"${raw.slice(1, -1).replace(/\\'/g, "'").replace(/"/g, '\\"')}"`
						: raw
				);
				if (!(text in el)) missing.set(text, file);
			}
}
for (const [text, file] of missing) console.log(`${file}: ${JSON.stringify(text)}`);
console.log(`\n${missing.size} missing`);
process.exitCode = missing.size ? 1 : 0;
