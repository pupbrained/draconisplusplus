#pragma once

// Fixes conflict in Windows with <windows.h>
#ifdef _WIN32
  #undef ERROR
#endif

#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__) || defined(__aarch64__) || defined(__ia64__) || defined(__ppc64__) || defined(__powerpc64__) || defined(__mips64) || defined(__LP64__)
  #define DRAC_ARCH_64BIT 1
#else
  #define DRAC_ARCH_64BIT 0
#endif

#if defined(__x86_64__) || defined(__amd64__)
  #define DRAC_ARCH_X86_64 1
#else
  #define DRAC_ARCH_X86_64 0
#endif

#if defined(__i686__) || defined(__i386__)
  #define DRAC_ARCH_I686 1
#else
  #define DRAC_ARCH_I686 0
#endif

#if defined(__aarch64__) || defined(__arm64__)
  #define DRAC_ARCH_AARCH64 1
#else
  #define DRAC_ARCH_AARCH64 0
#endif

#if defined(__arm__)
  #define DRAC_ARCH_ARM 1
#else
  #define DRAC_ARCH_ARM 0
#endif

/// Macro alias for trailing return type functions.
#define fn auto
