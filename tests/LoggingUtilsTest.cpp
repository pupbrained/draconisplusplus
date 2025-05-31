#include <string>
#include <string_view>

#include "Util/Logging.hpp" // For Colorize, Bold, Italic, LogLevelConst

#include "gtest/gtest.h"

class LoggingUtilsTest : public testing::Test {};

// Test cases for util::logging::Colorize
TEST_F(LoggingUtilsTest, Colorize_RedText) {
  const std::string_view        textToColorize = "Hello, Red World!";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Red;
  const std::string             expectedPrefix = std::string(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const std::string             expectedSuffix = std::string(util::logging::LogLevelConst::RESET_CODE);

  std::string colorizedText = util::logging::Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);                                            // Check prefix
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), std::string::npos); // Check if text is present
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix); // Check suffix
}

TEST_F(LoggingUtilsTest, Colorize_BlueText) {
  const std::string_view        textToColorize = "Blue Sky";
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Blue;
  const std::string             expectedPrefix = std::string(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const std::string             expectedSuffix = std::string(util::logging::LogLevelConst::RESET_CODE);

  std::string colorizedText = util::logging::Colorize(textToColorize, color);

  EXPECT_TRUE(colorizedText.rfind(expectedPrefix, 0) == 0);
  EXPECT_NE(colorizedText.find(textToColorize.data(), 0, textToColorize.length()), std::string::npos);
  EXPECT_TRUE(colorizedText.length() >= textToColorize.length() + expectedPrefix.length() + expectedSuffix.length());
  EXPECT_EQ(colorizedText.substr(colorizedText.length() - expectedSuffix.length()), expectedSuffix);
}

TEST_F(LoggingUtilsTest, Colorize_EmptyText) {
  const std::string_view        textToColorize;
  const ftxui::Color::Palette16 color          = ftxui::Color::Palette16::Green;
  const std::string             expectedPrefix = std::string(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const std::string             expectedSuffix = std::string(util::logging::LogLevelConst::RESET_CODE);

  std::string colorizedText = util::logging::Colorize(textToColorize, color);
  std::string expectedText  = expectedPrefix + std::string(textToColorize) + expectedSuffix;
  EXPECT_EQ(colorizedText, expectedText);
}

// Test cases for util::logging::Bold
TEST_F(LoggingUtilsTest, Bold_SimpleText) {
  const std::string_view textToBold     = "This is bold.";
  const std::string      expectedPrefix = std::string(util::logging::LogLevelConst::BOLD_START);
  const std::string      expectedSuffix = std::string(util::logging::LogLevelConst::BOLD_END);

  std::string boldedText   = util::logging::Bold(textToBold);
  std::string expectedText = expectedPrefix + std::string(textToBold) + expectedSuffix;

  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Bold_EmptyText) {
  const std::string_view textToBold;
  const std::string      expectedPrefix = std::string(util::logging::LogLevelConst::BOLD_START);
  const std::string      expectedSuffix = std::string(util::logging::LogLevelConst::BOLD_END);

  std::string boldedText   = util::logging::Bold(textToBold);
  std::string expectedText = expectedPrefix + std::string(textToBold) + expectedSuffix;
  EXPECT_EQ(boldedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_SimpleText) {
  const std::string_view textToItalicize = "This is italic.";
  const std::string      expectedPrefix  = std::string(util::logging::LogLevelConst::ITALIC_START);
  const std::string      expectedSuffix  = std::string(util::logging::LogLevelConst::ITALIC_END);

  std::string italicizedText = util::logging::Italic(textToItalicize);
  std::string expectedText   = expectedPrefix + std::string(textToItalicize) + expectedSuffix;

  EXPECT_EQ(italicizedText, expectedText);
}

TEST_F(LoggingUtilsTest, Italic_EmptyText) {
  const std::string_view textToItalicize;
  const std::string      expectedPrefix = std::string(util::logging::LogLevelConst::ITALIC_START);
  const std::string      expectedSuffix = std::string(util::logging::LogLevelConst::ITALIC_END);

  std::string italicizedText = util::logging::Italic(textToItalicize);
  std::string expectedText   = expectedPrefix + std::string(textToItalicize) + expectedSuffix;
  EXPECT_EQ(italicizedText, expectedText);
}

// Test combining styles
TEST_F(LoggingUtilsTest, Combined_BoldItalicRedText) {
  const std::string_view        textToStyle = "Styled Text";
  const ftxui::Color::Palette16 color       = ftxui::Color::Palette16::Magenta;

  const std::string colorPrefix  = std::string(util::logging::LogLevelConst::COLOR_CODE_LITERALS.at(color));
  const std::string colorSuffix  = std::string(util::logging::LogLevelConst::RESET_CODE);
  const std::string boldPrefix   = std::string(util::logging::LogLevelConst::BOLD_START);
  const std::string boldSuffix   = std::string(util::logging::LogLevelConst::BOLD_END);
  const std::string italicPrefix = std::string(util::logging::LogLevelConst::ITALIC_START);
  const std::string italicSuffix = std::string(util::logging::LogLevelConst::ITALIC_END);

  // Order of application matters for verification if we check intermediate strings.
  // Here, we check the final output of applying them sequentially.
  // util::logging::Bold(util::logging::Italic(util::logging::Colorize(text_to_style, color)))
  // This would result in: BOLD_START + ITALIC_START + COLOR_CODE + text + RESET_CODE + ITALIC_END + BOLD_END
  // However, the functions are independent, so we test one combination.
  // Let's test Colorize(Bold(Italic(text)))
  // Expected: COLOR_CODE + BOLD_START + ITALIC_START + text + ITALIC_END + BOLD_END + RESET_CODE

  std::string styledText = util::logging::Colorize(util::logging::Bold(util::logging::Italic(textToStyle)), color);

  std::string expectedInnerText = italicPrefix + std::string(textToStyle) + italicSuffix;
  expectedInnerText             = boldPrefix + expectedInnerText + boldSuffix;
  std::string expectedFinalText = colorPrefix + expectedInnerText + colorSuffix;

  EXPECT_EQ(styledText, expectedFinalText);
}


