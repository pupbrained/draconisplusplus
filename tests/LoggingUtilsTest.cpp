#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "gtest/gtest.h"

using util::types::String, util::types::StringView;

class LoggingUtilsTest : public testing::Test {};

TEST_F(LoggingUtilsTest, Colorize_RedText) {
  const StringView        textToColorize = "Hello, Red World!";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Red;
  const String             expectedPrefix = String(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String             expectedSuffix = String(util::logging::LogLevelConst::RESET_CODE);

  String colorizedText = util::logging::Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_BlueText) {
  const StringView        textToColorize = "Blue Sky";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Blue;
  const String             expectedPrefix = String(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String             expectedSuffix = String(util::logging::LogLevelConst::RESET_CODE);

  String colorizedText = util::logging::Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_EmptyText) {
  const StringView        textToColorize;
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Green;
  const String             expectedPrefix = String(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String             expectedSuffix = String(util::logging::LogLevelConst::RESET_CODE);

  String colorizedText = util::logging::Colorize(textToColorize, color);
  String expectedText  = expectedPrefix + String(textToColorize) + expectedSuffix;
  EXPECT_EQ(colorizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_SimpleText) {
  const StringView textToBold     = "This is bold.";
  const String      expectedPrefix = String(util::logging::LogLevelConst::BOLD_START);
  const String      expectedSuffix = String(util::logging::LogLevelConst::BOLD_END);

  String boldedText   = util::logging::Bold(textToBold);
  String expectedText = expectedPrefix + String(textToBold) + expectedSuffix;

  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_EmptyText) {
  const StringView textToBold;
  const String      expectedPrefix = String(util::logging::LogLevelConst::BOLD_START);
  const String      expectedSuffix = String(util::logging::LogLevelConst::BOLD_END);

  String boldedText   = util::logging::Bold(textToBold);
  String expectedText = expectedPrefix + String(textToBold) + expectedSuffix;
  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_SimpleText) {
  const StringView textToItalicize = "This is italic.";
  const String      expectedPrefix  = String(util::logging::LogLevelConst::ITALIC_START);
  const String      expectedSuffix  = String(util::logging::LogLevelConst::ITALIC_END);

  String italicizedText = util::logging::Italic(textToItalicize);
  String expectedText   = expectedPrefix + String(textToItalicize) + expectedSuffix;

  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_EmptyText) {
  const StringView textToItalicize;
  const String      expectedPrefix = String(util::logging::LogLevelConst::ITALIC_START);
  const String      expectedSuffix = String(util::logging::LogLevelConst::ITALIC_END);

  String italicizedText = util::logging::Italic(textToItalicize);
  String expectedText   = expectedPrefix + String(textToItalicize) + expectedSuffix;
  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Combined_BoldItalicRedText) {
  const StringView        textToStyle = "Styled Text";
  const ftxui::Color::Palette16 color       = ftxui::Color::Palette16::Magenta;

  const String colorPrefix  = String(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String colorSuffix  = String(util::logging::LogLevelConst::RESET_CODE);
  const String boldPrefix   = String(util::logging::LogLevelConst::BOLD_START);
  const String boldSuffix   = String(util::logging::LogLevelConst::BOLD_END);
  const String italicPrefix = String(util::logging::LogLevelConst::ITALIC_START);
  const String italicSuffix = String(util::logging::LogLevelConst::ITALIC_END);

  String styledText = util::logging::Colorize(util::logging::Bold(util::logging::Italic(textToStyle)), color);

  String expectedInnerText = italicPrefix + String(textToStyle) + italicSuffix;
  expectedInnerText             = boldPrefix + expectedInnerText + boldSuffix;
  String expectedFinalText = colorPrefix + expectedInnerText + colorSuffix;

  EXPECT_EQ(styledText, expectedFinalText);
}
