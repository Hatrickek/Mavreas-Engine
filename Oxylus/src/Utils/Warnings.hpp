#pragma once
// Pragmas to store / restore the warning state and to disable individual warnings
#ifdef OX_COMPILER_CLANG
  #define OX_PRAGMA(x) _Pragma(#x)
  #define OX_SUPPRESS_WARNING_PUSH OX_PRAGMA(clang diagnostic push)
  #define OX_SUPPRESS_WARNING_POP OX_PRAGMA(clang diagnostic pop)
  #define OX_CLANG_SUPPRESS_WARNING(w) OX_PRAGMA(clang diagnostic ignored w)
  #if __clang_major__ >= 13
    #define OX_CLANG_13_PLUS_SUPPRESS_WARNING(w) OX_CLANG_SUPPRESS_WARNING(w)
  #else
    #define OX_CLANG_13_PLUS_SUPPRESS_WARNING(w)
  #endif
  #if __clang_major__ >= 16
    #define OX_CLANG_16_PLUS_SUPPRESS_WARNING(w) OX_CLANG_SUPPRESS_WARNING(w)
  #else
    #define OX_CLANG_16_PLUS_SUPPRESS_WARNING(w)
  #endif
#else
  #define OX_CLANG_SUPPRESS_WARNING(w)
  #define OX_CLANG_13_PLUS_SUPPRESS_WARNING(w)
  #define OX_CLANG_16_PLUS_SUPPRESS_WARNING(w)
#endif
#ifdef OX_COMPILER_GCC
  #define OX_PRAGMA(x) _Pragma(#x)
  #define OX_SUPPRESS_WARNING_PUSH OX_PRAGMA(GCC diagnostic push)
  #define OX_SUPPRESS_WARNING_POP OX_PRAGMA(GCC diagnostic pop)
  #define OX_GCC_SUPPRESS_WARNING(w) OX_PRAGMA(GCC diagnostic ignored w)
#else
  #define OX_GCC_SUPPRESS_WARNING(w)
#endif
#ifdef OX_COMPILER_MSVC
  #define OX_PRAGMA(x) __pragma(x)
  #define OX_SUPPRESS_WARNING_PUSH OX_PRAGMA(warning(push))
  #define OX_SUPPRESS_WARNING_POP OX_PRAGMA(warning(pop))
  #define OX_MSVC_SUPPRESS_WARNING(w) OX_PRAGMA(warning(disable : w))
  #if _MSC_VER >= 1920 && _MSC_VER < 1930
    #define OX_MSVC2019_SUPPRESS_WARNING(w) OX_MSVC_SUPPRESS_WARNING(w)
  #else
    #define OX_MSVC2019_SUPPRESS_WARNING(w)
  #endif
#else
  #define OX_MSVC_SUPPRESS_WARNING(w)
  #define OX_MSVC2019_SUPPRESS_WARNING(w)
#endif

#define OX_PUSH_WARNING_LEVEL0 OX_PRAGMA(warning(push, 0))
#define OX_POP_WARNING_LEVEL0 OX_PRAGMA(warning(pop))
