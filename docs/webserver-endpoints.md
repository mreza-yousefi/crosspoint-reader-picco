# Webserver Endpoints

This document describes the HTTP, WebSocket, WebDAV, and discovery endpoints
available while CrossPoint Reader is in File Transfer or Calibre Wireless mode.

- HTTP server: port 80
- WebSocket upload server: port 81
- UDP discovery listener: port 8134
- WebDAV: port 80, handled by the same HTTP server

Examples use `crosspoint.local`. If mDNS does not resolve on your network, use
the IP address shown on the device screen.

## HTTP Pages

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/` | Home/status page |
| `GET` | `/files` | File manager page |
| `GET` | `/settings` | Web settings page |
| `GET` | `/fonts` | SD-card font manager page |
| `GET` | `/plugins` | Plugins page (web settings of SD-card plugins) |
| `GET` | `/stats` | Reading Stats page (this fork); data from the [Reading Stats API](#reading-stats-api) |
| `GET` | `/js/jszip.min.js` | JavaScript asset used by the file manager |
| `GET` | `/js/epub-optimizer.js` | File manager EPUB optimizer, loaded with JSZip only when needed |
| `GET` | `/js/plugin-host.js` | Browser plugin host shared by the Plugins, File Manager, and `/plugins-run` pages |

HTML page responses, including `304 Not Modified`, set the cookie `cpPlugins=1` while
plugins are enabled and `cpPlugins=0` otherwise. The page header uses it to hide the
Plugins link.

## Device Status

### `GET /api/status`

```bash
curl http://crosspoint.local/api/status
```

Response:

```json
{
  "version": "1.0.0",
  "firmwareFamily": "crosspoint",
  "pluginApi": 1,
  "pluginCapabilities": "catalog.lists,catalog.notices,catalog.bundle_install,catalog.version_tracking,catalog.compatibility,catalog.item_details,catalog.encoded_item_fields,catalog.list_index_mapping",
  "ip": "192.168.1.100",
  "mode": "STA",
  "rssi": -45,
  "freeHeap": 123456,
  "uptime": 3600,
  "device": "X4"
}
```

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | Firmware version |
| `firmwareFamily` | string | Stable compatibility family; stock CrossPoint uses `crosspoint` |
| `pluginApi` | number | Numeric plugin API level used for catalog compatibility |
| `pluginCapabilities` | string | Comma-separated firmware capabilities used for catalog compatibility |
| `ip` | string | Device IP address |
| `mode` | string | `"STA"` for joined Wi-Fi or `"AP"` for hotspot mode |
| `rssi` | number | Wi-Fi RSSI in dBm; `0` in AP mode |
| `freeHeap` | number | Free heap in bytes |
| `uptime` | number | Seconds since boot |
| `deviceId` | string | With `?plugin=<name>`: 64 hex chars, stable per device and plugin (SHA-256 of an on-device NVS secret and the name); omitted if NVS is unavailable |
| `device` | string | `"X3"` or `"X4"` hardware detection |

## File Management

### `GET /api/files`

Lists files and folders under a directory.

```bash
curl "http://crosspoint.local/api/files?path=/Books"
```

Query parameters:

| Parameter | Required | Default | Description |
|-----------|----------|---------|-------------|
| `path` | No | `/` | Directory to list |

Response:

```json
[
  {"name":"MyBook.epub","size":1234567,"isDirectory":false,"isEpub":true},
  {"name":"Notes","size":0,"isDirectory":true,"isEpub":false}
]
```

Hidden dotfiles are omitted unless the device setting `showHiddenFiles` is
enabled. `System Volume Information` and `XTCache` are always hidden/protected.

The saved-credential stores (`/.crosspoint/wifi.json`, `opds.json`,
`koreader.json`) can never be downloaded, uploaded, renamed, moved, deleted or
written by a plugin over the network, however the path is spelled.

### `GET /download`

Downloads a file from the SD card.

```bash
curl -OJ "http://crosspoint.local/download?path=/Books/MyBook.epub"
```

Query parameters:

| Parameter | Required | Description |
|-----------|----------|-------------|
| `path` | Yes | File path to download |

Protected dotfiles, `System Volume Information`, `XTCache`, and the credential
stores cannot be downloaded. EPUB files are served as `application/epub+zip`; other files use
`application/octet-stream`.

### `POST /upload`

Uploads a file with HTTP multipart form data.

```bash
curl -X POST -F "file=@mybook.epub" "http://crosspoint.local/upload?path=/Books"
```

Query parameters:

| Parameter | Required | Default | Description |
|-----------|----------|---------|-------------|
| `path` | No | `/` | Destination directory |
| `overwrite` | No | off | `1` or `true` replaces a file with the same name |

Successful response:

```text
File uploaded successfully: mybook.epub
```

Notes:

- Without `overwrite`, an existing file with the same name is kept and the
  request fails with `400` "File already exists: <name>".
- With `overwrite`, the upload is written to `<name>.part` and replaces the
  existing file only when it is complete, so a failed upload keeps the old file.
  The card needs free space for both copies while the upload runs. A file
  already named `<name>.part` is replaced. If a folder has the name, the
  request fails with `400` "A folder with this name already exists: <name>".
- EPUB cache data for the uploaded path is cleared after a successful upload.
- Uploading a book file (`.epub`, `.txt`, `.md`, `.xtc`), here or through the
  WebSocket upload, marks the Library index stale, so the book appears the next
  time the Library opens. Hidden files and folders (names that start with `.`)
  don't mark it.
- HTTP upload uses a 4 KB write buffer before flushing to the SD card.

### `POST /mkdir`

Creates a folder.

```bash
curl -X POST -d "name=NewFolder&path=/" http://crosspoint.local/mkdir
```

Form parameters:

| Parameter | Required | Default | Description |
|-----------|----------|---------|-------------|
| `name` | Yes | - | New folder name |
| `path` | No | `/` | Parent folder |

### `POST /rename`

Renames a file.

```bash
curl -X POST -d "path=/Books/old.epub&name=new.epub" http://crosspoint.local/rename
```

Form parameters:

| Parameter | Required | Description |
|-----------|----------|-------------|
| `path` | Yes | Existing file path |
| `name` | Yes | New file name, not a path |

Only files can be renamed through this endpoint. The old EPUB cache path is
cleared before the rename.

### `POST /move`

Moves a file into an existing folder.

```bash
curl -X POST -d "path=/Books/mybook.epub&dest=/Read" http://crosspoint.local/move
```

Form parameters:

| Parameter | Required | Description |
|-----------|----------|-------------|
| `path` | Yes | Existing file path |
| `dest` | Yes | Existing destination folder |

Only files can be moved through this endpoint. The old EPUB cache path is
cleared before the move.

### `POST /delete`

Deletes one or more files or empty folders.

```bash
curl -X POST -d "path=/Books/mybook.epub" http://crosspoint.local/delete
curl -X POST -d 'paths=["/Books/old.epub","/OldFolder"]' http://crosspoint.local/delete
```

Form parameters:

| Parameter | Required | Description |
|-----------|----------|-------------|
| `path` | Yes, unless `paths` is provided | Single path to delete |
| `paths` | Yes, unless `path` is provided | JSON array of paths to delete |

`System Volume Information`, `XTCache` and the saved-credential stores cannot be
deleted. Hidden items (names that start with `.`) can be deleted. Non-empty
folders are rejected. EPUB cache data for deleted files is cleared.

## Settings API

### `GET /api/settings`

Returns a streamed JSON array of editable settings. Each item contains common
fields plus type-specific fields.

```bash
curl http://crosspoint.local/api/settings
```

`?keys=` takes a comma-separated list of setting keys, without spaces, and
returns only those items in the same array format and in settings-list order.
Unknown keys are ignored. A plugin that needs one or two settings should use it
instead of reading the whole list.

```bash
curl 'http://crosspoint.local/api/settings?keys=koServerUrl,dictionaryName'
```

Example item:

```json
{
  "key": "fontSize",
  "name": "Reader Font Size",
  "category": "Reader",
  "type": "enum",
  "value": 1,
  "options": ["12 pt", "14 pt", "16 pt", "18 pt"]
}
```

`value` is always an index into `options`, never the option's text. The
`fontSize` options depend on the selected family. A `.cpfont` family installed
at 10/12/14 pt offers those three sizes. TTF/OTF/TTC families offer the
standard 12/14/16/18 pt sizes. The `fontFamily` and `dictionaryName` options
also depend on the SD card contents.

Types:

| Type | Extra fields |
|------|--------------|
| `toggle` | `value` (`0` or `1`) |
| `enum` | `value`, `options` |
| `value` | `value`, `min`, `max`, `step` |
| `string` | `value` |

The font-family setting includes SD-card font families when they are installed.

### `POST /api/settings`

Applies a partial settings update from a JSON object.

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"fontSize":2,"showHiddenFiles":1}' \
  http://crosspoint.local/api/settings
```

Successful response:

```text
Applied 2 setting(s)
```

## Font Management API

### `GET /api/fonts`

Lists installed SD-card font families. On devices that load TTF/OTF/TTC files,
these families appear with `sizes: [0]`. The `0` means that the font file has
no fixed point size; the reader offers 12, 14, 16, and 18 pt.

```bash
curl http://crosspoint.local/api/fonts
```

Response:

```json
{
  "maxFamilies": 128,
  "vectorFonts": false,
  "families": [
    {
      "name": "Literata",
      "sizes": [12, 14, 16, 18],
      "files": [
        {"name": "Literata_12.cpfont", "size": 123456}
      ]
    }
  ]
}
```

### `POST /api/fonts/upload`

Uploads one font file into a family folder: `.cpfont`, or `.ttf`/`.otf` on devices
with external RAM (`vectorFonts: true` in `GET /api/fonts`).

```bash
curl -X POST \
  -F "family=Literata" \
  -F "file=@Literata_12.cpfont" \
  http://crosspoint.local/api/fonts/upload
```

The handler validates the family name, filename extension, and magic bytes
(`CPFONT`, or a TrueType/OpenType header for `.ttf`/`.otf`) before accepting the
file.

Successful response:

```json
{"ok":true}
```

### `POST /api/fonts/delete`

Deletes an installed font family.

This endpoint removes family subfolders. To remove a loose TTF/OTF/TTC file
from a font root, delete the file from the SD card instead.

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"family":"Literata"}' \
  http://crosspoint.local/api/fonts/delete
```

Successful response:

```json
{"ok":true}
```

## OPDS Server API

### `GET /api/opds`

Lists saved OPDS servers. Passwords are never returned.

```bash
curl http://crosspoint.local/api/opds
```

Response:

```json
[
  {
    "index": 0,
    "name": "My Catalog",
    "url": "http://calibre.local:8080/opds",
    "username": "reader",
    "hasPassword": true
  }
]
```

### `POST /api/opds`

Adds or updates an OPDS server. Include `index` to update an existing entry.
If `password` is omitted during an update, the existing password is preserved.

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"name":"My Catalog","url":"http://calibre.local:8080/opds","username":"reader","password":"secret"}' \
  http://crosspoint.local/api/opds
```

### `POST /api/opds/delete`

Deletes an OPDS server by index.

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"index":0}' \
  http://crosspoint.local/api/opds/delete
```

## Wi-Fi Credential API

### `GET /api/wifi`

Lists saved Wi-Fi networks. Passwords are never returned.

```bash
curl http://crosspoint.local/api/wifi
```

Response:

```json
[
  {
    "index": 0,
    "ssid": "HomeWiFi",
    "hasPassword": true,
    "isLastConnected": true
  }
]
```

### `POST /api/wifi`

Adds or updates a saved Wi-Fi network. Include `index` to update an existing
entry. If `password` is omitted during an update, the existing password is
preserved.

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"ssid":"HomeWiFi","password":"secret"}' \
  http://crosspoint.local/api/wifi
```

### `POST /api/wifi/delete`

Deletes a saved Wi-Fi network by index.

```bash
curl -X POST \
  -H "Content-Type: application/json" \
  -d '{"index":0}' \
  http://crosspoint.local/api/wifi/delete
```

## Reading Stats API

Read-only data of the [reading statistics](reading-stats.md) (this fork), for the `/stats`
page and for scripts. Every reply is streamed with chunked transfer encoding; lists are written
row by row as they are read from the SD card. Each request first brings the summaries up to date
(`ReadingStats::prepareSummaries()`), which can take a few seconds after a firmware update of the
stats format. Days are local day numbers (days since 1970-01-01), times in seconds, positions in
basis points (10000 = end-of-book screen), local times `YYYY-MM-DD HH:MM:SS` at the session's
own offset. `book=` is the book's record number in `books.bin` (stable while the stats folder is
kept, not after it is deleted). Errors: `400 {"error":"..."}` for a missing, empty or non-numeric
argument, `404 {"error":"unknown book"}` for a book number the registry does not have (`book`,
`hooks`, `sessions?book=`), `503 {"error":"out of memory"}`. The first request after Ignore Short
Opens was changed rebuilds the summaries and can take a few seconds. A list stops being read when
the client disconnects. The server allows cross-origin requests (CORS), so a page open in a browser
on the same network can read these endpoints.

### `GET /api/stats/summary`

```bash
curl http://crosspoint.local/api/stats/summary
```

```json
{"days":[[20400,3600,2,1,15]],"clockKnown":true,"today":20409,"ignoreShortOpens":true,
 "totalSec":93600,"undatedSec":0,"longestStreak":12,"todaySec":1800,"weekSec":5400,
 "monthSec":36000,"yearSec":90000,"currentStreak":3,"finishedThisYear":4,"years":[2026,2025]}
```

| Field | Description |
|-------|-------------|
| `days` | Calendar days with reading, ascending: `[day, activeSec, sessions, sittings, pagesForward]` |
| `clockKnown` | The reader's clock is trusted; without it, `today` is the last day with reading and the `todaySec`...`finishedThisYear` fields are left out |
| `today` | Local day the charts end on |
| `totalSec`, `undatedSec` | All counted reading, and the part without a known date (not in `days`) |
| `longestStreak`, `currentStreak` | Days in a row with reading |
| `years` | Years with reading, newest first (this year alone when there is none and the clock is set) |

### `GET /api/stats/grid?range=30|365|all`

`{"clockKnown":true,"grid":[...]}`: 168 active-second values, weekday (Monday first) x 24 + hour,
over the last 30 or 365 days, or all (the default, and every range without a trusted clock).

### `GET /api/stats/books`

Every book with counted reading and every hidden book, in registry order:

```json
[{"book":0,"title":"Dune","author":"Frank Herbert","hidden":false,"active":36000,"sessions":20,
  "sittings":15,"lastUtc":1763400000,"first":"2026-10-01","last":"2026-11-17","finished":false,
  "finishedDate":"","lastBp":4520,"maxBp":4520,"pagesPerHour":38,"style":"steady"}]
```

`style` is `stalled`, `binge`, `snacking`, `steady`, `intermittent` or empty. `first`, `last`
and `finishedDate` are local dates at the reader's time zone, empty when unknown.

### `GET /api/stats/book?book=<n>`

One book's page (`404` for an unknown book): `title`, `author`, `hidden`, `style`, `active`, `sessions`, `sittings`,
`firstDay`/`lastDay` (when dated), `finished`, `finishedDay` (when known), `pagesPerHour`,
`secondsToFinish`, `daysToFinish`, `positionBp`, `maxBp`, `paperPage`, `paperPages`, `lookups`,
`lookupsPerHourTenths`, `dictSec`, `lateSessions`, `lateSec`, and `progress`: `[day, bp]` per
reading day (at most 129 points).

### `GET /api/stats/sessions[?book=<n>]`

Every session in `sessions.bin` order (then the running one, `"live":true`), or one book's
(`404` for an unknown book):

```json
[{"seq":42,"book":3,"utc":1763418600,"start":"2026-11-17 23:30:00","tz":60,"dur":3600,
  "active":3000,"dict":120,"pages":15,"fwd":40,"back":2,"jumps":0,"lookups":3,"startBp":9850,
  "endBp":10000,"chars":27000,"learnedMs":2400000,"font":12,"reason":"auto_sleep","flags":17,
  "counted":true,"finished":true,"hidden":false,"live":false}]
```

`utc` is 0 and `start` empty without a known wall time; `tz` is minutes east of UTC; `counted`
is false for a short open left out of the totals.

### `GET /api/stats/hooks?book=<n>`

What Hooked Me for one book (two passes over its raw log; can take a few seconds):
`sessions`, `oneMoreSessions`, `lastStop` (chapter index or -1), `highlights` (chapter index or
-1 for `mostTime`, `fastest`, `slowest`, `mostStarts`, `mostEnds`, `mostReread`, `mostBack`,
`mostPauses`, `mostLookups`) and `chapters` in book order: `title` (empty without the book's
cache), `spine`, `activeMs`, `pagesPerHour`, `starts`, `ends`, `pauses`, `back`, `oneMore`,
`lookups`, `rereadChars`.

### `GET /api/stats/records`

Each record present is `{"value":N,"book":n,"title":"...","start":"local time or empty"}`:
`longestSession`, `longestSitting` (seconds), `mostLookups`, `latestNight`, `earliestMorning`
(local minute of the day), `fastestBook` (pages per hour). Also `mostDay`/`mostDaySec`,
`mostWeekStart` (first day)/`mostWeekSec`, `longestStreak`, `lookups`, `activeSec`,
`lookupsPerHourTenths`, `lateSessions`, `lateSec`. Hidden books are left out.

### `GET /api/stats/year[?year=<y>]`

The year in review (default: the newest year with reading; `404` without one; `400` for a year
that is not a number from 1970 to 9999): `year`, `active`,
`sessions`, `lookups`, `lookupsPerHourTenths`, `lateSessions`, `lateSec`, `longestStreak`,
`finished`, `busiestMonth` (0-11), `busiestWeekday` (Monday = 0), `favouriteHour` (-1 when none),
`months` (12), `weekdays` (7), `hours` (24) in active seconds, `styles` (books per style today)
and `topBooks`: `[{"book":n,"title":"...","sec":N}]`.

### `GET /api/stats/speed`

`firstMonth` (year x 12 + month - 1 of the first of 12 months), `months` (12) and `dayParts`
(morning, afternoon, evening, night) as `[pagesPerHour, measuredSec]`, and `fonts`:
`[{"size":12,"pagesPerHour":38,"measuredSec":7200}]`. `pagesPerHour` is 0 below 5 minutes measured.

### `GET /api/stats/sessions.csv`, `GET /api/stats/books.csv`

Downloads (`Content-Disposition: attachment`, `text/csv; charset=utf-8`, starting with a UTF-8
BOM; pandas: `encoding="utf-8-sig"`) named `reading-sessions.csv` and `reading-books.csv`. Titles
and authors starting with `=`, `+`, `-`, `@`, TAB or CR get a leading `'`. Columns:
[reading-stats.md](reading-stats.md#web-page-and-csv-export).

```bash
curl -O -J http://crosspoint.local/api/stats/sessions.csv
```

## WebSocket Upload

### Port 81

The WebSocket path is used for fast binary uploads from the file manager and
Calibre plugin workflows.

Connection:

```text
ws://crosspoint.local:81/
```

Protocol:

1. Client sends text: `START:<filename>:<size>:<path>`
2. Server replies `READY`
3. Client sends binary chunks
4. Server sends `PROGRESS:<received>:<total>` every 64 KB or at completion
5. Server sends `DONE` when complete or `ERROR:<message>` on failure

Example session:

```text
Client -> START:mybook.epub:1234567:/Books
Server -> READY
Client -> [binary chunk]
Server -> PROGRESS:65536:1234567
...
Server -> DONE
```

Error messages include:

| Message | Cause |
|---------|-------|
| `ERROR:Upload already in progress` | A second upload was started before the first completed |
| `ERROR:Invalid START format` | Malformed START message or invalid size token |
| `ERROR:Failed to create file` | Destination file could not be opened |
| `ERROR:No upload in progress` | Binary data arrived without a matching START |
| `ERROR:Upload overflow` | Client sent more bytes than declared |
| `ERROR:Write failed - disk full?` | SD write failed |
| `ERROR:File already exists: <name>` | A file with that name exists; WebSocket uploads never replace files |

Incomplete WebSocket uploads are deleted on disconnect or error.

The server accepts two WebSocket connections at a time and closes further ones
without a reply. It closes a connection that sends nothing for two minutes, which
also deletes an unfinished upload.

## WebDAV

The same HTTP server registers a WebDAV-compatible handler for file manager clients.

Supported methods:

```text
OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, MKCOL, MOVE, COPY, LOCK, UNLOCK
```

Notes:

- `PUT` writes to a temporary `.davtmp` file first, then renames it into place.
- Protected paths are rejected.
- `LOCK` and `UNLOCK` are accepted for client compatibility only. The server
  does not implement full WebDAV Class 2 locking semantics such as persistent
  locks or lock discovery.

## UDP Discovery

The server listens on UDP port `8134`. When it receives the text payload
`hello`, it replies to the sender with:

```text
crosspoint (on <hostname>);81
```

The final field is the WebSocket upload port.

## Network Modes

### Station Mode (STA)

- Device joins an existing 2.4 GHz Wi-Fi network.
- `crosspoint.local` is advertised with mDNS when available.
- `/api/status` returns `"mode": "STA"` and RSSI in dBm.

### Access Point Mode (AP)

- Device creates an open hotspot named `CrossPoint-Reader`.
- The device shows a Wi-Fi QR code and URL QR code.
- The fallback IP is typically `192.168.4.1`.
- `/api/status` returns `"mode": "AP"` and `"rssi": 0`.
- The hotspot's DNS answers every name with the device's IP. A `GET` or `HEAD`
  request whose `Host` is any other domain name (not an IP address, a `.local`
  name or a single-label name such as `crosspoint`) gets
  `302 Location: http://<AP IP>/`, so phones and computers can open the home
  page as a captive portal. Other methods are not redirected.

### Calibre Wireless

Calibre Wireless starts the same web server in STA mode and displays setup
instructions plus WebSocket upload progress on the device screen.

## Plugin API

Device capabilities for browser-side SD plugins (`plugin.js`). Plugins normally
reach these through the plugin host wrappers in `/js/plugin-host.js`
(`api.relay`, `api.crypto`, `api.fetchToSd`, `api.writeFile`); the raw
contracts are below. Outbound TLS is encrypted but the peer is not verified
(the transport ships no CA bundle).

### `GET /api/plugins`

Installed plugins that ship a `plugin.js`:
`[{"name":"<folder>","dir":"<root>/<folder>","title":"<title>","mount":"settings"}, ...]`.
`dir` is where the plugin is installed (plugins keep their own files there);
`title` and `mount` come from the plugin's `manifest.json` when present.
Mount `settings` (the default) shows the card on the Plugins page (`/plugins`);
mount `files` shows it in the File Manager.

### `GET /plugin?name=<plugin>&file=<file>`

Serves one file from the plugin's folder (first root holding that name). Both
parameters must be single path components (no `/`, `\`, or `..`); 404 when
the plugin, file, or a directory is requested; 500 when the file cannot be read.

Successful responses carry an `ETag` (MD5 of the file contents) and
`Cache-Control: no-cache`, so a browser keeps the file and checks it again on
each load. A request whose `If-None-Match` matches gets `304` with no body.

### `POST /api/relay`

Makes an outbound HTTP(S) request for a plugin (any method; browsers can't, due
to CORS). Body: `{"plugin":"<name>","method":"GET","url":"https://...","headers":{...},"body":"..."}`.

- **Success:** `200` with the upstream body verbatim. The upstream status is
  in the `X-Relay-Status` response header, and its headers in `X-Relay-Headers`
  as JSON `[["name","value"], ...]` in receive order, duplicates kept (every
  `Set-Cookie`). `api.relay()` resolves to `{status, headers, body}`.
- **Redirects** are not followed: a 3xx comes back with its `location` header.
- **Limits:** the upstream body is capped at 64KB. The device forwards the body
  as it arrives and does not store it. Use `/api/fetch` for large files. A GET
  that fails with 502 can be retried with `/api/fetch` (`api.relay()` throws an
  error whose `status` is 502).
- **Errors:** `400 {"error":"missing plugin/url"}`; `502 {"error":"relay failed; ..."}`
  when the request fails before any of the body arrives, or the upstream
  `Content-Length` is over the cap (the device log names which). If the `200`
  has already started and the body is then cut short, or goes over the cap
  without a `Content-Length`, the device closes the connection. `api.relay()`
  reports that as the same 502 error.

### `POST /api/crypto`

Stateless wolfSSL primitives. Body `{"op":"<op>", ...}`; binary fields are
base64 in both directions, each at most 64KB encoded. Replies are `200` with
the result fields, or `200 {"error":"..."}`.

| `op` | Request fields | Result |
| --- | --- | --- |
| `random` | `len` (default 16, max 4096) | `data` |
| `sha1` | `data` | `data` (20 bytes) |
| `sha256` | `data` | `data` (32 bytes) |
| `aesenc` | `key`, `iv` (16 bytes each), `data` | `data`: AES-128-CBC with PKCS#7 padding |
| `aesdec` | `key`, `iv`, `data` (block-aligned) | `data`: AES-128-CBC, padding left in place |
| `keygen` | none | `public` (SPKI DER), `private` (PKCS#8 DER): RSA key pair |
| `pubencrypt` | `cert` (X.509 DER), `data` | `data`: RSAES-PKCS1-v1_5 to the certificate's key |
| `sign` | `private` (PKCS#8 DER), `hash` (20 bytes) | `data` (128 bytes): raw RSA signature |
| `rsadec` | `private` (PKCS#8 DER), `data` (one modulus block) | `data`: raw RSA private-key result, padding left in place |
| `pkcs12` | `data` (the bundle), `password` | `key`, `cert` (DER) |

### `POST /api/book-key`

Stores a protected book's content key for the reader. Body:
`{"path":"/abs/book.epub","key":"<base64, 16 bytes>","expires":<epoch seconds, optional>}`.
The device wraps the key with AES-256-GCM under a key derived from its NVS
secret and writes `<path>.key`; the header (including `expires`) is
authenticated, so the file only opens on this reader and its loan date cannot
be edited. The reader enforces `expires` when the book opens.

- **Success:** `200 {"ok":true}`.
- **Errors:** `400 {"error":"bad path/key"}`; `500 {"error":"cannot store key"}`.

### `POST /api/fetch`

Downloads a URL straight to SD, so a large body never passes through the
browser. Body: `{"plugin":"<name>","url":"...","dest":"/abs/path","headers":{...},"offset":0,"maxBytes":0}`,
plus optional `"method"` (`GET`, `POST`, `PUT`, `PATCH`, or `DELETE`; default
`GET`) and `"body"` (a string, at most 4KB).

- A request with a method other than GET, or with a non-empty body, is sent once: it is
  never replayed, and `offset`/`maxBytes` must be 0. A 3xx answer is not
  followed; it fails with `error:"http status"`. Use it for APIs that answer a
  POST with a large file. Such a request is not split into segments, so if the
  browser connection drops on a very large answer, check whether `dest` was
  installed.

- `dest` must be absolute with no `..`; missing parent folders are created.
- The transfer is staged in `<dest>.part` and replaces `dest` only when the
  whole body has arrived, so a failed update keeps the previous file.
- Installing a book file (`.epub`, `.txt`, `.md`, `.xtc`) marks the Library
  index stale, so the book appears the next time the Library opens. Requests
  that end with `complete:false`, failed transfers, other file types, and
  hidden files or folders (names that start with `.`) don't mark it.
- A GET without a body follows up to 5 redirects. `headers` (typically the plugin's
  `Authorization`) are sent only while the target keeps the starting URL's
  scheme, host, and port; a redirect elsewhere, including https→http, gets none.
- For a GET without a body, an answer cut short mid-transfer resumes on its own
  with a Range request.
- **Segments:** `maxBytes` (at most 4MB) ends the request after that many
  bytes with `complete:false`; send the next request with `offset` set to the
  returned `bytes` to continue the same `.part`. `api.fetchToSd()` does this
  loop, keeping each browser request short.
- A long transfer answers early with `200` chunked and sends whitespace every
  5s to keep the browser connection open; the JSON result follows at the end.
- **Result:** `{"status":200,"bytes":N,"complete":true,"total":N}` (`total`
  when the server reported a size). `status` is the upstream HTTP status, or
  -1 when no answer arrived. `error` is set to `transport failure`,
  `http status`, or `sd write failed` when the file was not installed. A failed
  answer may include `body`, up to its first 512 bytes (never ending in a cut
  UTF-8 character); currently only requests sent with a method other than GET
  or a non-empty body include it.
- **Errors:** `400 {"error":"bad url/dest"}`; `400 {"error":"unsupported method"}`;
  `400 {"error":"method must be a string"}`;
  `400 {"error":"body must be a string"}`; `400 {"error":"body over 4KB"}`;
  `400 {"error":"offset and maxBytes need a GET without a body"}`;
  `409 {"error":"offset mismatch","bytes":N}`
  when `offset` does not match the `.part` size; `502 {"error":"sd write failed" |
  "range unsupported" | "download truncated","bytes":N,"complete":false}`.

### `POST /api/plugin-fs?plugin=<name>&path=<abs path>`

Writes one small file to SD. The content is sent as a multipart file part
(`api.writeFile()` builds it), so binary data, including NUL bytes, arrives
intact. It streams to `<path>.tmp` and replaces `path` only after a complete,
non-empty body. `path` must be absolute, without `..`, and not a credential
store. A plugin keeps its own files in its install folder (`api.dir`). As with
`/api/fetch`, writing a book file marks the Library index stale.

- **Success:** `200 {"ok":true,"bytes":N}`.
- **Errors:** `400` (`bad path`, `empty body`, `missing file part`,
  `upload aborted`); `413 {"error":"too large"}` over 256KB; `500
  {"error":"cannot write" | "sd write failed"}`. The previous file is kept on
  every error.

## Plugin Job Queue

External systems trigger SD-plugin actions without the web UI. The firmware
stores opaque `{plugin, action, args}` blobs (fixed 6-slot pool; args/result
< 192 bytes JSON); an open page hosting the plugin executes them. See
`docs/sd-plugins.md` for the full contract.

### `POST /api/plugin-jobs`

```bash
curl -X POST http://crosspoint.local/api/plugin-jobs \
  -d '{"plugin":"<name>","action":"<action>","args":{"path":"/Books/somefile"}}'
# -> {"id":3}         (503 {"error":"job queue full"} when all slots busy,
#                      or {"error":"out of memory"} if the queue cannot be allocated)
```

### `GET /api/plugin-jobs/claim?plugin=<name>`

Executor-side (used by the plugin host page): returns the next pending job
`{"id":3,"claim":7,"action":"<action>","args":{...}}` and marks it running, or `{"id":0}`.
A running job whose lease expires returns to pending and gets a new `claim` when re-claimed.

### `POST /api/plugin-jobs/complete`

Executor-side: `{"id":3,"claim":7,"ok":true,"result":{...}}` -> `{"ok":true}`. The `claim`
must match the one from the claim response; a stale claim (the lease expired and another
executor re-claimed the job) gets `409 {"error":"stale claim"}` and changes nothing.

### `GET /api/plugin-jobs/status?id=<n>`

```bash
curl "http://crosspoint.local/api/plugin-jobs/status?id=3"
# -> {"id":3,"state":"done","result":{"title":"...","dest":"/Books/Book.epub"}}
```

States: `pending`, `running`, `done`, `error`, `unknown` (slot recycled —
poll promptly after completion).

### `GET /plugins-run`

Headless executor page: loads every plugin with its UI hidden so registered
actions run. Keep it open (browser tab or embedded webview) while jobs are
queued; jobs enqueued with no page open stay pending until one opens.
