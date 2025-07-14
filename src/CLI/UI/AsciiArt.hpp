#pragma once

#include <Drac++/Core/System.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Types.hpp>

namespace draconis::ui::ascii {
  namespace {
    using utils::types::Array;
    using utils::types::Pair;
    using utils::types::Result;
    using utils::types::String;
    using utils::types::StringView;
    using utils::types::usize;
    using utils::types::Vec;
  } // namespace

  namespace logos {
    constexpr StringView NIXOS =
      "\033[1m\033[38;5;12m          ▗▄▄▄       \033[38;5;14m▗▄▄▄▄    ▄▄▄▖\n"
      "\033[1m\033[38;5;12m          ▜███▙       \033[38;5;14m▜███▙  ▟███▛\n"
      "\033[1m\033[38;5;12m           ▜███▙       \033[38;5;14m▜███▙▟███▛\n"
      "\033[1m\033[38;5;12m            ▜███▙       \033[38;5;14m▜██████▛\n"
      "\033[1m\033[38;5;12m     ▟█████████████████▙ \033[38;5;14m▜████▛     \033[38;5;12m▟▙\n"
      "\033[1m\033[38;5;12m    ▟███████████████████▙ \033[38;5;14m▜███▙    \033[38;5;12m▟██▙\n"
      "\033[1m\033[38;5;14m           ▄▄▄▄▖           ▜███▙  \033[38;5;12m▟███▛\n"
      "\033[1m\033[38;5;14m          ▟███▛             ▜██▛ \033[38;5;12m▟███▛\n"
      "\033[1m\033[38;5;14m         ▟███▛               ▜▛ \033[38;5;12m▟███▛\n"
      "\033[1m\033[38;5;14m▟███████████▛                  \033[38;5;12m▟██████████▙\n"
      "\033[1m\033[38;5;14m▜██████████▛                  \033[38;5;12m▟███████████▛\n"
      "\033[1m\033[38;5;14m      ▟███▛ \033[38;5;12m▟▙               ▟███▛\n"
      "\033[1m\033[38;5;14m     ▟███▛ \033[38;5;12m▟██▙             ▟███▛\n"
      "\033[1m\033[38;5;14m    ▟███▛  \033[38;5;12m▜███▙           ▝▀▀▀▀\n"
      "\033[1m\033[38;5;14m    ▜██▛    \033[38;5;12m▜███▙ \033[38;5;14m▜██████████████████▛\n"
      "\033[1m\033[38;5;14m     ▜▛     \033[38;5;12m▟████▙ \033[38;5;14m▜████████████████▛\n"
      "\033[1m\033[38;5;12m           ▟██████▙       \033[38;5;14m▜███▙\n"
      "\033[1m\033[38;5;12m          ▟███▛▜███▙       \033[38;5;14m▜███▙\n"
      "\033[1m\033[38;5;12m         ▟███▛  ▜███▙       \033[38;5;14m▜███▙\n"
      "\033[1m\033[38;5;12m         ▝▀▀▀    ▀▀▀▀▘       \033[38;5;14m▀▀▀▘\033[0m";

    constexpr StringView MACOS =
      "\033[1m\033[38;5;10m                     ..'\n"
      "\033[1m\033[38;5;10m                 ,xNMM.\n"
      "\033[1m\033[38;5;10m               .OMMMMo\n"
      "\033[1m\033[38;5;10m               lMM\"\n"
      "\033[1m\033[38;5;10m     .;loddo:.  .olloddol;.\n"
      "\033[1m\033[38;5;10m   cKMMMMMMMMMMNWMMMMMMMMMM0:\n"
      "\033[1m\033[38;5;11m .KMMMMMMMMMMMMMMMMMMMMMMMWd.\n"
      "\033[1m\033[38;5;11m XMMMMMMMMMMMMMMMMMMMMMMMX.\n"
      "\033[1m\033[38;5;9m;MMMMMMMMMMMMMMMMMMMMMMMM:\n"
      "\033[1m\033[38;5;9m:MMMMMMMMMMMMMMMMMMMMMMMM:\n"
      "\033[1m\033[38;5;9m.MMMMMMMMMMMMMMMMMMMMMMMMX.\n"
      "\033[1m\033[38;5;9m kMMMMMMMMMMMMMMMMMMMMMMMMWd.\n"
      "\033[1m\033[38;5;13m 'XMMMMMMMMMMMMMMMMMMMMMMMMMMk\n"
      "\033[1m\033[38;5;13m  'XMMMMMMMMMMMMMMMMMMMMMMMMK.\n"
      "\033[1m\033[38;5;12m    kMMMMMMMMMMMMMMMMMMMMMMd\n"
      "\033[1m\033[38;5;12m     ;KMMMMMMMWXXWMMMMMMMk.\n"
      "\033[1m\033[38;5;12m       \"cooc*\"    \"*coo'\"\033[0m";

    constexpr StringView UBUNTU =
      "\033[1m\033[38;5;9m                             ....\n"
      "\033[1m\033[38;5;9m              .',:clooo:  .:looooo:.\n"
      "\033[1m\033[38;5;9m           .;looooooooc  .oooooooooo'\n"
      "\033[1m\033[38;5;9m        .;looooool:,'''.  :ooooooooooc\n"
      "\033[1m\033[38;5;9m       ;looool;.         'oooooooooo,\n"
      "\033[1m\033[38;5;9m      ;clool'             .cooooooc.  ,,\n"
      "\033[1m\033[38;5;9m         ...                ......  .:oo,\n"
      "\033[1m\033[38;5;9m  .;clol:,.                        .loooo'\n"
      "\033[1m\033[38;5;9m :ooooooooo,                        'ooool\n"
      "\033[1m\033[38;5;9m'ooooooooooo.                        loooo.\n"
      "\033[1m\033[38;5;9m'ooooooooool                         coooo.\n"
      "\033[1m\033[38;5;9m ,loooooooc.                        .loooo.\n"
      "\033[1m\033[38;5;9m   .,;;;'.                          ;ooooc\n"
      "\033[1m\033[38;5;9m       ...                         ,ooool.\n"
      "\033[1m\033[38;5;9m    .cooooc.              ..',,'.  .cooo.\n"
      "\033[1m\033[38;5;9m      ;ooooo:.           ;oooooooc.  :l.\n"
      "\033[1m\033[38;5;9m       .coooooc,..      coooooooooo.\n"
      "\033[1m\033[38;5;9m         .:ooooooolc:. .ooooooooooo'\n"
      "\033[1m\033[38;5;9m           .':loooooo;  ,oooooooooc\n"
      "\033[1m\033[38;5;9m               ..';::c'  .;loooo:'\033[0m";

    constexpr StringView ARCH =
      "\033[1m\033[38;5;14m                  -`\n"
      "\033[1m\033[38;5;14m                 .o+`\n"
      "\033[1m\033[38;5;14m                `ooo/\n"
      "\033[1m\033[38;5;14m               `+oooo:\n"
      "\033[1m\033[38;5;14m              `+oooooo:\n"
      "\033[1m\033[38;5;14m              -+oooooo+:\n"
      "\033[1m\033[38;5;14m            `/:-:++oooo+:\n"
      "\033[1m\033[38;5;14m           `/++++/+++++++:\n"
      "\033[1m\033[38;5;14m          `/++++++++++++++:\n"
      "\033[1m\033[38;5;14m         `/+++oooooooooooooo/`\n"
      "\033[1m\033[38;5;14m        ./ooosssso++osssssso+`\n"
      "\033[1m\033[38;5;14m       .oossssso-````/ossssss+`\n"
      "\033[1m\033[38;5;14m      -osssssso.      :ssssssso.\n"
      "\033[1m\033[38;5;14m     :osssssss/        osssso+++.\n"
      "\033[1m\033[38;5;14m    /ossssssss/        +ssssooo/-.\n"
      "\033[1m\033[38;5;14m  `/ossssso+/:-        -:/+osssso+-\n"
      "\033[1m\033[38;5;14m `+sso+:-`                 `.-/+oso:\n"
      "\033[1m\033[38;5;14m`++:.                           `-/+/\n"
      "\033[1m\033[38;5;14m.`                                 `/`\033[0m";

    constexpr StringView DEBIAN =
      "\033[1m\033[38;5;15m       _,met$$$$$gg.\n"
      "\033[1m\033[38;5;15m    ,g$$$$$$$$$$$$$$$P.\n"
      "\033[1m\033[38;5;15m  ,g$$P\"        \"\"\"Y$$.\".\n"
      "\033[1m\033[38;5;15m ,$$P'              `$$$.\n"
      "\033[1m\033[38;5;15m',$$P       ,ggs.     `$$b:\n"
      "\033[1m\033[38;5;15m`d$$'     ,$P\"'   \033[38;5;9m.\033[38;5;15m    $$$\n"
      "\033[1m\033[38;5;15m $$P      d$'     \033[38;5;9m,\033[38;5;15m    $$P\n"
      "\033[1m\033[38;5;15m $$:      $$.   \033[38;5;9m-\033[38;5;15m    ,d$$'\n"
      "\033[1m\033[38;5;15m $$;      Y$b._   _,d$P'\n"
      "\033[1m\033[38;5;15m Y$$.    \033[38;5;9m`.\033[38;5;15m`\"Y$$$$P\"'\n"
      "\033[1m\033[38;5;15m `$$b      \033[38;5;9m\"-.__\n"
      "\033[1m\033[38;5;15m  `Y$$\n"
      "\033[1m\033[38;5;15m   `Y$$.\n"
      "\033[1m\033[38;5;15m     `$$b.\n"
      "\033[1m\033[38;5;15m       `Y$$b.\n"
      "\033[1m\033[38;5;15m          `\"Y$b._\n"
      "\033[1m\033[38;5;15m              `\"\"\"\033[0m";

    constexpr StringView FEDORA =
      "\033[1m\033[38;5;12m             .',;::::;,'.\n"
      "\033[1m\033[38;5;12m         .';:cccccccccccc:;,.\n"
      "\033[1m\033[38;5;12m      .;cccccccccccccccccccccc;.\n"
      "\033[1m\033[38;5;12m    .:cccccccccccccccccccccccccc:.\n"
      "\033[1m\033[38;5;12m  .;ccccccccccccc;\033[38;5;15m.:dddl:.\033[38;5;12m;ccccccc;.\n"
      "\033[1m\033[38;5;12m .:ccccccccccccc;\033[38;5;15mOWMKOOXMWd\033[38;5;12m;ccccccc:.\n"
      "\033[1m\033[38;5;12m.:ccccccccccccc;\033[38;5;15mKMMc\033[38;5;12m;cc;\033[38;5;15mxMMc\033[38;5;12m;ccccccc:.\n"
      "\033[1m\033[38;5;12m,cccccccccccccc;\033[38;5;15mMMM.\033[38;5;12m;cc;\033[38;5;15m;WW:\033[38;5;12m;cccccccc,\n"
      "\033[1m\033[38;5;12m:cccccccccccccc;\033[38;5;15mMMM.\033[38;5;12m;cccccccccccccccc:\n"
      "\033[1m\033[38;5;12m:ccccccc;\033[38;5;15moxOOOo\033[38;5;12m;\033[38;5;15mMMM000k.\033[38;5;12m;cccccccccccc:\n"
      "\033[1m\033[38;5;12mcccccc;\033[38;5;15m0MMKxdd:\033[38;5;12m;\033[38;5;15mMMMkddc.\033[38;5;12m;cccccccccccc;\n"
      "\033[1m\033[38;5;12mccccc;\033[38;5;15mXMO'\033[38;5;12m;cccc;\033[38;5;15mMMM.\033[38;5;12m;cccccccccccccccc'\n"
      "\033[1m\033[38;5;12mccccc;\033[38;5;15mMMo\033[38;5;12m;ccccc;\033[38;5;15mMMW.\033[38;5;12m;ccccccccccccccc;\n"
      "\033[1m\033[38;5;12mccccc;\033[38;5;15m0MNc.\033[38;5;12mccc\033[38;5;15m.xMMd\033[38;5;12m;ccccccccccccccc;\n"
      "\033[1m\033[38;5;12mcccccc;\033[38;5;15mdNMWXXXWM0:\033[38;5;12m;cccccccccccccc:,\n"
      "\033[1m\033[38;5;12mcccccccc;\033[38;5;15m.:odl:.\033[38;5;12m;cccccccccccccc:,.\n"
      "\033[1m\033[38;5;12mccccccccccccccccccccccccccccc:'.\n"
      "\033[1m\033[38;5;12m:ccccccccccccccccccccccc:;,..\n"
      "\033[1m\033[38;5;12m ':cccccccccccccccc::;,.\033[0m";
  } // namespace logos

  constexpr fn GetAsciiArt(StringView operatingSystem) -> Vec<StringView> {
    using namespace logos;

    String matchOS(operatingSystem);

    // clang-format off
    static constexpr Array<Pair<StringView, StringView>, 6> LOGOS = {{
       {  "nixos",  NIXOS },
       {  "macos",  MACOS },
       { "ubuntu", UBUNTU },
       {   "arch",   ARCH },
       { "debian", DEBIAN },
       { "fedora", FEDORA },
    }};
    // clang-format on

    StringView asciiArt;

    for (const auto& [key, art] : LOGOS)
      if (matchOS.find(key) != String::npos) {
        asciiArt = art;
        break;
      }

    if (asciiArt.empty())
      return {};

    Vec<StringView> lines;
    usize           pos   = 0;
    usize           start = 0;

    while (pos < asciiArt.size()) {
      if (asciiArt[pos] == '\n') {
        lines.push_back(asciiArt.substr(start, pos - start));
        start = pos + 1;
      }

      pos++;
    }

    if (start < asciiArt.size())
      lines.push_back(asciiArt.substr(start));

    return lines;
  }
} // namespace draconis::ui::ascii
