#ifndef MORE_WARNINGS_H
#define MORE_WARNINGS_H

// warning options tested with gcc 4.4.6 and newer (up to 7.x).
// more gcc 5+ options tbd (if any) (clang defines some __GNUC__ value (4+?)).

// (Ø) public domain, like https://creativecommons.org/publicdomain/zero/1.0/

#if defined(__linux__) && __linux__ || defined(__CYGWIN__) && __CYGWIN__
// on linux: man feature_test_macros -- try ftm.c at the end of it
// outcomment on demand...
#define _DEFAULT_SOURCE 1
// for older glibc's on linux (< 2.19 -- e.g. rhel7 uses 2.17...)
#define _BSD_SOURCE 1
#define _SVID_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#define _ATFILE_SOURCE 1
// more extensions (less portability?)
//#define _GNU_SOURCE 1
#endif

// gcc -dM -E -xc /dev/null | grep -i gnuc
// clang -dM -E -xc /dev/null | grep -i gnuc
#if defined (__GNUC__)

#if 0 // change to '#if 1' whenever there is desire to see these...
#pragma GCC diagnostic warning "-Wpadded"
#pragma GCC diagnostic warning "-Wpedantic" // gcc 4.4.6 does not know this
#endif

// to relax, change 'error' to 'warning' -- or even 'ignored'
// selectively. use #pragma GCC diagnostic push/pop to change
// the rules temporarily

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"

#if __GNUC__ >= 7

// gcc manual says all kind of /* fall.*through */ regexp's work too
// but perhaps only when cpp does not filter comments out. thus...
#define FALL_THROUGH __attribute__ ((fallthrough))
#else
#define FALL_THROUGH ((void)0)

#endif /* __GNUC__ >= 7 */

#pragma GCC diagnostic error "-Wstrict-prototypes"
#pragma GCC diagnostic error "-Winit-self"

// -Wformat=2 ¡currently! (2017-11) equivalent of the following 4
#pragma GCC diagnostic error "-Wformat"
#pragma GCC diagnostic error "-Wformat-nonliteral"
#pragma GCC diagnostic error "-Wformat-security"
#pragma GCC diagnostic error "-Wformat-y2k"

#pragma GCC diagnostic error "-Wcast-align"
#pragma GCC diagnostic error "-Wpointer-arith"
#pragma GCC diagnostic error "-Wwrite-strings"
#pragma GCC diagnostic error "-Wcast-qual"
#pragma GCC diagnostic error "-Wshadow"
#pragma GCC diagnostic error "-Wmissing-include-dirs"
#pragma GCC diagnostic error "-Wundef"
#pragma GCC diagnostic error "-Wbad-function-cast"

#ifndef __clang__ // XXX revisit -- tried with clang 3.8.0
#pragma GCC diagnostic error "-Wlogical-op"
#endif

#pragma GCC diagnostic error "-Waggregate-return"
#pragma GCC diagnostic error "-Wold-style-definition"
#pragma GCC diagnostic error "-Wmissing-prototypes"
#pragma GCC diagnostic error "-Wmissing-declarations"
#pragma GCC diagnostic error "-Wredundant-decls"
#pragma GCC diagnostic error "-Wnested-externs"
#pragma GCC diagnostic error "-Winline"
#pragma GCC diagnostic error "-Wvla"
#pragma GCC diagnostic error "-Woverlength-strings"
#pragma GCC diagnostic error "-Wuninitialized"

//ragma GCC diagnostic error "-Wfloat-equal"
//ragma GCC diagnostic error "-Werror"
//ragma GCC diagnostic error "-Wconversion"

// avoiding known problems...
#if __GNUC__ == 4
#ifndef __clang__
#pragma GCC diagnostic warning "-Winline" // gcc 4.4.6
#pragma GCC diagnostic warning "-Wuninitialized" // gcc 4.4.6, 4.8.5 ...
#endif
#endif

#endif /* defined (__GNUC__) */

#endif /* MORE_WARNINGS_H */
