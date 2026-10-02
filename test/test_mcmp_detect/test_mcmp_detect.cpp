#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <helpers/Base91.h>
#include <helpers/mcmp/MCMPDetect.h>

namespace {

void putU32(std::vector<uint8_t>& out, uint32_t value) {
  for (int i = 0; i < 4; i++) out.push_back((uint8_t)(value >> (8 * i)));
}

void putName(std::vector<uint8_t>& out, const std::string& name) {
  out.push_back((uint8_t)name.size());   // names here stay under 128 bytes
  out.insert(out.end(), name.begin(), name.end());
}

// A v3 body: flags, timestamp, then the optional fields, then compressed text.
std::vector<uint8_t> body(uint8_t flags, uint32_t timestamp, const std::string& sender = "",
                          const std::string& reply_author = "", uint32_t reply_timestamp = 0,
                          const std::string& compressed = std::string("\x00\x12\x34", 3)) {
  std::vector<uint8_t> out;
  out.push_back(flags);
  putU32(out, timestamp);
  if (flags & 0x04) putName(out, sender);
  if (flags & 0x02) out.insert(out.end(), 64, 0xAA);
  if (flags & 0x01) {
    putName(out, reply_author);
    putU32(out, reply_timestamp);
  }
  out.insert(out.end(), compressed.begin(), compressed.end());
  return out;
}

std::string textV3(const std::vector<uint8_t>& body) {
  char encoded[512];
  size_t length;
  EXPECT_TRUE(mesh::base91::encode(body.data(), body.size(), encoded, sizeof(encoded), length));
  return "mcmp3:" + std::string(encoded, length);
}

std::vector<uint8_t> envelope(const std::string& outer_name, uint8_t subtype_version,
                              const std::vector<uint8_t>& body) {
  std::vector<uint8_t> out;
  putName(out, outer_name);
  out.push_back(subtype_version);
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

}  // namespace

TEST(MCMPDetect, TextForms) {
  EXPECT_EQ((int)mcmp::textForm("mcmp3:abc"), (int)mcmp::Form::TextV3);
  EXPECT_EQ((int)mcmp::textForm("mcmp2:abc"), (int)mcmp::Form::TextV2);
  EXPECT_EQ((int)mcmp::textForm("mcmp:abc"), (int)mcmp::Form::Legacy);
  EXPECT_EQ((int)mcmp::textForm("  mcmp3:abc"), (int)mcmp::Form::TextV3);
  EXPECT_EQ((int)mcmp::textForm("mcmp3:"), (int)mcmp::Form::None);
  EXPECT_EQ((int)mcmp::textForm("mcmp4:abc"), (int)mcmp::Form::None);
  EXPECT_EQ((int)mcmp::textForm("hello mcmp3:abc"), (int)mcmp::Form::None);
  EXPECT_EQ((int)mcmp::textForm(nullptr), (int)mcmp::Form::None);
}

TEST(MCMPDetect, V3ContainerMetadata) {
  const std::string text = textV3(body(0x07, 0x11223344, "Bob", "Ann", 16));
  mcmp::Meta meta;
  ASSERT_TRUE(mcmp::parseText(text.c_str(), meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::TextV3);
  EXPECT_TRUE(meta.container_ok);
  EXPECT_EQ(meta.timestamp, 0x11223344u);
  EXPECT_TRUE(meta.is_signed);
  EXPECT_TRUE(meta.has_sender);
  EXPECT_STREQ(meta.sender, "Bob");
  EXPECT_TRUE(meta.has_reply);
  EXPECT_STREQ(meta.reply_author, "Ann");
  EXPECT_EQ(meta.reply_timestamp, 16u);
  EXPECT_EQ(meta.compressed_length, 3u);
  char placeholder[96];
  EXPECT_GT(mcmp::formatPlaceholder(meta, placeholder, sizeof(placeholder)), 0);
  EXPECT_STREQ(placeholder, "@[Ann] <MCMP v3 signed message>");

  // A plain channel body carries none of the optional fields.
  const std::string plain = textV3(body(0x00, 7, "", "", 0, "\x01\xFF"));
  ASSERT_TRUE(mcmp::parseText(plain.c_str(), meta));
  EXPECT_TRUE(meta.container_ok);
  EXPECT_EQ(meta.timestamp, 7u);
  EXPECT_FALSE(meta.is_signed);
  EXPECT_FALSE(meta.has_sender);
  EXPECT_FALSE(meta.has_reply);
  EXPECT_EQ(meta.compressed_length, 2u);
}

TEST(MCMPDetect, UnreadableContainersKeepTheForm) {
  mcmp::Meta meta;
  // A reserved flag bit.
  ASSERT_TRUE(mcmp::parseText(textV3(body(0x08, 1)).c_str(), meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::TextV3);
  EXPECT_FALSE(meta.container_ok);
  EXPECT_FALSE(meta.has_sender);
  // A signature cut short.
  std::vector<uint8_t> cut = body(0x02, 1);
  cut.resize(cut.size() - 10);
  ASSERT_TRUE(mcmp::parseText(textV3(cut).c_str(), meta));
  EXPECT_FALSE(meta.container_ok);
  // A prefix alone is not enough: the body must at least be valid Base91.
  EXPECT_FALSE(mcmp::parseText("mcmp3:!!!\x01", meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::None);
  EXPECT_FALSE(mcmp::parseText("mcmp: plain text", meta));
  // Not MCMP.
  EXPECT_FALSE(mcmp::parseText("plain text", meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::None);
}

TEST(MCMPDetect, V2AndLegacyHaveNoContainer) {
  mcmp::Meta meta;
  ASSERT_TRUE(mcmp::parseText("mcmp2:AbCd", meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::TextV2);
  EXPECT_FALSE(meta.container_ok);
  ASSERT_TRUE(mcmp::parseText("mcmp:AbCd", meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::Legacy);
  EXPECT_FALSE(meta.container_ok);
}

TEST(MCMPDetect, BinaryEnvelope) {
  const std::vector<uint8_t> channel = envelope("Bob", 0x20, body(0x00, 99));
  EXPECT_TRUE(mcmp::isBinaryEnvelope(0x0120, channel.data(), channel.size()));
  mcmp::Meta meta;
  ASSERT_TRUE(mcmp::parseBinaryEnvelope(0x0120, channel.data(), channel.size(), meta));
  EXPECT_EQ((int)meta.form, (int)mcmp::Form::Binary);
  EXPECT_EQ(meta.revision, 0);
  EXPECT_TRUE(meta.container_ok);
  EXPECT_EQ(meta.timestamp, 99u);
  EXPECT_TRUE(meta.has_sender) << "the envelope names the sender";
  EXPECT_STREQ(meta.sender, "Bob");

  // A name embedded in the container wins over the envelope's.
  const std::vector<uint8_t> embedded = envelope("Relay", 0x20, body(0x04, 5, "Room"));
  ASSERT_TRUE(mcmp::parseBinaryEnvelope(0x0120, embedded.data(), embedded.size(), meta));
  EXPECT_STREQ(meta.sender, "Room");

  const std::vector<uint8_t> reply = envelope("Bob", 0x20, body(0x01, 5, "", "Ann", 16));
  ASSERT_TRUE(mcmp::parseBinaryEnvelope(0x0120, reply.data(), reply.size(), meta));
  EXPECT_TRUE(meta.has_reply);
  EXPECT_STREQ(meta.reply_author, "Ann");
  char binary_placeholder[96];
  EXPECT_GT(mcmp::formatPlaceholder(meta, binary_placeholder, sizeof(binary_placeholder)), 0);
  EXPECT_STREQ(binary_placeholder, "@[Ann] <MCMP v3 message>");

  // A newer revision: recognised, container left alone, envelope name kept.
  const std::vector<uint8_t> newer = envelope("Bob", 0x21, body(0x00, 99));
  EXPECT_TRUE(mcmp::isBinaryEnvelope(0x0120, newer.data(), newer.size()));
  ASSERT_TRUE(mcmp::parseBinaryEnvelope(0x0120, newer.data(), newer.size(), meta));
  EXPECT_EQ(meta.revision, 1);
  EXPECT_FALSE(meta.container_ok);
  EXPECT_STREQ(meta.sender, "Bob");

  // Other subtypes and data types are not MCMP.
  const std::vector<uint8_t> mcotxt = envelope("", 0x31, body(0x00, 1));
  EXPECT_FALSE(mcmp::isBinaryEnvelope(0x0120, mcotxt.data(), mcotxt.size()));
  EXPECT_FALSE(mcmp::parseBinaryEnvelope(0x0120, mcotxt.data(), mcotxt.size(), meta));
  EXPECT_FALSE(mcmp::isBinaryEnvelope(0x0121, channel.data(), channel.size()));
  const std::vector<uint8_t> cut = {0x05, 'B'};
  EXPECT_FALSE(mcmp::isBinaryEnvelope(0x0120, cut.data(), cut.size()));
  const std::vector<uint8_t> huge_name = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x20};
  EXPECT_FALSE(mcmp::isBinaryEnvelope(0x0120, huge_name.data(), huge_name.size()));
  EXPECT_FALSE(mcmp::parseBinaryEnvelope(0x0120, huge_name.data(), huge_name.size(), meta));
  EXPECT_FALSE(mcmp::isBinaryEnvelope(0x0120, nullptr, 0));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
