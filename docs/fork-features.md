# Fork features (BOOX Picco)

What this fork adds on top of upstream CrossPoint, and where each is documented.

## Touch lock (slide switch set to Touch Reader Controls)
Details: [USER_GUIDE.md section 3.6.3](../USER_GUIDE.md#363-controls).

| Feature | How |
|---|---|
| Full touch lock | Each switch move toggles it (position does not matter); all touch ignored, side buttons work; kept through sleep |
| Lock from the light panel | Touch tile (pointer) toggles the lock; locking closes the panel |
| Lock indicator | Padlock centred in the top bar and in the top-right page corner; open while overridden |
| Light panel under the lock | Up + Down together (touch works inside the panel) |
| Refresh under the lock | Power + Up together |
| Change book under the lock | Up + Down, then the house icon in the light panel goes to Home; pick a book with the buttons |
| Dictionary under the lock | Hold Up 0.2 s on a book page, tap a word: definition at once (no menu); Down backs out; short Up turns back on release |
| Unlock from Home | Hold Up 0.2 s on Home; ends on opening a book, holding Down, moving the switch, sleep, 2 min idle |
| Wake status | Waking into a book: light panel with "Touch locked · light check"; waking to Home: "Touch locked" note |
| Shortcut safety | A shortcut's buttons never also turn a page; a page turned by a not-quite-simultaneous press is undone; no sleep while a shortcut is held |

## Dictionary
Details: [docs/dictionary.md](dictionary.md).

- Kindle-style definition sheet over the lower 60% of the screen; swipe or side buttons page, tap above closes.
- Dictionary-mode menu: Look Up / Another word / + (then Clip / Bookmark / <). Under the lock no menu: the word is looked up on release, and the definition offers Another word / + above the sheet.
- Less sensitive word selection in dictionary mode; returns to the book after a lookup.
- Same large buttons in every selection menu; Look Up turns black with "Looking Up..." while it works.
- One clean refresh when the definition sheet appears, and one when "Not Found" closes.
- The light panel can open over the definition sheet; closing it puts back the page above the sheet.
- Lookup history in `/vocabulary/lookups.tsv`.
- Persian definitions: add Persian glyphs to your reading font with `scripts/merge_persian_font.py`.

## Light
- Light panel quick controls: night mode, refresh, orientation, touch, Home (replaces the Bluetooth tile; Bluetooth stays in Settings).
- Warmth reads Off, 1-99%, Full (Off = cool light only, Full = warm light only). On the Picco there is no colour jump inside 1-99% at any brightness; the one visible step sits at these ends.
- Smooth brightness and warmth steps using all ten vendor levels (`freeink-sdk` branch `picco-custom`, upstream report Free-Ink/freeink-sdk#167).
- Brightness/warmth greyed out while the light is off; lamp button reliably turns it on.

## Sleep screen
- Strip with "Battery at sleep N%" and "· Book N%" (or "Charging N%" on USB).

## Home Backup
Details: [USER_GUIDE.md section 3.6.8](../USER_GUIDE.md#368-home-backup-fork) and [docs/home-backup.md](home-backup.md).

- Incremental backup to a WebDAV server (manual, on sleep, while charging; daily), fetches books from `/to-reader/`.
- No server details compiled in: enter URL and login on the reader; for HTTPS put the server certificate at `/.crosspoint/backup-ca.pem`.

## Build and flashing
- `pio run -e picco -t upload` writes only the app image (`scripts/app_only_upload.py`; upstream issue #4248).
