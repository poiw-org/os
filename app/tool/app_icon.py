# Makes the app icons from the web logo with its po/iw lettering: black lettering on white for
# light mode, white lettering on the console's dark for dark mode.
#   iOS: one 1024 icon per appearance (iOS 18 switches with the system).
#   Android: mipmap-* light, mipmap-night-* dark (launchers that follow night mode pick it).
# Run from app/: python3 tool/app_icon.py
import json, os, glob
from PIL import Image

LIGHT = ('../web/static/poiw-dark.png', (0xFF, 0xFF, 0xFF))
DARK = ('../web/static/poiw.png', (0x18, 0x1B, 0x1E))


def icon(variant, size, fill=0.7):
    path, colour = variant
    logo = Image.open(path).convert('RGBA')
    logo = logo.crop(logo.getbbox())
    bg = Image.new('RGB', (size, size), colour)
    k = int(size * fill) / max(logo.size)
    logo = logo.resize((max(1, int(logo.width * k)), max(1, int(logo.height * k))), Image.LANCZOS)
    bg.paste(logo, ((size - logo.width) // 2, (size - logo.height) // 2), logo)
    return bg


d = 'ios/Runner/Assets.xcassets/AppIcon.appiconset'
for old in glob.glob(f'{d}/*.png'):
    os.remove(old)
icon(LIGHT, 1024).save(f'{d}/icon-light.png')
icon(DARK, 1024).save(f'{d}/icon-dark.png')
json.dump({
    'images': [
        {'filename': 'icon-light.png', 'idiom': 'universal', 'platform': 'ios', 'size': '1024x1024'},
        {'appearances': [{'appearance': 'luminosity', 'value': 'dark'}],
         'filename': 'icon-dark.png', 'idiom': 'universal', 'platform': 'ios', 'size': '1024x1024'},
    ],
    'info': {'author': 'xcode', 'version': 1},
}, open(f'{d}/Contents.json', 'w'), indent=2)

for q, n in {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}.items():
    icon(LIGHT, n).save(f'android/app/src/main/res/mipmap-{q}/ic_launcher.png')
    os.makedirs(f'android/app/src/main/res/mipmap-night-{q}', exist_ok=True)
    icon(DARK, n).save(f'android/app/src/main/res/mipmap-night-{q}/ic_launcher.png')
