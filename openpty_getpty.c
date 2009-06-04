/* ind/pty_getpty.c
 *
 * (BSD license without advertising clause below)
 *
 * Copyright (c) 2009 Thomas Habets. All rights reserved.
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
/* */
#elif defined (__SVR4) && defined (__sun)
/* */
#elif defined (HAVE__GETPTY)
#include <utmp.h>
#include <unistd.h>
#include <strings.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stropts.h>
#include <errno.h>
#include <stropts.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

struct winsize;
struct termios;

int do_close(int fd);

/**
 * taken from pts(7D), and added error handling
 */
int
openpty(int  *amaster, int *aslave, char *name,
	struct termios *termp, struct winsize *winp)
{
  int fds = -1;
  int fdm = -1;
  int saved_errno = 0;
  char *line;
  
  line = _getpty(&fdm, O_RDWR|O_NDELAY, 0600, 0);
  if (!line) {
    fprintf(stderr, "_getpty() failed\n");
    goto errout;
  }
  if (0 > (fds = open(line, O_RDWR))) {
    goto errout;
  }


  *amaster = fdm;
  *aslave = fds;

  if (name) {
    strcpy(name, ptsname(*amaster));
  }
  printf("%d %d\n", *amaster, *aslave);
  if (termp) {
    if (0 > tcsetattr(*aslave, TCSADRAIN, termp)) {
      fprintf(stderr, "lala\n");
      goto errout;
    } 
  }
  if (winp) {
    if (0 > ioctl(*aslave, TIOCSWINSZ, winp)) {
      fprintf(stderr, "lolo\n");
      goto errout;
    }
  }
  return 0;

 errout:
  saved_errno = errno;
  if (0 < fds) {
    do_close(fds);
  }
  if (0 < fdm) {
    do_close(fdm);
  }
  if (name) {
    *name = 0;
  }
  errno = saved_errno;
  return -1;
}

/**
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * fill-column: 79
 * End:
 */
#endif
