#include <helpers/mcmp/MCMPDetect.h>

#ifdef WITH_MCMP_DETECT

#include <stdio.h>
#include <string.h>

#include <helpers/Base91.h>
#include <helpers/UTF8Helpers.h>

namespace mcmp {
namespace {

const uint8_t kFlagReply = 0x01;
const uint8_t kFlagSigned = 0x02;
const uint8_t kFlagSender = 0x04;
const uint8_t kKnownFlags = kFlagReply | kFlagSigned | kFlagSender;
const size_t kSignatureBytes = 64;

// Longest `mcmp3:` payload a MeshCore text can hold once decoded from Base91.
const size_t kMaxTextPayloadBytes = 192;

struct ByteReader {
  const uint8_t* data;
  size_t length;
  size_t position;

  bool readByte(uint8_t& value) {
    if (position >= length) return false;
    value = data[position++];
    return true;
  }

  bool readUint32LE(uint32_t& value) {
    if (length - position < 4) return false;
    value = (uint32_t)data[position] | ((uint32_t)data[position + 1] << 8) |
            ((uint32_t)data[position + 2] << 16) | ((uint32_t)data[position + 3] << 24);
    position += 4;
    return true;
  }

  // Seven value bits per byte, least significant group first; rejected past
  // five bytes.
  bool readVarUint(uint32_t& value) {
    value = 0;
    uint8_t shift = 0;
    for (uint8_t bytes = 0; bytes < 5; bytes++) {
      uint8_t byte;
      if (!readByte(byte)) return false;
      if (shift == 28 && (byte & 0xF0) != 0) return false;
      value |= (uint32_t)(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0) return true;
      shift = (uint8_t)(shift + 7);
    }
    return false;
  }
};

const char* skipLeadingWhitespace(const char* text) {
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
  return text;
}

// Cuts [length] bytes of a name to the field on a character boundary: a long
// name is not an error.
void copyName(const uint8_t* bytes, size_t length, char* name) {
  char scratch[96];
  const size_t copy = length < sizeof(scratch) - 1 ? length : sizeof(scratch) - 1;
  memcpy(scratch, bytes, copy);
  scratch[copy] = '\0';
  const size_t keep = mesh::validUtf8PrefixLength(scratch, kMaxNameBytes - 1);
  memcpy(name, scratch, keep);
  name[keep] = '\0';
}

bool readName(ByteReader& reader, char* name) {
  uint32_t length;
  if (!reader.readVarUint(length)) return false;
  if (reader.length - reader.position < length) return false;
  copyName(reader.data + reader.position, length, name);
  reader.position += length;
  return true;
}

bool hasNameAndTrailingByte(const ByteReader& reader, uint32_t name_length) {
  if (reader.position > reader.length) return false;
  const size_t remaining = reader.length - reader.position;
  return remaining != 0 && (size_t)name_length <= remaining - 1;
}

// The v3 body: flags, timestamp, then the optional sender, signature and
// reply anchor, then the compressed text, which is only measured.
bool parseContainer(const uint8_t* body, size_t length, Meta& meta) {
  ByteReader reader = { body, length, 0 };
  uint8_t flags;
  if (!reader.readByte(flags)) return false;
  if ((flags & ~kKnownFlags) != 0) return false;
  if (!reader.readUint32LE(meta.timestamp)) return false;
  if (flags & kFlagSender) {
    if (!readName(reader, meta.sender)) return false;
    meta.has_sender = meta.sender[0] != '\0';
  }
  if (flags & kFlagSigned) {
    if (reader.length - reader.position < kSignatureBytes) return false;
    reader.position += kSignatureBytes;
    meta.is_signed = true;
  }
  if (flags & kFlagReply) {
    if (!readName(reader, meta.reply_author)) return false;
    if (!reader.readUint32LE(meta.reply_timestamp)) return false;
    meta.has_reply = true;
  }
  meta.compressed_length = reader.length - reader.position;
  meta.container_ok = true;
  return true;
}

}  // namespace

Meta::Meta()
    : form(Form::None), revision(0), container_ok(false), timestamp(0), is_signed(false),
      has_sender(false), has_reply(false), reply_timestamp(0), compressed_length(0) {
  sender[0] = '\0';
  reply_author[0] = '\0';
}

Form textForm(const char* text) {
  if (text == nullptr) return Form::None;
  text = skipLeadingWhitespace(text);
  if (strncmp(text, "mcmp3:", 6) == 0) return text[6] != '\0' ? Form::TextV3 : Form::None;
  if (strncmp(text, "mcmp2:", 6) == 0) return text[6] != '\0' ? Form::TextV2 : Form::None;
  if (strncmp(text, "mcmp:", 5) == 0) return text[5] != '\0' ? Form::Legacy : Form::None;
  return Form::None;
}

bool isBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length) {
  if (data_type != kChannelAppDataType || data == nullptr) return false;
  ByteReader reader = { data, data_length, 0 };
  uint32_t name_length;
  if (!reader.readVarUint(name_length)) return false;
  if (!hasNameAndTrailingByte(reader, name_length)) return false;
  return (data[reader.position + name_length] >> 4) == kSubtypeId;
}

bool parseText(const char* text, Meta& meta) {
  meta = Meta();
  meta.form = textForm(text);
  if (meta.form == Form::None) return false;

  text = skipLeadingWhitespace(text);
  const size_t prefix_length = meta.form == Form::Legacy ? 5U : 6U;
  uint8_t body[kMaxTextPayloadBytes];
  size_t body_length;
  if (!mesh::base91::decode(text + prefix_length, strlen(text + prefix_length), body,
                            sizeof(body), body_length) || body_length == 0) {
    meta = Meta();
    return false;
  }
  if (meta.form != Form::TextV3) return true;

  Meta parsed = meta;
  if (parseContainer(body, body_length, parsed)) meta = parsed;
  return true;
}

int formatPlaceholder(const Meta& meta, char* out, size_t out_capacity,
                      bool include_reply) {
  if (out == nullptr || out_capacity == 0) return 0;
  const unsigned version = meta.form == Form::Legacy ? 1U
                           : meta.form == Form::TextV2 ? 2U
                           : meta.form == Form::TextV3 ? 3U
                           : meta.form == Form::Binary ? (unsigned)meta.revision + 3U
                           : 0U;
  if (version == 0U) return 0;
  const char* signed_text = meta.is_signed ? " signed" : "";
  const bool with_reply = include_reply && meta.has_reply && meta.reply_author[0] != '\0';
  const int length = with_reply
      ? snprintf(out, out_capacity, "@[%s] <MCMP v%u%s message>",
                 meta.reply_author, version, signed_text)
      : snprintf(out, out_capacity, "<MCMP v%u%s message>", version, signed_text);
  if (length <= 0 || (size_t)length >= out_capacity) {
    out[0] = '\0';
    return 0;
  }
  return length;
}

bool parseBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length,
                         Meta& meta) {
  meta = Meta();
  if (data_type != kChannelAppDataType || data == nullptr) return false;

  // senderNameLength(varuint) | senderName | subtypeVersion | body
  ByteReader reader = { data, data_length, 0 };
  uint32_t name_length;
  if (!reader.readVarUint(name_length)) return false;
  if (!hasNameAndTrailingByte(reader, name_length)) return false;
  const uint8_t* name = data + reader.position;
  reader.position += name_length;
  const uint8_t subtype_version = data[reader.position++];
  if ((subtype_version >> 4) != kSubtypeId) return false;

  meta.form = Form::Binary;
  meta.revision = (uint8_t)(subtype_version & 0x0F);
  Meta parsed = meta;
  if (meta.revision == 0 &&
      parseContainer(data + reader.position, data_length - reader.position, parsed)) {
    meta = parsed;
  }
  if (!meta.has_sender && name_length > 0) {
    copyName(name, name_length, meta.sender);
    meta.has_sender = meta.sender[0] != '\0';
  }
  return true;
}

}  // namespace mcmp

#endif  // WITH_MCMP_DETECT
