# Reading statistics (fork)

The reader records when and how you read EPUB books (TXT and Markdown too, as they use the same
reader; XTC books are not recorded) and shows it on the **Reading Stats** screen. This page is the
contract for the data: file formats, the rules that turn page turns into "active reading time",
and what each screen reads. Stage 1 (this version) has the recorder, the Overview and the time
charts. Later stages add per-book views, reading patterns and a web page; they read the same files.

Code: `src/stats/` (recorder, rules, file formats, queries), `src/activities/stats/` (screens).
Host tests: `test/reading_stats/`.

**Not the upstream `reader.session` plugin event.** Upstream CrossPoint emits a `reader.session`
event (`ReaderSession`) with one book's start, end, duration and progress for sync plugins; it
keeps nothing on the device. These statistics are separate: they keep every session and page turn
on the SD card for the screens here. Both run side by side.

## Privacy

Everything stays on the SD card under `/.crosspoint/stats/`. Nothing is sent anywhere, except
that [Home Backup](home-backup.md) copies the folder to your own server like the rest of the
card. **Clear Reading Cache** does not delete it. To delete all statistics, delete the folder.

## Terms

- **Session**: one stretch of reading in one book, from opening it to closing it. A session ends
  when you close the book, go Home, open another book, the reader sleeps (Power button or
  automatic sleep), or restarts itself (heap clean-up after Wi-Fi, Bluetooth). The menu, the
  dictionary and other screens opened from the book do not end it.
- **Sitting**: sessions of the same book less than `SITTING_GAP_SEC` (10 minutes) apart, end to
  start. Turning the reader off and on again quickly is the same sitting. Sittings are not stored
  per session: the totals count them (`sittings` in `booktotals.bin` and `days.bin`), for the
  later per-book and pattern views.
- **Active time**: the part of a session's time you were reading (see below). All totals and
  charts use active time; the session's wall-clock length is stored too.
- **Short open**: a session with less than `SHORT_OPEN_SEC` (60 s) active time or no page turn.
  With **Ignore Short Opens** on (default), short opens are kept in the files but left out of every
  total and chart.
- **Hidden book** ("Hide from Stats" in Home's book menu): the book's sessions stay recorded and
  its own totals are kept, but it is left out of the calendar (days, charts, streaks, Overview
  totals). Show in Stats brings it back. Meant for test books.
- **Paper page**: 1,800 characters of the book's text (see `PaperPages`). 0 means "not counted
  yet" (the book's text count is not finished, or the status bar position is off). Paper pages
  are extra information only; reading speed comes from `charsRead` (below).
- **Finished**: a session that crosses `FINISHED_BP` (99%: starts below it and shows a page at or
  above it) or reaches the end-of-book screen gets `SESSION_FINISHED`; turning back afterwards
  keeps it. The book's finish time is the first such session's end, also when that session is a
  short open.

The one rule for what counts is in `StatsLogic`: `countsInTotals` (short-open rule) and
`countsInCalendar` (also a known wall time and a book that is not hidden).

## Active time

Each page shown on the screen starts a **dwell** that ends when the next page is shown, or when
the session ends. A dwell counts as active up to a **cap**; anything longer is counted as a pause
(you put the reader down, or fell asleep).

The cap follows how fast you read *this* book, measured per amount of text, so a dense page gets
more time than a page with one line:

```
cap = CAP_FACTOR x rate x pageChars / 1000, kept within [MIN_CAP_SEC, MAX_CAP_SEC]
```

| Constant | Value | Meaning |
|---|---|---|
| `CAP_FACTOR` | 3.5 | how many times slower than your usual speed still counts as reading |
| `MIN_CAP_SEC` | 60 s | a page always gets at least a minute (images, short pages) |
| `MAX_CAP_SEC` | 30 min | no page counts longer |
| `DEFAULT_CAP_SEC` | 5 min | the cap before any speed is known |
| `MIN_MODEL_SAMPLES` | 20 | samples a speed needs before it is used |
| `MIN_LEARN_DWELL_MS` / `MAX_LEARN_DWELL_MS` | 2 s / 30 min | dwells used for learning |
| `MIN_LEARN_CHARS` | 200 | pages with less text are not learned from |
| `DEFAULT_PAGE_CHARS` | 1000 | page size assumed before the book has a measured one |

- **rate** is the median of the book's learned speeds, in seconds per 1000 characters. Until the
  book has `MIN_MODEL_SAMPLES` samples, the median of all books is used; until that has enough,
  the cap is a flat `DEFAULT_CAP_SEC`. The rate in use is updated during the session once the book
  has enough samples.
- **Exact text length**: only a turn to the next page in the same chapter gives it (difference of
  the two pages' text offsets). Those pages add to the session's `charsRead`, and their active
  time to `learnedMs`; reading speed and time-to-finish use `charsRead / learnedMs`, so they work
  whatever the status bar shows. Those dwells (within the learning limits) are also the speed
  samples: a histogram of 40 buckets, 4 per doubling, starting at 1 s per 1000 characters
  (`addRateSample`, `BUCKETS_PER_OCTAVE`). A bucket that would pass 65535 halves the whole
  histogram, so old reading slowly fades.
- **pageChars** is the page's text length when known, else the book's typical page length (a
  running average of measured pages, kept per book).
- **Pauses**: while the reader menu (list or toolbar and its panels), a screen opened from the
  menu, or the Reading Stats screen is over the book, the clock stops; the dwell goes on when the
  page is back. A dictionary lookup counts as reading. Pauses are in the raw log (`E` records).
- Re-drawing the same page (clock update, menu closed, toolbar) does not end the dwell. A change
  of font or orientation shows a new page at almost the same place: it is recorded as a jump.
- The last dwell ends at the session end. After automatic sleep it includes the idle time before
  sleep, which the cap removes.

## Wall time and time zones

The reader keeps UTC in its clock and shows local time through the time zone set in Settings. A
session's start is stored as UTC plus the local offset at that moment (`tzOffsetMin`, minutes
east of UTC). Calendar buckets use that stored local time, so a day stays the day it was for you,
also after a time-zone change or daylight-saving switch.

The wall time is used only when the clock is trusted (`trustedtime::trustedNow()` is not 0: RTC,
time sync, or a restored floor). If it is not, the session is stored with `startUtc = 0`: its
active time counts in Total and in the book's totals, but not in days, weeks, streaks or
time-of-day views. If the clock becomes trusted during the session (Wi-Fi sync), the start is
set from that time minus the time already read, at the next write and at the end; the raw log
keeps the end time (`endUtc` in the `X` record), so a rebuild finds the same start.

A session belongs to the local day it started on. Weeks start on Monday (week 0 is the week of
1970-01-01). Months and years are calendar months and years.

## Files

All files are under `/.crosspoint/stats/`, little-endian, fixed-size records with no padding (the
structs in `src/stats/StatsFormat.h`). Each file starts with a 16-byte header:

| Field | Size | Meaning |
|---|---|---|
| magic | 4 | file type, below |
| version | 2 | format version |
| headerSize | 2 | bytes before the first record |
| recordSize | 2 | bytes per record (0 for the raw log, whose records vary) |
| flags | 1 | `booktotals.bin`, `days.bin`: 1 = built with Ignore Short Opens |
| reserved | 5 | 0 |

A reader that finds another version or size does not use the file (see Rebuilds).

| File | Magic | Kind |
|---|---|---|
| `books/<document id>.log` | `CPRL` | primary: raw log, append-only |
| `books.bin` | `CPBR` | primary: book registry |
| `sessions.bin` | `CPSI` | derived from the raw logs: session index |
| `booktotals.bin` | `CPBT` | derived from `sessions.bin`: per-book totals |
| `days.bin` | `CPDY` | derived from `sessions.bin`: per-day totals |
| `current.bin` | `CPCS` | the running session, for recovery after a reset |

### Raw logs: `books/<document id>.log` (version 1)

One file per book, append-only, the source of truth. The file name is the book's KOReader
document id (32 hex characters, the same id as `/.crosspoint/book-settings/`): it is computed
from the book's content, so renaming or moving the book keeps its statistics.

After the header come records. Each starts with its **type** (1 byte) and its **size** in bytes
(1 byte, the whole record). **The format grows only by new record types and by fields appended
at the end of a type**; a field is never moved or changed in meaning, and a new field must mean
"not known" when 0. A reader:

- skips types it does not know;
- accepts a record of a known type when it is at least that type's version 1 size (32, 20, 8, 48
  bytes for `S`, `T`, `E`, `X`; `rawMinSize`, fixed forever), reads the fields it has and treats
  missing later fields as 0, and ignores bytes past the fields it knows;
- treats a shorter record as damaged and skips it.

A record cut short (power loss) ends the readable log. Before a session's first log write,
`current.bin` is written, so every cut has a recovery record; recovery cuts the log after its last
complete record (an incomplete header empties the file) before appending.

Text offsets are the page's visible-text offset in its spine item; `0xFFFFFFFF` = unknown.

**`S` session start (32 bytes)**: type, size, flags (`SESSION_*`), font size (pt), spine item,
tzOffsetMin, startUtc (0 = unknown), text offset, percent (basis points, 0-10000), paper page,
paper pages in the book, orientation, reserved, session number, ms from session start to the
first page.

**`T` page change (20 bytes)**: type, size, kind (1 next, 2 previous, 3 jump forward, 4 jump
back), reserved, spine item, percent, ms since session start, text offset, paper page, reserved.
Every page change is recorded: page turns (queued ones too), chapter skips, links, footnotes, the
table of contents, Go to %, bookmarks and sync jumps.

**`E` event (8 bytes)**: type, size, kind (1 dictionary lookup, 2 clipping saved, 3 bookmark
added, 4 end-of-book screen shown, 5 pause, 6 resume), reserved, ms since session start.

**`X` session end (48 bytes)**: type, size, reason (1 closed/Home/other book/restart, 2 sleep, 3
automatic sleep, 4 reset), flags, length in ms, active seconds, endUtc (0 = unknown),
`charsRead`, `learnedMs`, page changes forward, back, jumps, lookups, clippings, bookmarks, end
paper page, end percent, end spine item, tzOffsetMin at the end, reserved.

### Session index: `sessions.bin` (version 1, 56-byte records)

One record per finished session, in the order they finished (by book after a rebuild). Built from
the raw logs; the screens read this instead of the logs.

| Field | Meaning |
|---|---|
| startUtc, tzOffsetMin | start; 0 = unknown wall time |
| durationSec, activeSec | wall length, active time |
| bookIndex | record number in `books.bin` |
| turnsFwd, turnsBack, jumps | page changes by kind |
| lookups, clippings, bookmarks | events |
| startPercentBp, endPercentBp | position, basis points (10000 = end-of-book screen) |
| startPaperPage, endPaperPage | position, paper pages (0 = unknown) |
| startSpine, endSpine | spine items |
| flags | 1 wall time known, 2 short open, 4 closed by recovery, 8 light on at start, 16 finished the book |
| endReason | as in `X` |
| fontSize, orientation | at start |
| sessionSeq | session number, unique, also in the raw log |
| charsRead, learnedMs | exact text read and its active time (speed) |

### Book registry: `books.bin` (version 1, 512-byte records)

A 128-byte header (the file header, next session number, and the speed histogram of all books)
and one record per book, never removed. Primary data only: document id, flags (1 = hidden from
stats), learned speed histogram and sample count, typical page length, and the latest title,
author and path (UTF-8, cut to 120/56/176 bytes; updated at each open). The record number is the
book's index in every other file.

Books are found through a RAM list built from `books.bin` once per boot (17 bytes per book:
part of the id, the record number, the hidden flag), so a book open does not scan the registry.
It is built again when the registry's book count differs from the list (a deleted or replaced
stats folder).

### Book totals: `booktotals.bin` (version 1, 40-byte records by book index)

First and last session start, last session end (for sittings), active seconds, sessions,
sittings, the time the book was finished (1 = at an unknown time), highest and last position,
`charsRead`, `learnedMs`. The short-open rule applies, the hidden flag does not (a hidden book
keeps its own totals). Books without a counted session may have no record yet.

### Day totals: `days.bin` (version 1, 16-byte records)

A 32-byte header (file header; active seconds of counted sessions without wall time) and one
record per local day with counted reading, in day order: local day number (days since
1970-01-01), active seconds, sessions, sittings, paper pages moved forward. Hidden books and
sessions without wall time are not in it.

### Session in progress: `current.bin` (version 1)

The running session's totals so far (header, document id, a session record). Written in place
(same size every time) before the session's records first reach the log (the first flush, or
the session end), then at each flush that has news. Deleted when the session ends. If it is found later (the
reader reset):

- if `sessions.bin` already ends with that session, the reset came while the session was being
  stored: the file is deleted and the summaries are rebuilt;
- it also stays when writing the session to `sessions.bin` failed, so the next start tries again;
- else the book's log is cut after its last complete record; if the log already has the
  session's `X`, that is used, else an `X` with reason 4 is appended from the snapshot; then the
  session is stored as below.

## Writing

- Page changes are kept in RAM (a 2 KB buffer, room for 100 page changes) and written by the
  reader's main loop in idle frames: every 3 minutes (`FLUSH_INTERVAL_MS`) when something changed,
  when the buffer is half full, and at the session end. Nothing is written on the page-turn path;
  the screen update only adds a record to the buffer. A reset loses at most the last 3 minutes.
- Sleep ends the session early in the sleep sequence. A restart by the reader itself (heap
  clean-up after Wi-Fi, Bluetooth, USB storage handoff) ends it first too.
- At the session end, in this order: the `X` record to the raw log, the record to
  `sessions.bin`, the speed histograms to `books.bin`, then the book's totals and the day. The
  summaries are only updated in place when they exist and were built with the current Ignore
  Short Opens value; otherwise (or when a late session falls between two existing days) they are
  deleted and rebuilt the next time a book or the stats screen opens, never on the sleep path.

## Rebuilds

- `booktotals.bin` and `days.bin` are rebuilt from `sessions.bin` when one is missing, has
  another version, or was built with another Ignore Short Opens value, and right away when a book
  is hidden or shown or the setting is changed on the stats screen. This reads `sessions.bin`
  once and keeps 41 bytes per book and 16 per reading day in RAM.
- `sessions.bin` is rebuilt from the raw logs when it is missing or has another version (a
  version bump of the index is handled this way). Sessions without an `X` record are closed at
  their last record, with the active time re-estimated from the book's current speed model.
- The raw logs and the registry are never rebuilt.

Checks happen when a book opens and when the Reading Stats screen opens
(`ReadingStats::prepareSummaries()`).

## Home Backup

The registry, the summaries and `current.bin` are rewritten in place, so their size often stays
the same. Home Backup compares files up to 64 KB by content; larger ones by size and
modification time. The reader sets the SD card's file dates from its trusted clock
(`HalStorage::setFileClock`, set at boot), so a rewritten file gets a new modification time and is
backed up. Without a trusted clock, files get SdFat's default date, as before.

## Screens

| Screen | Reads |
|---|---|
| Overview (today, this week/month/year, total, current and longest streak, books finished this year) | `days.bin`, its header (undated time), `booktotals.bin` (finished), hidden flags from `books.bin`, the running session |
| Days (30), Weeks (26), Months (12), Years (up to 10) bar charts | `days.bin` + running session |
| Day list (sessions of one day) | `sessions.bin` (one pass), titles and hidden flags from `books.bin` |
| Later: per-book view (totals, session lengths, progress over time, speed, time to finish) | `booktotals.bin`, `sessions.bin` filtered by book, the book's raw log for progress |
| Later: weekday x hour grid, reading style labels, compare two books, records, year in review | `sessions.bin` (start, length, active time, gaps between days) |
| Later: "what hooked me" (time and speed per chapter, where pauses and sessions start, turns back) | the book's raw log (`T`, `E` and `S` records by spine item) |
| Later: lookups per hour | `E` records, `/Vocabulary.txt` |
| Later: speed trends by month, time of day, font size | `sessions.bin` (`charsRead`, `learnedMs`, font size) |
| Later: web page, CSV export | all of the above |

A streak counts days with active reading; it is current when it ends today, or yesterday while
today has no reading yet.
