#pragma once

// Fixes conflict in Windows with <windows.h>
#ifdef _WIN32
  #undef ERROR
#endif // _WIN32

/// Macro alias for trailing return type functions.
#define fn auto

/// Macro alias for dynamically typed variables.
#define var auto
