# Repeater Time Sync from Advert Timestamps

## Problem

Repeaters without GPS use `VolatileRTCClock` or `ESP32RTCClock` (hardcoded start date).
Time is lost or wrong after reboot. Correct time is required for replay-attack protection
and message timestamping.

## Solution

Collect timestamps from incoming verified advertisement packets into a ring buffer.
Use a quorum/median algorithm to filter out nodes with wrong clocks, then adjust
the local clock when enough sources agree.

## Security

Every advert timestamp is covered by an Ed25519 signature over
`pub_key(32) || timestamp(4) || app_data`. The mesh layer verifies the signature
**before** calling `onAdvertRecv` — forged timestamps are rejected cryptographically
and never reach the sync logic.

Remaining threats:

| Threat | Protection |
|---|---|
| Legitimate node with wrong clock | Quorum — single outlier cannot reach threshold |
| Far-future timestamp | `MAX_VALID_TS = 2050-01-01` bound + quorum |
| Clock pushed backwards | `MAX_JUMP = 3600 s` limit per correction |

## Sync Modes

### Fast initial sync
Active when: `!clock.isTimeReliable() && sync_count == 0`
(no hardware RTC chip found, never synced via adverts)

- Buffer: last 5 samples
- Quorum: 3/5 within ±60 s window
- Action: apply median immediately (no drift threshold)

### Drift correction
Active when: clock is already trusted (hardware RTC present or previously synced)

- Buffer: last 10 samples
- Quorum: 7/10 within ±60 s window
- Action: apply only if `|median − current| > 120 s && < 3600 s`

## Clock Trust (`isTimeReliable()`)

`RTCClock::isTimeReliable()` — new virtual method, `false` by default.

| Clock source | `isTimeReliable()` |
|---|---|
| `VolatileRTCClock` | false |
| `ESP32RTCClock` (hardcoded start) | false |
| `AutoDiscoverRTCClock` with HW chip | **true** (DS3231 / RV3028 / PCF8563 / RX8130CE) |

## Cluster Algorithm

Timestamps are sorted. A sliding window of `CLUSTER_WINDOW` seconds is swept across the sorted array to find the densest cluster (most samples within the window). This correctly handles mixed networks where GPS-synced nodes (accurate) are outnumbered by nodes with wrong hardcoded clocks — the GPS cluster is detected even if smaller, as long as it reaches quorum size.

The median of the **winning cluster** (not all samples) is used as the sync target.

## Apply Logic

When quorum is reached and adjustment is needed:

```
if GPS present && GPS has fix:
    gps->syncTime()          // GPS re-syncs clock on next valid NMEA sentence
else:
    clock->setCurrentTime(median)   // direct adjustment
```

This ensures GPS remains the authoritative time source when available. If GPS has no
fix yet, the mesh-quorum time is used as a fallback.

## Changed Files

| File | Change |
|---|---|
| `src/MeshCore.h` | Added `virtual bool isTimeReliable() const` to `RTCClock` |
| `src/helpers/AutoDiscoverRTCClock.h/.cpp` | Override `isTimeReliable()` — returns true if HW chip found |
| `examples/simple_repeater/MyMesh.h` | Added ring buffer fields and sync counters |
| `examples/simple_repeater/MyMesh.cpp` | `tryTimeSyncFromBuf()`, updated `onAdvertRecv()`, `timesync` CLI command |
| `docs/repeater_time_sync.md` | User-facing documentation |

## CLI: `timesync`

Available remotely via companion radio. Example output before first sync:

```
TimeSync: no sync yet
Adverts: 3 rx / 3 valid
Buf: 3/10
Clock: 08:50:51 2024-05-15 UTC
```

After sync:

```
TimeSync: 2 syncs
Last: 14:22 2025-01-15 UTC (4320s ago)
Adj: +145s
Adverts: 18 rx / 18 valid
Buf: 10/10
Clock: 15:54:01 2025-01-15 UTC
```

## Constants

| Constant | Value | Description |
|---|---|---|
| `MIN_VALID_TS` | 1577836800 | 2020-01-01 UTC lower bound |
| `MAX_VALID_TS` | 2524608000 | 2050-01-01 UTC upper bound |
| `CLUSTER_WINDOW` | 60 s | Clustering window for quorum |
| `DRIFT_THRESHOLD` | 120 s | Min drift to trigger correction |
| `MAX_JUMP` | 3600 s | Max single clock adjustment |
| Fast quorum | 3 / 5 | For initial sync |
| Full quorum | 7 / 10 | For drift correction |
