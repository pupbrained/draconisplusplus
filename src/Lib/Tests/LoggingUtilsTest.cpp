#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "gtest/gtest.h"

using namespace testing;
using draconis::utils::logging::Bold;
using draconis::utils::logging::Colorize;
using draconis::utils::logging::Italic;
using draconis::utils::logging::LogColor;
using draconis::utils::logging::LogLevelConst;
using draconis::utils::types::i32;
using draconis::utils::types::String;
using draconis::utils::types::StringView;

class LoggingUtilsTest : public Test {};

TEST_F(LoggingUtilsTest, Colorize_RedText) {
  constexpr StringView textToColorize = "Hello, Red World!";
  constexpr LogColor   color          = LogColor::Red;
  const String         expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<size_t>(color)));
  const String         expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_BlueText) {
  constexpr StringView textToColorize = "Blue Sky";
  constexpr LogColor   color          = LogColor::Blue;
  const String         expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<size_t>(color)));
  const String         expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_EmptyText) {
  constexpr StringView textToColorize;
  constexpr LogColor   color          = LogColor::Green;
  const String         expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<size_t>(color)));
  const String         expectedSuffix = String(LogLevelConst::RESET_CODE);

  const String colorizedText = Colorize(textToColorize, color);
  const String expectedText  = expectedPrefix + String(textToColorize) + expectedSuffix;
  EXPECT_EQ(colorizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_SimpleText) {
  constexpr StringView textToBold     = "This is bold.";
  const String         expectedPrefix = String(LogLevelConst::BOLD_START);
  const String         expectedSuffix = String(LogLevelConst::BOLD_END);

  const String boldedText   = Bold(textToBold);
  const String expectedText = expectedPrefix + String(textToBold) + expectedSuffix;

  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_EmptyText) {
  constexpr StringView textToBold;
  const String         expectedPrefix = String(LogLevelConst::BOLD_START);
  const String         expectedSuffix = String(LogLevelConst::BOLD_END);

  const String boldedText   = Bold(textToBold);
  const String expectedText = expectedPrefix + String(textToBold) + expectedSuffix;
  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_SimpleText) {
  constexpr StringView textToItalicize = "This is italic.";
  const String         expectedPrefix  = String(LogLevelConst::ITALIC_START);
  const String         expectedSuffix  = String(LogLevelConst::ITALIC_END);

  const String italicizedText = Italic(textToItalicize);
  const String expectedText   = expectedPrefix + String(textToItalicize) + expectedSuffix;

  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_EmptyText) {
  constexpr StringView textToItalicize;
  const String         expectedPrefix = String(LogLevelConst::ITALIC_START);
  const String         expectedSuffix = String(LogLevelConst::ITALIC_END);

  const String italicizedText = Italic(textToItalicize);
  const String expectedText   = expectedPrefix + String(textToItalicize) + expectedSuffix;
  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Combined_BoldItalicRedText) {
  constexpr StringView textToStyle = "Styled Text";
  constexpr LogColor   color       = LogColor::Magenta;

  const String colorPrefix  = String(LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<size_t>(color)));
  const String colorSuffix  = String(LogLevelConst::RESET_CODE);
  const String boldPrefix   = String(LogLevelConst::BOLD_START);
  const String boldSuffix   = String(LogLevelConst::BOLD_END);
  const String italicPrefix = String(LogLevelConst::ITALIC_START);
  const String italicSuffix = String(LogLevelConst::ITALIC_END);

  const String styledText = Colorize(Bold(Italic(textToStyle)), color);

  String expectedInnerText       = italicPrefix + String(textToStyle) + italicSuffix;
  expectedInnerText              = boldPrefix + expectedInnerText + boldSuffix;
  const String expectedFinalText = colorPrefix + expectedInnerText + colorSuffix;

  EXPECT_EQ(styledText, expectedFinalText);
}

fn main(i32 argc, char** argv) -> i32 {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}