#include <helpers/mcotxt/MCOtxtTransport.h>

#ifdef WITH_MCOTXT

#include <string.h>

#include <helpers/Base91.h>
#include <helpers/UTF8Helpers.h>
#include <helpers/mcotxt/MCOtxtCodec.h>
#include <helpers/mcotxt/MCOtxtModels.h>

namespace mcotxt {
namespace {

const uint8_t kKnownFlags = kFlagReply | kFlagSenderName | kFlagTimestampInherited;

// Longest `mct:` payload a MeshCore text can hold once decoded from Base91.
const size_t kMaxTextPayloadBytes = 192;

struct ByteReader {
  const uint8_t* data;
  size_t length;
  size_t position;

  bool done() const { return position == length; }

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

MessageStatus statusForDecode(DecodeStatus status) {
  switch (status) {
    case DecodeStatus::Ok: return MessageStatus::Ok;
    case DecodeStatus::ModelUnavailable:
    case DecodeStatus::UnsupportedGeneration: return MessageStatus::ModelUnavailable;
    case DecodeStatus::OutputTooSmall: return MessageStatus::TooLong;
    default: return MessageStatus::Malformed;
  }
}

size_t byteLengthForBits(uint32_t bit_length) {
  return (size_t)(bit_length / 8U) + (bit_length % 8U != 0U ? 1U : 0U);
}

bool hasNameAndTrailingByte(const ByteReader& reader, uint32_t name_length) {
  if (reader.position > reader.length) return false;
  const size_t remaining = reader.length - reader.position;
  return remaining != 0 && (size_t)name_length <= remaining - 1;
}

// One string field: a mode byte, then a frame or a counted UTF-8 run.
MessageStatus readString(ByteReader& reader, char* out, size_t out_capacity, size_t& out_length) {
  out_length = 0;
  uint8_t mode;
  if (!reader.readByte(mode)) return MessageStatus::Malformed;
  if (mode == kStringModeMCOtxt) {
    uint32_t bit_length;
    if (!reader.readVarUint(bit_length)) return MessageStatus::Malformed;
    const size_t byte_length = byteLengthForBits(bit_length);
    if (reader.length - reader.position < byte_length) return MessageStatus::Malformed;
    const DecodeResult decoded = decodeStream(reader.data + reader.position, byte_length,
                                              bit_length, out, out_capacity, out_length);
    reader.position += byte_length;
    return statusForDecode(decoded.status);
  }
  if (mode == kStringModeUtf8) {
    uint32_t byte_length;
    if (!reader.readVarUint(byte_length)) return MessageStatus::Malformed;
    if (reader.length - reader.position < byte_length) return MessageStatus::Malformed;
    const uint8_t* bytes = reader.data + reader.position;
    size_t position = 0;
    uint32_t codepoint;
    while (position < byte_length) {
      if (!utf8Decode(bytes, byte_length, position, codepoint)) return MessageStatus::Malformed;
    }
    if (byte_length + 1 > out_capacity) {
      // Keep the longest prefix of whole characters that fits.
      size_t prefix = 0;
      position = 0;
      while (position < byte_length && utf8Decode(bytes, byte_length, position, codepoint) &&
             position + 1 <= out_capacity) {
        prefix = position;
      }
      memcpy(out, bytes, prefix);
      out[prefix] = '\0';
      out_length = prefix;
      reader.position += byte_length;
      return MessageStatus::TooLong;
    }
    memcpy(out, reader.data + reader.position, byte_length);
    out[byte_length] = '\0';
    out_length = byte_length;
    reader.position += byte_length;
    return MessageStatus::Ok;
  }
  return MessageStatus::Malformed;
}

// A name is decoded into a scratch buffer and cut to the field on a UTF-8
// boundary: a long name must not fail the message.
MessageStatus readName(ByteReader& reader, char* name, size_t name_capacity) {
  char scratch[96];
  size_t length;
  const MessageStatus status = readString(reader, scratch, sizeof(scratch), length);
  if (status != MessageStatus::Ok) return status;
  const size_t keep = mesh::validUtf8PrefixLength(scratch, name_capacity - 1);
  memcpy(name, scratch, keep);
  name[keep] = '\0';
  return MessageStatus::Ok;
}

MessageStatus decodeContainer(const uint8_t* body, size_t body_length, char* out,
                              size_t out_capacity, DecodedMessage& message) {
  ByteReader reader = { body, body_length, 0 };
  uint8_t flags;
  if (!reader.readByte(flags)) return MessageStatus::Malformed;
  if ((flags & ~kKnownFlags) != 0) return MessageStatus::Malformed;

  if ((flags & kFlagTimestampInherited) == 0) {
    if (!reader.readUint32LE(message.timestamp)) return MessageStatus::Malformed;
    message.has_timestamp = true;
  }
  if ((flags & kFlagSenderName) != 0) {
    const MessageStatus status = readName(reader, message.sender, sizeof(message.sender));
    if (status != MessageStatus::Ok) return status;
    message.has_sender = true;
  }
  if ((flags & kFlagReply) != 0) {
    const MessageStatus status = readName(reader, message.reply_author, sizeof(message.reply_author));
    if (status != MessageStatus::Ok) return status;
    if (!reader.readUint32LE(message.reply_timestamp)) return MessageStatus::Malformed;
    message.has_reply = true;
  }
  size_t text_length;
  const MessageStatus status = readString(reader, out, out_capacity, text_length);
  if (status != MessageStatus::Ok) return status;
  if (!reader.done()) return MessageStatus::Malformed;
  return MessageStatus::Ok;
}

// subtypeVersion, then the container. NotMCOtxt for another subtype.
MessageStatus decodeSubtypedBody(const uint8_t* data, size_t length, char* out,
                                 size_t out_capacity, DecodedMessage& message) {
  if (length == 0) return MessageStatus::Malformed;
  const uint8_t subtype_version = data[0];
  if ((subtype_version >> 4) != kSubtypeId) return MessageStatus::NotMCOtxt;
  message.declared_version = (uint8_t)(subtype_version & 0x0F);
  if (message.declared_version != kWireVersion) return MessageStatus::UnsupportedVersion;
  return decodeContainer(data + 1, length - 1, out, out_capacity, message);
}

const char* skipLeadingWhitespace(const char* text) {
  while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
  return text;
}

bool startsWithReplyMention(const char* text, const char* author) {
  if (text == nullptr || author == nullptr || author[0] == '\0') return false;
  const size_t author_length = strlen(author);
  const size_t mention_length = author_length + 3;  // "@[" + name + "]"
  if (strlen(text) < mention_length) return false;
  return text[0] == '@' && text[1] == '[' &&
         strncmp(text + 2, author, author_length) == 0 &&
         text[author_length + 2] == ']';
}

}  // namespace

char* scratch() {
  static char buffer[kScratchBytes];
  return buffer;
}

DecodedMessage::DecodedMessage()
    : status(MessageStatus::NotMCOtxt), declared_version(0), has_timestamp(false),
      timestamp(0), has_sender(false), has_reply(false), reply_timestamp(0) {
  sender[0] = '\0';
  reply_author[0] = '\0';
}

bool isBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length) {
  if (data_type != kChannelAppDataType || data == nullptr) return false;
  ByteReader reader = { data, data_length, 0 };
  uint32_t name_length;
  if (!reader.readVarUint(name_length)) return false;
  if (!hasNameAndTrailingByte(reader, name_length)) return false;
  return (data[reader.position + name_length] >> 4) == kSubtypeId;
}

bool isTextPayload(const char* text) {
  if (text == nullptr) return false;
  text = skipLeadingWhitespace(text);
  const size_t prefix_length = sizeof(kTextPrefix) - 1;
  return strncmp(text, kTextPrefix, prefix_length) == 0 && text[prefix_length] != '\0';
}

MessageStatus decodeTextPayload(const char* text, char* out, size_t out_capacity,
                                DecodedMessage& message) {
  message = DecodedMessage();
  if (out == nullptr || out_capacity == 0) return message.status = MessageStatus::TooLong;
  out[0] = '\0';
  if (!isTextPayload(text)) return message.status = MessageStatus::NotMCOtxt;
  text = skipLeadingWhitespace(text) + (sizeof(kTextPrefix) - 1);

  uint8_t payload[kMaxTextPayloadBytes];
  size_t payload_length;
  if (!mesh::base91::decode(text, strlen(text), payload, sizeof(payload), payload_length)) {
    return message.status = MessageStatus::Malformed;
  }
  return message.status = decodeSubtypedBody(payload, payload_length, out, out_capacity, message);
}

bool ensureReplyMentionPrefix(const DecodedMessage& message, char* text,
                              size_t text_capacity, bool& truncated) {
  truncated = false;
  if (text == nullptr || text_capacity == 0) return false;
  if (!message.has_reply || message.reply_author[0] == '\0') return true;
  if (startsWithReplyMention(text, message.reply_author)) return true;

  const size_t author_length = strlen(message.reply_author);
  const size_t mention_length = author_length + 3;  // "@[" + name + "]"
  const size_t text_length = strlen(text);
  const size_t separator_length = text_length == 0 ? 0 : 1;
  if (mention_length + separator_length + 1 > text_capacity) return false;

  size_t keep = text_length;
  if (mention_length + separator_length + text_length + 1 > text_capacity) {
    const size_t available = text_capacity - mention_length - separator_length - 1;
    keep = mesh::validUtf8PrefixLength(text, available);
    truncated = keep < text_length;
  }

  memmove(text + mention_length + separator_length, text, keep);
  text[0] = '@';
  text[1] = '[';
  memcpy(text + 2, message.reply_author, author_length);
  text[author_length + 2] = ']';
  if (separator_length != 0) text[mention_length] = ' ';
  text[mention_length + separator_length + keep] = '\0';
  return true;
}

MessageStatus decodeBinaryEnvelope(uint16_t data_type, const uint8_t* data, size_t data_length,
                                   char* out, size_t out_capacity, DecodedMessage& message) {
  message = DecodedMessage();
  if (out == nullptr || out_capacity == 0) return message.status = MessageStatus::TooLong;
  out[0] = '\0';
  if (data_type != kChannelAppDataType || data == nullptr) {
    return message.status = MessageStatus::NotMCOtxt;
  }

  // senderNameLength(varuint) | senderName | subtypeVersion | body
  ByteReader reader = { data, data_length, 0 };
  uint32_t name_length;
  if (!reader.readVarUint(name_length)) return message.status = MessageStatus::NotMCOtxt;
  if (!hasNameAndTrailingByte(reader, name_length)) {
    return message.status = MessageStatus::NotMCOtxt;
  }
  const uint8_t* name = data + reader.position;
  reader.position += name_length;
  const MessageStatus status = decodeSubtypedBody(data + reader.position,
                                                  data_length - reader.position, out,
                                                  out_capacity, message);
  if ((status == MessageStatus::Ok || status == MessageStatus::TooLong) && !message.has_sender &&
      name_length > 0) {
    const size_t keep = mesh::validUtf8PrefixLength((const char*)name,
                                                    name_length < sizeof(message.sender) - 1
                                                        ? name_length
                                                        : sizeof(message.sender) - 1);
    memcpy(message.sender, name, keep);
    message.sender[keep] = '\0';
    message.has_sender = keep > 0;
  }
  return message.status = status;
}

}  // namespace mcotxt

#endif  // WITH_MCOTXT
