# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ([2.61])
AC_INIT([ind], [0.12], [thomas@habets.pp.se])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([ind.c])
AC_CONFIG_HEADERS(config.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
#AC_CHECK_LIB([nsl], [netname2user])
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([util], [openpty])

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h strings.h sys/ioctl.h sys/socket.h termios.h unistd.h utmp.h pty.h util.h libutil.h alloca.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([openpty dup2 memchr select strchr strdup strerror _getpty])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

echo "
  $PACKAGE_NAME version $PACKAGE_VERSION
  Prefix.........: $prefix
  Debug Build....: $debug
  C Compiler.....: $CC $CFLAGS $CPPFLAGS
  Linker.........: $LD $LDFLAGS $LIBS
"
