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
| Wake status | Waking into a book: light panel with "Touch locked · light check - tap below to read"; waking to Home: "Touch locked" note |
| Shortcut safety | A shortcut's buttons never also turn a page; a page turned by a not-quite-simultaneous press is undone; no sleep while a shortcut is held |

## Dictionary
Details: [docs/dictionary.md](dictionary.md).

- Definition sheet over the lower 60% of the screen, with the book page visible above; swipe or side buttons page, tap above closes.
- Dictionary-mode menu: Look Up / Another word / + (then Clip / Bookmark / <). Under the lock no menu: the word is looked up on release, and the definition offers Another word / + above the sheet.
- Less sensitive word selection in dictionary mode; returns to the book after a lookup.
- Same large buttons in every selection menu; Look Up turns black with "Looking Up..." while it works.
- One clean refresh when the definition sheet appears, and one when "Not Found" closes.
- The light panel can open over the definition sheet; closing it puts back the page above the sheet.
- Lookup history in `/vocabulary/lookups.tsv`.
- Persian definitions: add Persian glyphs to your reading font with `scripts/merge_persian_font.py`.

## Light
- Light panel: a Home tile replaces the Bluetooth tile (Bluetooth stays in Settings). Under the touch lock, open the panel with Up + Down and tap Home to change books.
- Warmth reads Off, 1-99%, Full (Off = cool light only, Full = warm light only). On the BOOX Picco there is no colour jump inside 1-99% at any brightness; the only visible step is into Off or Full. Fix merged upstream: Free-Ink/freeink-sdk#172.
- Smooth brightness and warmth steps using all ten vendor levels (`freeink-sdk` branch `picco-custom`; reported as Free-Ink/freeink-sdk#167, since fixed in the upstream SDK as well).
- Brightness/warmth greyed out while the light is off; lamp button reliably turns it on.
- Wake with light: hold Down while pressing Power to wake; the light comes on at the saved brightness/warmth, with no screenshot or page turn; the light panel opens with a note so the light can be adjusted (not when the slide switch is set to Frontlight).

## Per-book text settings
Details: [USER_GUIDE.md section 3.6.2](../USER_GUIDE.md#362-reader).

- Each EPUB book keeps its own Text Settings (font, size, spacing, indent, alignment, margin, style toggles) in `/.crosspoint/book-settings/<KOReader document id>.json`; the id hashes the book's content, so renaming or moving the book keeps them, and the book file is never changed.
- A new book starts from the last-used values; Text Settings opened from Settings with no book open set those values for books not opened yet.
- Text Settings → Layout → Reset Text Settings (book open only) restores the factory values for this book.
- Clear Reading Cache does not delete them; Home Backup includes them.

## Reader
- EPUB book position `~page/~total` in the top-right page margin, left of the padlock; uses the exact page counts of chapters already laid out with the current settings and estimates the rest from their size (pages per byte of those chapters, ignoring chapters under 3 pages or under 300 bytes per page), so it gets more accurate as more chapters are opened; the `~` is dropped when every chapter, the current one included, is fully laid out. Shown with Book Progress Percentage on and a screen margin (set per book) of 20 px or more; hidden while some chapters are not laid out and no laid-out chapter is large enough to estimate from ([USER_GUIDE.md section 3.6.1](../USER_GUIDE.md#361-display)).
- Status-bar clock (EPUB, TXT, Markdown) repaints at each new minute with one fast refresh: no gray pass, no step towards the periodic clean refresh. Not during automatic page turn, with a menu, panel, popup or message over the page, or on UC8279 absolute-gray image pages ([USER_GUIDE.md section 3.6.1](../USER_GUIDE.md#361-display)).
- The RTC is set at each SNTP sync and from the Home Backup server's `Date` header after a run that ends Ok or Partial.

## Sleep screen
- Strip with "Battery at sleep N%" and "· Book N%". On USB power the first part reads "Charging N%" or "Charged N%".

## Home Backup
Details: [USER_GUIDE.md section 3.6.8](../USER_GUIDE.md#368-home-backup-fork) and [docs/home-backup.md](home-backup.md).

- Incremental backup to a WebDAV server (on request, or once a day at sleep; up to 60 s on battery, 15 min on USB power), fetches books from `/to-reader/`.
- No server details compiled in: enter URL and login on the reader; for HTTPS put the server certificate at `/.crosspoint/backup-ca.pem`.

## Build and flashing
- `pio run -e picco -t upload` writes only the app image (`scripts/app_only_upload.py`; upstream issue #4248).
