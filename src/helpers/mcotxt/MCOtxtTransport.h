#pragma once

#include <helpers/OptionalFeatureFlags.h>

#ifdef WITH_MCOTXT

#include <stddef.h>
#include <stdint.h>

// The layers above the MCOtxt stream: the frame (varuint bit count + bytes),
// the application container (flags, timestamp, sender name, reply anchor,
// text) and the two transports MeshCore Open Advanced uses for chat, the
// `mct:` Base91 text and the 0x0120 GROUP_DATA envelope with subtype 0x03.
namespace mcotxt {

static const uint16_t kChannelAppDataType = 0x0120;
static const uint8_t kSubtypeId = 0x03;
static const uint8_t kWireVersion = 0x01;
static const uint8_t kSubtypeVersion = (uint8_t)((kSubtypeId << 4) | kWireVersion);
static const char kTextPrefix[] = "mct:";
static const size_t kMaxNameBytes = 32;   // including the terminator

// The container: flags, then a mode byte in front of every string.
static const uint8_t kFlagReply = 0x01;
static const uint8_t kFlagSenderName = 0x02;
static const uint8_t kFlagTimestampInherited = 0x04;
static const uint8_t kStringModeMCOtxt = 0x00;
static const uint8_t kStringModeUtf8 = 0x01;

// One decode scratch for everything that decodes MCOtxt on the loop task: the
// display and the conversion for an app without MCOtxt. Sized for the longest
// text a full packet can hold; -D MCOTXT_SCRATCH_BYTES=... resizes it.
#ifndef MCOTXT_SCRATCH_BYTES
#define MCOTXT_SCRATCH_BYTES 1280
#endif
static const size_t kScratchBytes = MCOTXT_SCRATCH_BYTES;
char* scratch();

enum class MessageStatus : uint8_t {
  Ok,
  NotMCOtxt,            // not this transport at all; nothing was touched
  UnsupportedVersion,   // subtype 0x03 with a revision other than 1
  Malformed,            // container or stream rejected
  ModelUnavailable,     // a language this build has no tables for
  TooLong,              // the text does not fit: the output holds the longest prefix that does
};

struct DecodedMessage {
  MessageStatus status;
  uint8_t declared_version;
  bool has_timestamp;
  uint32_t timestamp;
  bool has_sender;
  char sender[kMaxNameBytes];
  bool has_reply;
  char reply_author[kMaxNameBytes];
  uint32_t reply_timestamp;

  DecodedMessage();
};

// True for text that starts with `mct:` after optional leading whitespace and
// carries something after the prefix.
bool isTextPayload(const char* text);

// True for a GROUP_DATA payload of type 0x0120 whose subtype nibble is 0x3.
// The envelope name is skipped and nothing else is checked, so a message that
// then fails to decode still counts as MCOtxt and is shown with its status.
bool isBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length);

// Decodes a `mct:` text payload into NUL-terminated UTF-8. The decoded name
// fields, when the container carries them, land in [message]; the caller's
// transport supplies the timestamp when the container inherits it.
MessageStatus decodeTextPayload(const char* text, char* out, size_t out_capacity,
                                DecodedMessage& message);

// Decodes a GROUP_DATA 0x0120 envelope of subtype 0x03. Any other data type
// or subtype is NotMCOtxt, so other application data keeps its own handler.
// The sender is the container's when present, else the envelope's.
MessageStatus decodeBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length,
                                   char* out, size_t out_capacity, DecodedMessage& message);

// Compatibility helper for apps that receive decoded MCOtxt instead of the
// original container: preserve the exact-reply anchor as the familiar
// "@[Name]" prefix. The prefix is added only when the decoded text does not
// already start with the same mention. [truncated] is raised if the tail had
// to be shortened to keep the result NUL-terminated.
bool ensureReplyMentionPrefix(const DecodedMessage& message, char* text,
                              size_t text_capacity, bool& truncated);

}  // namespace mcotxt

#endif  // WITH_MCOTXT
