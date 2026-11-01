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

The reader trusts exactly one certificate: `SERVER_CA` in `src/backup/BackupEngine.cpp`. It is used as the only root
for the TLS handshake (wolfSSL `SecureClient`). For a self-signed server certificate, paste its PEM there. Its
SAN must match the host in the Server URL (an IP SAN works). Then rebuild and flash.

When the server certificate changes, the reader fails with "network error" until the firmware is updated.

## Files on the reader

| Path | Content |
|---|---|
| `/.crosspoint/backup.json` | Server URL, user, obfuscated password, auto flag, last result and time |
| `/.crosspoint/backup/manifest.tsv` | `path`, `size`, `mtime`, `crc32` of each uploaded file |

Deleting `manifest.tsv` makes the next run upload everything again.

## Change detection

- Files of 64 KB or less are compared by CRC32 (progress files keep their size; FAT dates are unreliable without a
  clock).
- Larger files are compared by size and modification time.

## Code map

- `src/backup/BackupEngine.*`: scan, manifest, drop-folder fetch, minimal keep-alive WebDAV client (streamed `PUT`)
- `src/backup/BackupConfig.*`: settings file
- `src/activities/settings/BackupSettingsActivity.*`, `BackupRunActivity.*`: UI
- `src/main.cpp` `runSleepBackup()`: automatic run at sleep entry
