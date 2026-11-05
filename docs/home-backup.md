# Home Backup (fork feature)

Home Backup pushes the reader's SD card to a WebDAV server on your home network, and pulls books queued on that
server. User-facing behavior: [USER_GUIDE.md section 3.6.8](../USER_GUIDE.md#368-home-backup-fork).

## Server requirements

- A WebDAV endpoint (tested with `rclone serve webdav`) with HTTP Basic auth and **one dedicated user** for the reader.
- HTTPS with a certificate the firmware trusts (see below). Plain `http://` URLs also work, but then the password
  crosses the network in the clear.
- The endpoint root is the backup root. The reader creates folders with `MKCOL`, uploads with `PUT`, and never
  sends `DELETE` except for files it fetched from `/to-reader/`.
- Optional: a writable `/to-reader/` folder (for example through File Browser) to queue books for the reader.
- Snapshots (for example a daily `restic` run over the backup root) give you history. The reader keeps only the
  latest copy of each file.

Requests carry `User-Agent: CrossPoint/<version>`.

## Certificate pinning

For `https://` the reader trusts exactly one certificate: the PEM file `/.crosspoint/backup-ca.pem` on the SD card.
It is used as the only root for the TLS handshake (wolfSSL `SecureClient`). For a self-signed server, copy the
server's own certificate there. Its SAN must match the host in the Server URL (an IP SAN works). No rebuild is
needed.

Copy the file straight onto the card (USB drive mode or a card reader). File Transfer cannot read or write this
file or `backup.json`. A swapped certificate would let another device on the network read the backup login.
Without it an `https://` backup does not start (Last backup shows "no certificate", and the daily backup does not
join Wi-Fi); the reader never falls back to an unverified connection. When the server certificate changes, replace
the file; until then runs fail with "network error".

## Files on the reader

| Path | Content |
|---|---|
| `/.crosspoint/backup.json` | Server URL, user, obfuscated password, web view address, auto flag, last result and time |
| `/.crosspoint/backup-ca.pem` | Pinned server certificate for `https://` (you copy it there) |
| `/.crosspoint/backup/manifest.tsv` | `path`, `size`, `mtime`, `crc32` of each uploaded file |

Deleting `manifest.tsv` makes the next run upload everything again.

Wi-Fi, sync and OPDS logins and tokens, `backup.json` and `backup-ca.pem` stay on the reader.

## Change detection

- Files of 64 KB or less are compared by CRC32 (progress files keep their size; FAT dates are unreliable without a
  clock).
- Larger files are compared by size and modification time.

## Code map

- `src/backup/BackupEngine.*`: scan, manifest, drop-folder fetch, minimal keep-alive WebDAV client (streamed `PUT`)
- `src/backup/BackupConfig.*`: settings file
- `src/activities/settings/BackupSettingsActivity.*`, `BackupRunActivity.*`: UI
- `src/main.cpp` `runSleepBackup()`: automatic run at sleep entry
