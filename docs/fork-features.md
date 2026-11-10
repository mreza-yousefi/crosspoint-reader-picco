# Fork features (BOOX Picco)

What this fork adds on top of upstream CrossPoint, and where each is documented.

## Touch lock (slide switch set to Touch Reader Controls)
Details: [USER_GUIDE.md section 3.6.3](../USER_GUIDE.md#363-controls).

| Feature | How |
|---|---|
| Full touch lock | Each switch move toggles it (position does not matter); all touch ignored, side buttons work; kept through sleep |
| Lock from the light panel | Touch tile (pointer) toggles the lock; locking closes the panel |
| Lock indicator | Padlock centred in the top bar and in the top page margin (top-right corner when the margin is too small); open while overridden |
| Light panel under the lock | Up + Down together (touch works inside the panel) |
| Refresh under the lock | Power + Up together |
| Change book under the lock | Up + Down, then the house icon in the light panel goes to Home; pick a book with the buttons |
| Dictionary under the lock | Hold Up 0.4 s on a book page, tap a word: definition at once (no menu); Down backs out; short Up turns back on release |
| Unlock from Home | Hold Up 0.4 s on Home; ends on opening a book, holding Down, moving the switch, sleep, 2 min idle |
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
- Lookup history: upstream Vocabulary History (`/Vocabulary.txt`), on by default in this fork (replaces the earlier `/vocabulary/lookups.tsv`).
- Persian definitions: add Persian glyphs to your reading font with `scripts/merge_persian_font.py`.

## Home
Details: [USER_GUIDE.md section 3.1](../USER_GUIDE.md#31-home-screen).

- Hold a recent book on Home (touch long-press on the cover or Continue Reading row, or a 1 s Confirm hold where a Confirm button exists; under the touch lock hold Up on Home first): menu with the book title, **Open** and **Remove from Recents**. Remove asks to confirm, then Home reloads without the book. Only the Recents entry is removed; the book file, progress, bookmarks and per-book settings stay. Home shows only books from the Recents list, newest first, as many as fit (the first books of the Library's Recent tab); in the Cover Grid, empty slots stay empty instead of being filled from the Library. A removed book stays off Home until it is opened again.
- A touch hold on a menu row or Cover Grid tab still opens it (when the hold is detected, not on release); a Confirm hold on a menu item opens it on release, as before. A hold on empty space does nothing; a hold on the status bar opens the light panel, like a tap.

## Light
- Light panel: a Home tile replaces the Bluetooth tile (Bluetooth stays in Settings). Under the touch lock, open the panel with Up + Down and tap Home to change books.
- Warmth reads Off, 1-99%, Full (Off = cool light only, Full = warm light only). On the BOOX Picco there is no colour jump inside 1-99% at any brightness; the only visible step is into Off or Full. Fix merged upstream: Free-Ink/freeink-sdk#172.
- Smooth brightness and warmth steps using all ten vendor levels (`freeink-sdk` branch `picco-custom`; reported as Free-Ink/freeink-sdk#167, since fixed in the upstream SDK as well).
- Brightness/warmth greyed out while the light is off; lamp button reliably turns it on.
- Holding - or + in the light panel keeps stepping (first repeat after 0.4 s, then every 0.15 s); the settings are saved once, when the panel closes.
- Wake with light: hold Down while pressing Power to wake; the light comes on at the saved brightness/warmth, with no screenshot or page turn; the light panel opens with a note so the light can be adjusted (not when the slide switch is set to Frontlight). The light is saved as on, so the next wake with Restore Light on Wake on keeps it on.

## Per-book text settings
Details: [USER_GUIDE.md section 3.6.2](../USER_GUIDE.md#362-reader).

- Each EPUB book keeps its own Text Settings (font, size, spacing, indent, alignment, margin, style toggles) in `/.crosspoint/book-settings/<KOReader document id>.json`; the id hashes the book's content, so renaming or moving the book keeps them, and the book file is never changed.
- A new book starts from the last-used values; Text Settings opened from Settings with no book open set those values for books not opened yet.
- Text Settings → Layout → Reset Text Settings (book open only) restores the factory values for this book.
- Clear Reading Cache does not delete them; Home Backup includes them.

## Reader
- Battery and clock in the top page margin (EPUB, TXT, Markdown), as in the Home header: clock top-left (or left of the battery when set to Right), battery and percentage top-right, padlock centred. Only when the screen margin is larger than the small font's line height; otherwise they stay in the bottom status bar. XTC unchanged.
- Fast page-turn presses (EPUB, TXT, Markdown) are queued while a page draws (up to 20) and each page is drawn in turn; a press the other way cancels one queued turn. Bluetooth remote turns queue behind them. Opening a menu, panel or tool, adding a bookmark, or jumping (link, footnote, chapter) drops the queue.
- Book position (EPUB, TXT, Markdown) in paper pages, `p. page/total`, in the bottom-left corner of the status bar. A paper page is 1,800 characters of the book's text (like a typical paperback; may differ from a specific printed edition), whitespace runs counted once, so the number does not depend on font, size, margins or orientation. The text of each spine item is counted once per book in idle loop frames, one item per frame, and saved as `paper_pages.bin` in the book cache after each item, so a count stopped by sleep or closing the book goes on from there. A button press, or a touch on the BOOX Picco, stops the item being counted at once; it starts over in a later idle frame. Counting pauses while a menu or panel is open, while the Bluetooth page turner holds its heap, and while a chapter builds. The position appears a few seconds after the first time it can show in a book. The current page uses the page's exact text offset in the chapter. Shown with Book Progress Percentage on; see [USER_GUIDE.md section 3.6.1](../USER_GUIDE.md#361-display).
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
