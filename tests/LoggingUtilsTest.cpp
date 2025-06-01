#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "gtest/gtest.h"

using util::logging::LogLevelConst, util::logging::Bold, util::logging::Colorize, util::logging::Italic;
using util::types::String, util::types::StringView;

class LoggingUtilsTest : public testing::Test {};

TEST_F(LoggingUtilsTest, Colorize_RedText) {
  const StringView              textToColorize = "Hello, Red World!";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Red;
  const String                  expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String                  expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_BlueText) {
  const StringView              textToColorize = "Blue Sky";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Blue;
  const String                  expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String                  expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_EmptyText) {
  const StringView              textToColorize;
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Green;
  const String                  expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String                  expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);
  String expectedText  = expectedPrefix + String(textToColorize) + expectedSuffix;
  EXPECT_EQ(colorizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_SimpleText) {
  const StringView textToBold     = "This is bold.";
  const String     expectedPrefix = String(LogLevelConst::BOLD_START);
  const String     expectedSuffix = String(LogLevelConst::BOLD_END);

  String boldedText   = Bold(textToBold);
  String expectedText = expectedPrefix + String(textToBold) + expectedSuffix;

  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_EmptyText) {
  const StringView textToBold;
  const String     expectedPrefix = String(LogLevelConst::BOLD_START);
  const String     expectedSuffix = String(LogLevelConst::BOLD_END);

  String boldedText   = Bold(textToBold);
  String expectedText = expectedPrefix + String(textToBold) + expectedSuffix;
  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_SimpleText) {
  const StringView textToItalicize = "This is italic.";
  const String     expectedPrefix  = String(LogLevelConst::ITALIC_START);
  const String     expectedSuffix  = String(LogLevelConst::ITALIC_END);

  String italicizedText = Italic(textToItalicize);
  String expectedText   = expectedPrefix + String(textToItalicize) + expectedSuffix;

  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_EmptyText) {
  const StringView textToItalicize;
  const String     expectedPrefix = String(LogLevelConst::ITALIC_START);
  const String     expectedSuffix = String(LogLevelConst::ITALIC_END);

  String italicizedText = Italic(textToItalicize);
  String expectedText   = expectedPrefix + String(textToItalicize) + expectedSuffix;
  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Combined_BoldItalicRedText) {
  const StringView              textToStyle = "Styled Text";
  const ftxui::Color::Palette16 color       = ftxui::Color::Palette16::Magenta;

  const String colorPrefix  = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String colorSuffix  = String(LogLevelConst::RESET_CODE);
  const String boldPrefix   = String(LogLevelConst::BOLD_START);
  const String boldSuffix   = String(LogLevelConst::BOLD_END);
  const String italicPrefix = String(LogLevelConst::ITALIC_START);
  const String italicSuffix = String(LogLevelConst::ITALIC_END);

  String styledText = Colorize(Bold(Italic(textToStyle)), color);

  String expectedInnerText = italicPrefix + String(textToStyle) + italicSuffix;
  expectedInnerText        = boldPrefix + expectedInnerText + boldSuffix;
  String expectedFinalText = colorPrefix + expectedInnerText + colorSuffix;

  EXPECT_EQ(styledText, expectedFinalText);
}
