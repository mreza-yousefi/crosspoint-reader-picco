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
- EPUB cover thumbnails that cannot be made are remembered, so Home (standard and Cover Grid) no longer shows the loading bar on every visit. An empty thumbnail file for that size in the book cache marks the cover, as upstream already did for unsupported image formats, and Home draws the placeholder. Only clear cases are marked: the book names no cover (and its guide cover page, if any, was read), the cover file is missing from the book, the image format is not supported, or the decoder failed with enough free memory and the whole cover extracted. Everything else (low memory, SD errors, other extraction errors, GIF covers) is tried again on the next visit. The serial log has one line per failed attempt: `cover thumb: marked (...)` or `cover thumb: retry later (...)`. To try a marked cover again, open the book and use **Delete Book Cache** in the reader menu (this book only; reading progress is kept), or **Clear Reading Cache** (all books; reading progress is lost). Replacing the book over Wi-Fi also clears it. A different thumbnail size (other theme) tries again too.
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

## Display
- Fast Page Turns (Settings → Display, BOOX Picco with the SSD2677 panel only; default On): keeps the panel powered between refreshes, so a page turn skips the power-on and power-off waits. Off powers the panel down after every refresh, as the upstream SDK and the stock firmware do. Whatever the setting, the panel is powered down before sleep (from the start of the sleep sequence, so the sleep screen and the USB-powered sleep wait run with it off). Switching the setting off powers the panel down at once. Needs a fork change in `freeink-sdk` (branch `picco-custom`): a revert of upstream commit 0ca892a, so the SSD2677 driver follows the caller's turn-off flag again. The boot log shows the mode (`Panel power between refreshes`).

## Reader
- Battery and clock in the top page margin (EPUB, TXT, Markdown), as in the Home header: clock top-left (or left of the battery when set to Right), battery and percentage top-right, padlock centred. Only when the screen margin is larger than the small font's line height; otherwise they stay in the bottom status bar. XTC unchanged.
- Fast page-turn presses (EPUB, TXT, Markdown) are queued while a page draws (up to 20) and each page is drawn in turn; a press the other way cancels one queued turn. Bluetooth remote turns queue behind them. Opening a menu, panel or tool, adding a bookmark, or jumping (link, footnote, chapter) drops the queue.
- Book position (EPUB, TXT, Markdown) in paper pages, `p. page/total`, in the right corner of the status bar; chapter page and percentage moved to the left corner, before battery and clock (also in XTC books). A paper page is 1,800 characters of the book's text (like a typical paperback; may differ from a specific printed edition), whitespace runs counted once, so the number does not depend on font, size, margins or orientation. The text of each spine item is counted once per book in idle loop frames, one item per frame, and saved as `paper_pages.bin` in the book cache after each item, so a count stopped by sleep or closing the book goes on from there. A button press, or a touch on the BOOX Picco, stops the item being counted at once; it starts over in a later idle frame. Counting pauses while a menu or panel is open, while the Bluetooth page turner holds its heap, and while a chapter builds. The position appears a few seconds after the first time it can show in a book. The current page uses the page's exact text offset in the chapter. Shown with Book Progress Percentage on; see [USER_GUIDE.md section 3.6.1](../USER_GUIDE.md#361-display).
- Space Above Status Bar / Space Below Clock/Battery (Customise Status Bar; None/Small/Medium/Large = 0/8/16/32 px; defaults Small/None) add space between the page text and the bottom status bar, and between the top-margin battery/clock line and the text (EPUB, TXT, Markdown). The bottom space applies when a line is drawn at the bottom (an item that stays in the bottom bar, or the automatic page turn line); the top space when battery or clock is shown in the top margin. The page viewport is smaller, so section caches rebuild (viewport height is in the section header): after updating, each book lays its chapters out once again on first open. Paper pages are unaffected.
- Status-bar clock (EPUB, TXT, Markdown) repaints at each new minute with one fast refresh: no gray pass, no step towards the periodic clean refresh. Not during automatic page turn, with a menu, panel, popup or message over the page, or on UC8279 absolute-gray image pages ([USER_GUIDE.md section 3.6.1](../USER_GUIDE.md#361-display)).
- The RTC is set at each SNTP sync and from the Home Backup server's `Date` header after a run that ends Ok or Partial.

## Reading statistics
Details: [docs/reading-stats.md](reading-stats.md) (data, rules, formats) and [USER_GUIDE.md section 3.9](../USER_GUIDE.md#39-reading-stats-fork).

- Every EPUB, TXT and Markdown reading session and every page change is recorded on the SD card in `/.crosspoint/stats/` (XTC is not). A book is recognised by its KOReader document id, so renaming or moving it keeps its statistics.
- Active time: a page counts up to about 3.5x the time you usually need for that much text in this book (at least 1 minute, at most 30); longer is a pause. The speed is learned per book; before that, from all books, else a flat 5 minutes.
- A session ends when the book closes, you go Home or sleep. Sessions of one book less than 10 minutes apart are one sitting. Time with the reader menu or the Stats screen over the book is not counted; dictionary lookups are. **Ignore Short Opens in Stats** (Settings → Reader; also in the Stats Overview; default On) leaves out sessions under 1 minute or without a page turn.
- Reading speed comes from the exact text of pages turned forward in a chapter, so it does not need the status-bar page count. A book is finished when a session takes it past 99% (the end-of-book screen counts as 100%).
- Without a trusted clock, reading counts in Total but not in days or charts.
- **Reading Stats** screen: Overview (today, this week, month and year, total, current and longest streak, books finished this year) and bar charts of reading time per day (30), week (26), month (12) and year. Tap a bar or walk the bars with the buttons; the arrows, a swipe, or walking past the oldest bar pages back in time. A day bar opens that day's sessions.
- Open it from Home (bar chart icon in the row of icons at the bottom of Home in the Cover Grid theme, **Reading Stats** in the Home list of the other themes) or from the reader menu (**Reading Stats**). Back goes to Home or to the book it was opened from.
- Hold a book on Home: **Hide from Stats** / **Show in Stats** keeps a test book out of the totals and charts; its data stays.
- Page data is kept in RAM and written every 3 minutes and at the session end, never on the page-turn path; a reset loses at most the last 3 minutes. Clear Reading Cache does not delete the statistics; Home Backup includes them.
- File dates on the SD card now come from the reader's clock (when it is set), so Home Backup sees files changed in place.

## Sleep screen
- Strip with "Battery at sleep N%" and "· Book N%". On USB power the first part reads "Charging N%" or "Charged N%".

## Home Backup
Details: [USER_GUIDE.md section 3.6.8](../USER_GUIDE.md#368-home-backup-fork) and [docs/home-backup.md](home-backup.md).

- Incremental backup to a WebDAV server (on request, or once a day at sleep; up to 60 s on battery, 15 min on USB power), fetches books from `/to-reader/`.
- No server details compiled in: enter URL and login on the reader; for HTTPS put the server certificate at `/.crosspoint/backup-ca.pem`.

## Build and flashing
- `pio run -e picco -t upload` writes only the app image (`scripts/app_only_upload.py`; upstream issue #4248).
