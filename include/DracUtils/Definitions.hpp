#pragma once

// Fixes conflict in Windows with <windows.h>
#ifdef _WIN32
  #undef ERROR
#endif // _WIN32

#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) || defined(__ia64__) || defined(__ppc64__) || defined(__powerpc64__) || defined(__mips64) || defined(__LP64__)
  #define DRAC_ARCH_64BIT
#else
  #define DRAC_ARCH_32BIT
#endif

/// Macro alias for trailing return type functions.
#define fn auto
