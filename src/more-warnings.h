#ifndef MORE_WARNINGS_H
#define MORE_WARNINGS_H 1

// (Ø) public domain, like https://creativecommons.org/publicdomain/zero/1.0/

#if defined(__linux__) && __linux__ || defined(__CYGWIN__) && __CYGWIN__
// on linux: man feature_test_macros -- try ftm.c at the end of it
#define _DEFAULT_SOURCE 1
// for older glibc's on linux (< 2.19 -- e.g. rhel7 uses 2.17...)
#define _BSD_SOURCE 1
#define _SVID_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#define _ATFILE_SOURCE 1
// more extensions (less portability?)
//#define _GNU_SOURCE 1
#endif

// hint: gcc -dM -E -xc /dev/null | grep -i gnuc
// also: clang -dM -E -xc /dev/null | grep -i gnuc
#if defined (__GNUC__) && defined (__STDC__)

//#define WERROR 1 // uncomment or enter -DWERROR on command line/the includer

#define DO_PRAGMA(x) _Pragma(#x)
#if defined (WERROR) && WERROR
#define PRAGMA_GCC_DIAG(w) DO_PRAGMA(GCC diagnostic error #w)
#else
#define PRAGMA_GCC_DIAG(w) DO_PRAGMA(GCC diagnostic warning #w)
#endif

#define PRAGMA_GCC_DIAG_E(w) DO_PRAGMA(GCC diagnostic error #w)
#define PRAGMA_GCC_DIAG_W(w) DO_PRAGMA(GCC diagnostic warning #w)
#define PRAGMA_GCC_DIAG_I(w) DO_PRAGMA(GCC diagnostic ignored #w)

#if 0 // use of -Wpadded gets complicated, 32 vs 64 bit systems
PRAGMA_GCC_DIAG_W (-Wpadded)
#endif

// to relax, change 'error' to 'warning' -- or even 'ignored'
// selectively. use #pragma GCC diagnostic push/pop to change the
// rules for a block of code in the source files including this.

PRAGMA_GCC_DIAG (-Wall)
PRAGMA_GCC_DIAG (-Wextra)

#if __GNUC__ >= 8 // impractically strict in gccs 5, 6 and 7
PRAGMA_GCC_DIAG (-Wpedantic)
#endif

#if __GNUC__ >= 7 || defined (__clang__) && __clang_major__ >= 12

// gcc manual says all kind of /* fall.*through */ regexp's work too
// but perhaps only when cpp does not filter comments out. thus...
#define FALL_THROUGH __attribute__ ((fallthrough))
#else
#define FALL_THROUGH ((void)0)
#endif

#ifndef __cplusplus
PRAGMA_GCC_DIAG (-Wstrict-prototypes)
PRAGMA_GCC_DIAG (-Wbad-function-cast)
PRAGMA_GCC_DIAG (-Wold-style-definition)
PRAGMA_GCC_DIAG (-Wmissing-prototypes)
PRAGMA_GCC_DIAG (-Wnested-externs)
#endif

// -Wformat=2 ¡currently! (2020-11-11) equivalent of the following 4
PRAGMA_GCC_DIAG (-Wformat)
PRAGMA_GCC_DIAG (-Wformat-nonliteral)
PRAGMA_GCC_DIAG (-Wformat-security)
PRAGMA_GCC_DIAG (-Wformat-y2k)

PRAGMA_GCC_DIAG (-Winit-self)
PRAGMA_GCC_DIAG (-Wcast-align)
PRAGMA_GCC_DIAG (-Wpointer-arith)
PRAGMA_GCC_DIAG (-Wwrite-strings)
PRAGMA_GCC_DIAG (-Wcast-qual)
PRAGMA_GCC_DIAG (-Wshadow)
PRAGMA_GCC_DIAG (-Wmissing-include-dirs)
PRAGMA_GCC_DIAG (-Wundef)

#ifndef __clang__ // XXX revisit -- tried with clang 3.8.0
PRAGMA_GCC_DIAG (-Wlogical-op)
#endif

#ifndef __cplusplus // supported by c++ compiler but perhaps not worth having
PRAGMA_GCC_DIAG (-Waggregate-return)
#endif

PRAGMA_GCC_DIAG (-Wmissing-declarations)
PRAGMA_GCC_DIAG (-Wredundant-decls)
PRAGMA_GCC_DIAG (-Winline)
PRAGMA_GCC_DIAG (-Wvla)
PRAGMA_GCC_DIAG (-Woverlength-strings)
PRAGMA_GCC_DIAG (-Wuninitialized)

//PRAGMA_GCC_DIAG (-Wfloat-equal)
//PRAGMA_GCC_DIAG (-Wconversion)

// avoiding known problems (turning some errors set above to warnings)...
#if __GNUC__ == 4
#ifndef __clang__
PRAGMA_GCC_DIAG_W (-Winline) // gcc 4.4.6 ...
PRAGMA_GCC_DIAG_W (-Wuninitialized) // gcc 4.4.6, 4.8.5 ...
#endif
#endif

#undef PRAGMA_GCC_DIAG_I
#undef PRAGMA_GCC_DIAG_W
#undef PRAGMA_GCC_DIAG_E
#undef PRAGMA_GCC_DIAG
#undef DO_PRAGMA

#endif /* defined (__GNUC__) && defined (__STDC__) */

/* name and interface from talloc.c */
#ifndef discard_const_p // probably never defined, but...
//#include <stdint.h>
#if defined (__INTPTR_TYPE__) /* e.g. gcc 4.8.5 - gcc 10.2 (2020-11-11) */
#define discard_const_p(type, ptr) ((type *)((__INTPTR_TYPE__)(ptr)))
#elif defined (__PTRDIFF_TYPE__) /* e.g. gcc 4.4.6 */
#define discard_const_p(type, ptr) ((type *)((__PTRDIFF_TYPE__)(ptr)))
#else
#define discard_const_p(type, ptr) ((type *)(ptr))
#endif
#endif

#endif /* MORE_WARNINGS_H */
