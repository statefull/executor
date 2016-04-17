#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(executor, 1.0, statefull@gmail.com)
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADERS([config.h])

EXECUTOR_CPPFLAGS="-I../includes --pedantic -Wall -std=c++11 -pthread"
AC_SUBST(EXECUTOR_CPPFLAGS)

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
# Checks Makefile
AM_INIT_AUTOMAKE


# Checks for library functions.
LT_INIT

AC_OUTPUT(Makefile src/Makefile)
