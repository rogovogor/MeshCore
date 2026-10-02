#pragma once

#include <helpers/OptionalFeatureFlags.h>

#ifdef WITH_MCMP_DETECT

#include <stddef.h>
#include <stdint.h>

// Recognises MCMP messages from MeshCore Open Advanced and reads the open
// metadata of a v3 container. The node has no room for the compressor's
// model, so the text itself stays compressed, but the sender, the timestamp,
// the reply anchor and the signature flag are plain bytes in front of it.
// Specification: the client's docs/MCMP_V3_PROTOCOL.md.
namespace mcmp {

static const uint16_t kChannelAppDataType = 0x0120;
static const uint8_t kSubtypeId = 0x02;
static const size_t kMaxNameBytes = 32;   // including the terminator

enum class Form : uint8_t {
  None,     // not MCMP
  Legacy,   // `mcmp:`, the first text form, no container
  TextV2,   // `mcmp2:`, compressed bytes only
  TextV3,   // `mcmp3:`, the v3 container
  Binary,   // GROUP_DATA 0x0120 subtype 0x2, the v3 container
};

struct Meta {
  Form form;
  uint8_t revision;            // Binary: the low nibble of subtypeVersion, 0 today
  bool container_ok;           // a v3 container was read to its end
  uint32_t timestamp;          // container_ok: the message's own time
  bool is_signed;              // container_ok: a 64-byte Ed25519 signature follows the header
  bool has_sender;             // the name embedded in the container, else the envelope's
  char sender[kMaxNameBytes];
  bool has_reply;
  char reply_author[kMaxNameBytes];
  uint32_t reply_timestamp;
  size_t compressed_length;    // container_ok: bytes of compressed text behind the header

  Meta();
};

// The kind of a text payload, from its prefix after optional leading
// whitespace; None when it is not MCMP or the prefix carries nothing.
Form textForm(const char* text);

// True for a GROUP_DATA payload of type 0x0120 whose subtype nibble is 0x2.
bool isBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length);

// Reads what a text payload carries. Legacy and v2 have only their form; a v3
// payload is Base91-decoded and its container read, and one that does not
// read leaves [container_ok] false. Returns false when [text] is not MCMP.
bool parseText(const char* text, Meta& meta);

// Reads a binary envelope: the container's embedded name is the sender when
// present, else the envelope's outer name. Returns false when the payload is
// not MCMP.
bool parseBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length,
                         Meta& meta);

// Formats the compatibility text shown to apps/UI that cannot decode MCMP.
// For v3 containers with a reply anchor, [include_reply] prefixes the stub
// with "@[replyAuthor] " so exact replies keep the visible mention convention.
int formatPlaceholder(const Meta& meta, char* out, size_t out_capacity,
                      bool include_reply = true);

}  // namespace mcmp

#endif  // WITH_MCMP_DETECT
