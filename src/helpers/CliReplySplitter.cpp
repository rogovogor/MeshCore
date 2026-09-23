#include <helpers/CliReplySplitter.h>

#include <string.h>

namespace cli_reply {
namespace {

size_t decimalDigits(size_t value) {
  size_t digits = 1;
  while (value >= 10) {
    value /= 10;
    ++digits;
  }
  return digits;
}

size_t boundedPayloadCapacity(size_t available) {
  return available < MAX_CHUNK_TEXT ? available : MAX_CHUNK_TEXT;
}

}  // namespace

size_t channelChunkCapacity(size_t frame_size, size_t frame_header_size,
                            size_t node_name_length, size_t reply_length) {
  // Every synthetic channel reply starts with "<node name>: ".
  const size_t node_prefix_length = node_name_length + 2;
  if (frame_header_size > frame_size ||
      node_prefix_length > frame_size - frame_header_size) {
    return 0;
  }

  const size_t single_capacity = boundedPayloadCapacity(
      frame_size - frame_header_size - node_prefix_length);
  if (reply_length <= single_capacity) return single_capacity;

  // "[i/n] " consists of both decimal indices plus '[', '/', ']', and ' '.
  const size_t marker_length = decimalDigits(MAX_CHUNKS) * 2 + 4;
  const size_t fixed_length = frame_header_size + node_prefix_length;
  if (fixed_length > frame_size || marker_length > frame_size - fixed_length) {
    return 0;
  }
  return boundedPayloadCapacity(frame_size - fixed_length - marker_length);
}

SplitResult split(const char* input, size_t max_chunk_length, Chunks& output) {
  output.count = 0;
  if (input == nullptr || max_chunk_length == 0 ||
      max_chunk_length > MAX_CHUNK_TEXT) {
    return SplitResult::InvalidArgument;
  }

  const char* cursor = input;
  while (*cursor) {
    if (output.count >= MAX_CHUNKS) {
      output.count = 0;
      return SplitResult::TooManyChunks;
    }

    const size_t remaining = strlen(cursor);
    if (remaining <= max_chunk_length) {
      memcpy(output.text[output.count], cursor, remaining + 1);
      ++output.count;
      return SplitResult::Ok;
    }

    size_t cut = max_chunk_length;
    for (size_t i = max_chunk_length; i > 0; --i) {
      if (cursor[i - 1] == '\n') {
        cut = i;
        break;
      }
    }
    memcpy(output.text[output.count], cursor, cut);
    output.text[output.count][cut] = '\0';
    ++output.count;
    cursor += cut;
  }

  return SplitResult::Ok;
}

}  // namespace cli_reply
