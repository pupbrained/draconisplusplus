#ifdef __linux__

#include <fmt/core.h>
#include <fstream>
#include <playerctl/playerctl.h>

#include "os.h"

using std::string;

uint64_t ParseLineAsNumber(const string& input) {
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

uint64_t MeminfoParse(std::ifstream input) {
  string line;

  // Skip every line before the one that starts with "MemTotal"
  while (std::getline(input, line) && !line.starts_with("MemTotal"))
    ;

  // Parse the number from the line
  const uint64_t num = ParseLineAsNumber(line);

  return num;
}

uint64_t GetMemInfo() {
  return MeminfoParse(std::ifstream("/proc/meminfo")) * 1024;
}

PlayerctlPlayer* InitPlayerctl() {
  // Create a player manager
  PlayerctlPlayerManager* playerManager = playerctl_player_manager_new(nullptr);

  // Create an empty player list
  GList* availablePlayers = nullptr;

  // Get the list of available players and put it in the player list
  g_object_get(playerManager, "player-names", &availablePlayers, nullptr);

  // If no players are available, return nullptr
  if (!availablePlayers)
    return nullptr;

  // Get the first player
  PlayerctlPlayerName* playerName =
      static_cast<PlayerctlPlayerName*>(availablePlayers->data);

  // Create the player
  PlayerctlPlayer* const currentPlayer =
      playerctl_player_new_from_name(playerName, nullptr);

  // If no player is available, return nullptr
  if (!currentPlayer)
    return nullptr;

  // Manage the player
  playerctl_player_manager_manage_player(playerManager, currentPlayer);

  // Unref the player
  g_object_unref(currentPlayer);

  return currentPlayer;
}

string GetNowPlaying() {
  if (PlayerctlPlayer* currentPlayer = InitPlayerctl())
    return playerctl_player_get_title(currentPlayer, nullptr);

  return "Could not get now playing info";
}

#endif
