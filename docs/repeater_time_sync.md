# Repeater Time Synchronization

Repeaters without GPS use `VolatileRTCClock` (ESP32 system clock in RAM) and lose time on
power-off. Correct time is required for replay-attack protection and message timestamping.

## How It Works

Every incoming advertisement packet carries a UNIX timestamp signed with the sender's
Ed25519 private key. The mesh layer verifies the signature before calling `onAdvertRecv`,
so a forged timestamp is rejected cryptographically before it can reach the sync logic.

The repeater collects timestamps from verified advertisements into a 10-entry ring buffer
and uses a quorum/median algorithm to filter out nodes with incorrect clocks.

## Algorithm

### Fast initial sync (clock unset)

Triggered when current clock < 2020-01-01 (i.e. clock was never set).

- Looks at the last **5** samples in the buffer
- Requires **3** of them to fall within a ±60 s window of each other
- Sets the clock to the median of the cluster

Expected time to first sync with 10 neighbors advertising every 12 h: **~3–4 hours**.

### Drift correction (clock already set)

Runs continuously after initial sync.

- Looks at the last **10** samples in the buffer
- Requires **7** of them to fall within a ±60 s window of each other
- Adjusts the clock only if the median differs from current time by more than **120 s**
  and less than **3600 s** (prevents large jumps from a single bad actor)

## Security Properties

| Threat | Protection |
|---|---|
| Forged timestamp from unknown node | Ed25519 signature check in mesh layer |
| Valid node with wrong clock (e.g. RTC reset) | Quorum — single outlier cannot reach threshold |
| Far-future timestamp attack | `MAX_VALID_TS` = 2050-01-01 bound + quorum |
| Clock pushed backwards | `MAX_JUMP` = 3600 s limit per correction |

## Hardware RTC (optional)

If an I2C RTC module is connected, `AutoDiscoverRTCClock` will use it instead of the
volatile fallback, providing persistence across reboots. Supported chips:

| Chip | I2C address |
|---|---|
| DS3231 | 0x68 |
| RV3028 | 0x52 |
| PCF8563 | 0x51 |
| RX8130CE | 0x32 |

On **ESP32 generic-e22** boards the I2C bus is on **GPIO 21 (SDA)** and **GPIO 22 (SCL)**.

When a hardware RTC is present the quorum sync still runs; `setCurrentTime` will persist
the corrected time to the chip.

## CLI Commands

### `timesync`
Shows time sync status remotely via companion radio:

```
TimeSync: 3 syncs
Last: 14:22 2025-01-15 UTC (4320s ago)
Adj: +145s
Buf: 8/10
Clock: 15:54:01 2025-01-15 UTC
```

| Field | Description |
|---|---|
| syncs | Total number of clock adjustments since boot |
| Last | Time and date of the last adjustment |
| Adj | Seconds added/subtracted on the last adjustment |
| Buf | How many advert timestamps are in the buffer |
| Clock | Current repeater clock |

Before first sync:
```
TimeSync: no sync yet
Buf: 2/10 samples
Clock: 08:50:51 2024-05-15 UTC
```

### `clock`
Shows current time only (existing command).

### `clock sync`
Forces immediate sync from the sender's timestamp (existing command).

## Constants

| Constant | Value | Description |
|---|---|---|
| `MIN_VALID_TS` | 1577836800 | 2020-01-01 UTC — minimum plausible timestamp |
| `MAX_VALID_TS` | 2524608000 | 2050-01-01 UTC — maximum plausible timestamp |
| `CLUSTER_WINDOW` | 60 s | Window for quorum clustering |
| `DRIFT_THRESHOLD` | 120 s | Minimum drift to trigger correction |
| `MAX_JUMP` | 3600 s | Maximum allowed single clock adjustment |
| Fast quorum | 3 / 5 | Samples required for initial sync |
| Full quorum | 7 / 10 | Samples required for drift correction |
