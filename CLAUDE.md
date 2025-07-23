# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

This project uses Meson build system. A Nix development environment is available but not required.

### Core Commands
- **Build**: `meson compile -C build`
- **Configure**: `meson setup build --wipe`
- **Run**: `meson compile -C build && build/draconis++`
- **Tests**: Enable with `-Dbuild_tests=true` during setup, then run tests from build directory

### Development Setup (Optional)
A Nix shell provides dependencies and helper scripts:
```bash
nix develop
# Then use: build, clean, run scripts
```

### Available Options
Key meson options:
- `-Dprecompiled_config=true/false` - Use precompiled config vs runtime TOML parsing
- `-Dbuild_tests=true/false` - Build test suite
- `-Dbuild_examples=true/false` - Build example applications
- `-Dweather=enabled/disabled` - Weather service support
- `-Dpackagecount=enabled/disabled` - Package counting features
- `-Dnowplaying=enabled/disabled` - Media player integration

## Architecture

Draconis++ is a cross-platform system information utility written in modern C++26.

### Core Structure
- **`src/CLI/`** - Main CLI application and UI components
- **`src/Lib/`** - Core library with platform abstractions
- **`include/Drac++/`** - Public API headers
- **`examples/`** - Example applications (MCP server, Vulkan app, etc.)

### Key Components
1. **Core System (`include/Drac++/Core/System.hpp`)** - Cross-platform system info interface
2. **Services** - Weather APIs, package managers, media player integration
3. **Platform Abstractions (`src/Lib/OS/`)** - OS-specific implementations (Windows, macOS, Linux, BSD, Haiku, SerenityOS)
4. **Wrappers (`src/Lib/Wrappers/`)** - C++ wrappers for system APIs (Curl, DBus, Wayland, XCB)

### Type System
Uses custom type aliases in `Drac++/Utils/Types.hpp`:
- `String`, `Vec<T>`, `Option<T>`, `Result<T>` 
- Platform detection macros: `DRAC_ARCH_*`, `DRAC_ENABLE_*`
- `fn` macro for `auto` function return types

### Dependencies
- **Magic Enum** - Enum reflection
- **TOML++** - Configuration parsing (runtime mode only)
- **Glaze** - JSON/BEVE serialization  
- **libcurl** - HTTP requests for weather services
- **Platform-specific**: DBus, XCB/Wayland (Linux), Windows APIs, macOS frameworks

### Configuration
Two modes:
1. **Runtime TOML parsing** - Reads TOML file from user's configuration directory
2. **Precompiled configuration** - Uses `config.hpp` with compile-time constants (based on `config.example.hpp`)

For precompiled mode, copy `config.example.hpp` to `config.hpp` and customize.

## Testing

Test files are in `src/Lib/Tests/`:
- Unit tests for core utilities
- Integration tests for package counting
- Mock tests for external services

## Platform Support

Supports Windows, macOS, Linux, *BSD, Haiku, and SerenityOS with platform-specific optimizations and feature detection.