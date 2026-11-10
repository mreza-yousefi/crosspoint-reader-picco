# CrossPoint Picco

CrossPoint Picco is a fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) for the
BOOX Picco. It is a personal project. It is built on CrossPoint Reader and the
[FreeInk SDK](https://github.com/Free-Ink/freeink-sdk), and keeps their reading engine, file formats and most of
their features. This fork refines some of them and adds features for daily reading on the BOOX Picco. It is not the
official CrossPoint Reader.

## What this fork adds

A detailed list is in [docs/fork-features.md](docs/fork-features.md).

### Touch lock and button shortcuts

Details: [User Guide, Controls](USER_GUIDE.md#363-controls).

- The slide switch is a touch lock toggle (setting **Slide Switch: Touch Reader Controls**). Touch is ignored, the
  side buttons still turn pages, and the lock stays on through sleep.
- In the light panel, the touch tile (pointer icon) turns the lock on or off. Locking closes the panel.
- A padlock shows the lock in the top bar and at the top of the book page.
- Up + Down opens the light panel while touch is locked. Its Home button lets you change the book.
- Power + Up gives a full screen refresh while touch is locked.
- Hold Up on Home to unlock touch. The lock comes back when you open a book, hold Down, move the switch, or leave
  the reader unused for 2 minutes.
- Waking into a book with the lock on shows the light panel first, so a light left on is easy to notice.
- A button shortcut never also turns a page.

### Dictionary

Details: [docs/dictionary.md](docs/dictionary.md#the-definition-screen).

- Definitions open as a sheet over the lower part of the screen. The book page stays visible above it.
- With touch locked, hold Up on a book page and tap a word. The definition opens at once, without a menu.
- The selection menu is simpler: Look Up, Another word and +. After a lookup, the reader goes back to the book.
- Each lookup is added to a list in `/vocabulary/lookups.tsv`.
- Persian definitions: add Persian letters to your reading font with a script
  ([how](docs/dictionary.md#persian-and-other-arabic-script-definitions)).

### Front light

Details: [User Guide, Control Center and Frontlight](USER_GUIDE.md#control-center-and-frontlight).

- Smoother brightness and warmth steps.
- Warmth reads Off, 1-99%, Full, with no colour jump inside 1-99%.
- A Home button in the light panel. It takes the place of the Bluetooth button; Bluetooth stays in Settings.
- Brightness and warmth are greyed out while the light is off.
- Hold Down while pressing Power to wake with the light on, for reading in the dark. The light is then saved as on, so with
  Restore Light on Wake on, the next wake keeps it on.
- Hold - or + in the light panel to keep changing the level.

### Home Backup

Details: [User Guide, Home Backup](USER_GUIDE.md#368-home-backup-fork) and [docs/home-backup.md](docs/home-backup.md).

- Backs up the SD card to your own WebDAV server: on request, or once a day when the reader goes to sleep. Only
  changed files are sent.
- Downloads books you put in the server's `/to-reader/` folder.
- No server details are built into the firmware. For HTTPS the reader trusts only a certificate you copy to the SD
  card.

### Reading

Details: [User Guide, Reader](USER_GUIDE.md#362-reader).

- Per-book Text Settings (EPUB): font, size, layout and style are saved for each book. A book is recognised by
  its content, so the settings stay when you rename or move the file. Home Backup includes them.
  **Text Settings → Layout → Reset Text Settings** sets the book back to the default values.
- With enough screen margin, the battery and clock show at the top of the page, as on Home: clock top-left,
  battery top-right. Otherwise they stay in the bottom status bar.
- Fast page-turn presses are not lost: each one shows its page in turn (up to 20 can wait).
- The position in the whole book, `p. 212/380`, shows in the bottom-left corner of the status bar. It counts paper
  pages: about 1,800 characters each, like a typical paperback, so it does not change with font, size, margins or
  orientation. It may differ from a specific printed edition. It appears a few seconds after the first time the
  position can show in a book, once the book's text is counted. Counting pauses while you press buttons or a menu is
  open. It shows when **Book Progress Percentage** is on (Settings → Reader → Customise Status Bar).
- The status-bar clock (Settings → Reader → Customise Status Bar → **Clock**) updates at each new minute while you
  read (EPUB, TXT, Markdown), but not during automatic page turn or with a menu open. It is corrected at each Wi-Fi
  time sync and after each Home Backup that does not fail.

### Sleep screen

Details: [User Guide, Sleep Screen](USER_GUIDE.md#37-sleep-screen).

- A strip shows the battery level at sleep and the progress in the open book. On USB power it shows the charging
  state in place of the battery level.

### Home

Details: [User Guide, Home Screen](USER_GUIDE.md#31-home-screen).

- Hold a book on Home for a menu: **Open** or **Remove from Recents**. Only the Recents entry is removed; the book and
  its progress stay. Home shows only books from your Recents, the
  same books that come first in the Library's Recent tab. A removed book stays off Home until you open it again.

### Other

- `pio run -e picco -t upload` writes only the app image. The vendor bootloader and recovery partition stay in
  place (upstream issue [#4248](https://github.com/crosspoint-reader/crosspoint-reader/issues/4248)).

## Contributions upstream

- The BOOX Picco warmth fix (no colour jump inside 1-99%) was merged into the FreeInk SDK
  ([Free-Ink/freeink-sdk#172](https://github.com/Free-Ink/freeink-sdk/pull/172)).

## Install

There are no ready-made release files. You build the firmware from source and flash it over USB. It replaces the
stock firmware on your reader. Do this at your own risk. There is no warranty.

1. Set up PlatformIO (pioarduino) as described in [Getting started](docs/contributing/getting-started.md).
2. Clone the repository with its submodules. The FreeInk SDK is a submodule
   ([fork, branch `picco-custom`](https://github.com/mreza-yousefi/freeink-sdk-picco/tree/picco-custom)).

   ```sh
   git clone --recurse-submodules https://github.com/mreza-yousefi/crosspoint-reader-picco.git
   cd crosspoint-reader-picco
   ```

3. Build:

   ```sh
   pio run -e picco
   ```

4. Switch the reader on and leave it on a normal screen, such as Home (not USB Drive mode). Connect it by USB and
   flash:

   ```sh
   pio run -e picco -t upload
   ```

The upload writes only the app into the stock `app` slot. The vendor bootloader, NVS and the `safeboot` recovery
partition are not changed (see [partitions_picco.csv](partitions_picco.csv) and
[scripts/app_only_upload.py](scripts/app_only_upload.py)). This repository does not document how to go back to the
stock firmware.

## Screenshots

Coming soon.

## Licence

- Upstream CrossPoint Reader code is under the [MIT License](LICENSE).
- This fork's changes and additions are under the
  [PolyForm Noncommercial License 1.0.0](LICENSE-FORK.md). You may use, change and share them for noncommercial
  purposes.
- For a commercial licence, open an issue in this repository or contact the author through GitHub.

See [NOTICE.md](NOTICE.md) for the details.

## Credits

This fork would not exist without the contributors of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
and the [FreeInk SDK](https://github.com/Free-Ink/freeink-sdk). For upstream features, other devices and how to
contribute to CrossPoint, see the [upstream README](docs/upstream-README.md).

This project is not affiliated with the device maker.
