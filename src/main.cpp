#include <boost/json/src.hpp>
#include <cpr/cpr.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <playerctl/playerctl.h>
#include <toml++/toml.hpp>

using fmt::format;
using fmt::formatter;
using std::string;

struct b_to_gib {
  uint64_t value;
};

template <> struct formatter<b_to_gib> : formatter<double> {
  template <typename FormatContext>
  auto format(const b_to_gib b, FormatContext &ctx) {
    auto out = formatter<double>::format(
        b.value / std::pow(1024, 3), ctx); // NOLINT(*-narrowing-conversions);
    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';
    return out;
  }
};

uint64_t parse_line_as_number(const string &input) {
  // Find the first number
  string::size_type start = 0;

  // Skip leading non-numbers
  while (!isdigit(input[++start]))
    ;

  // Start searching from the start of the number
  string::size_type end = start;

  // Increment to the end of the number
  while (isdigit(input[++end]))
    ;

  // Return the substring containing the number
  return std::stoul(input.substr(start, end - start));
}

uint64_t meminfo_parse(std::ifstream is) {
  string line;

  // Skip every line before the one that starts with "MemTotal"
  while (std::getline(is, line) && !line.starts_with("MemTotal"))
    ;

  // Parse the number from the line
  const auto num = parse_line_as_number(line);

  return num;
}

PlayerctlPlayer *init_playerctl() {
  // Create a player manager
  PlayerctlPlayerManager *const player_manager =
      playerctl_player_manager_new(nullptr);

  // Create an empty player list
  GList *available_players = nullptr;

  // Get the list of available players and put it in the player list
  g_object_get(player_manager, "player-names", &available_players, nullptr);

  // If no players are available, return nullptr
  if (!available_players)
    return nullptr;

  // Get the first player
  const auto player_name =
      static_cast<PlayerctlPlayerName *>(available_players->data);

  // Create the player
  PlayerctlPlayer *const current_player =
      playerctl_player_new_from_name(player_name, nullptr);

  // If no player is available, return nullptr
  if (!current_player)
    return nullptr;

  // Manage the player
  playerctl_player_manager_manage_player(player_manager, current_player);

  // Unref the player
  g_object_unref(current_player);

  return current_player;
}

enum date_num { Ones, Twos, Threes, Default };

date_num parse_date(string const &inString) {
  if (inString == "1" || inString == "21" || inString == "31")
    return Ones;

  if (inString == "2" || inString == "22")
    return Twos;

  if (inString == "3" || inString == "23")
    return Threes;

  return Default;
}

boost::json::object get_weather() {
  using namespace cpr;
  using namespace boost::json;

  Response r = Get(Url{format("https://api.openweathermap.org/data/2.5/"
                              "weather?lat={}&lon={}&appid={}&units={}",
                              "39.9537", "-74.1979",
                              "00000000000000000000000000000000", "imperial")});

  value json = parse(r.text);

  return json.as_object();
}

int main() {
  const toml::parse_result config = toml::parse_file("./config.toml");

  const char *name = config["general"]["name"].value_or(getlogin());

  if (config["playerctl"]["enable"].value_or(false)) {
    if (PlayerctlPlayer *current_player = init_playerctl()) {
      gchar *song_title = playerctl_player_get_title(current_player, nullptr);
      fmt::println("Now playing: {}", song_title);
    }
  }

  fmt::println("Hello {}!", name);

  const uint64_t meminfo = meminfo_parse(std::ifstream("/proc/meminfo")) * 1024;

  fmt::println("{:.2f}", b_to_gib{meminfo});

  const std::time_t t = std::time(nullptr);

  string date = fmt::format("{:%d}", fmt::localtime(t));

  switch (parse_date(date)) {
    case Ones:
      date += "st";
      break;

    case Twos:
      date += "nd";
      break;

    case Threes:
      date += "rd";
      break;

    case Default:
      date += "th";
      break;
  }

  fmt::println("{:%B} {}, {:%-I:%0M %p}", fmt::localtime(t), date,
               fmt::localtime(t));

  auto json = get_weather();

  auto town_name =
      json["name"].is_string() ? json["name"].as_string().c_str() : "Unknown";

  fmt::println("{}", town_name);

  return 0;
}
