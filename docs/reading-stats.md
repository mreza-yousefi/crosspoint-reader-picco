# Reading statistics (fork)

The reader records when and how you read EPUB books (TXT and Markdown too, as they use the same
reader; XTC books are not recorded) and shows it on the **Reading Stats** screen. This page is the
contract for the data: file formats, the rules that turn page turns into "active reading time",
and what each screen reads. Stage 1 has the recorder, the Overview and the time charts. Stage 2
adds the book lists and book pages, reading styles, the weekday x hour grid, comparing two books,
the week and month drill-down, dictionary time and the automatic-sleep idle trim. Stage 3 adds
records, the year in review, speed trends, lookups per hour, late nights and "what hooked me" per
chapter. Stage 4 (this version) adds a web page and CSV export on the device's web server. Stages
3 and 4 add no file and read the files below.

Code: `src/stats/` (recorder, rules, file formats, queries; Stage 3 rules in `StatsInsights`, the
JSON and CSV text in `StatsExport`), `src/activities/stats/` (screens), `src/network/StatsWebApi.cpp`
and `src/network/html/StatsPage.html` (web page).
Host tests: `test/reading_stats/`.

**Not the upstream `reader.session` plugin event.** Upstream CrossPoint emits a `reader.session`
event (`ReaderSession`) with one book's start, end, duration and progress for sync plugins; it
keeps nothing on the device. These statistics are separate: they keep every session and page turn
on the SD card for the screens here. Both run side by side.

## Privacy

Everything stays on the SD card under `/.crosspoint/stats/`. Nothing is sent anywhere, except
that [Home Backup](home-backup.md) copies the folder to your own server like the rest of the
card. **Clear Reading Cache** does not delete it. To delete all statistics, delete the folder.

The web page and the CSV files (below) are served only by the reader's own web server, which runs
only while File Transfer or Calibre Wireless is on, on the local network (or the reader's hotspot).
Like the files on the card, anyone on that network can read them then: the web server has no
login. It also allows cross-origin requests (CORS), so a web page open in a browser on that network
could read `/api/stats` too. The page loads nothing from the internet and the reader makes no call
for it.

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
- **Hidden book** ("Hide from Stats" in Home's book menu or on the book's page): the book's
  sessions stay recorded and its own totals are kept, but it is left out of the calendar (days,
  charts, streaks, the grid, Overview totals) and the Books list. Show in Stats (or the switch on
  its page, reached from Hidden Books) brings it back. Meant for test books.
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
| `IDLE_TRIM_DEFAULT_SEC` | 60 s | last stretch after automatic sleep, before any rate is known |
| `DICT_MAX_SEC` | 10 min | one dictionary sheet counts at most this long |

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
  menu, or a Reading Stats screen is over the book, the clock stops; the dwell goes on when the
  page is back. The light panel alone does not pause; opened with the toolbar (Up + Down under the
  touch lock) it pauses like the toolbar. Pauses are in the raw log (`E` records).
- **Dictionary time**: selecting a word counts as page time. While a lookup's definition sheet
  is up (`DictionaryDefinitionActivity` with a lookup, and the screens it opens), the time counts
  as reading, apart from the page dwell: no page cap, no speed sample, and the page's dwell goes
  on when the sheet closes. One sheet counts at most `DICT_MAX_SEC` (a sheet left open with
  automatic sleep set to Never). Opening the sheet ends a pause (Look Up from the menu); a page
  change or a pause closes it. It is recorded as `E` kinds 7 and 8 and kept per session as
  `dictSec` (part of `activeSec`).
- **Lookups**: a lookup is counted (`E` kind 1) once its definition has been on screen for 1 s;
  a definition closed sooner, and a word not found, are not counted.
- Re-drawing the same page (clock update, menu closed, toolbar) does not end the dwell. A change
  of font or orientation shows a new page at almost the same place: it is recorded as a jump.
- The last dwell ends at the session end. **Idle trim**: when automatic sleep ends the session
  (reason 3), the open stretch (the page on screen, or the dictionary sheet when it is up) counts
  at most one typical page dwell of the book: the rate in use (the book's own, else all books',
  as for the cap) x the book's typical page length (`ActiveClock::typicalDwellMs`), or
  `IDLE_TRIM_DEFAULT_SEC` (1 min) without a rate. That stretch is mostly the sleep timeout
  without input. A session that sleeps while paused (a menu over the page) is not trimmed: its
  last dwell ended at the pause. Manual sleep keeps the normal cap. The recorder applies it when
  it writes the session and sets flag 32 (`SESSION_IDLE_TRIMMED`) in `X` and `sessions.bin`. A
  rebuild of `sessions.bin` applies it to older automatic-sleep sessions without the flag (their
  `X` holds the untrimmed time; the replayed clock gives the part to take off), so it is applied
  once. The rebuild uses the book's speed model of today, not of the session's day, so it is an
  approximation; it is skipped for a book with fewer than `MIN_MODEL_SAMPLES` samples.

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
back), reserved, spine item, percent, ms since session start, text offset, paper page,
`dwellDs` (the active time the recorder counted for the page that ended, in 0.1 s; 0 = not
known). `dwellDs` was a reserved field the recorder always wrote as 0, so older logs read "not
known" and the log version stays 1. Every page change is recorded: page turns (queued ones too), chapter skips, links, footnotes, the
table of contents, Go to %, bookmarks and sync jumps.

**`E` event (8 bytes)**: type, size, kind (1 dictionary lookup, 2 clipping saved, 3 bookmark
added, 4 end-of-book screen shown, 5 pause, 6 resume, 7 dictionary sheet open, 8 dictionary sheet
closed), reserved, ms since session start. Readers skip kinds they do not know.

**`X` session end (48 bytes)**: type, size, reason (1 closed/Home/other book/restart, 2 sleep, 3
automatic sleep, 4 reset), flags, length in ms, active seconds, endUtc (0 = unknown),
`charsRead`, `learnedMs`, page changes forward, back, jumps, lookups, clippings, bookmarks, end
paper page, end percent, end spine item, tzOffsetMin at the end, `dictSec` (dictionary time in
seconds, part of the active seconds; this was a reserved field written as 0, so older logs read
0). The flags are the session's (`SESSION_*`, below).

### Session index: `sessions.bin` (version 2, 64-byte records)

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
| flags | 1 wall time known, 2 short open, 4 closed by recovery, 8 light on at start, 16 finished the book, 32 idle-trimmed (ended by automatic sleep, see Active time), 64 raw records dropped (the recorder's buffer was full; set in `X` and copied here) |
| endReason | as in `X` |
| fontSize, orientation | at start |
| sessionSeq | session number, unique, also in the raw log |
| charsRead, learnedMs | exact text read and its active time (speed) |
| dictSec | time in the dictionary sheet, part of activeSec (version 2) |
| paperPagesTotal | paper pages in the book as last known in the session, 0 = unknown (version 2); then 2 reserved bytes |

Version 2 appended `dictSec`, `paperPagesTotal` and brought the idle trim. A version 1 file is
rebuilt from the raw logs at the next check, which also trims older automatic-sleep sessions.
Only that rebuild creates `sessions.bin`: a session that ends while it is missing or of another
version is not appended (it is in its raw log), so a new file never holds a single session and
passes as complete.

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

### Session in progress: `current.bin` (version 2)

The running session's totals so far (header, document id, a session record; 112 bytes). Version 1
(104 bytes, a version 1 session record) is still read, with the version 2 fields 0, so a session
cut by a reset before an update is still recovered. Written in place
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
  their last record, with the active and dictionary time re-estimated from the book's current
  speed model. Automatic-sleep sessions without flag 32 get the idle trim. The new index is
  written to `sessions.bin.tmp` and replaces the old file only when every book was replayed and
  written; otherwise the old file stays and the next check tries again. The book or stats screen
  that triggers it shows "Updating reading stats"; the log gives the time it took.
- The raw logs and the registry are never rebuilt.

Checks happen when a book opens and when the Reading Stats screen opens
(`ReadingStats::prepareSummaries()`).

## Home Backup

The registry, the summaries and `current.bin` are rewritten in place, so their size often stays
the same. Home Backup compares files up to 64 KB by content; larger ones by size and
modification time. The reader sets the SD card's file dates from its trusted clock
(`HalStorage::setFileClock`, set at boot), so a rewritten file gets a new modification time and is
backed up. Without a trusted clock, files get SdFat's default date, as before.

## Per book

The book screens add up the book's counted sessions (`countsInTotals`) from `sessions.bin` in
file order plus the running session (`StatsLogic::addBookSession`); the hidden flag does not
apply. They do not read `booktotals.bin`, so they always match the sessions they list.

- **Reading speed**: paper pages (1,800 characters) per hour from `charsRead / learnedMs`; shown
  once 5 minutes of reading are measured.
- **Time to finish**: the paper pages left (the latest known `paperPagesTotal` of the book's
  sessions minus the latest known paper page) at that speed; else, without a length
  or speed, the time spent per percent so far times the percent left (from where the first
  counted session started to the last position, at least 1%). Not shown for a finished book.
- **Days to finish**: local calendar days from the first dated session to the finish, both
  included, each day by its session's own offset.
- **Dates** (first and last read, finished, the Books list's last read day): local days by each
  session's own offset, as in the day list.
- **Progress** chart: the position where each reading day's last session ended, from the first
  reading day to the last, in at most 60 columns; a column without reading carries the previous
  value. At most `MAX_PROGRESS_POINTS` + 1 (129) points of 8 bytes are kept: past that every
  second one is dropped, the first and the last stay.
- **Sessions** list: the newest `MAX_BOOK_SESSIONS` (100) counted sessions.
- **Lookups per hour**: the book's lookups x 3600 / its active seconds, one decimal
  (`lookupsPerHourTenths`).
- **Late nights**: counted sessions with active time in the late-night hours, and that time
  (`lateNightSec`, below).

### Reading style

From the book's counted sessions with a known date (`StyleAcc`, `classifyStyle`). Terms: *reading
days* = local days with such a session; *span* = days from the first reading day to the last,
both included; *dense* = reading days x 2 >= span. The first rule that matches:

| Style | Rule |
|---|---|
| (none, shown "-") | fewer than `STYLE_MIN_SESSIONS` (2) dated sessions |
| Stalled | not finished, the clock is set, and the last reading day is more than `STALLED_DAYS` (30) days ago |
| Binge | dense, and active time / reading days >= `BINGE_DAY_SEC` (1 h per reading day on average) |
| Snacking | active time / sessions < `SNACK_SESSION_SEC` (10 min) |
| Steady | dense |
| Intermittent | anything else |

### Weekday x hour grid

Calendar sessions (`countsInCalendar`) that started on or after the range's first day: the last
30 days, the last 365 days, or all. Without a trusted clock every range shows all. Each
session's active time is spread evenly over the local hours its wall time covers (start to start
plus length; a session of more than a day goes to its start hour), in cells of weekday (Monday
first) x hour (`addToHourGrid`). A cell is shaded in thirds of the busiest cell (`gridShade`).

## Records, year in review, trends

All three read `sessions.bin` (one pass) and the running session; the records and the year also
read `days.bin` for the streaks and the day and week totals. Every local time is the session's own
(`tzOffsetMin` at its start).

| Constant | Value | Meaning |
|---|---|---|
| `LATE_START_HOUR`, `LATE_END_HOUR` | 23, 4 | late-night hours, 23:00-04:00 local |
| `FASTEST_MIN_MEASURED_MS` | 30 min | measured reading a book needs to be the fastest book |
| `YEAR_TOP_BOOKS` | 3 | top books in a year |
| `SPEED_MONTHS` | 12 | months in the speed trend |
| `MAX_FONT_SIZES` | 12 | font sizes in the speed trend; sessions at a 13th size are left out of that view |
| `MORNING_START_HOUR`, `AFTERNOON_START_HOUR`, `EVENING_START_HOUR`, `NIGHT_START_HOUR` | 5, 12, 17, 22 | parts of the day |

- **Late-night time** (`lateNightSec`): the session's active time spread evenly over the local
  hours its wall time covers (as the weekday x hour grid), the part in the late-night hours. A
  session is a late-night session when that part is not 0.
- **Records** (`addRecordSession`, `finishRecords`) use the counted sessions of books not
  hidden. Longest session (active time), most lookups in one session and the fastest book take
  undated sessions too (shown without a date); the others need a known wall time. Longest
  sitting: the active time of a sitting (sessions of one book less than `SITTING_GAP_SEC` apart,
  in file order; an undated session breaks it). Latest night: the latest local end time of a
  session, the clock counted from `LATE_END_HOUR` (so 01:00 is later than 23:00); a session that
  reads on past `LATE_END_HOUR` ends at 03:59 for this. Earliest morning: the earliest local
  start time, counted the same way (03:00 is not early, it is late). Sessions of a day or longer
  have no end time for these. Most in one day and week: from `days.bin` with the running
  session. Fastest book: the highest pages per hour (`pagesPerHour`) of a book's sessions with
  at least `FASTEST_MIN_MEASURED_MS` measured. Longest streak: as on the Overview. Then lookups
  per hour (all these sessions) and late nights (dated ones). Ties keep the first.
- **Year in review** (`addYearSession`, `finishYearReview`): the calendar sessions that started
  in the local year. Total, sessions, average session, lookups (and per hour), late nights, the
  busiest month (by session start), the busiest weekday and the favourite hour (from the weekday
  x hour grid of those sessions), the longest streak counting only the year's days, the top
  `YEAR_TOP_BOOKS` books by time. Books finished: as the Overview (`booktotals.bin`). Reading
  styles: the books with time in the year, each by its style today (all its counted sessions,
  as on the book page; labelled "today"). The tabs are the years with counted calendar reading
  (`days.bin`), newest first, at most 6; without any, this year alone (empty).
- **Speed trends** (`addSpeedSession`): calendar sessions with measured reading (`learnedMs`,
  summed in 64 bits), speed as on the book page (`charsRead / learnedMs`, shown from 5 minutes measured). By month:
  the 12 months up to this one. By part of the day: the session's local start hour; Morning
  05-12, Afternoon 12-17, Evening 17-22, Night 22-05. By font size: the size at the session
  start.

## What hooked me (per chapter)

From one book's raw log (`books/<document id>.log`), replayed twice (`ChapterReplay`), opened
from the book page. Nothing is stored; the result is kept while the screen is open.

| Constant | Value | Meaning |
|---|---|---|
| `MAX_CHAPTERS` | 256 | spine items counted; later ones add to the last entry |
| `MAX_HOOK_SESSIONS` | 8192 | sessions whose short-open state pass 1 keeps (1 bit each); later sessions count |
| `MAX_SESSION_CHAPTERS` | 16 | chapters one session's time is spread over; more add to the 16th |
| `ONE_MORE_PAGES` | 3 | next pages in a new chapter for "one more chapter" |

- **Pass 1** replays the log with `LogReplay` (the same rules and speed model as a rebuild of
  `sessions.bin`) to find which sessions count (`countsInTotals`) and the highest spine item.
  **Pass 2** replays it again and adds the counted sessions to one counter set per spine item
  (36 bytes each: at most 9 KB, sized to the highest spine item read, usually 1-3 KB). Before
  the passes the recorder writes its buffered records, so the running session counts up to now,
  closed as a reset would close it.
- **Time**: each page dwell goes to the spine item of its page, with the dictionary time on that
  page. The dwell is the turn record's `dwellDs` (what the recorder counted); in logs from before
  that field (0) it is estimated by `LogReplay`'s clock with the book's speed model of today. For
  a session with an `X` record, the last page is what its active time leaves when every dwell was
  recorded and none was dropped (flag 64), else the clock's estimate; then the session's chapter
  times are scaled to add up to its active time (the rounding goes to its largest share). A
  session without an `X` record (the running one, or one cut by a reset) has no recorded total:
  its dwells and the clock's estimate of its last page are used as they are, without scaling. So
  chapter time is exact for sessions recorded since `dwellDs`, and an estimate, spread in
  proportion, for older ones. **Speed**:
  `charsRead` over the dwells of the next pages in the chapter.
- **Starts** and **stops**: the spine item of a session's first page and of its last page.
  **Last stop**: where the last counted session ended.
- **One more chapter**: a session turns to the next page from one spine item into the next one,
  the two in different table-of-contents entries (neither without one, so the cover and a
  chapter split into files do not count), and then turns `ONE_MORE_PAGES` more next pages in that
  entry. It counts once per entry, in that chapter; the Highlights show the sessions with at least
  one. Without the book's cache each spine item is its own entry.
- **Re-read**: the text of next pages in a spine item that lies before the furthest text offset
  reached by next pages there earlier (`rereadChars`), shown in paper pages.
- **Turns back**: previous pages and jumps back, counted where they started.
- **Pauses**: pause events (`E` kind 5: a menu or screen opened over the page), not pages left
  longer than their cap. **Lookups**: `E` kind 1.
- **Chapters**: when the book is at its registry path and its spine/TOC cache (`book.bin` in the
  book's cache folder) is there, read without opening the book, spine items are named by their
  table-of-contents entry and consecutive spine items of one entry are merged into one chapter;
  titles are cut to 48 bytes. Otherwise each spine item is a chapter named "Chapter N" (spine
  item + 1). Chapters without reading are left out.

## Screens

All screens work with the buttons alone; touch is optional.

| Screen | Reads |
|---|---|
| Overview (today, this week/month/year, total, current and longest streak, books finished this year; opens Books, Hidden Books, When I Read) | `days.bin`, its header (undated time), `booktotals.bin` (finished), hidden flags from `books.bin`, the running session |
| Days (30), Weeks (26), Months (12), Years (up to 10) bar charts | `days.bin` + running session |
| Week or month drill-down (reading days of a Weeks or Months bar) | `days.bin` + running session |
| Day list (sessions of one day; Confirm opens the session's book page) | `sessions.bin` (one pass), titles and hidden flags from `books.bin` |
| Books, Hidden Books (time, last read, style; last read first) | `sessions.bin` (one pass, 16 + 16 + 1 bytes per book while loading), hidden flags and titles from `books.bin`, the running session |
| Book page: Summary (with lookups per hour and late nights), Progress, Sessions (from the lists, the day list, the book's hold menu on Home or in the Library's Recent view, the reader menu) | `sessions.bin` filtered by book, the running session, title and hidden flag from `books.bin`; up to 100 session rows of 28 bytes and 129 progress points of 8 bytes |
| Compare two books (time, speed, sessions, sittings, style, days to finish; the second book from the books not hidden) | as the book page, without the session rows |
| When I Read (weekday x hour grid) | `sessions.bin` + running session |
| Records (longest session and sitting, most in a day and week, fastest book, longest streak, most lookups in a session, latest night, earliest morning; lookups per hour and late nights overall) | `sessions.bin` + running session (24 bytes per book while loading), `days.bin`, titles from `books.bin` |
| Year in review (a tab per year) | `sessions.bin` + running session (24 bytes per book while loading), `days.bin`, `booktotals.bin` (finished), titles from `books.bin` |
| Reading speed trends: by month, time of day, font size | `sessions.bin` + running session (`charsRead`, `learnedMs`, font size) |
| What hooked me: Highlights and Chapters (from the book page) | the book's raw log (`S`, `T`, `E`, `X` records; the recorder's buffer is written first), its record in `books.bin` (speed model, path), the book's `book.bin` spine/TOC cache for chapter entries and titles |
| Web page `/stats` and CSV export (below) | as the screens above; the CSV files stream `sessions.bin`, `books.bin` and `booktotals.bin` |

A streak counts days with active reading; it is current when it ends today, or yesterday while
today has no reading yet.

## Web page and CSV export

While File Transfer (or Calibre Wireless) is on, `GET /stats` serves a static page from flash
(`src/network/html/StatsPage.html`, gzip, built like the other web pages). It fetches JSON from the
endpoints below and draws the charts with SVG in the browser; it uses no external resource. The
endpoints and their fields are in [webserver-endpoints.md](webserver-endpoints.md#reading-stats-api).

| Endpoint | Data | Reads |
|---|---|---|
| `/api/stats/summary` | Overview values (`OverviewAccumulator`, as the Overview screen), the day list (for the Days/Weeks/Months/Years charts), the years with reading (`loadReadingYears`) | `days.bin` streamed twice (`forEachDay`), `booktotals.bin` (finished this year) |
| `/api/stats/grid` | weekday x hour grid | as When I Read |
| `/api/stats/books` | every book with counted reading and every hidden book (`hidden` marks them) | `books.bin`, `booktotals.bin`, one pass over `sessions.bin` for the style (33 bytes per book while loading) |
| `/api/stats/book` | one book's summary and progress points | as the book page, without the session rows |
| `/api/stats/sessions` | every session, or one book's | `sessions.bin` streamed |
| `/api/stats/hooks` | one book's chapters and highlights | as What Hooked Me |
| `/api/stats/records`, `/year`, `/speed` | as the screens | as the screens |
| `/api/stats/sessions.csv`, `/books.csv` | the CSV files below | `sessions.bin` or the books, with titles from `books.bin` |

Each request first runs `ReadingStats::prepareSummaries()`, as the screens do; the first request
after Ignore Short Opens was changed in Settings rebuilds the summaries (a few seconds with much
reading). The day, book and session lists and the CSV files are written row by row into a 1 KB chunk
buffer (`ChunkedReply`) as they are read, so they cost the same memory for any number of rows; the
books keep the 33 bytes per book of the Books list pass. The records, year, speed, grid, book and
chapter replies hold what the matching screen holds, on the heap. `forEachSession` resets the task
watchdog every 256 records; every 32 rows a list resets it too and checks the browser is still
connected; when it has gone, the day and session iterators stop early (a stop flag). The calendar
charts follow the device: hidden books and sessions without a known wall time are not in them;
Total includes the undated time. The Books list and the CSV files include hidden books.

`book` in the endpoints and the CSV files is the book's record number in `books.bin`. It stays the
same while the stats folder is kept; deleting the folder starts the numbers again.

Both CSV files are UTF-8 with a byte order mark (BOM), so spreadsheets read accented titles
correctly (in pandas: `encoding="utf-8-sig"`). A title or author that starts with `=`, `+`, `-`,
`@`, TAB or CR gets a leading `'`, so a spreadsheet shows it as text instead of running it as a
formula.

### `reading-sessions.csv`

One row per ended session in `sessions.bin` (file order; the running session is not included),
comma-separated, CRLF line ends, quoted per RFC 4180 when a field holds a comma, a quote or a line
break. Times are local at the session's own offset (`tzOffsetMin` at its start); they are empty
when the wall time is unknown.

| Column | Meaning |
|---|---|
| `session` | session number (`sessionSeq`) |
| `book` | index in `books.bin` |
| `title`, `author` | from `books.bin` |
| `hidden` | 1 when the book is hidden from the stats |
| `start_local`, `end_local` | `YYYY-MM-DD HH:MM:SS`; the end is the start plus the wall length |
| `tz_min` | offset from UTC at the start, in minutes east (60 = UTC+1); empty without a wall time |
| `duration_sec`, `active_sec`, `dict_sec` | wall length, active time, dictionary time (part of the active time) |
| `pages` | paper pages moved forward (as `days.bin` counts them; 0 when unknown) |
| `turns_forward`, `turns_back`, `jumps`, `lookups`, `clippings`, `bookmarks` | counts |
| `start_percent`, `end_percent` | position, one decimal; 100.0 = end-of-book screen |
| `chars_read`, `learned_ms` | exact text read and its active time (reading speed) |
| `font_size` | at the start |
| `end_reason` | `closed`, `sleep`, `auto_sleep`, `reset` |
| `flags` | the session flags (`SESSION_*`, above) as a number |
| `counted` | 1 when the session counts in the totals (`countsInTotals`; 0 for a short open with Ignore Short Opens on) |
| `finished` | 1 when the session finished the book (flag 16) |

### `reading-books.csv`

One row per book with counted reading and per hidden book, in registry order. The totals are the
book's `booktotals.bin` record (the short-open rule applies, the hidden flag does not); dates are
local days at the time zone set on the reader.

| Column | Meaning |
|---|---|
| `book` | index in `books.bin` |
| `title`, `author` | from `books.bin` |
| `hidden` | 1 when hidden from the stats |
| `active_sec`, `sessions`, `sittings` | totals |
| `first_read`, `last_read` | `YYYY-MM-DD` of the first and last dated session start |
| `finished` | 1 when a session finished the book |
| `finished_date` | its day; empty when not finished or the time is unknown |
| `position_percent`, `furthest_percent` | where the last session ended, the furthest position |
| `chars_read`, `learned_ms` | exact text read and its active time |
| `pages_per_hour` | reading speed (0 below 5 minutes measured) |
| `style` | reading style: `stalled`, `binge`, `snacking`, `steady`, `intermittent`, or empty |
