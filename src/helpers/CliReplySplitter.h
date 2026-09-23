#pragma once

#include <stddef.h>

namespace cli_reply {

static constexpr size_t MAX_CHUNKS = 8;
static constexpr size_t MAX_CHUNK_TEXT = 150;
static constexpr size_t CHUNK_STORAGE = MAX_CHUNK_TEXT + 1;

struct Chunks {
  char text[MAX_CHUNKS][CHUNK_STORAGE];
  size_t count;
};

enum class SplitResult {
  Ok,
  InvalidArgument,
  TooManyChunks,
};

// Calculates the payload budget for a local TerminalCLI channel reply.  A
// single-part reply has no "[i/n] " marker; multipart replies reserve enough
// space for the largest marker supported by MAX_CHUNKS.
size_t channelChunkCapacity(size_t frame_size, size_t frame_header_size,
                            size_t node_name_length, size_t reply_length);

// Splits text without allocation, preferring a newline at or before the
// supplied limit.  On failure count is reset to zero so callers cannot send a
// partially split reply.
SplitResult split(const char* input, size_t max_chunk_length, Chunks& output);

}  // namespace cli_reply
