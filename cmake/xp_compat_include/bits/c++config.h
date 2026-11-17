#pragma once

#include_next <bits/c++config.h>

/* Arch mingw-w64 libstdc++ is configured for UCRT; undefine UCRT-only functions
   when targeting Windows XP msvcrt-os (-mcrtdll=msvcrt-os). */
#undef _GLIBCXX_HAVE_AT_QUICK_EXIT
#undef _GLIBCXX_HAVE_QUICK_EXIT
#undef _GLIBCXX_HAVE_ALIGNED_ALLOC
#undef _GLIBCXX_HAVE_TIMESPEC_GET
