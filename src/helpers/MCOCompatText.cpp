#include <helpers/MCOCompatText.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <helpers/OptionalFeatureFlags.h>
#ifdef WITH_MCOTXT
#include <helpers/mcotxt/MCOtxtTransport.h>
#endif
#ifdef WITH_MCMP_DETECT
#include <helpers/mcmp/MCMPDetect.h>
#endif
#ifdef WITH_MCOIMG_DETECT
#include <helpers/mcoimg_detect/MCOImgDetect.h>
#endif

namespace mco_compat {
namespace {

// A radio text frame is shorter than this. Keeping the token local avoids a
// second large persistent buffer next to the existing MCOtxt scratch buffer.
const size_t kMaxTokenBytes = 256;
const char kBase91Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
    "!#$%&()*+,./:;<=>?@[]^_`{|}~\"";

enum class Kind : uint8_t { None, MCOtxt, MCMP, MCOimg };

struct Match {
  Kind kind;
  size_t prefix_length;
};

bool startsWith(const char* text, const char* prefix) {
  return strncmp(text, prefix, strlen(prefix)) == 0;
}

// True for code points that are letters in the scripts MeshCore messages
// actually use. Digits are handled by the ASCII path; symbols and punctuation
// return false.
bool isLetterCodepoint(uint32_t cp) {
  if (cp >= 0xC0 && cp <= 0xFF) return cp != 0xD7 && cp != 0xF7;  // Latin-1
  if (cp >= 0x100 && cp <= 0x24F) return true;                     // Latin Ext-A/B
  if (cp >= 0x370 && cp <= 0x3FF) return true;                     // Greek
  if (cp >= 0x400 && cp <= 0x4FF) return true;                     // Cyrillic
  if (cp >= 0x500 && cp <= 0x52F) return true;                     // Cyrillic suppl.
  return false;
}

bool isBoundary(const char* input, const char* position) {
  if (position == input) return true;
  const char* prev = position - 1;
  // Walk back over UTF-8 continuation bytes to the lead byte of the character
  // immediately preceding the token position.
  while (prev > input && ((unsigned char)*prev & 0xC0) == 0x80) --prev;
  const unsigned char lead = (unsigned char)*prev;
  if (lead < 0x80) {
    // ASCII: punctuation and whitespace are valid boundaries; letters, digits
    // and '_' are part of the preceding word.
    return !isalnum(lead) && lead != '_';
  }
  // Decode the UTF-8 code point so non-ASCII letters (Cyrillic, Greek, ...)
  // still count as word characters while non-ASCII punctuation/whitespace
  // remains a valid boundary.
  uint32_t cp = 0;
  size_t length = 0;
  if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; length = 2; }
  else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; length = 3; }
  else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; length = 4; }
  else return false;  // stray continuation/lead byte — not a boundary
  for (size_t i = 1; i < length; ++i) {
    const unsigned char byte = (unsigned char)prev[i];
    if ((byte & 0xC0) != 0x80) return false;  // malformed UTF-8
    cp = (cp << 6) | (byte & 0x3F);
  }
  return !isLetterCodepoint(cp);
}

Match matchAt(const char* input, const char* position, const Options& options) {
  if (!isBoundary(input, position)) return { Kind::None, 0 };
#ifdef WITH_MCOTXT
  if (options.mcotxt && startsWith(position, "mct:")) return { Kind::MCOtxt, 4 };
#endif
#ifdef WITH_MCMP_DETECT
  if (options.mcmp) {
    if (startsWith(position, "mcmp3:")) return { Kind::MCMP, 6 };
    if (startsWith(position, "mcmp2:")) return { Kind::MCMP, 6 };
    if (startsWith(position, "mcmp:")) return { Kind::MCMP, 5 };
  }
#endif
#ifdef WITH_MCOIMG_DETECT
  if (options.mcoimg && position[0] == 'i' && position[1] == 'm') {
    if (position[2] == ':') return { Kind::MCOimg, 3 };
    const char* cursor = position + 2;
    unsigned version = 0;
    unsigned digits = 0;
    while (*cursor >= '0' && *cursor <= '9') {
      version = version * 10U + (unsigned)(*cursor++ - '0');
      if (++digits > 2 || version > 15U) return { Kind::None, 0 };
    }
    if (digits != 0 && *cursor == ':')
      return { Kind::MCOimg, (size_t)(cursor + 1 - position) };
  }
#endif
  return { Kind::None, 0 };
}

bool isBase91(char value) {
  return value != '\0' && strchr(kBase91Alphabet, value) != nullptr;
}

bool appendBytes(char* output, size_t capacity, size_t& length,
                 const char* bytes, size_t count) {
  if (length >= capacity) return false;
  const size_t available = capacity - length - 1;
  const size_t copy = count < available ? count : available;
  if (copy != 0) memcpy(output + length, bytes, copy);
  length += copy;
  output[length] = '\0';
  return copy == count;
}

bool copyToken(const char* begin, const char* end, char* token) {
  const size_t length = (size_t)(end - begin);
  if (length == 0 || length >= kMaxTokenBytes) return false;
  memcpy(token, begin, length);
  token[length] = '\0';
  return true;
}

bool outputEndsWithReplyMention(const char* output, size_t length, const char* author) {
  if (output == nullptr || author == nullptr || author[0] == '\0') return false;
  while (length > 0 && output[length - 1] == ' ') length--;
  const size_t author_length = strlen(author);
  const size_t mention_length = author_length + 3;
  if (length < mention_length) return false;
  const size_t start = length - mention_length;
  return output[start] == '@' && output[start + 1] == '[' &&
         strncmp(output + start + 2, author, author_length) == 0 &&
         output[start + author_length + 2] == ']';
}

bool replaceToken(Kind kind, const char* begin, const char* end,
                  char* output, size_t output_capacity, size_t& output_length,
                  bool& truncated) {
  truncated = false;
  char token[kMaxTokenBytes];
  if (!copyToken(begin, end, token)) return false;

#ifdef WITH_MCOTXT
  if (kind == Kind::MCOtxt) {
    if (output_length >= output_capacity) return false;
    mcotxt::DecodedMessage message;
    const mcotxt::MessageStatus status = mcotxt::decodeTextPayload(
        token, output + output_length, output_capacity - output_length, message);
    if (status != mcotxt::MessageStatus::Ok && status != mcotxt::MessageStatus::TooLong)
      return false;
    bool mention_truncated = false;
    if (!mcotxt::ensureReplyMentionPrefix(
            message, output + output_length, output_capacity - output_length,
            mention_truncated)) {
      return false;
    }
    output_length += strlen(output + output_length);
    truncated = status == mcotxt::MessageStatus::TooLong || mention_truncated;
    return true;
  }
#endif

#ifdef WITH_MCMP_DETECT
  if (kind == Kind::MCMP) {
    mcmp::Meta meta;
    if (!mcmp::parseText(token, meta)) return false;
    char placeholder[96];
    const bool already_mentions_reply = meta.has_reply &&
        outputEndsWithReplyMention(output, output_length, meta.reply_author);
    const int length = mcmp::formatPlaceholder(
        meta, placeholder, sizeof(placeholder), !already_mentions_reply);
    if (length <= 0) return false;
    truncated = !appendBytes(output, output_capacity, output_length,
                             placeholder, (size_t)length);
    return true;
  }
#endif

#ifdef WITH_MCOIMG_DETECT
  if (kind == Kind::MCOimg) {
    mcoimg_detect::Meta meta;
    if (!mcoimg_detect::parseText(token, meta)) return false;
    char placeholder[64];
    const int length = meta.has_explicit_version
        ? snprintf(placeholder, sizeof(placeholder), "<MCOimg v%u image>",
                   (unsigned)meta.version)
        : snprintf(placeholder, sizeof(placeholder), "<MCOimg image>");
    if (length <= 0) return false;
    truncated = !appendBytes(output, output_capacity, output_length,
                             placeholder, (size_t)length);
    return true;
  }
#endif
  return false;
}

}  // namespace

Result transform(const char* input, char* output, size_t output_capacity,
                 const Options& options) {
  Result result = { false, false, 0 };
  if (output == nullptr || output_capacity == 0) return result;
  output[0] = '\0';
  if (input == nullptr) return result;

  const char* cursor = input;
  const char* plain = input;
  while (*cursor != '\0') {
    const Match match = matchAt(input, cursor, options);
    if (match.kind == Kind::None) {
      ++cursor;
      continue;
    }

    const char* encoded = cursor + match.prefix_length;
    const char* encoded_end = encoded;
    while (isBase91(*encoded_end)) ++encoded_end;
    if (encoded_end == encoded) {
      ++cursor;
      continue;
    }

    const size_t before_plain_length = result.length;
    if (!appendBytes(output, output_capacity, result.length,
                     plain, (size_t)(cursor - plain))) {
      result.truncated = true;
      break;
    }

    // Punctuation belongs to the Base91 alphabet too. Try progressively
    // shorter suffixes so formats with structural validation (notably MCOtxt)
    // can isolate a complete container before adjacent punctuation.
    const char* accepted_end = nullptr;
    bool token_truncated = false;
    for (const char* candidate_end = encoded_end; candidate_end > encoded; --candidate_end) {
      const size_t saved_length = result.length;
      output[result.length] = '\0';
      if (replaceToken(match.kind, cursor, candidate_end,
                       output, output_capacity, result.length, token_truncated)) {
        accepted_end = candidate_end;
        break;
      }
      result.length = saved_length;
      output[result.length] = '\0';
    }

    if (accepted_end == nullptr) {
      // The apparent prefix was ordinary text. It stays in the pending plain
      // range, and scanning resumes one byte later.
      result.length = before_plain_length;
      output[result.length] = '\0';
      ++cursor;
      continue;
    }

    result.changed = true;
    cursor = accepted_end;
    plain = accepted_end;
    if (token_truncated) {
      result.truncated = true;
      break;
    }
    if (result.length + 1 >= output_capacity) {
      result.truncated = *plain != '\0';
      break;
    }
  }

  if (!result.truncated && *plain != '\0' &&
      !appendBytes(output, output_capacity, result.length, plain, strlen(plain))) {
    result.truncated = true;
  }
  return result;
}

}  // namespace mco_compat
