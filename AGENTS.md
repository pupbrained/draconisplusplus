# Draconis++ Agent Guidelines

## Build Commands
- Build project: `meson compile -C build`
- Run tests: `meson test -C build`
- Run specific test: `meson test -C build CoreTypesTest`
- Lint: `clang-tidy -p build src/**/*.cpp`

## Code Style
- C++ Standard: C++26
- Formatting: Follow `.clang-format` (based on Chromium)
- Column limit: Unlimited (prefer reasonable length)
- Indentation: 2 spaces, indented namespaces, BlockIndent for open brackets

## Naming Conventions
- Classes/Structs/Enums: `CamelCase`
- Methods/Variables: `camelBack` 
- Private members: `m_camelBack`
- Static Constants: `UPPER_CASE`
- Template Parameters: `CamelCase` for types, `lower_case` for non-types

## Import Organization
1. Standard library headers `<...>`
2. Drac++/Utils headers
3. Other Drac++ headers
4. Project headers in quotes `"..."`

## Error Handling
- Use `draconis::utils::Error` class for error propagation
- Avoid exceptions for control flow
- Prefer early returns for error conditions

## Type System
- Use type aliases from `Drac++/Utils/Types.hpp` (i32, String, etc.)
- Use strongly typed wrappers like `BytesToGiB` for domain types