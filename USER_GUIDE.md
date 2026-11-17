# CrossPoint User Guide

Welcome to the **CrossPoint** firmware. This guide outlines the hardware controls, navigation, and reading features of the device.

- [CrossPoint User Guide](#crosspoint-user-guide)
  - [1. Hardware Overview](#1-hardware-overview)
    - [Button Layout](#button-layout)
    - [Taking a Screenshot](#taking-a-screenshot)
    - [Control Center and Frontlight](#control-center-and-frontlight)
  - [2. Power \& Startup](#2-power--startup)
    - [Power On / Off](#power-on--off)
    - [First Launch](#first-launch)
  - [3. Screens](#3-screens)
    - [3.1 Home Screen](#31-home-screen)
    - [3.2 Reading Mode](#32-reading-mode)
    - [3.3 Browse Files Screen](#33-browse-files-screen)
    - [3.4 Library Screen](#34-library-screen)
    - [3.5 File Transfer Screen](#35-file-transfer-screen)
    - [3.5.1 Calibre Wireless Transfers](#351-calibre-wireless-transfers)
      - [Installing the Plugin in Calibre](#installing-the-plugin-in-calibre)
      - [Configuring the CrossPoint Plugin in Calibre](#configuring-the-crosspoint-plugin-in-calibre)
      - [Uploading Books](#uploading-books)
      - [Removing a Book](#removing-a-book)
    - [3.6 Settings](#36-settings)
      - [3.6.1 Display](#361-display)
      - [3.6.2 Reader](#362-reader)
      - [3.6.3 Controls](#363-controls)
      - [3.6.4 System](#364-system)
      - [3.6.5 OPDS Servers (Multiple Libraries)](#365-opds-servers-multiple-libraries)
      - [3.6.6 Web Settings (Wi-Fi + OPDS)](#366-web-settings-wi-fi--opds)
      - [3.6.7 KOReader Sync Quick Setup](#367-koreader-sync-quick-setup)
        - [Option A: CrossPoint Sync Server (`sync.crosspointreader.com`, default)](#option-a-crosspoint-sync-server-synccrosspointreadercom-default)
        - [Option B: Legacy Public KOReader Server (`sync.koreader.rocks`)](#option-b-legacy-public-koreader-server-synckoreaderrocks)
        - [Option C: Self-Hosted Server (Docker Compose)](#option-c-self-hosted-server-docker-compose)
        - [Syncing While Reading](#syncing-while-reading)
      - [3.6.8 Home Backup (fork)](#368-home-backup-fork)
    - [3.7 Sleep Screen](#37-sleep-screen)
      - [Cover settings](#cover-settings)
      - [Custom images](#custom-images)
    - [3.8 Custom Fonts (SD Card)](#38-custom-fonts-sd-card)
    - [3.9 Reading Stats (fork)](#39-reading-stats-fork)
  - [4. Reading Mode](#4-reading-mode)
    - [Page Turning](#page-turning)
    - [Chapter Navigation](#chapter-navigation)
    - [Auto Page Turn](#auto-page-turn)
    - [Tilt Page Turn (X3 only)](#tilt-page-turn-x3-only)
    - [Links and footnotes](#links-and-footnotes)
    - [Dictionary Lookup](#dictionary-lookup)
    - [System Navigation](#system-navigation)
    - [Supported Languages](#supported-languages)
  - [5. Reader Menu](#5-reader-menu)
    - [5.1 Chapter Selection](#51-chapter-selection)
    - [5.2 Bookmarks](#52-bookmarks)
    - [5.3 Clippings and Highlights](#53-clippings-and-highlights)
  - [6. Current Limitations \& Roadmap](#6-current-limitations--roadmap)
  - [7. Troubleshooting Issues \& Escaping Bootloop](#7-troubleshooting-issues--escaping-bootloop)

## 1. Hardware Overview

The device utilises the standard buttons on the Xteink X4 (in the same layout as the manufacturer firmware, by default):

### Button Layout

| Location        | Buttons                                              |
| --------------- | ---------------------------------------------------- |
| **Bottom Edge** | **Back**, **Confirm**, **Left**, **Right**           |
| **Right Side**  | **Power**, **Side Up**, **Side Down**, **Reset** |

Button layout can be customized in the **[Controls Settings](#363-controls)**.

### Taking a Screenshot

When the Power button and the lower side button (Side Down) are pressed at the same time, it will take a screenshot and save it in the folder `screenshots/`.

Alternatively, while reading a book, press the **Confirm** button to open the reader menu and select **Take screenshot**.

### Control Center and Frontlight

The Control Center is available on all touch devices. Devices with a frontlight also show light controls. The X4 Pro and the BOOX Picco have adjustable brightness and warmth; the Paper Mono has adjustable brightness only. This fork's docs call this panel the light panel.

* **Open the panel:** Swipe down from the top edge of the screen, from almost any screen (Home, Browse Files, Reading Mode, etc.). You can also tap (or hold) the status bar on Home, Browse Files, or Settings. On devices with a frontlight, drag the brightness slider (and the warmth slider, where available) to adjust the light live, or tap the sun icon to turn it on or off.
* **Quick controls:** The panel has one row of icons: moon for night mode, circular arrows to refresh the screen, an underlined **T** to cycle reading direction, and the Settings Controls pointer to toggle reader touch controls. **This fork:** a fifth button, a house, opens Home. It replaces the Bluetooth button; the Bluetooth page turner is in **Settings → Connectivity**. With the touch lock on, press Up + Down to open the panel, then tap the house to change books. On Home, choose a book with the buttons, or hold Up to unlock touch. The **T** rotates with the selected direction. A filled toggle means enabled; an outlined toggle means disabled. Refresh, direction and Home always have outlines. Turning reader touch controls off leaves the panel usable, unless the slide switch is set to Touch controls while reading: then the tile is the touch lock, and locking closes the panel.
* **Warmth readout (this fork):** on devices with warmth the readout goes **Off**, 1–99%, **Full**. Off is cool light only and Full is warm light only, the warmest the light can go. On the BOOX Picco both LEDs stay on between 1 and 99%, so the colour changes smoothly at every brightness; the only visible step is into Off or Full.
* **Wake with the light on (BOOX Picco, this fork):** hold the **Down** side button, press **Power** to wake, and keep holding Down until the light comes on. It comes on at your saved brightness and warmth, even with Restore Light on Wake off, and the press takes no screenshot and turns no page. The screen starts while you still hold the buttons; they do nothing until you let go. The light panel then opens with the note "Light on (Down + Power wake)", so you can adjust or switch off the light; tap below it to continue. The reader then saves the light as on, so with Restore Light on Wake on, the next wake also has the light on. It does not apply when the slide switch is set to Frontlight; then the switch decides.
* **Quick toggle:** On devices with a frontlight, set **Settings → Controls → Power button → Double click** to **Frontlight** (the default on X4 Pro and EEGO A4), then double-click the **Power** button to turn the frontlight on or off instantly, without opening the panel. Not when the slide switch is set to Frontlight; then the switch decides.

> [!NOTE]
> Frontlight brightness and warmth are intentionally not listed in **[Display Settings](#361-display)** — the swipe panel is the only place to adjust them. The on/off state can also be toggled with the Power-button double-click above.

If the frontlight doesn't come back on after the device wakes from sleep, check **Restore Light on Wake** in **[Display Settings](#361-display)** (on by default). Turning it off is intentional if you'd rather have the light stay off on wake and switch it on yourself each time — but it's easy to forget you changed it.

---

## 2. Power & Startup

### Power On / Off

To turn the device on or off, **press and hold the Power button for approximately half a second**.
In the **[Controls Settings](#363-controls)** you can configure the power button to turn the device off with a short press instead of a long one.

To reboot the device (for example after a firmware update or if it's frozen), press and release the Reset button, and then quickly press and hold the Power button for a few seconds.

### First Launch

Upon turning the device on for the first time, you will be placed on the **[Home](#31-home-screen)** screen.

> [!NOTE]
> On subsequent restarts, the firmware will automatically reopen the last book you were reading.

---

## 3. Screens

### 3.1 Home Screen

The Home screen is the main entry point to the firmware. From here you can navigate to **[Reading Mode](#4-reading-mode)** with the most recently read book, **[Browse Files](#33-browse-files-screen)**, the **[Library](#34-library-screen)**, **[File Transfer](#35-file-transfer-screen)**, **[Reading Stats](#39-reading-stats-fork)** (this fork), or **[Settings](#36-settings)**.

**Remove a book from Recents (this fork):** long-press a book's cover (or its Continue Reading row), or select it and hold **Confirm** for one second (devices with a Confirm button). A menu with the book's title offers **Open** and **Remove from Recents**, and **Hide from Stats** or **Show in Stats** and **Reading Stats** (the book's own statistics) once the book has [reading statistics](#39-reading-stats-fork). Remove asks you to confirm; Home then shows the list without the book. Only the Recents entry is removed: the book file, reading progress, bookmarks and per-book settings stay. Home shows only books from your Recents, newest first, as many as fit: the same books that come first in the Library's **Recent** tab. In the Cover Grid, empty places stay empty. A removed book stays off Home until you open it again. With touch locked (BOOX Picco), hold Up on Home to unlock touch first.

**Placeholder cover (this fork):** if an EPUB's cover cannot be made into a thumbnail, Home shows a placeholder and stops trying. To try again, open the book and use **Delete Book Cache** in the reader menu.

### 3.2 Reading Mode

See [Reading Mode](#4-reading-mode) below for more information.

### 3.3 Browse Files Screen

The Browse Files screen acts as a file and folder browser. The full path to the current directory is shown at the top of the screen. File extensions are displayed alongside each filename (turn off **Show File Extensions** in **Settings → System** to give names the full row; files that differ only by extension, such as `book.epub` and `book.xtc`, then look alike), and directories are shown with brackets (e.g. `[folder-name]`). Hidden directories (those beginning with `.`) are also visible.

* **Navigate List:** Use **Left** (or **Side Up**), or **Right** (or **Side Down**) to move the selection cursor up and down through folders and books. You can also long-press these buttons to scroll a full page up or down.
* **Open Selection:** Press **Confirm** to open a folder or start reading a selected book. Selecting a `.bmp` file will open the image viewer.
* **Delete Files or Folders:** Hold and release **Confirm** to delete the selected file or folder. You will be given an option to either confirm or cancel. Multiple files can be selected for deletion in a single operation.
* **Rename or Move:** Files can be renamed or moved to a different folder from within the browse screen.

### 3.4 Library Screen

The Library indexes up to 4,096 supported books on the SD card and shows their titles and authors without requiring you to remember their folders. Its three tabs provide different views. An arrow beside the active tab shows the sort direction:

- **Recent** shows the books you opened most recently (up to ten) at the top, followed by the rest of the library ordered by when each file arrived on the card. Down shows the newest first; up shows the oldest files first, without the opened books on top. With **Hide New Files in Recent** turned on, the tab lists only the books you opened: down shows the most recently opened first, and up shows the least recently opened first. The setting applies only while no search is active; search results include books you have not opened. Holding a book in this tab opens a menu that also has **Reading Stats** for a book with [reading statistics](#39-reading-stats-fork) (this fork).
- **Title** groups books by the first letter of the title. Up sorts A-Z and down sorts Z-A. Titles beginning with numbers or punctuation appear under `#`; letters from non-English scripts, including Hebrew, have their own groups.
- **Author** groups books by author. Up sorts A-Z and down sorts Z-A.

`My Clippings.txt` and `crash_report.txt` are not listed in the Library. They remain available in the file browser.

On a button-only device:

- Use **Up/Down** or **Left/Right** to move one row at a time. Hold a direction to move a page at a time.
- Press **Confirm** to open the selected book.
- Press **Back** from the book list to focus the tabs. While the tabs are focused, press **Confirm** to switch to the next tab, hold **Confirm** to reverse the sort direction, press **Up** to open Search, or press **Down** to return to the list.
- In the Recent view, hold **Confirm** on a book to open its options: **Open**, **Remove from Recents** (opened books only), **Delete**, and **Refresh Library**.
- In the Title or Author views, hold **Confirm** on a book to collapse the list to its letter or author groups. The matching group remains selected. Press **Confirm** to enter a group, or **Back** to restore the exact book and position you came from.
- While a search is active in the Title or Author views, hold **Confirm** on a book to delete it. Press **Back** to clear the search.

On a touch device, tap tabs, books, and the Search icon directly. Tap the active tab again to reverse its sort direction. Swipe up or down to scroll. Long-press a book in the Recent view to open its options. Long-press a book in a Title or Author view to collapse to the group list, then tap a group to expand it. Swipe a row sideways to reveal a delete button. Tap the refresh icon in the header to rebuild the index.

The index is created automatically the first time the Library is opened, and it is rebuilt after books are added through file transfer. To pick up other file changes or updated metadata, choose **Refresh Library** from a Recent book's options, or tap the refresh icon on a touch device. **Settings → Library → Refresh Library** does the same from any device, including when the Library shows no books. The **Use Book Metadata** setting controls whether the index reads titles and authors stored inside books.

### 3.5 File Transfer Screen

The File Transfer screen allows you to upload and manage files on the device. When you enter the screen, choose **Join a Network**, **Calibre Wireless**, or **Create Hotspot**. The reader then starts the web server for the selected mode. The screen also lists **OPDS Browser**, which opens your OPDS catalogs; with no server configured it opens the server list so you can add one (see [OPDS Servers](#365-opds-servers-multiple-libraries)). When the plugin system is off and a server is configured, OPDS Browser is on the Home screen instead.

**Home Backup** (this fork) is also listed here: it backs the reader up to your home server and fetches books queued there. See [Home Backup](#368-home-backup-fork).

See the [web server docs](./docs/webserver.md) for more information on how to connect to the web server and upload files.

The web interface also supports **WebDAV**, allowing you to mount the device as a network drive and manage files directly from your computer's file manager.

Download links for files already on the device are available in the web interface, so you can retrieve books or screenshots over Wi-Fi without connecting a cable.

A **Wi-Fi signal strength indicator** (dBm) is displayed on-screen during joined-network web server sessions.

> [!TIP]
> Advanced users can also manage files programmatically or via the command line using `curl`. See the [web server docs](./docs/webserver.md) for details.
> [!TIP]
> If your EPUBs have compatibility issues, you can run the built-in **EPUB Optimizer** directly from the device to clean up and reprocess books for better rendering.

### 3.5.1 Calibre Wireless Transfers

CrossPoint supports sending books from Calibre using the CrossPoint Reader device plugin.

#### Installing the Plugin in Calibre

If you don't already have the plugin installed:

1. Head to https://github.com/crosspoint-reader/calibre-plugins/releases to download the latest version of the crosspoint_reader plugin.
2. Download the zip file.
3. Open Calibre → Preferences → Plugins → Load plugin from file → Select the zip file.
4. Restart Calibre.

#### Configuring the CrossPoint Plugin in Calibre
1. In Calibre select Preferences.
2. In the Preferences dialog select Plugins.
3. In Plugins search for "crosspoint".
4. Click on "Customize plugin".
5. Update the value for "Host" to match the IP for your device.
6. Leave the other settings as they are.
7. [optional] Modify the "Upload path" to point to a subfolder other than the root "/" folder. Enter this as a path relative to the root folder. Example: `/mybooks`
8. Restart Calibre.

<img width="420" height="385" alt="Image" src="https://github.com/user-attachments/assets/01fc7e33-a9a7-48ba-9e26-2e68d1f9daec" />

#### Uploading Books

To upload a book using the CrossPoint plugin in Calibre:

1. On the device: File Transfer -> Calibre Wireless, then join a network.
2. Select one or more books.
3. Right-click on that selection.
4. Select "Send to Device" > "Send to main memory"

The CrossPoint plugin will connect to your device, create a folder for the book's author in the root folder (or the folder you configured for the plugin), then copy the book into that folder.

<img width="783" height="310" alt="Image" src="https://github.com/user-attachments/assets/741b0909-2e1d-4f16-8af0-2c43fbda5ce6" />

#### Removing a Book

Books cannot be removed from your device through Calibre. Use the web interface instead.

### 3.6 Settings

Settings are grouped into **System**, **Display**, **Reader**, **Controls**, **Library**, and **Connectivity**. System contains language, keyboard layouts, hidden-file visibility, file-extension visibility, Clock (where available), firmware updates, and About. Library contains Use Book Metadata, Hide New Files in Recent, Clear Read Books from Recent List, Move Finished Books to Read Folder, and Refresh Library. Time to Sleep is under Display; Clear Reading Cache is under Reader. Connectivity contains Wi-Fi, Bluetooth Page Turner, CrossPoint Sync, OPDS, and the Plugins toggle. The options below describe these settings.

#### 3.6.1 Display

- **Sleep Screen**: Which sleep screen to display when the device sleeps:
  
  - "Dark" (default) - The default dark Crosspoint logo sleep screen
  - "Light" - The same default sleep screen, on a white background
  - "Custom" - Custom images from the SD card; see [Sleep Screen](#37-sleep-screen) below for more information
  - "Cover" - The book cover image
  - "None" - A blank screen
  - "Cover + Custom" - The book cover image while actively reading, falls back to "Custom" behavior otherwise
  - "Quick resume" - The text of the last page read will be displayed on the sleep screen and a moon icon is shown on the edge of the screen. Waking up the device will return to the same page of the opened book. This is useful for quickly resuming reading without waiting for the device to fully wake up and load the book.
  - "Transparent" - A transparent overlay image drawn over the current screen; see [Sleep Screen](#37-sleep-screen) below for more information
  - "Cover + Transparent" - The transparent overlay image drawn over the cover of the last opened book, falls back to "Transparent" behavior if there is no book cover
- **Sleep Screen Cover Mode**: How to display the book cover or custom sleep image:
  
  - "Fit" (default) - Scale the image to fit centered on the screen, padding with white borders as necessary
  - "Crop" - Scale the image and crop as necessary to try to fill the screen

- **Sleep Screen Cover Filter**: What filter will be applied to the book cover when "Cover" sleep screen is selected:
  
  - "None" (default) - The cover image will be converted to a grayscale image and displayed as it is
  - "Contrast" - The image will be displayed as a black & white image without grayscale conversion
  - "Inverted" - The image will be inverted as in white & black and will be displayed without grayscale conversion

- **Quick Resume on Timeout**: Whether to enable the "Quick Resume" sleep screen when the device goes to sleep due to inactivity (Display > Time to Sleep). This is useful for quickly resuming reading without waiting for the device to fully wake up and load the book. This overwrites the Sleep Screen Cover Mode when enabled.

- **Status Bar**: Configure the status bar displayed while reading:
  
  - "None" - No status bar
  - "No Progress" - Show status bar without reading progress
  - "Full w/ Percentage" - Show status bar with book progress (as percentage)
  - "Full w/ Book Bar" - Show status bar with book progress (as bar)
  - "Book Bar Only" - Show book progress (as bar)
  - "Full w/ Chapter Bar" - Show status bar with chapter progress (as bar)

- **Battery and clock at the top of the page (EPUB, TXT, Markdown, this fork)**: when the screen margin leaves room above the text, the battery and the clock move from the bottom status bar to the top of the page, placed as on Home: the clock in the top-left corner, the battery (with its percentage, unless **Hide Battery %** hides it) in the top-right corner. With **Clock** set to Right, the clock sits left of the battery instead. When the margin is too small, they stay in the bottom status bar. The bottom bar keeps the chapter title, page counter and progress bar. XTC books keep the battery and clock in their status bar.

- **Status bar clock (this fork)**: when **Settings → Reader → Customise Status Bar → Clock** is set to Right or Left, the clock updates by itself at each new minute while you read an EPUB, TXT or Markdown book, not only when you turn a page. During automatic page turn it updates only when the page turns. While a menu, panel or message is open over the page, it waits until the page is back. The clock is set again each time the reader gets the time over Wi-Fi. It is also set from your server's time after each Home Backup run that finishes or stops at its time limit, so keep the server's clock right.

- **Book position (EPUB, TXT, Markdown, this fork)**: the right corner of the status bar shows your position in the whole book as `p. 212/380` (page in book / pages in book). These are paper pages: the book's text is cut into pages of about 1,800 characters, like a typical paperback. So the numbers do not change with font, font size, margins or orientation, and they do not jump when a new chapter starts. They may differ from a specific printed edition of the book. The first time the position can show in a book, the reader counts the book's text in the background while you read, and the position appears after a few seconds (longer for a very large book); after that it shows at once. Counting pauses while you press buttons or a menu is open, and goes on from where it stopped, also after sleep. Clearing the reading cache makes the reader count again. The left corner shows the page within the chapter and the percentage, followed by the battery and clock when they are not at the top of the page. The position is shown when **Settings → Reader → Customise Status Bar → Book Progress Percentage** is on. In this fork the page within the chapter and the percentage are always in the left corner of the status bar, also in XTC books.

- **Space around the status bar (EPUB, TXT, Markdown, this fork)**: two settings in **Settings → Reader → Customise Status Bar** add empty space between the page text and the status lines. Each can be None, Small, Medium or Large (about 0, 8, 16 or 32 pixels).
  - **Space Above Status Bar** (default Small): space between the last line of text and the bottom status bar. It is used when a line is drawn at the bottom: any status bar item that stays at the bottom (title, page counter, percentage, progress bar, or battery and clock when they are not at the top), or the automatic page turn line.
  - **Space Below Clock/Battery** (default None): space between the battery and clock at the top of the page and the first line of text. It is used only when the battery or the clock is shown in the top margin.

  A larger space leaves less room for text, so pages hold fewer lines. After a change, each chapter is laid out again the next time you open it. After updating to this version, each book lays its chapters out once again on first open (pages hold slightly fewer lines with the default Small space). The book position in paper pages does not change. The space is added on top of the screen margin, which the text now also keeps above the status bar.

- **Hide Battery %**: Configure where to suppress the battery percentage display in the status bar; the battery icon will still be shown:
  
  - "Never" (default) - Always show battery percentage
  - "In Reader" - Show battery percentage everywhere except in reading mode
  - "Always" - Always hide battery percentage

- **Refresh Frequency**: Set how often the screen does a full refresh while reading to reduce ghosting; options are every 1, 5, 10, 15, or 30 pages.

- **UI Theme**: Set which UI theme to use:
  
  - "Classic" - The original Crosspoint theme
  - "Lyra" - The new theme for Crosspoint featuring rounded elements and menu icons
  - "Lyra Extended" - Lyra, but displays 3 books instead of 1 on the **[Home Screen](#31-home-screen)**
  - "RoundedRaff" - A rounded theme with additional visual styling

- **Sunlight Fading Fix**: Configure whether to enable a software-fix for the issue where white X4 models may fade when used in direct sunlight:
  
  - "OFF" (default) - Disable the fix
  - "ON" - Enable the fix

- **Fast Page Turns** (BOOX Picco with the SSD2677 panel only): Keeps the screen powered between page turns, so each
  turn is a little faster:

  - "ON" (default) - Keep the screen powered between refreshes
  - "OFF" - Power the screen down after every refresh. Should use less battery while a page stays on screen, but each
    page turn is slower

  Whatever the setting, the screen is powered down before sleep.

#### 3.6.2 Reader

**Per-book text settings (this fork).** Each EPUB book keeps its own text settings: font family and size, line, word and character spacing, extra paragraph spacing, paragraph indentation, alignment, screen margin, focus reading, hyphenation, embedded style and text anti-aliasing. Other reader settings (such as Images and Reading Orientation) stay global.

- Changes made in **Text Settings** from the Reader Menu, or in the **Text** panel of the reader toolbar, apply to the open book only. The values are saved for that book and are used again every time you open it. The **Text** panel has a **Text Anti-Aliasing** on/off row (this fork); turning it off cleans the gray edges with one full refresh.
- **Settings → Reader → Text Settings** holds the defaults: the values a book you open for the first time starts with. Only this screen (and the web settings page) changes them; changes inside a book never do. Settings always shows the defaults, not the values of the last book you read.
- When you leave **Settings → Reader → Text Settings** after changing something, it asks **Apply changes to**:
  - **New books only** (or **Back**) keeps the books you already opened as they are. Going Home or to sleep from this screen does the same.
  - **All books** also writes the changed values to every book you opened before, while "Applying..." shows. Only the settings you changed are written: if you turned Text Anti-Aliasing off, every book gets it off and keeps its own font, size and other values. A font change keeps each book's own size unless you also chose a size. If a layout value changed (font, size, spacing, indentation, alignment, margin, hyphenation, embedded style, focus reading), each book is laid out again the next time you open it. If some books could not be updated, "Some books not updated" shows.
- Text settings changed on the web settings page change only the values for new books; there is no "All books" there.
- **Text Settings → Layout → Reset Text Settings** (only with a book open) sets this book's text settings back to the factory defaults and lays the book out again.
- The settings are stored in `/.crosspoint/book-settings/`, one small file per book. The file name comes from the book's content, so the settings stay with a book you rename or move. **Clear Reading Cache** does not delete them, and Home Backup includes them.

The Reader settings are:

- **Reader Font Family**: Choose the font used for reading:
  
  - "Noto Sans" (default) - Google's sans-serif font
  - Installed SD card families

- **Reader Font Size**: Choose a point size. Built-in and direct TTF/OTF/TTC fonts offer 12, 14, 16, and 18 pt. A `.cpfont` family offers the sizes installed for that family.

- **Reader Line Spacing**: Space added between lines, as a percentage of font size: 0%, +10%, +20% (default), +30%, +45%, +60%, or +75%.

- **Reader Screen Margin**: Controls the screen margins in Reading Mode between 5 and 40 pixels in 5-pixel increments.

- **Reader Paragraph Alignment**: Set the alignment of paragraphs; options are "Justified" (default), "Left", "Center", or "Right".

- **Embedded Style**: Whether to use the EPUB file's embedded HTML and CSS stylisation and formatting; options are "ON" or "OFF".

- **Hyphenation**: Whether to hyphenate text in Reading Mode; options are "ON" or "OFF". Korean text wraps only at spaces when "OFF"; when "ON", a Korean word may also wrap at the end of a line between syllables or where it meets digits, Latin letters, or brackets (no hyphen is drawn).

- **Reading Orientation**: Set the screen orientation for reading EPUB files:
  
  - "Portrait" (default) - Standard portrait orientation
  - "Landscape CW" - Landscape, rotated clockwise
  - "Inverted" - Portrait, upside down
  - "Landscape CCW" - Landscape, rotated counter-clockwise

- **Extra Paragraph Spacing**: Set how to handle paragraph breaks:
  
  - "ON" - Vertical space will be added between paragraphs in Reading Mode
  - "OFF" - Paragraphs will not have vertical space added, but will have first-line indentation

- **Dictionary**: Select the StarDict dictionary used for word lookups while reading, or "None" to disable lookups. *(Only shown when at least one dictionary folder exists under `/dictionaries/` on the SD card — see [docs/dictionary.md](docs/dictionary.md) for setup and usage.)*

- **Save Vocabulary Lookups**: Save each successful lookup started from a book page to `/Vocabulary.txt` ([format](docs/dictionary.md#vocabulary-history)). On by default in this fork. *(Shown with **Dictionary**.)*

- **Ignore Short Opens in Stats** (this fork): Leave reading sessions under a minute, or without a page turn, out of the [Reading Stats](#39-reading-stats-fork). They stay recorded; switching this changes the totals at once. On by default.

- **Text Anti-Aliasing**: Whether to show smooth grey edges (anti-aliasing) on text in reading mode. Note this slows down page turns slightly. **This fork:** saved per book; to change it for books you already opened, choose **All books** when you leave Text Settings (see [Per-book text settings](#362-reader)).

- **Images**: How embedded images (JPG/PNG/GIF) in EPUB files are shown. GIFs render only the first frame; interlaced GIFs are not supported.
  - **Display** (default): images in gray levels. Pages with images take an extra grayscale refresh.
  - **Placeholder**: the image's alt text in place of the image.
  - **Suppress**: images are left out.
  - **Black & White**: images drawn as a black-and-white dot pattern. Pages with images turn like text pages, without the extra refresh.

- **Focus Reading**: Bolds the first part of each word to create visual fixation points, similar to Bionic Reading. This can help improve reading speed and focus; options are "ON" or "OFF" (default). In this fork, words in Arabic script (Persian, Arabic, Urdu) are left as they are, because a bold start would break the joining of their letters.

#### 3.6.3 Controls

Controls are grouped into Home button gestures, Power button, Front buttons,
Side buttons, and Taps and gestures where supported by the device. See the
[Controls guide](docs/controls.md) for the menu hierarchy and power-button double-click actions.

- **Remap Front Buttons**: A menu for customising the function of each bottom edge button.

- **Layout (reader only)**: Swap the order of the up and down side buttons from "Prev/Next" (default) to "Next/Prev". You can also disable them entirely. This change is only in effect when reading.

- **Orient Front Buttons**: Make front-button directions follow the reading orientation. Available on devices without touch.
- **Orient Side Buttons**: Available on all supported devices with side page buttons, including touch and button-only devices. Reverse the reader's selected **Layout (reader only)** in both landscape modes when enabled. Normal portrait always keeps the selected layout, and upside-down portrait always reverses it, whether this option is on or off. Layouts that give both buttons the same action, or disable them, are unchanged. This option is off by default, including when upgrading from a version without it. Existing saved choices are preserved.

- **Page turn long press**: Set the action for a long press of a page-turn button.
  Open **Settings → Controls → Front buttons** or **Settings → Controls → Side buttons** on supported devices.
  Both entries change the same setting:

  - "Off" (default) - No special long-press action.
  - "Chapter Skip" - In EPUBs, hold Next to open the next chapter or Previous to return to the chapter start. At the chapter start, hold Previous to open the previous chapter.
  - "Orientation Change" - Hold a page-turn button to rotate the reading orientation.

- **Menu long press**: Selects the function bound to holding the menu button (Confirm) while reading an EPUB. **Cycles through the available functions** each time the setting is selected — additional functions may be added in future releases, so this is not a binary on/off toggle. A short press of Confirm always opens the reader menu as normal:
  - "Bookmark" (default) - Hold Confirm (~0.4 second) to drop a bookmark at the current page.
  - "CrossPoint Sync" - Hold Confirm (~1 second) to launch KOReader sync directly.
  - "Dictionary" - Hold Confirm (~0.4 second) to start dictionary word selection on the current page (see [docs/dictionary.md](docs/dictionary.md)).
  - "Create Clipping" - Hold Confirm (~0.4 second) to start clipping selection on the current page.
  - "Disabled" - Long-press is ignored; only short-press opens the reader menu.

- **Short click**: Controls the effect of a short click of the power button:
  
  - "Ignore" (default) - Require a long press to turn off the device
  - "Sleep" - A short press puts the device into sleep mode
  - "Page Turn" - A short press in reading mode turns to the next page; a long press turns the device off
  - "Links and Footnotes" - A short press in reading mode opens link selection. If the page has only one link, it opens directly. After following a link, a short press returns to the previous location if no other jump came after it (this fork); otherwise use **Back** in the Reader Menu.
  - "Refresh" - A short press triggers a manual full-screen refresh, useful for clearing ghosting
- **Quick return from links**: Controls whether a short press of the power button acts as Back after following a link or footnote.

- **Slide Switch (BOOX Picco)**: Chooses what the side slide switch controls: **Frontlight**, **Night Mode**, or **Touch controls while reading**. For Frontlight and Night Mode, the switch position always wins over the matching on-screen toggle.
  - With **Touch controls while reading** (this fork), the switch is a **touch lock toggle**: each move of the switch, in either direction, turns the lock on or off, so the position (red or white) does not matter; the padlock shows the state. While locked, the whole touchscreen is ignored, including taps, swipes, edge gestures, long-press, and the header Back button. The side Up/Down buttons still turn pages. Touches while locked do not keep the reader awake.
  - The touch tile (pointer icon) in the light panel toggles the same lock: filled = touch on. Locking from the tile closes the panel at once.
  - The lock stays as it was through sleep and restart. Moving the switch while the reader sleeps is ignored. Moving it while touch is unlocked from Home (see below) ends that unlock.
  - A touch that starts while locked stays ignored until the finger lifts, even if you unlock meanwhile.

- **Light panel (this fork)**: while the frontlight is off, the brightness and warmth controls are greyed out and inactive; turn the light on with the lamp button first. A tap on **-** or **+** changes the level by 1%; hold the button to keep changing it (after about 0.4 s, then several steps per second).

- **Light panel under the touch lock (BOOX Picco, this fork)**: while the touch lock is on, press the **Up and Down side buttons together** to open the light panel (brightness and warmth). Touch works inside the panel while it is open; close it (tap below it, swipe up, or Up+Down again) and the lock is active again. The two presses never turn a page; if they were not quite simultaneous and the first one already turned a page, that turn is undone before the panel appears. Without the lock, swipe down from the top as usual.
  - In a book (EPUB, TXT, Markdown, with **Reader Menu Style** set to Toolbar), Up+Down also opens the reader toolbar at the bottom, the one a tap in the middle of the page opens without the lock. Touch works in both. Tap the Contents, Text or More icon (or a chapter arrow, the progress bar, or the history Back/Forward arrows) to close the light panel and use that tool; touch stays on until you are back on the page. Up+Down again, Back, or a tap on the page between the two closes both, and the lock is active again. With the list menu style, or in an XTC book, only the light panel opens.
  - When the reader wakes straight into a book with the touch lock on, the light panel slides down over the page first, marked "Touch locked · light check - tap below to read", so a light left on is easy to notice. Waking to Home with the lock on shows a short "Touch locked" note instead. Tap below it to close it; the lock is active again.

- **Touch lock indicator (this fork)**: while the touch lock is engaged, a small padlock appears in the middle of the top bar (Home, settings), and in the middle of the top page margin of a book (in the top-right corner when the margin is too small for the battery and clock). While the lock is overridden from Home (see below), the padlock in the top bar is shown open; in a book, the padlock is open while the toolbar opened with Up+Down has touch.

- **Unlock touch from Home (this fork)**: with the touch lock on, hold **Up** for about 0.4 s on Home to unlock touch for a while (open padlock in the top bar; note "Touch unlocked · hold Down to lock"). The override ends when you open a book, hold **Down** for about 0.2 s, move the switch, the reader sleeps or switches off, or after 2 minutes without use.

- **Dictionary under the touch lock (BOOX Picco, this fork)**: hold **Up** (the upper side button) for about 0.4 s on a book page to open dictionary word selection ("Dictionary: tap or select a word" at the top). Touch works during the lookup. Under the lock, a short Up press turns the page as usual when you release the button, so a hold does not turn a page. The hold replaces Up's long-press chapter skip. Pressing **Down** during word selection backs out to the book (handy after an accidental hold). Returning to the book (after a definition, or after **Close** when a word is not found) re-locks touch, and global gestures stay off during the lookup. Without the lock, long-press a word and choose Look Up.
  - Under the touch lock there is no menu: lifting your finger off a word (or a phrase you dragged across) looks it up at once, and the top band reads "Looking Up..." while your finger is down. Above the definition, **Another word** returns to word selection on the same page, and **+** returns with the word still selected and **Clip | Bookmark | <** below it (**<** goes back to picking a word). A tap anywhere else above the definition closes it.
  - When a word is not found, "Not Found" shows **Another word** (pick another word on the same page) and **Close** (back to the book). Under the lock Down also closes it. Without the lock: Confirm = Another word, Back = Close. A tap on the page outside the box closes it too.
  - Opened without the lock (Reader Menu **Dictionary**, or a Confirm hold or Home button set to Dictionary), the dictionary-mode selection menu shows **Look Up | Another word | +**. **Another word** clears the selection so you can tap a different word; **+** switches to **Clip | Bookmark | <** (and **<** back). A small finger slide keeps the touched word selected; drag clearly to extend the selection.
  - Every word you look up, and whose definition stays on screen for at least 1 second, is saved to `/Vocabulary.txt` (one JSON record per line: word, dictionary headword, definition, time, book title, author, chapter and page) for later review; Home Backup copies it to your server too. **Save Vocabulary Lookups** (below **Dictionary**) turns this off; it is on by default in this fork. Earlier builds wrote `/vocabulary/lookups.tsv`, which is no longer updated. A definition closed within 1 second is taken as a mistaken lookup: it is not saved and not counted in Reading Stats.

- **Refresh shortcut under the touch lock (BOOX Picco, this fork)**: while the touch lock is on (also with the light panel open), press **Power and Up together** for a full screen refresh (clears ghosting), like **Power and Down** takes a screenshot. Without the lock, use the refresh button in the light panel. As with Up+Down, the presses never turn a page, a page turn from a not-quite-simultaneous press is undone, and holding the pair does not put the reader to sleep.

#### 3.6.4 System

- **Time to Sleep** (**Settings → Display**): Set the duration of inactivity before the device automatically goes to sleep; options are 1, 3, 5, 10 (default), 15 or 30 minutes.

- **Wi-Fi Networks**: Connect to Wi-Fi networks for file transfers and firmware updates.

- **Bluetooth Page Turner** (**Settings → Connectivity**): Enable Bluetooth, then open **Scan for Remotes** to connect a page turner. **Paired Remotes** lists saved remotes; selecting a row marked **Forget** removes it. Once connected, open **Map Buttons**, choose a page, chapter, Confirm, Back, reader menu, or clipping action, and press the remote button within 15 seconds. Release the button within 15 seconds: a hold of at least 0.7 seconds is mapped separately from its tap. If the remote never reports a release, learning waits 15 seconds and saves the press as a tap. Hold Confirm on an action, or long-press its row, to clear it. Back cancels learning or returns to the Bluetooth menu. Mapping is optional: without custom mappings, the default remote controls remain available. Each button gesture maps to one action; assigning it again replaces its previous action. Confirm and Back act like the device buttons while reading and in the reader list menu. Next/Previous Page navigate items while a reader menu is open and turn pages when it is closed. Bluetooth can also be toggled from the reader list menu or the toolbar’s **More** menu.

- **CrossPoint Sync**: Settings for syncing book progress with CrossPoint Sync or a compatible KOSync server. **Smart sync** is the default for new configurations and auto-resolves simple push/pull decisions. Existing credential files retain **Ask every time** when migrated; you can switch Sync Behavior at any time if you prefer manual confirmation.

- **OPDS Servers**: Manage one or more OPDS [(Open Publication Distribution System)](https://en.wikipedia.org/wiki/Open_Publication_Distribution_System) libraries for browsing and downloading books. See [OPDS Servers (Multiple Libraries)](#365-opds-servers-multiple-libraries) below.

- **Clear Reading Cache** (**Settings → Reader**): Clear the internal SD card cache.

- **Use book metadata**: Read the title and author stored inside each book when the Library index is rebuilt. When disabled or unavailable, the Library uses the filename.

- **Refresh Library** (**Settings → Library**): Rescan the SD card for books while preserving the arrival history of books already in the index.

- **Check for updates**: Check for Crosspoint firmware updates over Wi-Fi. Firmware can also be updated without a USB connection by placing a `firmware.bin` file on the SD card. Neither option is available on the BOOX Picco, which is updated over USB.

- **Plugins** (**Settings → Connectivity**): Enable or disable SD-card plugins. Clean installs start disabled; upgrades with existing plugins are kept enabled automatically. Disabling plugins leaves all plugin files on the SD card but stops plugin discovery and execution until they are enabled again.
  - Enabling plugins immediately adds **Plugins** to the Home screen, even when no plugins are installed yet. Sideloaded SD-card plugins work without Plugin Hub.
  - Open **Home → Plugins** to view installed plugins. Hold **Confirm** on a selected installed plugin for one second (or long-press its row on a touch device) to open an uninstall confirmation. Confirming deletes that plugin's installed folder and files; cancelling leaves it installed. If no usable on-device Plugin Hub is available and its installer has not been hidden, a **Plugin Hub — Download, discover, and update plugins** entry is also shown. A manually sideloaded Hub with a valid `device.json` is recognized without requiring the optional README or browser files.
  - Selecting that Plugin Hub entry offers **Install**, **Hide**, or **Cancel**. **Install** downloads Plugin Hub over Wi-Fi. If you back out of Wi-Fi selection, installation is cancelled, Wi-Fi is turned back off, and you return to Plugins with the installer still available. **Hide** removes only the Plugin Hub install entry; it does not disable plugins or affect sideloaded plugins. **Cancel** leaves the entry unchanged.
  - **Hide** persists across reboots. Setting **Settings → Connectivity → Plugins** to **Enabled** again brings the installer back.
  - Plugins with web settings appear on the **Plugins** page of the web UI while **File Transfer** is running. Its link is hidden while plugins are disabled.
  - Plugin Hub is optional. Users who prefer manual installation can download it directly from [crosspoint-plugin-hub releases](https://github.com/crosspoint-reader/crosspoint-plugin-hub/releases) and copy/extract the `pluginhub` folder so it ends up at `/.crosspoint/plugins/pluginhub/`, `/plugins/pluginhub/`, or `/.plugins/pluginhub/`. Once installed, Plugin Hub appears as a normal entry under **Home → Plugins**.

- **Language**: Set the UI language. CrossPoint supports 32 languages: English, Spanish, French, German, Czech, Brazilian Portuguese, European Portuguese, Russian, Swedish, Romanian, Catalan, Ukrainian, Belarusian, Italian, Polish, Finnish, Danish, Dutch, Turkish, Kazakh, Hungarian, Lithuanian, Slovenian, Valencian, Hebrew, Arabic, Slovak, Bosnian, Vietnamese, Norwegian Bokmål, Indonesian, and Orangutan.

- **Manage Fonts**: Browse, download, and manage custom font families installed from the SD card. See [Custom Fonts (SD Card)](#38-custom-fonts-sd-card) for more information.

#### 3.6.5 OPDS Servers (Multiple Libraries)

CrossPoint supports saving multiple OPDS servers and switching between them when browsing catalogs. Browse them from **OPDS Browser** on the Home screen (plugin system off and a server saved), otherwise from **File Transfer -> OPDS Browser**.

1. Open **Settings -> Connectivity -> OPDS Servers**.

2. Select **Add Server** to create a new entry, or select an existing server to edit it.

3. Configure these fields:
   
   - **Server Name**: Optional display name (for example, "Home Calibre" or "Public Catalog").
   
   - **OPDS Server URL**: Full catalog root URL (for Calibre Content Server, usually ends with `/opds`).
   
   - **Username / Password**: Optional credentials for authenticated servers.

4. Use **Delete Server** inside a server entry to remove it.

Behavior notes:

- You can store up to 8 OPDS servers.
- OPDS authentication supports HTTP Basic auth. If you use Calibre Content Server with authentication enabled, set it to Basic (not Digest).

You can also manage OPDS servers from the web interface while in File Transfer mode:

1. Connect to the device web UI.
2. Open `http://<device-ip>/settings`.
3. Use the **OPDS Servers** card to add, edit, or delete entries.

For web-based Wi-Fi network management, see [Web Settings (Wi-Fi + OPDS)](#366-web-settings-wi-fi--opds).

#### 3.6.6 Web Settings (Wi-Fi + OPDS)

While in **File Transfer** mode, the web settings page includes management cards for both **Wi-Fi Networks** and **OPDS Servers**.

1. On device: open **File Transfer** and connect through **Join a Network** or **Create Hotspot**.
2. In a browser, open `http://<device-ip>/settings` or `http://crosspoint.local`.
3. In **Wi-Fi Networks**, add, edit, or delete saved network entries (SSID + optional password).
4. In **OPDS Servers**, add, edit, or delete OPDS catalogs.

Behavior notes:

- Passwords are never shown back in the web UI after saving.
- Leaving Password blank while editing keeps the existing saved password unchanged.
- The web UI can save hidden-network SSIDs, but connecting to hidden networks still depends on the device-side Wi-Fi connection flow.

#### 3.6.7 KOReader Sync Quick Setup

CrossPoint can sync reading progress with KOReader-compatible sync servers.
It also interoperates with KOReader apps/devices when they use the same server and credentials.

**Server Type** controls which progress and metadata format CrossPoint sends:

- **CrossPoint** - For the default service or a self-hosted CrossPoint-compatible sync server. Sends standard KOReader progress plus CrossPoint's richer reading position when available.
- **KoSync** - For standard KOReader Sync servers such as `sync.koreader.rocks` or `koreader/kosync`. Uses the strict standard payload without CrossPoint-specific rich position data.
- **Other** - For custom servers that explicitly support CrossPoint's enhanced payload. Rich position is enabled, and when **Send Metadata** is on CrossPoint may also send recognized ISBN, ASIN, series name, and series index in addition to title/author metadata.

**Send Metadata** remains the privacy switch for bibliographic data. Turn it off if you want to sync progress without sending title, author, ISBN/ASIN, or series information.

##### Option A: CrossPoint Sync Server (`sync.crosspointreader.com`, default)

When **Sync Server URL** is left empty, CrossPoint uses the free CrossPoint sync server at `https://sync.crosspointreader.com`. It speaks the standard KOReader sync protocol (so KOReader apps can use it too). CrossPoint records page starts as chapter-content offsets and sends the corresponding standard KOReader XPath, so devices with different fonts or layouts can return to the same text.

1. On each CrossPoint device:

   - Go to **Settings -> Connectivity -> CrossPoint Sync**.

   - Set **Username** and **Password** (enter the plain password; CrossPoint computes MD5 internally, and use the same values on all devices).

   - Leave **Sync Server URL** empty (or set it to `https://sync.crosspointreader.com`).

   - Set **Server Type** to **CrossPoint**.

   - On the first device, run **Sign Up** once to create the account directly from the device. On every other device, just run **Authenticate**.

Accounts are per server. Existing `sync.koreader.rocks` credentials do not exist on the CrossPoint server; either sign up again with the same username/password or use Option B to keep using the legacy server.

##### Option B: Legacy Public KOReader Server (`sync.koreader.rocks`)

Use this if you already sync KOReader devices against the official public server.

1. On each CrossPoint device:

   - Go to **Settings -> Connectivity -> CrossPoint Sync**.

   - Set **Sync Server URL** to `https://sync.koreader.rocks` (required; an empty URL now points at the CrossPoint server instead).

   - Set **Server Type** to **KoSync**.

   - Set **Username** and **Password** to your existing KOReader Sync credentials.

   - Run **Authenticate**.

2. If you do not have an account yet, run **Sign Up** on the device, or register once with curl:

```bash
USERNAME="user"
PASSWORD="pass"
PASSWORD_MD5="$(printf '%s' "$PASSWORD" | openssl md5 | awk '{print $2}')"

curl -i "https://sync.koreader.rocks/users/create" \
  -H "Accept: application/vnd.koreader.v1+json" \
  -H "Content-Type: application/json" \
  --data "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD_MD5\"}"
```

When this returns `HTTP 402` with `{"code":2002,"message":"Username is already registered."}`, pick a different username or use that existing account.

##### Option C: Self-Hosted Server (Docker Compose)

1. Start a sync server:

```bash
mkdir -p kosync-quickstart
cd kosync-quickstart

cat > compose.yaml <<'YAML'
services:
  kosync:
    image: koreader/kosync:latest
    ports:
      - "7200:7200"
      - "17200:17200"
    volumes:
      - ./data/redis:/var/lib/redis
    environment:
      - ENABLE_USER_REGISTRATION=true
    restart: unless-stopped
YAML

# Docker
docker compose up -d

# Podman (alternative)
podman compose up -d
```

> [!NOTE]
> `ENABLE_USER_REGISTRATION=true` is convenient for first setup. After creating your users, set it to `false` (or remove it) to avoid unexpected registrations.

2. Verify the server:

```bash
curl -H "Accept: application/vnd.koreader.v1+json" "http://<server-ip>:17200/healthcheck"
# Expected: {"state":"OK"}
```

3. Register a user once.
   CrossPoint authenticates against KOReader Sync (`koreader/kosync`) using an MD5 key, so register using the MD5 of your password:

> [!WARNING]
> Sending a reusable MD5-derived password over plain HTTP is insecure.
> Create unique sync-only credentials and do not reuse main account passwords.
> Prefer `https://<server-ip>:7200` whenever traffic leaves a fully trusted LAN or when using untrusted networks.
> Use `curl -k` only for self-signed certificate testing.

```bash
USERNAME="user"
PASSWORD="pass"
PASSWORD_MD5="$(printf '%s' "$PASSWORD" | openssl md5 | awk '{print $2}')"

curl -i "http://<server-ip>:17200/users/create" \
  -H "Accept: application/vnd.koreader.v1+json" \
  -H "Content-Type: application/json" \
  --data "{\"username\":\"$USERNAME\",\"password\":\"$PASSWORD_MD5\"}"
```

If this returns `HTTP 402` with `{"code":2002,"message":"Username is already registered."}`, the account already exists.

4. On each CrossPoint device:
   
   - Go to **Settings -> Connectivity -> CrossPoint Sync**.
   
   - Set **Username** and **Password** (enter the plain password; CrossPoint computes MD5 internally, and use the same values on all devices).
   
   - Set **Sync Server URL** to `http://<server-ip>:17200`.

   - Set **Server Type** to **KoSync**.
   
   - Run **Authenticate**.

If you use the HTTPS listener, use `https://<server-ip>:7200` (`curl -k` only for self-signed certificate testing).

##### Syncing While Reading

Once any of the options above is set up, press **Confirm** while reading to open the reader menu, then select **Sync Progress**. Alternatively, set **Settings -> Controls -> Front buttons -> Menu long press** to **CrossPoint Sync** and hold Confirm to launch sync directly.

- **Sync Clippings** is off by default. Enable it only for a compatible CrossPoint Sync server; manual **Sync Progress** then uploads and downloads this book’s clippings too. Leave it off for ordinary KOSync servers.
- With **Sync Behavior** set to **Ask every time**, choose **Apply Remote** to jump to remote progress or **Upload Local** to push current progress.
- With **Sync Behavior** set to **Smart sync**, CrossPoint auto-resolves simple cases: upload when no remote progress exists, confirm and leave both unchanged when local and remote progress are already synchronized, upload when local progress is further ahead, or apply remote when remote progress is further ahead.

#### 3.6.8 Home Backup (fork)

This fork can back the reader up to a WebDAV server on your home network, and fetch books you queue there. Open **Settings → Connectivity → Home Backup**, or choose **Home Backup** on the File Transfer screen.

- **Server URL**, **Username**, **Password**: the WebDAV server and its login (no defaults; enter your own). The password is stored obfuscated on the SD card (`/.crosspoint/backup.json`).
- **Automatic backup (daily)**: when the reader goes to sleep and the last good backup is more than 24 hours old, it joins the last-used Wi-Fi network and backs up first. On battery (at least 30%) it stops after 60 seconds; on USB power it may run up to 15 minutes. Whatever is left continues on the next sleep. Sleep is never blocked for longer.
- **Last backup**: local date and time of the last good backup ("Today 14:32", "Yesterday 09:10", "28 Oct 21:05"), plus the reason if the latest run failed.
- **Back up now**: runs a backup with a progress counter (files and MB).
- **Web view (browser)**: a note of the address where you browse the backup on your computer (for example a File Browser page). The reader does not use it.
- **Clock**: after a run that finishes or stops at its time limit, the reader sets its clock from the server's time. Keep the server's clock right.

What a run does:

1. **Fetches queued books**: every file in the server's `/to-reader/` folder is downloaded into `/books` (as `name (2).epub` if the name exists), then deleted from `/to-reader/`. Files whose size still changes during a 2-second check, such as a browser upload in progress, wait for the next run.
2. **Uploads changes**: only files that changed since the last run (tracked in `/.crosspoint/backup/manifest.tsv`) are sent. It never deletes anything on the server.

Included: everything on the card (books, fonts, dictionaries, screenshots, sleep images) plus reading progress, bookmarks, clippings, per-book text settings (`book-settings/`), reading statistics (`stats/`), `settings.json`, `state.json` and `recent.json`. Excluded: Wi-Fi, sync and OPDS logins and tokens, the backup login and certificate, and layout caches the reader rebuilds.

For an `https://` server the reader trusts only one pinned certificate: copy the server's certificate (PEM) to `/.crosspoint/backup-ca.pem` on the SD card, directly (USB drive mode or a card reader); File Transfer cannot change it. Without it the backup does not start, and Last backup shows "(no certificate)". See [docs/home-backup.md](docs/home-backup.md) for the server side.

### 3.7 Sleep Screen

The **Sleep Screen** setting controls what is displayed when the device goes to sleep. On the BOOX Picco (this fork) a strip along the bottom also shows the battery level at the moment it went to sleep ("Battery at sleep 72%"; it does not update while asleep), followed by the progress in the open book ("· Book 34%") when it went to sleep from a book. On USB power the first part reads "Charging" or "Charged" with the level instead.

Modes:

| Mode               | Behavior                                                                                                                     |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| **Dark** (default) | The CrossPoint logo on a dark background.                                                                                    |
| **Light**          | The CrossPoint logo on a white background.                                                                                   |
| **Custom**         | A custom image from the SD card (see below). Falls back to **Dark** if no custom image is found.                             |
| **Cover**          | The cover of the currently open book. Falls back to **Dark** if no book is open.                                             |
| **Cover + Custom** | The cover of the currently open book, shown only while actively reading. Falls back to **Custom** behavior when not reading. |
| **Transparent**    | A BMP or PNG overlay drawn over the current screen. Supports PNG and 32-bit BGRA alpha transparency, and treats white as transparent in regular BMPs. Falls back to **Dark** if no valid overlay image is found. |
| **Cover + Transparent** | The **Transparent** overlay drawn over the cover of the last opened book, from any screen. Falls back to **Transparent** behavior if there is no book cover, and to the plain cover if no valid overlay image is found. |
| **None**           | A blank screen.                                                                                                              |

#### Cover settings

When using **Custom**, **Cover**, **Cover + Custom** or **Cover + Transparent**, two additional settings apply:

- **Sleep Screen Cover Mode**: **Fit** (scale to fit, white borders) or **Crop** (scale and crop to fill the screen).
- **Sleep Screen Cover Filter**: **None** (grayscale), **Contrast** (black & white), or **Inverted** (inverted black & white).

#### Custom images

To use custom sleep images, set the sleep screen mode to **Custom** or **Cover + Custom**, then place images on the SD card:

- **Multiple Images (recommended):** Create a `.sleep` directory in the root of the SD card and place any number of `.bmp` images inside. One will be randomly selected each time the device sleeps. (A directory named `sleep` is also accepted as a fallback.)
- **Single Image:** Place a file named `sleep.bmp` in the root directory. This takes priority over the `.sleep`/`sleep` directories.

Like book covers, custom images are scaled to the screen using **Sleep Screen Cover Mode**. The first sleep with a new image takes longer while it is converted.

#### Transparent overlay images

To use transparent sleep overlays, set the sleep screen mode to **Transparent** or **Cover + Transparent**, then place BMP or PNG files on the SD card:

- **Multiple Images (recommended):** Create a `.sleep-overlay` directory in the root of the SD card and place any number of valid overlay `.bmp` or `.png` images inside. One will be randomly selected each time the device sleeps. A directory named `sleep-overlay` is also accepted as a fallback.
- **Single Image:** Place `sleep-overlay.bmp` or `sleep-overlay.png` in the root directory. A root BMP takes priority over a root PNG, and both take priority over the `.sleep-overlay`/`sleep-overlay` directories.

Transparent overlay files are intentionally separate from normal sleep images. Regular BMP formats supported by CrossPoint are accepted; white pixels leave the existing screen unchanged. For per-pixel alpha transparency, use a PNG with an alpha channel or a 32-bit BGRA BMP with both visible and non-opaque pixels. Opaque white pixels in alpha images erase the content behind them.

> [!TIP]
> For best results:
> - For non-transparent **Custom** mode, use uncompressed BMP files with 24-bit color depth.
> - For **Transparent** mode, use a PNG or uncompressed 32-bit BGRA BMP for per-pixel alpha, or a regular BMP for white-as-transparent artwork.
> - X4: Use a resolution of 480x800 pixels to match the device's screen resolution.
> - X3: Use a resolution of 528x792 pixels to match the device's screen resolution.

> [!TIP]
> You can set an image as the sleep screen cover directly from the BMP image viewer in the **[Browse Files](#33-browse-files-screen)** screen. On touch devices, long-press the image.

---

### 3.8 Custom Fonts (SD Card)

Noto Serif and OpenDyslexic are optional SD card fonts. Install `NotoSerif` to use
the former built-in serif family. On upgrade, an existing Noto Serif selection
uses `NotoSerif` if installed; otherwise, the reader switches to Noto Sans. After
installing the family later, select it in **Settings → Reader → Font Family**.

CrossPoint loads additional fonts from the SD card. Custom fonts can add Chinese, Japanese, Korean, and other scripts that the built-in reader fonts lack. If your device have external RAM, you can copy `.ttf`, `.otf`, and `.ttc` files directly. Otherwise, use `.cpfont` files made from those fonts. 

There are three ways to install fonts:

1. **Download from device (recommended):** Go to **Settings -> Reader -> Manage Fonts**, browse the available font families, and select one to download over Wi-Fi.
2. **Upload via web interface:** While in **File Transfer** mode, open the web UI and use the **Fonts** tab to upload `.cpfont` files. If your device has external RAM, the Fonts tab also accepts `.ttf` and `.otf` files. Choose the folder that contains the font family's files; the picker selects folders, not single files. The Fonts tab does not accept `.ttc` files; on devices with external RAM, copy those to the SD card instead (see item 3).
3. **Manual SD card copy:** Copy `.cpfont` families from the [crosspoint-fonts repository](https://github.com/crosspoint-reader/crosspoint-fonts) to `/.fonts/` or `/fonts/`. If your device have external RAM, you can also copy TTF/OTF/TTC files there without conversion.

Once installed, custom fonts appear in **Text Settings → Font** alongside the built-in fonts. **This fork:** Text Settings are saved per book, so to use a new font in a book you already opened, choose it in **Text Settings** from that book's Reader Menu.

If the reader shows **Damaged font file**, a `.cpfont` file on the SD card is broken (for example, from an interrupted copy). The affected characters are drawn blank. Delete the font family's folder and install it again.

See [docs/sd-card-fonts.md](./docs/sd-card-fonts.md) for full installation details and SD card folder structure.

### 3.9 Reading Stats (fork)

The reader records your reading in EPUB, TXT and Markdown books (not XTC) and shows it on the **Reading Stats** screen. Open it from Home (the bar chart icon in the row of icons at the bottom of Home in the Cover Grid theme, or **Reading Stats** in the Home list of the other themes) or from the Reader Menu. **Back** returns to where you opened it: Home or the book.

**What counts.** A session runs from opening a book to closing it, going Home, or sleep. Only *active* time counts: if a page stays open much longer than you usually need for that much text in this book, the extra time is a pause. Time with the Reader Menu (or a screen opened from it) or this screen over the book does not count; looking words up does: the time the definition is open counts (at most 10 minutes per definition) and is shown apart as **Dictionary Time**. When the reader goes to sleep by itself after a while without input, the last page (or the open definition) counts only as long as you usually need for a page: your speed in this book, else in all books, else 1 minute. Not if the Reader Menu was open: that page ended when the menu opened. The reader learns your speed for each book as you turn pages; until it knows it, it uses your speed in other books, or else counts at most 5 minutes per page. Sessions of a book less than 10 minutes apart (you turned the reader off and on again) are one sitting; sittings are stored for later views. With **Ignore Short Opens** on (default), sessions under a minute or without a page turn are left out. Days and charts need the clock: reading while the clock is not set counts only in **Total**.

**Overview.** Today, This Week (from Monday), This Month, This Year, Total, Current Streak (days in a row with reading, up to today or yesterday), Longest Streak, Books Finished This Year (a book counts when a session takes it past 99%, or to the end-of-book screen), **Books**, **Hidden Books**, **When I Read**, **Records**, **Year in Review**, **Reading Speed Trends**, and the **Ignore Short Opens** switch.

**Days, Weeks, Months, Years.** Bar charts of active time: the last 30 days, 26 weeks, 12 months, and up to 10 years. The line above the chart names the selected bar and its time; the line below it gives the total and average of the chart. Tap a bar to select it; tap the arrows, or swipe right (older) and left (newer), to page in time. With the buttons, Up/Down (or Left/Right) walk the bars and page back when you go past the oldest one; holding a button changes the tab. On **Days**, tapping a bar (or Confirm) opens the day's sessions: book, start and end time, active time and dictionary time; Confirm on a session opens the book's page (below), not the book. On **Weeks** and **Months**, a bar opens the days of that week or month that have reading; Confirm on a day opens its sessions.

**Books.** All books with reading time, last read first, with the day you last read each one, its reading style and its total time. Confirm opens the book's page. **Hidden Books** lists the books you hid, the same way.

**A book's page.** Also from the book's hold menu on Home or in the Library's Recent view, and from the Reader Menu (**Reading Stats for This Book**). Three tabs:
- **Summary**: total time, sessions, sittings, average session, first and last read, reading style, reading speed (paper pages of 1,800 characters per hour, once 5 minutes of reading are measured), time to finish (or the day you finished it), progress, lookups, lookups per hour of reading, dictionary time, and late nights (sessions with reading between 23:00 and 04:00, and that time). **Hide from Stats** is a switch here (Confirm), so a hidden book can be shown again. **What Hooked Me** opens the book's chapters (below). **Compare with Another Book** lists the other books that are not hidden; pick one to see both side by side: time, speed, sessions, sittings, style and days from the first session to the finish.
- **Progress**: where you were at the end of each reading day, from the first to the last.
- **Sessions**: the newest 100 sessions: day and time, position from and to, dictionary time, active time.

**Reading style** (from your sessions of the book with a known date; at least 2): **Stalled** when you have not read an unfinished book for over 30 days; **Binge** when you read on at least half the days from the first to the last and an hour or more per reading day on average; **Snacking** when sessions are under 10 minutes on average; **Steady** when you read on at least half the days; else **Intermittent**.

**What Hooked Me.** How you read one book, chapter by chapter, from its page changes. Two tabs:
- **Highlights**: **One More Chapter** (sessions in which you read on into the next chapter of the table of contents and turned at least 3 more pages in it, of all sessions; the cover and a chapter split into several files do not count), and the chapter with the most time, the fastest and slowest chapter (once 5 minutes are measured in it), the most session starts, the most stops (sessions that ended there), the last stop (where your last session ended), the most re-read text (pages you turned forward through again), the most turns back (previous page or a jump back), the most pauses (times a menu or screen was opened over the page; not the long-page pauses under **What counts**) and the most lookups. Confirm shows that chapter in the list.
- **Chapters**: each chapter you read, in book order: its time, and speed, starts, stops, turns back and pauses (menu or screen opened over the page).
Chapter names come from the book's table of contents while the book is at the same place on the card and its reading cache is there (**Clear Reading Cache** removes it until you open the book again); otherwise the list shows **Chapter N** (the book's file part). Files of one table-of-contents entry are shown as one chapter. The current session counts too. A chapter's time is exact for reading recorded with this version; for older reading it is an estimate, and each session's time is shared over the chapters it touched in proportion.

**Records.** Over your reading in books not hidden. Longest session, most lookups in a session and fastest book include reading while the clock was not set (shown without a date); the others need the date. The records: longest session, longest sitting, most in one day, most in one week, fastest book (pages per hour, at least 30 minutes measured), longest streak, most lookups in a session, latest night (the latest time a session ended, counting the night until 04:00; a session that goes on past 04:00 counts as 03:59) and earliest morning (the earliest time a session started, from 04:00), then lookups per hour and late nights over all reading. Confirm on a record with a book opens the book's page.

**Year in Review.** One tab per year with reading (newest first, up to 6; only this year, empty, before any reading): total time, sessions, average session, books finished, the top 3 books by time (Confirm opens the book's page), busiest month, busiest weekday, favourite hour, longest streak in the year, lookups (and per hour), late nights, and **Reading Styles (Today)**: the books you read that year, each by its reading style today.

**Reading Speed Trends.** Your reading speed in pages per hour, with the reading time it is measured from: by month (the last 12), by time of day (Morning 05:00-12:00, Afternoon 12:00-17:00, Evening 17:00-22:00, Night 22:00-05:00, by the session's start), and by font size.

**When I Read.** A grid of weekdays (rows) and hours (columns), darker where you read more, over the last 30 days, 12 months or all time (the tabs). Up/Down walk the weekdays; the line above the grid gives the selected day's time and its busiest hour (on the tabs: all days).

**Hide a book.** Hold the book on Home and choose **Hide from Stats** (for example a test book), or use the switch on the book's page. Its sessions stay recorded but are left out of the totals and charts until you choose **Show in Stats** (or find it under **Hidden Books** and switch it off).

**In a browser.** While **File Transfer** (or Calibre Wireless) is on, open the address the reader shows and choose **Reading Stats** at the top of the page (or go to `/stats`). The page shows the same statistics with larger charts: the Overview, Days/Weeks/Months/Years bars (60 days, 52 weeks, 24 months, all years; **Older** and **Newer** page in time), When I Read, the Books list (hidden books are listed and marked; filter by title or author, sort by a column) with each book's Summary, Progress, all its Sessions and Chapters (What Hooked Me), Records, Year in Review and Speed. The **Export** tab downloads two CSV files for a spreadsheet: `reading-sessions.csv` (one row per ended session) and `reading-books.csv` (one row per book); both include hidden books, marked in the `hidden` column, and open in spreadsheets with accented titles intact. The page works without internet access. Like the files, the statistics can be read by anyone on the same network while File Transfer is on, also by a web page open in a browser on that network; they are not sent anywhere else. Tap a bar or a cell of a chart to see its value.

The data is in `/.crosspoint/stats/` on the SD card and stays on the reader (Home Backup copies it to your own server). **Clear Reading Cache** does not delete it; delete the folder to start over. Formats and rules: [docs/reading-stats.md](docs/reading-stats.md).

---

## 4. Reading Mode

Once you have opened a book, the button layout changes to facilitate reading.

### Page Turning

| Action            | Buttons                              |
| ----------------- | ------------------------------------ |
| **Previous Page** | Press **Left** _or_ **Side Up**    |
| **Next Page**     | Press **Right** _or_ **Side Down** |

The role of the side buttons can be swapped in the **[Controls Settings](#363-controls)**.

If the **Short click** setting is set to "Page Turn", you can also turn to the next page by briefly pressing the Power button.

**Fast presses (EPUB, TXT, Markdown, this fork):** every press counts. Presses made while a page is still being drawn wait in a queue, and each page is shown in turn, so the screen may lag behind. Up to 20 turns can wait. A press in the other direction cancels one waiting turn.

### Chapter Navigation

* **Next Chapter:** Press and **hold** the **Right** (or **Side Down**) button briefly, then release.
* **Previous Chapter:** Press and **hold** the **Left** (or **Side Up**) button briefly, then release.

This feature can be disabled in the **[Controls Settings](#363-controls)** to help avoid changing chapters by mistake.

### Auto Page Turn

Auto Page Turn automatically advances pages at a set interval, useful for hands-free reading. This feature can be enabled and configured from the **[Reader Menu](#5-reader-menu)** while reading an EPUB.

### Tilt Page Turn (X3 only)

On the **Xteink X3**, the gyroscope can be used to turn pages by tilting the device. This feature is available in the Controls settings.

### Links and footnotes

Internal EPUB links include chapter links, cross-references, and footnotes. Tap a link on a touchscreen device, or choose "Links and Footnotes" from the Reader Menu to select a link. Press Back to return to the previous location.

If the device sleeps or you close the book after following a link, the book reopens on the page you were viewing. Back still returns you to where you followed the link, as long as no other jump came after it (this fork). Each link followed is also an entry of the [jump history](#5-reader-menu), so **Back** and **Forward** in the Reader Menu work for links too.

### Dictionary Lookup

Words on the current page can be looked up in an offline StarDict dictionary stored on the SD card. Copy a dictionary to the `/dictionaries/` folder, select it in **Settings → Reader → Dictionary**, then start a lookup by choosing **Look Up** in the **[Reader Menu](#5-reader-menu)** (or by holding **Confirm**, if the **Menu long press** setting in **[Controls Settings](#363-controls)** is set to "Dictionary"). Use **Left/Right** to highlight a word and press **Confirm** to show its definition.

See [docs/dictionary.md](docs/dictionary.md) for supported formats, setup, and where to find dictionaries.

### System Navigation

* **Return to Home:** Press the **Back** button to close the book and return to the **[Home](#31-home-screen)** screen.
* **Return to Browse Files:** Press and hold the **Back** button to close the book and return to the **[Browse Files](#33-browse-files-screen)** screen.
* **Reader Menu:** Press **Confirm** to open the **[Reader Menu](#5-reader-menu)**, which includes chapter navigation, reading options, and more.
* **Long-press Confirm (configurable):** Holding **Confirm** runs the function chosen by the **Menu long press** setting in **[Controls Settings](#363-controls)** — including bookmarks, KOReader Sync, dictionary lookup, or clipping selection. A short press always opens the Reader Menu.

### Supported Languages

CrossPoint renders text using the following Unicode character blocks, enabling support for a wide range of languages:

* **Latin Script (Basic, Supplement, Extended-A/B):** Covers English, German, French, Spanish, Portuguese, Italian, Dutch, Swedish, Norwegian, Danish, Finnish, Polish, Czech, Hungarian, Romanian, Slovak, Slovenian, Turkish, Catalan, and others.
* **Cyrillic Script (Standard and Extended):** Covers Russian, Ukrainian, Belarusian, Bulgarian, Serbian, Macedonian, Kazakh, Kyrgyz, Mongolian, and others.
* **Vietnamese:** Supported via extended Latin glyph coverage in the built-in reader fonts.

The UI includes Arabic and Hebrew (menus use built-in fonts with presentation-form coverage). Built-in **reader** fonts do not cover Chinese, Japanese, Korean, Arabic, Greek, Hebrew, or Farsi for book text. **CJK, Hebrew, Arabic, Greek, and other extended scripts can be enabled for reading by installing custom SD card fonts** — see [Custom Fonts (SD Card)](#38-custom-fonts-sd-card).

---

## 5. Reader Menu

Press **Confirm** while reading to open the Reader Menu. From here you can access reading utilities and navigation options without leaving the book.

The list menu remembers the selected entry while the book stays open. If that entry is no longer available, the cursor returns to the first entry. Closing the book resets the selection.

Available options include:

- **Select Chapter** – Open the table of contents to jump to a specific chapter (see [Chapter Selection](#51-chapter-selection) below).
- **Back** / **Forward** (this fork, EPUB) – Go back to where you were before a jump, like Back in a web browser; **Forward** goes to where you pressed **Back**. In the list menu, the value on the right is the book percent of that position. A jump is a chapter from the contents, a link or footnote, **Go to %**, a bookmark or clipping, a chapter skip (long press, Bluetooth remote, or the toolbar's chapter arrows and progress bar) and a position downloaded by **Sync Progress**; page turns are not jumps. A new jump after **Back** removes the **Forward** entries. Several chapter skips in a row (without a page turn between them) count as one jump. Each book keeps its last 50 entries, also after sleep or closing the book; **Delete Book Cache** clears them. Positions are found again by text position after a font, size or orientation change. The entries show only when there is somewhere to go, and not on the end-of-book screen. With **Reader Menu Style** set to Toolbar they are buttons on the toolbar itself: a left arrow (**Back**) before the Contents icon and a right arrow (**Forward**) after the More icon, each shown only when it can act (their places stay free otherwise, so the other icons do not move). One tap goes there and closes the toolbar; with the buttons, Left/Right move to them and Confirm presses them. They work the same when the toolbar was opened with Up+Down under the touch lock. The toolbar's chapter arrows next to the progress bar only skip chapters.
- **Links and Footnotes** – Select an internal link on the current page. This option appears when the page contains links.
- **Text Settings** – Change the font and layout of this book; the values are kept for this book only (see [Per-book text settings](#362-reader)).
- **Look Up** – Select a word on the current page and show its dictionary definition (see [docs/dictionary.md](docs/dictionary.md)). Requires a dictionary to be selected in **Settings → Reader → Dictionary**.
- **Bluetooth Page Turner** – Toggle Bluetooth for the saved remote. This entry appears after pairing a remote in **Settings → Connectivity → Bluetooth Page Turner** and disappears if that selected remote is forgotten.
- **Create Clipping** – Select a range of text on the current page and save it as a highlighted clipping.
- **View Clippings** – Open this book's saved clippings *(shown after the first clipping is saved)*.
- **Reading Orientation** – Cycle through screen orientations without leaving the reader.
- **Auto Turn (Pages Per Minute)** – Cycle through automatic page turn speed options for hands-free reading.
- **Go to %** – Jump to a specific position in the book by percentage.
- **Take screenshot** – Save a screenshot of the current page to the `screenshots/` folder.
- **Show page as QR** – Display a QR code encoding the current reading position.
- **Reading Stats** (this fork) – Open the [Reading Stats](#39-reading-stats-fork) screen; Back returns to the book.
- **Reading Stats for This Book** (this fork) – Open this book's page of the [Reading Stats](#39-reading-stats-fork); Back returns to the book.
- **Go Home** – Close the book and return to the Home screen.
- **Sync Progress** – Push or pull reading progress with a KOReader sync server (see [KOReader Sync Quick Setup](#367-koreader-sync-quick-setup)). This entry appears when sync credentials are saved or an enabled plugin subscribes to device events. Without sync credentials, it connects to Wi-Fi to send queued plugin events.
- **Delete Book Cache** – Clear the cached layout data for the current book, forcing a re-index on next open.

Press **Back** at any time to close the menu and return to your current page.

### 5.1 Chapter Selection

Accessible by selecting **Chapters** from the Reader Menu.

1. Use **Left** (or **Side Up**), or **Right** (or **Side Down**) to highlight the desired chapter.
2. Press **Confirm** to jump to that chapter.
3. *Alternatively, press **Back** to cancel and return to your current page.*

---

### 5.2 Bookmarks

Bookmarks can be created to quickly save and restore your place in a book.

To create a bookmark, hold **Confirm** for about half a second while inside a book. A popup will appear letting you know a bookmark was created. The popup message will automatically disappear in a couple of seconds.

To open bookmarks, press **Confirm** while inside a book. Then navigate to the **Bookmarks** menu. Bookmarks can be opened by navigating to them and pressing **Confirm**, which will redirect you to that place in the book. You can delete bookmarks by holding **Confirm** for about 0.7 seconds, and then pressing **Confirm** again to confirm deletion, or **Back** to cancel.

Bookmarks are stored in the `.crosspoint/bookmarks` folder in the JSON format.

### 5.3 Clippings and Highlights

Select **Create Clipping** from the Reader Menu, move to the first word, and press **Confirm**. Move to the last word and press **Select** again to choose **Look Up**, **Clip**, or **Bookmark**. **Back** closes that popup without losing the selection. On touch devices, press and hold a word in the reader to select it. Drag either selection handle to adjust the range, then tap **Look Up**, **Clip**, or **Bookmark** above it. Hold the ending handle at the bottom edge or over the final word for one second to extend onto the next preloaded page. Selection covers up to three pages in the same chapter, subject to the 240 selectable-word limit. The toolbar overlays the page without shifting the text. Releasing a handle keeps the selection open; tapping outside the text dismisses it. **Look Up** searches the selected text in your dictionary; **Bookmark** toggles the bookmark on the selection’s final page. Saved ranges with source offsets remain highlighted after changes to font, spacing, orientation, or pagination. Ranges without source offsets can only be highlighted when their saved layout still matches. Press and hold highlighted text to remove that clipping; its deletion is sent on the next manual sync when **Sync Clippings** is enabled.

Select **View Clippings** to browse saved excerpts for the current book. Press **Confirm** to choose **Open** or **Go to Passage**, or hold **Confirm** for about 0.7 seconds to delete it. **Go to Passage** uses the saved text offset and is available for new, source-anchored clippings. Older clippings can still be read or deleted. Touch devices can open a clipping with a tap or delete it with a long press.

Each book can store up to **1,024 clippings on ESP32-S3 devices** or **256 on ESP32-C3 devices**. These limits also apply to clippings downloaded through sync.

The structured clipping store under `.crosspoint/clippings` keeps up to **4 KiB of UTF-8 text per clipping**. Longer selections must be shortened before saving. CrossPoint also appends up to **2,000 UTF-8 bytes** of each saved excerpt to `My Clippings.txt` in the root of the SD card, using Kindle-compatible formatting. If that export fails, the clipping remains saved and the message reports the export failure.

Local book and folder moves carry clipping data with them. Deleting a local book removes its local clipping data; it does not delete already synced server copies or historical entries in `My Clippings.txt`. Deleting an individual clipping likewise leaves the text export untouched, but queues its server deletion for the next enabled manual sync. `My Clippings.txt` is an independent append-only export, not the source used to restore or sync highlights.

## 6. Current Limitations & Roadmap

Please note that this firmware is currently in active development. The following features are **not yet supported** but are planned for future updates:

* **Cover Images:** Large cover images embedded into EPUB require several seconds (~10s for ~2000 pixel tall image) to convert for sleep screen and home screen thumbnail. Consider optimizing the EPUB with e.g. https://github.com/bigbag/epub-to-xtc-converter to speed this up.
* **Image Support:** Most JPG, PNG and non-interlaced GIF images in EPUBs render correctly, including book covers. Animated GIFs show only the first frame. Interlaced GIFs are not supported and use an outlined image placeholder.

---

## 7. Troubleshooting Issues & Escaping Bootloop

If an issue or crash is encountered while using Crosspoint, feel free to raise an issue ticket and attach the logs.

**Crash reports on SD card:** After a crash, CrossPoint automatically saves a crash report to the SD card (no USB connection needed). Check the root of the SD card for a crash log file and include it with any bug report.

**Serial monitor logs:** For more detailed debugging, connect the device to a computer and run the custom debugging monitor script (requires Python 3 with `pyserial`, `colorama`, and `matplotlib`; install via `pip3 install pyserial colorama matplotlib`):

```
python3 scripts/debugging_monitor.py
```

The script auto-detects the serial port. You can also specify one explicitly:

```
python3 scripts/debugging_monitor.py /dev/ttyACM0        # Linux
python3 scripts/debugging_monitor.py /dev/tty.usbmodem1  # macOS
python3 scripts/debugging_monitor.py COM7                # Windows
```

**Features:**

- Color-coded log output by category (errors, memory, display, EPUB parsing, etc.)
- Live memory usage graph (free RAM, total RAM, max contiguous allocation) updated every second
- Interactive command prompt — type a command and press Enter to send it to the device
- Screenshot capture — saves the current display to `screenshot.bmp` when triggered by the device

**Options:**

| Option               | Description                                               |
| -------------------- | --------------------------------------------------------- |
| `--baud RATE`        | Baud rate (default: 115200)                               |
| `--filter KEYWORD`   | Show only lines containing the keyword (case-insensitive) |
| `--suppress KEYWORD` | Hide lines containing the keyword (case-insensitive)      |

**Examples:**

```
# Show only memory-related log lines
python3 scripts/debugging_monitor.py --filter MEM

# Hide noisy SD card log lines
python3 scripts/debugging_monitor.py --suppress "[SD]"
```

Press **Ctrl-C** or close the graph window to exit.

If the device is stuck in a bootloop, press and release the Reset button. Then, press and hold on to the configured Back button and the Power Button to boot to the Home Screen.

There can be issues with broken cache or config. In this case, delete the `.crosspoint` directory on your SD card (or consider deleting only `settings.json`, `state.json`, or `epub_*` cache directories in the `.crosspoint/` folder).
