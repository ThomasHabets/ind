/* ind/ind.c
 *
 * ind
 *
 * By Thomas Habets <thomas@habets.pp.se>
 *
 */
/*
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
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <termios.h>
#include <utmp.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __OpenBSD__
#include <util.h>
#endif

#ifdef __FreeBSD__
#include <libutil.h>
#endif

#ifdef __linux__
#include <pty.h>
#endif

#include "pty_solaris.h"

/* Needed for IRIX */
#ifndef STDIN_FILENO
#define STDIN_FILENO    0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO   1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO   2
#endif

/* arbitrary maxlength for prefixes and postfixes. Should be enough */
static const size_t max_indstr_length = 1048576;

static const char *argv0;
static const float version = 0.11f;
static int verbose = 0;

/**
 * EINTR-safe close()
 *
 * @param   fd:  fd to close
 */
int
do_close(int fd)
{
  int err;
  do {
    err = close(fd);
  } while ((-1 == err) && (errno = EINTR)); 
  return err;
}

/**
 * Just like write(), except it really really writes everything, unless
 * there is a real (non-EINTR) error.
 *
 * Note that if there is a non-EINTR error, this function does not
 * reveal how much has been written
 *
 * @param   fd:  fd to write to
 * @param   buf: data to write
 * @param   len: length of data
 *
 * @return  Number of bytes written, or -1 on error (errno set)
 */
static ssize_t
safe_write(int fd, const void *buf, size_t len)
{
  int ret;
  int offset = 0;
  
  while(len > 0) {
    do {
      ret = write(fd, (unsigned char *)buf + offset, len);
    } while ((-1 == ret) && (errno == EINTR));

    if(ret < 0) {
      return ret;
    } else if(ret == 0) {
      return offset;
    }
    offset += ret;
    len -= ret;
  }

  return offset;
}

/**
 * Set up stdout/stderr and exec subprocess
 *
 * @param   fd:   stdin/stdout fd
 * @param   efd:  stderr fd
 * @param   argv: argv of subprocess
 *
 */
static void
child(int fd, int efd, char **argv)
{
  if (0 > login_tty(fd)) {
    fprintf(stderr, "%s: login_tty() failed: %s\n",
	    argv0, strerror(errno));
    exit(1);
  }

  /* stderr fd is overwritten to sidechannel */
  if (-1 == dup2(efd, STDERR_FILENO)) {
    fprintf(stderr, "%s: dup2(stderr) failed: %s\n",
	    argv0, strerror(errno));
    exit(1);
  }

  do_close(efd);
  execvp(argv[0], argv);
  fprintf(stderr, "%s: %s: %s\n", argv0, argv[0], strerror(errno));
  exit(1);
}

/**
 * 
 *
 * 
 */
static struct termios orig_stdin_tio;
static int orig_stdin_tio_ok = 0;
static void
reset_stdin_terminal()
{
  if (!orig_stdin_tio_ok) {
    return;
  }

  /* reset terminal settings */
  if (tcsetattr(STDIN_FILENO, TCSADRAIN, &orig_stdin_tio)) {
    fprintf(stderr,
	    "%s: Unable to restore terminal settings:"
	    "%s: tcsetattr(STDIN_FILENO, TCSADRAIN, orig): %s",
	    argv0, argv0, strerror(errno));
  }
}

/**
 * Print usage information and exit
 *
 * @param  err: value sent to exit()
 */
static void
usage(int err)
{
  
  printf("ind %.2f, by Thomas Habets <thomas@habets.pp.se>\n"
	 "usage: %s [ -h ] [ -p <fmt> ] [ -a <fmt> ] [ -P <fmt> ] "
	 "[ -A <fmt> ]  \n"
	 "          <command> <args> ...\n"
	 "\t-a        Postfix stdout (default: \"\")\n"
	 "\t-A        Postfix stderr (default: \"\")\n"
	 "\t-h        Show this help text\n"
	 "\t-p        Prefix stdout (default: \"  \")\n"
	 "\t-P        Prefix stderr (default: \">>\") \n"
	 "\t-v        Verbose (repeat -v to increase verbosity)\n"
	 "Format:\n"
	 " Normal text, except:\n"
	 "\t%%%%        Insert %%%%\n"
	 "\t%%c        Insert output from ctime(3) function\n"
	 , version, argv0);
  exit(err);
}

/**
 * just like strpbrk(), but haystack is not null-terminated
 *
 * @param  p:       string to search in
 * @param  chars:   characters to look for
 * @param  len:     length of string to search in
 *
 * @return   Pointer to first occurance of any of the characters, or NULL
 *           if not found.
 */
static char *
mempbrk(const char *p, const char *chars, size_t len)
{
  int c;
  char *ret = NULL;
  char *tmp;

  for(c = strlen(chars); c; c--) {
    tmp = memchr(p, chars[c-1], len);
    if (tmp && (!ret || tmp < ret)) {
      ret = tmp;
    }
  }
  return ret;
}

/**
 * In-place remove of all trailing newlines (be they CR or LF)
 *
 * @param     str:    String to change (may be written to)
 * @return            New length of string
 */
static size_t
chomp(char *str)
{
  size_t len;
  len = strlen(str);
  while (len && strchr("\n\r",str[len-1])) {
    str[len - 1] = 0;
    len--;
  }
  return len;
}

/**
 * return malloc()ed and created string, caller calls free()
 * exit(1)s on failure (malloc() failed)
 *
 * @param   fmt:     format string, as specified in the manpage (%c is ctime
 *                   for example)
 * @param   dowarn:  Print warnings on format to stderr
 * @param   output:  place to store the output string pointer at
 */
static void
format(const char *fmt, char **output, int dowarn)
{
  size_t len = 0;
  const char *p = fmt;
  char *out;
  char ct[128];
  time_t t;

  while (*p) {
    if (*p == '%') {
      p++;
      switch(*p) {
      case 0:
	p--;
	break;
      case '%':
	len++;
	break;
      case 'c':
	if (-1 == time(&t)) {
	  static int done = 0;
	  if (!done) {
	    fprintf(stderr, "%s: time() failed: %s\n", argv0, strerror(errno));
	    fprintf(stderr,
		    "%s: time() failed: this message won't be repeated\n",
		    argv0);
	  }
	}
	strncpy(ct, ctime(&t), sizeof(ct)-1);
	ct[sizeof(ct)-1] = 0;
	len += chomp(ct);
	break;
      default:
	break;
      }
    } else {
      len++;
    }
    p++;
    if (len >= max_indstr_length) {
      /* this also prevents integer overflows in the malloc */
      break;
    }
  }
  if (!(*output = (char*)malloc(len+2))) {
    fprintf(stderr, "%s: %s\n", argv0, strerror(errno));
    reset_stdin_terminal();
    exit(1);
  }
  out = *output;
  p = fmt;
  while (*p) {
    if (*p == '%') {
      p++;
      switch(*p) {
      case 'c':
	strcpy(out,ct);
	out = index(out,0);
	break;
      case '%':
	*out++ = '%';
	break;
      case 0:
	p--;
	if (dowarn) {
	  fprintf(stderr, "%s: String ends in %%. Change to %%%%.\n",
		  argv0);
	}
	break;
      default:
	if (dowarn) {
	  fprintf(stderr, "%s: Invalid escape char: %%%c. "
		  "Did you mean %%%%%c?\n",
		  argv0, *p, *p);
	}
	break;
      }
    } else {
      *out++ = *p;
    }
    p++;
  }
  *out = 0;
  assert(strlen(*output) == (unsigned)len);
}

/**
 * Main functionality function.
 * Read from fdin, if crossing a newline add magic.
 *
 * emptyline must point to 1 on first call, since the line is empty
 * before anything is written to it (makes sense).
 *
 * @param   fdin       source fd
 * @param   fdout      destination fd
 * @param   pre        prefix string
 * @param   prelen     prefix length (sent for efficiency)
 * @param   post       postfix string
 * @param   postlen    postfix length (sent for efficiency)
 * @param   emptyline  last this function was called, was it an empty line?
 *
 * @return        0 on success, !0 on "no more data will be readable ever"
 */
static int
process(int fdin,int fdout,
	const char *prefix,
	const char *postfix, int *emptyline)
{
  int n;
  char buf[128];

  char *pre = 0;
  char *post = 0;

  if (!(n = read(fdin, buf, sizeof(buf)-1))) {
    goto errout;
  }

  if (0 > n) {
    switch(errno) {
      /* non-fatal errors */
    case EAGAIN:
    case EINTR:
      goto okout;
      
      /* these mean internal error */
    case EFAULT:
    case EINVAL:
    case EBADF:
    case EISDIR:
      fprintf(stderr, "%s: Internal error: read() returned %d (%s)\n",
	      argv0, errno, strerror(errno));
      exit(1);

      /* these errors mean we may as well close the whole fd */
    case EIO:
    default:
	goto errout;
    }
  } else {
    char *p = buf;
    char *q;

    format(prefix, &pre, 0);
    format(postfix, &post, 0);

    while ((q = mempbrk(p,"\r\n",n))) {
      if (*emptyline) {
	if (0 > safe_write(fdout,pre,strlen(pre))) {
	goto errout;
	}
	*emptyline = 0;
      }
      if (0 > safe_write(fdout,p,q-p)) {
	goto errout;
      }
      if (0 > safe_write(fdout,post,strlen(post))) {
	goto errout;
      }
      if (0 > safe_write(fdout,q,1)) {
	goto errout;
      }
      *emptyline = 1;
      n-=(q-p+1);
      p=q+1;
    }
    if (n) {
      if (*emptyline) {
	if (0 > safe_write(fdout,pre,strlen(pre))) {
	goto errout;
	}
	*emptyline = 0;
      }
      if (0 > safe_write(fdout,p,n)) {
	goto errout;
      }
    }
  }

 okout:
  free(pre);
  free(post);
  return 0;

 errout:
  free(pre);
  free(post);
  return 1;
}

/**
 *
 */
int
main(int argc, char **argv)
{
  int c;
  int s01m, s01s;
  int es[2];
  char *prefix = "  ";
  char *eprefix = ">>";
  char *postfix = "";
  char *epostfix = "";
  unsigned int nclosed = 0;
  int emptyline = 1;
  int eemptyline = 1;
  int childpid;
  int stdin_fileno = STDIN_FILENO;
  struct winsize *wsp;
  struct termios *tiop;

  argv0 = argv[0];
  if (argv[argc]) {
    fprintf(stderr,
	    "%s: Your system is not C compliant!\n"
	    "%s: C99 5.1.2.2.1 says argv[argc] must be NULL.\n"
	    "%s: C89 also requires this.\n"
	    "%s: Please make a bug report to your operating system vendor.\n",
	    argv0, argv0, argv0, argv0);
    return 1;
  }

  while (-1 != (c = getopt(argc, argv, "+hp:a:P:A:v"))) {
    switch(c) {
    case 'h':
      usage(0);
    case 'p':
      prefix = optarg;
      break;
    case 'a':
      postfix = optarg;
      break;
    case 'P':
      eprefix = optarg;
      break;
    case 'A':
      epostfix = optarg;
      break;
    case 'v':
      verbose++;
      break;
    default:
      usage(1);
    }
  }

  if (optind >= argc) {
    usage(1);
  }

  { /* print warnings on format */
    char *tmp;
    format(prefix, &tmp, 1);
    free(tmp);
    format(postfix, &tmp, 1);
    free(tmp);
    format(eprefix, &tmp, 1);
    free(tmp);
    format(epostfix, &tmp, 1);
    free(tmp);
  }

  /* set up winsize */
  wsp = alloca(sizeof(struct winsize));
  if (0 > ioctl(STDOUT_FILENO, TIOCGWINSZ, wsp)) {
    /* if parent terminal (if even a terminal at all) won't give up info
     * on terminal, neither will we.
     */
    wsp = 0;
  }

  if (wsp) {
    int sub = 0;
    char *tmp;

    format(prefix, &tmp, 0);
    sub += strlen(tmp);
    free(tmp);

    format(postfix, &tmp, 0);
    sub += strlen(tmp);
    free(tmp);

    if (sub >= wsp->ws_col) {
      wsp = 0;
    } else {
      wsp->ws_col -= sub;
    }
  }

  /* set up termios */
  tiop = alloca(sizeof(struct termios));
  if (0 > tcgetattr(STDOUT_FILENO, tiop)) {
    /* if parent terminal (if even a terminal at all) won't give up info
     * on terminal, neither will we.
     */
    tiop = 0;
  }

  if (-1 == openpty(&s01m, &s01s, NULL, tiop, wsp)) {
    fprintf(stderr, "%s: openpty() failed: %s\n", argv0, strerror(errno));
    exit(1);
  }

  {
    char *tty;
#ifdef CONSTANT_PTSMASTER
    if (verbose) {
      printf("%s: pty master name: %s\n", argv0, CONSTANT_PTSMASTER);
    }
#else
    if (!(tty = ttyname(s01m))) {
      fprintf(stderr, "%s: ttyname(master) failed: %d %s\n",
	      argv0, errno, strerror(errno));
    } else if (verbose) {
      printf("%s: pty master name: %s\n", argv0, tty);
    }
#endif
    if (!(tty = ttyname(s01s))) {
      fprintf(stderr, "%s: ttyname(slave) failed: %d %s\n",
	      argv0, errno, strerror(errno));
    } else if (verbose) {
      printf("%s: pty slave name: %s\n", argv0, tty);
    }
  }

  if (-1 == pipe(es)) {
    fprintf(stderr, "%s: pipe() failed: %s\n", argv[0], strerror(errno));
    exit(1);
  }

  switch ((childpid = fork())) {
  case 0:
    do_close(es[0]);
    do_close(s01m);
    child(s01s, es[1], &argv[optind]);
  case -1:
    fprintf(stderr, "%s: fork() failed: %s\n", argv[0], strerror(errno));
    exit(1);
  }
  do_close(s01s);
  do_close(es[1]);

  /* Raw stdin */
  if (tcgetattr(stdin_fileno, &orig_stdin_tio)) {
    /* if we can't get stdin attrs, don't even try to set them */
  } else {
    orig_stdin_tio_ok = 1;

    if (!tcgetattr(stdin_fileno, tiop)) {
      tiop->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
      tiop->c_oflag |= (OPOST|ONLCR); /* But with cr+nl on output */
      tiop->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
      
      /*tiop->c_cflag  = CLOCAL | CREAD; */
      tiop->c_cc[VMIN]  = 1;
      tiop->c_cc[VTIME] = 0;
      
      if (tcsetattr(stdin_fileno, TCSADRAIN, tiop)) {
	fprintf(stderr, "%s: tcsetattr(stdin, ) failed: %s\n",
		argv[0], strerror(errno));
	exit(1);
      }
    }
  }

  /* main loop */
  for(;;) {
    fd_set fds;
    int n;
    int fdmax;

    fdmax = -1;

    /* done when both channels to/from child are closed */
    if (nclosed == 2) {
      break;
    }

    FD_ZERO(&fds);
    if (-1 < s01m) {
      FD_SET(s01m, &fds);
      fdmax = fdmax > s01m ? fdmax : s01m;
    }
    if (-1 < es[0]) {
      FD_SET(es[0], &fds);
      fdmax = fdmax > es[0] ? fdmax : es[0];
    }
    if (-1 < stdin_fileno) {
      FD_SET(stdin_fileno, &fds);
      fdmax = fdmax > stdin_fileno ? fdmax : stdin_fileno;
    }

    n = select(fdmax + 1, &fds, NULL, NULL, NULL);

    if (0 > n) {
      fprintf(stderr, "%s: select(): %s", argv0, strerror(errno));
      continue;
    }

    if (-1 < s01m && FD_ISSET(s01m, &fds)) {
      if (process(s01m, STDOUT_FILENO, prefix,
		  postfix,&emptyline)) {
	nclosed++;
	s01m = -1;
      }
    }

    if (-1 < es[0] && FD_ISSET(es[0], &fds)) {
      if (process(es[0], STDERR_FILENO, eprefix,
		  epostfix, &eemptyline)) {
	nclosed++;
	es[0] = -1;
      }
    }

    if (-1 < stdin_fileno && FD_ISSET(stdin_fileno, &fds)) {
      char buf[128];
      ssize_t n;

      n = read(stdin_fileno, buf, sizeof(buf));
      if (!n) {
	stdin_fileno = -1;
      } else {
	/* FIXME: this should be nonblocking to not deadlock with child */
	write(s01m, buf, n);
      }
    }
  }

  reset_stdin_terminal();

  {
    int status;
    if (-1 == waitpid(childpid, &status, 0)) {
      fprintf(stderr, "%s: waitpid(%d): %d %s", argv0,
	      childpid, errno, strerror(errno));
      status = 1;
    }
    return status;
  }
}

/**
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * fill-column: 79
 * End:
 */
