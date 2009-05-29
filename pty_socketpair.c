/* ind/pty_socketpair.c - fake openpty() using socketpair()
 *
 * (BSD license without advertising clause below)
 *
 * Copyright (c) 2005-2008 Thomas Habets. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const int ISO_C_forbids_an_empty_source_file = 1;
#ifdef HAVE_OPENPTY
/* system has real openpty() */
#elif defined (__SVR4) && defined (__sun)
/* We have one locally for Solaris */
#else
/* don't have real openpty(), and have no fix. Fake using socketpair() */
#include <utmp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>

struct winsize;
struct termios;

/**
 *
 */
int
openpty(int  *amaster,  int  *aslave,  char  *name, struct termios
	*termp, struct winsize * winp)
{
  int fd[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) {
    return -1;
  }
  *amaster = fd[0];
  *aslave = fd[1];
  if (name) {
    strcpy(name, "/dev/FIXME");
  }
  return 0;

 errout:
  saved_errno = errno;
  if (0 < fd[0]) {
    close(fd[0]);
  }
  if (0 < fd[1]) {
    close(fd[1]);
  }
  if (name) {
    *name = 0;
  }
  errno = saved_errno;
  return -1;
}

/**
 *
 */
int
login_tty(int fd)
{
#ifdef NOT_A_TTY
  if (0 > setsid()) {
    return -1;
  }
  if (ioctl(fd, TIOCSCTTY, 1) < 0) {
    return -1;
  }
#endif
  if (0 > dup2(fd, 0)) {
    return -1;
  }
  if (0 > dup2(fd, 1)) {
    return -1;
  }
  if (0 > dup2(fd, 2)) {
    return -1;
  }
  close(fd);
  return 0;
}

/**
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * fill-column: 79
 * End:
 */

#endif
