#include <gtest/gtest.h>

#include <helpers/CliReplySplitter.h>

#include <cstring>
#include <string>

namespace {

std::string makeText(size_t length) {
  std::string text;
  text.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    text.push_back(static_cast<char>('A' + (i % 26)));
  }
  return text;
}

std::string join(const cli_reply::Chunks& chunks) {
  std::string result;
  for (size_t i = 0; i < chunks.count; ++i) result += chunks.text[i];
  return result;
}

}  // namespace

TEST(CliReplySplitter, CalculatesWorstCaseChannelBudgets) {
  EXPECT_EQ(cli_reply::channelChunkCapacity(176, 11, 31, 132), 132u);
  EXPECT_EQ(cli_reply::channelChunkCapacity(176, 11, 31, 133), 126u);
  EXPECT_EQ(cli_reply::channelChunkCapacity(176, 8, 31, 135), 135u);
  EXPECT_EQ(cli_reply::channelChunkCapacity(176, 8, 31, 136), 129u);
  EXPECT_EQ(cli_reply::channelChunkCapacity(176, 11, 5, 150), 150u);
}

TEST(CliReplySplitter, KeepsFullChunkWhenItFitsExactly) {
  const std::string input = makeText(132);
  cli_reply::Chunks chunks = {};
  const size_t capacity = cli_reply::channelChunkCapacity(176, 11, 31, input.size());

  EXPECT_EQ(cli_reply::split(input.c_str(), capacity, chunks),
            cli_reply::SplitResult::Ok);
  ASSERT_EQ(chunks.count, 1u);
  EXPECT_EQ(std::string(chunks.text[0]), input);
}

TEST(CliReplySplitter, SplitsWithoutLosingCharacters) {
  const std::string input = makeText(512);
  cli_reply::Chunks chunks = {};
  const size_t capacity = cli_reply::channelChunkCapacity(176, 11, 31, input.size());

  ASSERT_EQ(capacity, 126u);
  EXPECT_EQ(cli_reply::split(input.c_str(), capacity, chunks),
            cli_reply::SplitResult::Ok);
  ASSERT_EQ(chunks.count, 5u);
  EXPECT_EQ(join(chunks), input);
  for (size_t i = 0; i < chunks.count; ++i) {
    EXPECT_LE(strlen(chunks.text[i]), capacity);
  }
}

TEST(CliReplySplitter, PrefersNewlineWithinCapacity) {
  const std::string input = "first line\nsecond line which is longer";
  cli_reply::Chunks chunks = {};

  EXPECT_EQ(cli_reply::split(input.c_str(), 16, chunks),
            cli_reply::SplitResult::Ok);
  ASSERT_EQ(chunks.count, 3u);
  EXPECT_EQ(std::string(chunks.text[0]), "first line\n");
  EXPECT_EQ(join(chunks), input);
}

TEST(CliReplySplitter, RejectsOverflowWithoutExposingPartialChunks) {
  const std::string input = makeText(cli_reply::MAX_CHUNKS * 10 + 1);
  cli_reply::Chunks chunks = {};

  EXPECT_EQ(cli_reply::split(input.c_str(), 10, chunks),
            cli_reply::SplitResult::TooManyChunks);
  EXPECT_EQ(chunks.count, 0u);
}

TEST(CliReplySplitter, RejectsInvalidLimits) {
  cli_reply::Chunks chunks = {};
  EXPECT_EQ(cli_reply::split(nullptr, 10, chunks),
            cli_reply::SplitResult::InvalidArgument);
  EXPECT_EQ(cli_reply::split("text", 0, chunks),
            cli_reply::SplitResult::InvalidArgument);
  EXPECT_EQ(cli_reply::split("text", cli_reply::MAX_CHUNK_TEXT + 1, chunks),
            cli_reply::SplitResult::InvalidArgument);
}

// The native environment's googletest package ships no gtest_main, so every
// public-line test file provides its own entry point.
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
