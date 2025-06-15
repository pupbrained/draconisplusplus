#include <ftxui/screen/color.hpp> // ftxui::Color

#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>

#include "gtest/gtest.h"

using namespace util::logging;
using namespace util::types;

class LoggingUtilsTest : public testing::Test {};

TEST_F(LoggingUtilsTest, Colorize_RedText) {
  const SZStringView            textToColorize = "Hello, Red World!";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Red;
  const SZString                expectedPrefix = SZString(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const SZString                expectedSuffix = SZString(LogLevelConst::RESET_CODE);

  SZString colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), SZString::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_BlueText) {
  const SZStringView            textToColorize = "Blue Sky";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Blue;
  const SZString                expectedPrefix = SZString(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const SZString                expectedSuffix = SZString(LogLevelConst::RESET_CODE);

  SZString colorizedText = Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), SZString::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_EmptyText) {
  const SZStringView            textToColorize;
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Green;
  const SZString                expectedPrefix = SZString(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const SZString                expectedSuffix = SZString(LogLevelConst::RESET_CODE);

  SZString colorizedText = Colorize(textToColorize, color);
  SZString expectedText  = expectedPrefix + SZString(textToColorize) + expectedSuffix;
  EXPECT_EQ(colorizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_SimpleText) {
  const SZStringView textToBold     = "This is bold.";
  const SZString     expectedPrefix = SZString(LogLevelConst::BOLD_START);
  const SZString     expectedSuffix = SZString(LogLevelConst::BOLD_END);

  SZString boldedText   = Bold(textToBold);
  SZString expectedText = expectedPrefix + SZString(textToBold) + expectedSuffix;

  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_EmptyText) {
  const SZStringView textToBold;
  const SZString     expectedPrefix = SZString(LogLevelConst::BOLD_START);
  const SZString     expectedSuffix = SZString(LogLevelConst::BOLD_END);

  SZString boldedText   = Bold(textToBold);
  SZString expectedText = expectedPrefix + SZString(textToBold) + expectedSuffix;
  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_SimpleText) {
  const SZStringView textToItalicize = "This is italic.";
  const SZString     expectedPrefix  = SZString(LogLevelConst::ITALIC_START);
  const SZString     expectedSuffix  = SZString(LogLevelConst::ITALIC_END);

  SZString italicizedText = Italic(textToItalicize);
  SZString expectedText   = expectedPrefix + SZString(textToItalicize) + expectedSuffix;

  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_EmptyText) {
  const SZStringView textToItalicize;
  const SZString     expectedPrefix = SZString(LogLevelConst::ITALIC_START);
  const SZString     expectedSuffix = SZString(LogLevelConst::ITALIC_END);

  SZString italicizedText = Italic(textToItalicize);
  SZString expectedText   = expectedPrefix + SZString(textToItalicize) + expectedSuffix;
  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Combined_BoldItalicRedText) {
  const SZStringView            textToStyle = "Styled Text";
  const ftxui::Color::Palette16 color       = ftxui::Color::Palette16::Magenta;

  const SZString colorPrefix  = SZString(LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const SZString colorSuffix  = SZString(LogLevelConst::RESET_CODE);
  const SZString boldPrefix   = SZString(LogLevelConst::BOLD_START);
  const SZString boldSuffix   = SZString(LogLevelConst::BOLD_END);
  const SZString italicPrefix = SZString(LogLevelConst::ITALIC_START);
  const SZString italicSuffix = SZString(LogLevelConst::ITALIC_END);

  SZString styledText = Colorize(Bold(Italic(textToStyle)), color);

  SZString expectedInnerText = italicPrefix + SZString(textToStyle) + italicSuffix;
  expectedInnerText          = boldPrefix + expectedInnerText + boldSuffix;
  SZString expectedFinalText = colorPrefix + expectedInnerText + colorSuffix;

  EXPECT_EQ(styledText, expectedFinalText);
}

fn main(i32 argc, char** argv) -> i32 {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}