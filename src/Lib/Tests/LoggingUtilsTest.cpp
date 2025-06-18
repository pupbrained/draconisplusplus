#include <ftxui/screen/color.hpp> // ftxui::Color

#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>

#include "gtest/gtest.h"

using namespace testing;
using draconis::utils::logging::LogLevelConst, draconis::utils::logging::Colorize, draconis::utils::logging::Bold, draconis::utils::logging::Italic;
using draconis::utils::types::String, draconis::utils::types::StringView, draconis::utils::types::i32;

class LoggingUtilsTest : public Test {};

TEST_F(LoggingUtilsTest, Colorize_RedText) {
  constexpr StringView              textToColorize = "Hello, Red World!";
  constexpr ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Red;
  const String                      expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String                      expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_BlueText) {
  constexpr StringView              textToColorize = "Blue Sky";
  constexpr ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Blue;
  const String                      expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String                      expectedSuffix = String(LogLevelConst::RESET_CODE);

  String colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), String::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_EmptyText) {
  constexpr StringView              textToColorize;
  constexpr ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Green;
  const String                      expectedPrefix = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const String                      expectedSuffix = String(LogLevelConst::RESET_CODE);

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
  constexpr StringView              textToStyle = "Styled Text";
  constexpr ftxui::Color::Palette16 color       = ftxui::Color::Palette16::Magenta;

  const String colorPrefix  = String(LogLevelConst::COLOR_CODE_LITERALS.at(color));
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