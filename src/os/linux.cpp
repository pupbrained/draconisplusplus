module;

#include <fmt/core.h>
#include <playerctl/playerctl.h>
#include <fstream>

export module OS;

using std::string;

uint64_t parse_line_as_number(const string& input) {
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

export uint64_t get_meminfo() {
  return meminfo_parse(std::ifstream("/proc/meminfo")) * 1024;
}

PlayerctlPlayer* init_playerctl() {
  // Create a player manager
  PlayerctlPlayerManager* const player_manager =
      playerctl_player_manager_new(nullptr);

  // Create an empty player list
  GList* available_players = nullptr;

  // Get the list of available players and put it in the player list
  g_object_get(player_manager, "player-names", &available_players, nullptr);

  // If no players are available, return nullptr
  if (!available_players)
    return nullptr;

  // Get the first player
  const auto player_name =
      static_cast<PlayerctlPlayerName*>(available_players->data);

  // Create the player
  PlayerctlPlayer* const current_player =
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

export string get_nowplaying() {
  if (PlayerctlPlayer* current_player = init_playerctl()) {
    return playerctl_player_get_title(current_player, nullptr);
  }

  return "Could not get now playing info";
}
