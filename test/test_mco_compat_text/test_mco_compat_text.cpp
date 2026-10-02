#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <helpers/Base91.h>
#include <helpers/MCOCompatText.h>

namespace {

std::string base91(const std::vector<uint8_t>& bytes) {
  char encoded[256];
  size_t length = 0;
  EXPECT_TRUE(mesh::base91::encode(bytes.data(), bytes.size(), encoded,
                                   sizeof(encoded), length));
  return std::string(encoded, length);
}

void putU32(std::vector<uint8_t>& out, uint32_t value) {
  for (int i = 0; i < 4; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

void putName(std::vector<uint8_t>& out, const char* name) {
  const size_t length = strlen(name);
  EXPECT_LT(length, 128u);
  out.push_back((uint8_t)length);
  out.insert(out.end(), name, name + length);
}

std::string mcmpV3Reply(const char* author, bool is_signed = false) {
  std::vector<uint8_t> container;
  container.push_back((uint8_t)(0x01 | (is_signed ? 0x02 : 0x00)));
  putU32(container, 0x11223344);
  if (is_signed) container.insert(container.end(), 64, 0xAA);
  putName(container, author);
  putU32(container, 0x10);
  container.insert(container.end(), {'\0', '\x12', '\x34'});
  return "mcmp3:" + base91(container);
}

std::string mctUtf8(const char* text) {
  const size_t length = strlen(text);
  EXPECT_LT(length, 128u);
  std::vector<uint8_t> container = {0x31, 0x04, 0x01, (uint8_t)length};
  container.insert(container.end(), text, text + length);
  return "mct:" + base91(container);
}

std::string mctUtf8Reply(const char* author, const char* text) {
  const size_t author_length = strlen(author);
  const size_t text_length = strlen(text);
  EXPECT_LT(author_length, 128u);
  EXPECT_LT(text_length, 128u);
  std::vector<uint8_t> container = {0x31, 0x05, 0x01, (uint8_t)author_length};
  container.insert(container.end(), author, author + author_length);
  container.insert(container.end(), {0x10, 0x00, 0x00, 0x00});
  container.insert(container.end(), {0x01, (uint8_t)text_length});
  container.insert(container.end(), text, text + text_length);
  return "mct:" + base91(container);
}

mco_compat::Options all() { return { true, true, true }; }

}  // namespace

TEST(MCOCompatText, ReplacesAnInlineImageAndPreservesReplyAndProse) {
  const std::string input =
      "Sender: @[QRb] привет, смотри! im3:abc как она тебе?\nМне понравилась";
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.truncated);
  EXPECT_STREQ(output,
      "Sender: @[QRb] привет, смотри! <MCOimg v3 image> как она тебе?\nМне понравилась");
}

TEST(MCOCompatText, ReplacesMultipleIndependentTokens) {
  const std::string input = "before mcmp2:AbCd middle im3:abc after";
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_STREQ(output,
      "before <MCMP v2 message> middle <MCOimg v3 image> after");
}

TEST(MCOCompatText, AddsReplyMentionToMCMPv3Placeholder) {
  const std::string input = "Sender: " + mcmpV3Reply("Ann");
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.truncated);
  EXPECT_STREQ(output, "Sender: @[Ann] <MCMP v3 message>");
}

TEST(MCOCompatText, DoesNotDuplicateExistingReplyMentionBeforeMCMPv3) {
  const std::string input = "Sender: @[Ann] " + mcmpV3Reply("Ann", true);
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.truncated);
  EXPECT_STREQ(output, "Sender: @[Ann] <MCMP v3 signed message>");
}

TEST(MCOCompatText, DecodesInlineMCOtxtWithoutRemovingSurroundingText) {
  const std::string input = "Sender: @[QRb] обычный текст, а теперь " + mctUtf8("hello") +
                            " как тебе?";
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.truncated);
  EXPECT_STREQ(output,
               "Sender: @[QRb] обычный текст, а теперь hello как тебе?");
}

TEST(MCOCompatText, AddsReplyMentionToDecodedMCOtxt) {
  const std::string input = "Sender: " + mctUtf8Reply("Bob", "hello");
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.truncated);
  EXPECT_STREQ(output, "Sender: @[Bob] hello");
}

TEST(MCOCompatText, KeepsExistingReplyMentionInDecodedMCOtxt) {
  const std::string input = "Sender: " + mctUtf8Reply("Bob", "@[Bob] hello");
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_FALSE(result.truncated);
  EXPECT_STREQ(output, "Sender: @[Bob] hello");
}

TEST(MCOCompatText, RecognisesLegacyAndVersionedDetectorPrefixes) {
  const std::string input =
      "mcmp:AbCd mcmp2:EfGh mcmp3:IjKl im:MnOp im4:QrSt";
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  EXPECT_TRUE(result.changed);
  EXPECT_STREQ(output,
      "<MCMP v1 message> <MCMP v2 message> <MCMP v3 message> "
      "<MCOimg image> <MCOimg v4 image>");
}

TEST(MCOCompatText, KeepsDisabledFormatsAndPrefixesInsideWords) {
  const mco_compat::Options images_only = { false, false, true };
  const std::string input = "swim3:abc стрим3:abc mcmp2:AbCd im3:abc";
  char output[256];
  const mco_compat::Result result = mco_compat::transform(
      input.c_str(), output, sizeof(output), images_only);
  EXPECT_TRUE(result.changed);
  EXPECT_STREQ(output,
               "swim3:abc стрим3:abc mcmp2:AbCd <MCOimg v3 image>");
}

TEST(MCOCompatText, LeavesMalformedTokensUntouched) {
  const std::string input = "before im3:abc def after mct:!!!\x01 tail";
  char output[256];
  const mco_compat::Result result =
      mco_compat::transform(input.c_str(), output, sizeof(output), all());
  // im3:abc is currently a valid detector-level image token; malformed
  // MCOtxt remains byte-identical instead of deleting surrounding prose.
  EXPECT_TRUE(result.changed);
  EXPECT_STREQ(output, "before <MCOimg v3 image> def after mct:!!!\x01 tail");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
