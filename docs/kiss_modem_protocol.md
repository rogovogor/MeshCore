# MeshCore KISS Modem Protocol

Serial protocol for the KISS modem firmware. Enables sending/receiving MeshCore packets over LoRa and cryptographic operations using the modem's identity.

## Serial Configuration

115200 baud, 8N1, no flow control.

## Frame Format

Standard KISS framing with byte stuffing.

| Byte | Name | Description |
|------|------|-------------|
| `0xC0` | FEND | Frame delimiter |
| `0xDB` | FESC | Escape character |
| `0xDC` | TFEND | Escaped FEND (FESC + TFEND = 0xC0) |
| `0xDD` | TFESC | Escaped FESC (FESC + TFESC = 0xDB) |

```
┌──────┬─────────┬──────────────┬──────┐
│ FEND │ Command │ Data (escaped)│ FEND │
│ 0xC0 │ 1 byte  │ 0-510 bytes  │ 0xC0 │
└──────┴─────────┴──────────────┴──────┘
```

Maximum unescaped frame size: 512 bytes.

## Commands

### Request Commands (Host → Modem)

| Command | Value | Data |
|---------|-------|------|
| `CMD_DATA` | `0x00` | Packet (2-255 bytes) |
| `CMD_GET_IDENTITY` | `0x01` | - |
| `CMD_GET_RANDOM` | `0x02` | Length (1 byte, 1-64) |
| `CMD_VERIFY_SIGNATURE` | `0x03` | PubKey (32) + Signature (64) + Data |
| `CMD_SIGN_DATA` | `0x04` | Data to sign |
| `CMD_ENCRYPT_DATA` | `0x05` | Key (32) + Plaintext |
| `CMD_DECRYPT_DATA` | `0x06` | Key (32) + MAC (2) + Ciphertext |
| `CMD_KEY_EXCHANGE` | `0x07` | Remote PubKey (32) |
| `CMD_HASH` | `0x08` | Data to hash |
| `CMD_SET_RADIO` | `0x09` | Freq (4) + BW (4) + SF (1) + CR (1) |
| `CMD_SET_TX_POWER` | `0x0A` | Power dBm (1) |
| `CMD_SET_SYNC_WORD` | `0x0B` | Sync word (1) |
| `CMD_GET_RADIO` | `0x0C` | - |
| `CMD_GET_TX_POWER` | `0x0D` | - |
| `CMD_GET_SYNC_WORD` | `0x0E` | - |
| `CMD_GET_VERSION` | `0x0F` | - |

### Response Commands (Modem → Host)

| Command | Value | Data |
|---------|-------|------|
| `CMD_DATA` | `0x00` | SNR (1) + RSSI (1) + Packet |
| `RESP_IDENTITY` | `0x11` | PubKey (32) |
| `RESP_RANDOM` | `0x12` | Random bytes (1-64) |
| `RESP_VERIFY` | `0x13` | Result (1): 0x00=invalid, 0x01=valid |
| `RESP_SIGNATURE` | `0x14` | Signature (64) |
| `RESP_ENCRYPTED` | `0x15` | MAC (2) + Ciphertext |
| `RESP_DECRYPTED` | `0x16` | Plaintext |
| `RESP_SHARED_SECRET` | `0x17` | Shared secret (32) |
| `RESP_HASH` | `0x18` | SHA-256 hash (32) |
| `RESP_OK` | `0x19` | - |
| `RESP_RADIO` | `0x1A` | Freq (4) + BW (4) + SF (1) + CR (1) |
| `RESP_TX_POWER` | `0x1B` | Power dBm (1) |
| `RESP_SYNC_WORD` | `0x1C` | Sync word (1) |
| `RESP_VERSION` | `0x1D` | Version (1) + Reserved (1) |
| `RESP_ERROR` | `0x1E` | Error code (1) |
| `RESP_TX_DONE` | `0x1F` | Result (1): 0x00=failed, 0x01=success |

## Error Codes

| Code | Value | Description |
|------|-------|-------------|
| `ERR_INVALID_LENGTH` | `0x01` | Request data too short |
| `ERR_INVALID_PARAM` | `0x02` | Invalid parameter value |
| `ERR_NO_CALLBACK` | `0x03` | Radio callback not set |
| `ERR_MAC_FAILED` | `0x04` | MAC verification failed |
| `ERR_UNKNOWN_CMD` | `0x05` | Unknown command |
| `ERR_ENCRYPT_FAILED` | `0x06` | Encryption failed |

## Data Formats

### Radio Parameters (CMD_SET_RADIO / RESP_RADIO)

All values little-endian.

| Field | Size | Description |
|-------|------|-------------|
| Frequency | 4 bytes | Hz (e.g., 869618000) |
| Bandwidth | 4 bytes | Hz (e.g., 62500) |
| SF | 1 byte | Spreading factor (5-12) |
| CR | 1 byte | Coding rate (5-8) |

### Received Packet (CMD_DATA response)

| Field | Size | Description |
|-------|------|-------------|
| SNR | 1 byte | Signal-to-noise × 4, signed |
| RSSI | 1 byte | Signal strength dBm, signed |
| Packet | variable | Raw MeshCore packet |

## Notes

- Modem generates identity on first boot (stored in flash)
- SNR values multiplied by 4 for 0.25 dB precision
- Wait for `RESP_TX_DONE` before sending next packet
- See [packet_structure.md](./packet_structure.md) for packet format
