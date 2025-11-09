# Stats Binary Frame Structures

Binary frame structures for companion radio stats commands. All multi-byte integers use little-endian byte order.

## Command Codes

| Command | Code | Description |
|---------|------|-------------|
| `CMD_GET_STATS_CORE` | 56 | Get core device statistics |
| `CMD_GET_STATS_RADIO` | 57 | Get radio statistics |
| `CMD_GET_STATS_PACKETS` | 58 | Get packet statistics |

## Response Codes

| Response | Code | Description |
|----------|------|-------------|
| `RESP_CODE_STATS_CORE` | 24 | Core stats response |
| `RESP_CODE_STATS_RADIO` | 25 | Radio stats response |
| `RESP_CODE_STATS_PACKETS` | 26 | Packet stats response |

---

## RESP_CODE_STATS_CORE (24)

**Total Frame Size:** 10 bytes

| Offset | Size | Type | Field Name | Description | Range/Notes |
|--------|------|------|------------|-------------|-------------|
| 0 | 1 | uint8_t | response_code | Always `0x18` (24) | - |
| 1 | 2 | uint16_t | battery_mv | Battery voltage in millivolts | 0 - 65,535 |
| 3 | 4 | uint32_t | uptime_secs | Device uptime in seconds | 0 - 4,294,967,295 |
| 7 | 2 | uint16_t | errors | Error flags bitmask | - |
| 9 | 1 | uint8_t | queue_len | Outbound packet queue length | 0 - 255 |

### Example Structure (C/C++)

```c
struct StatsCore {
    uint8_t  response_code;  // 0x18
    uint16_t battery_mv;
    uint32_t uptime_secs;
    uint16_t errors;
    uint8_t  queue_len;
} __attribute__((packed));
```

---

## RESP_CODE_STATS_RADIO (25)

**Total Frame Size:** 13 bytes

| Offset | Size | Type | Field Name | Description | Range/Notes |
|--------|------|------|------------|-------------|-------------|
| 0 | 1 | uint8_t | response_code | Always `0x19` (25) | - |
| 1 | 2 | int16_t | noise_floor | Radio noise floor in dBm | -140 to +10 |
| 3 | 1 | int8_t | last_rssi | Last received signal strength in dBm | -128 to +127 |
| 4 | 1 | int8_t | last_snr | SNR scaled by 4 | Divide by 4.0 for dB |
| 5 | 4 | uint32_t | tx_air_secs | Cumulative transmit airtime in seconds | 0 - 4,294,967,295 |
| 9 | 4 | uint32_t | rx_air_secs | Cumulative receive airtime in seconds | 0 - 4,294,967,295 |

### Example Structure (C/C++)

```c
struct StatsRadio {
    uint8_t  response_code;  // 0x19
    int16_t  noise_floor;
    int8_t   last_rssi;
    int8_t   last_snr;       // Divide by 4.0 to get actual SNR in dB
    uint32_t tx_air_secs;
    uint32_t rx_air_secs;
} __attribute__((packed));
```

---

## RESP_CODE_STATS_PACKETS (26)

**Total Frame Size:** 25 bytes

| Offset | Size | Type | Field Name | Description | Range/Notes |
|--------|------|------|------------|-------------|-------------|
| 0 | 1 | uint8_t | response_code | Always `0x1A` (26) | - |
| 1 | 4 | uint32_t | recv | Total packets received | 0 - 4,294,967,295 |
| 5 | 4 | uint32_t | sent | Total packets sent | 0 - 4,294,967,295 |
| 9 | 4 | uint32_t | flood_tx | Packets sent via flood routing | 0 - 4,294,967,295 |
| 13 | 4 | uint32_t | direct_tx | Packets sent via direct routing | 0 - 4,294,967,295 |
| 17 | 4 | uint32_t | flood_rx | Packets received via flood routing | 0 - 4,294,967,295 |
| 21 | 4 | uint32_t | direct_rx | Packets received via direct routing | 0 - 4,294,967,295 |

### Notes

- Counters are cumulative from boot and may wrap.
- `recv = flood_rx + direct_rx`
- `sent = flood_tx + direct_tx`

### Example Structure (C/C++)

```c
struct StatsPackets {
    uint8_t  response_code;  // 0x1A
    uint32_t recv;
    uint32_t sent;
    uint32_t flood_tx;
    uint32_t direct_tx;
    uint32_t flood_rx;
    uint32_t direct_rx;
} __attribute__((packed));
```

---

## Usage Example (Python)

```python
import struct

def parse_stats_core(frame):
    """Parse RESP_CODE_STATS_CORE frame (10 bytes)"""
    response_code, battery_mv, uptime_secs, errors, queue_len = \
        struct.unpack('<B H I H B', frame)
    return {
        'battery_mv': battery_mv,
        'uptime_secs': uptime_secs,
        'errors': errors,
        'queue_len': queue_len
    }

def parse_stats_radio(frame):
    """Parse RESP_CODE_STATS_RADIO frame (13 bytes)"""
    response_code, noise_floor, last_rssi, last_snr, tx_air_secs, rx_air_secs = \
        struct.unpack('<B h b b I I', frame)
    return {
        'noise_floor': noise_floor,
        'last_rssi': last_rssi,
        'last_snr': last_snr / 4.0,  # Unscale SNR
        'tx_air_secs': tx_air_secs,
        'rx_air_secs': rx_air_secs
    }

def parse_stats_packets(frame):
    """Parse RESP_CODE_STATS_PACKETS frame (25 bytes)"""
    response_code, recv, sent, flood_tx, direct_tx, flood_rx, direct_rx = \
        struct.unpack('<B I I I I I I', frame)
    return {
        'recv': recv,
        'sent': sent,
        'flood_tx': flood_tx,
        'direct_tx': direct_tx,
        'flood_rx': flood_rx,
        'direct_rx': direct_rx
    }
```

---

## Usage Example (JavaScript/TypeScript)

```typescript
interface StatsCore {
    battery_mv: number;
    uptime_secs: number;
    errors: number;
    queue_len: number;
}

interface StatsRadio {
    noise_floor: number;
    last_rssi: number;
    last_snr: number;
    tx_air_secs: number;
    rx_air_secs: number;
}

interface StatsPackets {
    recv: number;
    sent: number;
    flood_tx: number;
    direct_tx: number;
    flood_rx: number;
    direct_rx: number;
}

function parseStatsCore(buffer: ArrayBuffer): StatsCore {
    const view = new DataView(buffer);
    return {
        battery_mv: view.getUint16(1, true),
        uptime_secs: view.getUint32(3, true),
        errors: view.getUint16(7, true),
        queue_len: view.getUint8(9)
    };
}

function parseStatsRadio(buffer: ArrayBuffer): StatsRadio {
    const view = new DataView(buffer);
    return {
        noise_floor: view.getInt16(1, true),
        last_rssi: view.getInt8(3),
        last_snr: view.getInt8(4) / 4.0,  // Unscale SNR
        tx_air_secs: view.getUint32(5, true),
        rx_air_secs: view.getUint32(9, true)
    };
}

function parseStatsPackets(buffer: ArrayBuffer): StatsPackets {
    const view = new DataView(buffer);
    return {
        recv: view.getUint32(1, true),
        sent: view.getUint32(5, true),
        flood_tx: view.getUint32(9, true),
        direct_tx: view.getUint32(13, true),
        flood_rx: view.getUint32(17, true),
        direct_rx: view.getUint32(21, true)
    };
}
```

---

## Field Size Considerations

- Packet counters (uint32_t): May wrap after extended high-traffic operation.
- Time fields (uint32_t): Max ~136 years.
- SNR (int8_t, scaled by 4): Range -32 to +31.75 dB, 0.25 dB precision.

