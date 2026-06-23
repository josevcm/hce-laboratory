# DESFire Card Structure

Reference document for the logical structure of a MIFARE DESFire EV1/EV2 card and the JSON format used by HCE Laboratory to define card images for physical emulation.

---

## 1. Logical Structure

A DESFire card organises data in three levels:

```
PICC (card)
├── PICC Master Application  (AID 000000)
│   └── Master Key (Key 0)
└── User Application 1  (AID xxxxxx)
│   ├── Keys  (Key 0 … Key 13)
│   └── Files (File 0 … File 31)
│       ├── Standard Data File
│       ├── Backup Data File
│       ├── Value File
│       ├── Linear Record File
│       └── Cyclic Record File
└── User Application N  …
```

### 1.1 PICC

The card root. Identifies itself during ISO 14443-4 anticollision with:

| Field | Description |
|-------|-------------|
| **UID** | 7-byte unique identifier (hex string) |
| **ATQA** | Answer-To-Request type A (2 bytes, e.g. `0x4403` = 17411) |
| **SAK** | Select Acknowledge byte (e.g. `0x20` = 32 → ISO 14443-4 compliant) |
| **ATS** | Answer-To-Select: `TB1` (frame waiting time), `TC1` (capabilities), `HB` (historical bytes) |

The PICC also responds to `GetVersion` with hardware and software version info and a production batch number.

### 1.2 Applications

Each application is an isolated security domain identified by a 3-byte **AID** (`000000` is reserved for the PICC master application).

| Field | Description |
|-------|-------------|
| **AID** | Application Identifier, 3 bytes (integer in JSON) |
| **keySettings1** | Access control byte for the application master key (see §1.4) |
| **keySettings2** | Packed byte encoding crypto mode, max keys, and ISO enable flag (see §1.4) |
| **keys** | Array of key entries (Key 0 is always the application master key) |
| **files** | Array of file entries |
| **isoId** | (optional) ISO 7816-4 FID for the application DF — required when ISO mode is enabled |
| **isoName** | (optional) DF name as hex string (e.g. `D2760000850101`) — used for ISO SELECT by name |

#### keySettings1 — bit map

| Bit | Mask | Meaning when set |
|-----|------|-----------------|
| 0 | `0x01` | Master key is changeable |
| 1 | `0x02` | Free directory listing (no auth needed for `GetFileIDs`) |
| 2 | `0x04` | Free application create/delete |
| 3 | `0x08` | Configuration changeable |
| 7:4 | `0xF0` | Change key access: `0x0` = master key, `0xE` = target key itself, `0xF` = frozen |

Common values: `0x0F` (all rights, master key changeable), `0x0B` (no free listing, key self-change frozen).

#### keySettings2 — bit map

| Bits | Meaning |
|------|---------|
| 3:0 | Maximum number of keys in the application (1–14) |
| 5 | `1` = ISO/IEC 7816-4 DF/EF naming enabled (requires `isoId` / `isoName`) |
| 7:6 | Crypto mode: `00` = 2K3DES (Legacy), `01` = 3K3DES (ISO), `10` = AES |

Examples: `0x01` = 1 key, Legacy, no ISO; `0x82` = 2 keys, AES, no ISO; `0x22` = 2 keys, ISO 3K3DES with ISO DF naming.

### 1.3 Files

Up to 32 files per application (IDs 0–31). All file types share a common header:

| Field | Type | Description |
|-------|------|-------------|
| **id** | int | File number (0–31) |
| **type** | int | File type (see below) |
| **commSettings** | int | Communication security: `0` = plain, `1` = MACed, `3` = encrypted |
| **accessRights** | int | 16-bit packed access control word (see §1.5) |
| **isoId** | int | (optional) ISO 7816-4 EF file ID — required when application has ISO mode enabled |

#### File types

| Value | Name | Required extra fields |
|-------|------|-----------------------|
| `0` | Standard Data File | `size`, `data` |
| `1` | Backup Data File | `size`, `data` |
| `2` | Value File | `lowerLimit`, `upperLimit`, `value`, `limitedCredit`, `features` |
| `3` | Linear Record File | `size`, `recordSize`, `maxRecords`, `data` |
| `4` | Cyclic Record File | `size`, `recordSize`, `maxRecords`, `data` |

**Standard / Backup Data File** — flat byte array:

| Field | Type | Description |
|-------|------|-------------|
| `size` | int | File size in bytes |
| `data` | hex string | Current file content (hex-encoded, length must equal `size`) |

**Value File** — signed 32-bit counter with limits:

| Field | Type | Description |
|-------|------|-------------|
| `lowerLimit` | int | Minimum allowed value |
| `upperLimit` | int | Maximum allowed value |
| `value` | int | Current counter value |
| `limitedCredit` | int | Maximum amount allowed per `LimitedCredit` operation (`0` = disabled) |
| `features` | int | `0x01` = limited credit enabled, `0x02` = free read access |

**Linear / Cyclic Record File** — array of fixed-size records:

| Field | Type | Description |
|-------|------|-------------|
| `size` | int | Total file size in bytes (`recordSize × maxRecords`) |
| `recordSize` | int | Size of each record in bytes |
| `maxRecords` | int | Maximum number of records |
| `data` | hex string | Current records, concatenated (most recent first for cyclic) |

### 1.4 Keys

Each key entry inside an application:

| Field | Type | Description |
|-------|------|-------------|
| **id** | int | Key number (0 = master key) |
| **type** | int | `0` = 2K3DES (16 bytes), `1` = 3K3DES (24 bytes), `2` = AES-128 (16 bytes) |
| **version** | int | Key version byte (informational, returned by `GetKeyVersion`) |
| **value** | hex string | Key material — 32 hex chars for 2K3DES/AES, 48 for 3K3DES |

Default factory key for all types: all-zeros (`00000000000000000000000000000000`).

### 1.5 Access Rights

The `accessRights` field is a 16-bit integer encoding four 4-bit key numbers:

```
Bits 15:12  Read key
Bits 11:8   Write key
Bits  7:4   Read+Write key
Bits  3:0   Change settings key
```

Special values per nibble:
- `0x0`–`0xD` → key number with that index must be authenticated
- `0xE` → **FREE** (no authentication required)
- `0xF` → **NEVER** (operation permanently denied)

Examples:

| Value (hex) | Decimal | Read | Write | R+W | Change |
|-------------|---------|------|-------|-----|--------|
| `0xEEEE` | 61166 | FREE | FREE | FREE | FREE |
| `0x1230` | 4656 | Key1 | Key2 | Key3 | Key0 |
| `0x2420` | 9248 | Key2 | Key4 | Key2 | Key0 |
| `0x0FF0` | 4080 | Key0 | NEVER | NEVER | Key0 |

### 1.6 Version / Production Info

Returned by the `GetVersion` command. Values simulate a real EV1 chip:

| Field | Meaning | Typical value |
|-------|---------|---------------|
| `hw.vendor` | NXP Semiconductors | `4` |
| `hw.type` | DESFire | `1` |
| `hw.subtype` | EV1 | `1` |
| `hw.version` | Major.Minor packed (e.g. 1.0 = `0x0100`) | `256` |
| `hw.storage` | Storage size code (`0x18` = 8KB) | `24` |
| `hw.protocol` | ISO 14443-4 | `5` |
| `sw.*` | Same fields for firmware version | — |
| `tr.batch` | Production batch number | any |
| `tr.week` | Production week | 1–52 |
| `tr.year` | Production year (last two digits) | e.g. `22` |

### 1.7 Implemented commands and access keys

`X` marks the key family that can authorize the command. A blank cell means the command does not depend on that file-key family; in those cases the note column indicates the real requirement (PICC master key, free listing/create-delete flag, prior authentication, or stubbed command).

| Category | Command | Read Key | Write Key  | R/W Key | Conf Key | Notes |
|----------|---------|----------|------------|---------|----------|-------|
| Security / auth | `Authenticate` |          |            |         |          | Starts legacy authentication |
| Security / auth | `AuthenticateISO` |          |            |         |          | Starts ISO authentication |
| Security / auth | `AuthenticateAES` |          |            |         |          | Starts AES authentication |
| Security / auth | `ChangeKey` |          |            |         | X        | Requires prior authentication and the configured change-key / master-key path |
| Security / auth | `ChangeKeySettings` |          |            |         | X        | PICC master key + `isAllowChangeConfig()` |
| Security / auth | `GetKeySettings` |          |            |         |          | PICC master key or free directory listing |
| Security / auth | `GetKeyVersion` |          |            |         |          | Selected application only; the key must exist |
| Security / auth | `SetConfiguration` |          |            |         | X        | PICC master key |
| PICC | `GetCardUID` |          |            |         |          | Prior authentication is required (any mode) |
| PICC | `CreateApplication` |          |            |         |          | PICC master key or free create/delete |
| PICC | `DeleteApplication` |          |            |         |          | PICC master key or free create/delete |
| PICC | `FormatPICC` |          |            |         |          | PICC master key |
| PICC | `GetFreeMemory` |          |            |         |          | Public |
| PICC | `GetVersion` |          |            |         |          | Public |
| PICC | `ListApplications` |          |            |         |          | PICC master key or free directory listing |
| PICC | `ListDFNames` |          |            |         |          | PICC master key or free directory listing |
| PICC | `SelectApplication` |          |            |         |          | Public |
| PICC | `GetIsoFileIDs` |          |            |         |          | Application selected; PICC master key or free directory listing |
| Application / file | `CreateStdDataFile` |          |            |         |          | PICC master key or free create/delete |
| Application / file | `CreateBackupDataFile` |          |            |         |          | PICC master key or free create/delete |
| Application / file | `CreateValueFile` |          |            |         |          | PICC master key or free create/delete |
| Application / file | `CreateLinearRecordFile` |          |            |         |          | PICC master key or free create/delete |
| Application / file | `CreateCyclicRecordFile` |          |            |         |          | PICC master key or free create/delete |
| Application / file | `ChangeFileSettings` |          |            |         | X        | Uses the file `changeRightsKey()` |
| Application / file | `GetFileSettings` |          |            |         |          | Application selected; no file-key gate |
| Application / file | `DeleteFile` |          |            |         |          | PICC master key or free create/delete |
| Application / file | `ListFiles` |          |            |         |          | PICC master key or free directory listing |
| Application / file | `ReadData` | X        |            | X       |          | Requires `readKey()` or `readWriteKey()` |
| Application / file | `WriteData` |          | X          | X       |          | Requires `writeKey()` or `readWriteKey()` |
| Application / file | `GetValue` | X        | X          | X       |          | Requires `readKey()`, `writeKey()`, or `readWriteKey()` |
| Application / file | `Credit` |          |            | X       |          | Requires `readWriteKey()` |
| Application / file | `Debit` | X        | X          | X       |          | Requires `readKey()`, `writeKey()`, or `readWriteKey()` |
| Application / file | `LimitedCredit` |          | X          | X       |          | Requires `writeKey()` or `readWriteKey()` and the file feature must be enabled |
| Application / file | `ReadRecords` | X        |            | X       |          | Requires `readKey()` or `readWriteKey()` |
| Application / file | `WriteRecord` |          | X          | X       |          | Requires `writeKey()` or `readWriteKey()` |
| Application / file | `ClearRecordFile` |          |            | X       |          | Requires `readWriteKey()` |
| Application / file | `CommitTransaction` |          |            |         |          | Application selected; no file-key gate |
| Application / file | `AbortTransaction` |          |            |         |          | Application selected; no file-key gate |
| ISO 7816-4 | `SELECT FILE` |          |            |         |          | Public |
| ISO 7816-4 | `READ BINARY` | X        |            | X       |          | Requires `readKey()` or `readWriteKey()` |
| ISO 7816-4 | `UPDATE BINARY` |          | X          | X       |          | Requires `writeKey()` or `readWriteKey()` |
| ISO 7816-4 | `READ RECORDS` |          |            |         |          | Stubbed, currently returns `0x6F00` |
| ISO 7816-4 | `UPDATE RECORD` |          |            |         |          | Stubbed, currently returns `0x6F00` |
| ISO 7816-4 | `APPEND RECORD` |          |            |         |          | Stubbed, currently returns `0x6F00` |
| ISO 7816-4 | `GET CHALLENGE` |          |            |         |          | Part of ISO authentication flow |
| ISO 7816-4 | `EXTERNAL AUTHENTICATE` |          |            |         |          | Part of ISO authentication flow |
| ISO 7816-4 | `INTERNAL AUTHENTICATE` |          |            |         |          | Part of ISO authentication flow |

---

## 2. JSON Card Image Format

The format used by HCE Laboratory as input to `Desfire(const std::string &tag)`.

### 2.1 Top-level structure

```json
{
  "type": "desfire",
  "version": 1,
  "discovery": { ... },
  "payload": {
    "info": { ... },
    "directory": [ ... ]
  }
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `type` | yes | Must be `"desfire"` |
| `version` | yes | Format version, currently `1` |
| `discovery` | yes | ISO 14443-4 layer parameters |
| `payload.info` | yes | Version info returned by `GetVersion` |
| `payload.directory` | yes | Array of applications. First entry must be AID `0` (PICC master) |

### 2.2 Complete annotated example

```json
{
  "type": "desfire",
  "version": 1,

  "discovery": {
    "UID":  "04A1B2C3D4E5F6",
    "ATQA": 17411,
    "SAK":  32,
    "ATS": {
      "TB1": 129,
      "TC1": 2,
      "HB":  "80"
    }
  },

  "payload": {
    "info": {
      "hw": { "vendor": 4, "type": 1, "subtype": 1, "version": 256, "storage": 24, "protocol": 5 },
      "sw": { "vendor": 4, "type": 1, "subtype": 1, "version": 260, "storage": 24, "protocol": 5 },
      "tr": { "batch": 690199491770, "week": 8, "year": 22 }
    },

    "directory": [

      {
        "aid": 0,
        "isoId": 16128,
        "isoName": "D2760000850100",
        "keySettings1": 15,
        "keySettings2": 1,
        "keys": [
          { "id": 0, "type": 0, "version": 0, "value": "00000000000000000000000000000000" }
        ]
      },

      {
        "aid": 16220162,
        "isoId": 57616,
        "isoName": "D2760000850101",
        "keySettings1": 9,
        "keySettings2": 130,
        "keys": [
          { "id": 0, "type": 2, "version": 1, "value": "00000000000000000000000000000000" },
          { "id": 1, "type": 2, "version": 0, "value": "AABBCCDDEEFF00112233445566778899" }
        ],
        "files": [

          {
            "id": 1,
            "type": 0,
            "commSettings": 0,
            "accessRights": 61166,
            "isoId": 57603,
            "size": 15,
            "data": "000F20003B00340406E1040FFE0000"
          },

          {
            "id": 2,
            "type": 1,
            "commSettings": 3,
            "accessRights": 9280,
            "size": 256,
            "data": "00000000000000000000000000000000..."
          },

          {
            "id": 3,
            "type": 2,
            "commSettings": 1,
            "accessRights": 4368,
            "lowerLimit": 0,
            "upperLimit": 10000,
            "value": 5000,
            "limitedCredit": 100,
            "features": 1
          },

          {
            "id": 4,
            "type": 3,
            "commSettings": 3,
            "accessRights": 9280,
            "size": 200,
            "recordSize": 20,
            "maxRecords": 10,
            "data": ""
          },

          {
            "id": 5,
            "type": 4,
            "commSettings": 3,
            "accessRights": 9280,
            "size": 100,
            "recordSize": 20,
            "maxRecords": 5,
            "data": ""
          }
        ]
      }
    ]
  }
}
```

### 2.3 Validation rules

- `directory[0].aid` must be `0` (PICC master application).
- When `keySettings2 & 0x20` is set (ISO mode), `isoId` is mandatory for both the application and each file.
- The PICC master application ISO name must be `D2760000850100` (constant `DESFIRE_ISO_MASTERFILE_NAME`).
- `data` field length in hex chars must equal `size × 2` for data/backup files.
- `size` for record files must equal `recordSize × maxRecords`.
- Key `value` length: 32 hex chars (2K3DES / AES), 48 hex chars (3K3DES).
- `keySettings2 & 0x0F` (max keys) must be ≥ the number of entries in `keys`.
- File `id` values must be unique within each application (0–31).

---

## 3. Quick Reference

### Access rights shortcuts

| Pattern | Value | Meaning |
|---------|-------|---------|
| All FREE | `0xEEEE` = 61166 | Public read/write |
| Key0 all | `0x0000` = 0 | Master key controls everything |
| R=FREE, W=Key1, RW=Key1, C=Key0 | `0xE110` = 57616 | Typical NDEF |
| R=Key2, W=Key4, RW=Key2, C=Key0` | `0x2420` = 9248 | Typical app data |

### commSettings values

| Value | Mode | Description |
|-------|------|-------------|
| `0` | PLAIN | No security on data |
| `1` | MACed | Integrity via CMAC |
| `3` | ENCRYPTED | Full encryption (EV0: 3DES CBC, EV1+: AES/3DES CMAC) |

### keySettings2 composition

```
keySettings2 = (cryptoMode << 6) | (isoEnabled ? 0x20 : 0x00) | maximumKeys
```

Examples:
- AES, 2 keys, no ISO: `(2 << 6) | 0 | 2` = `0x82` = 130
- Legacy, 6 keys, with ISO: `(0 << 6) | 0x20 | 6` = `0x26` = 38
- AES, 1 key, no ISO: `(2 << 6) | 0 | 1` = `0x81` = 129
