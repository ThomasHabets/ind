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

#if defined (__SVR4) && defined (__sun)
#include <alloca.h>
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
static const float version = 0.12f;
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
  if (fd == -1) {
    return 0;
  }
  do {
    err = close(fd);
  } while ((-1 == err) && (errno == EINTR)); 
  if (verbose) {
    if (err) {
      fprintf(stderr, "%s: close(%d): %s\n", argv0, fd, strerror(errno));
    }
  }
  return err;
}

/**
 *
 */
static void
terminfo(int fd)
{
  struct winsize w;
  char *tty;
  
  fprintf(stderr, "%s: fd: %d\n", argv0, fd);
  if (!isatty(fd)) {
    fprintf(stderr, "%s: \tNot a tty!\n", argv0);
    return;
  }
  if (-1 == ioctl(fd, TIOCGWINSZ, &w)) {
    fprintf(stderr, "%s: \tioctl(TIOCGWINSZ) fail: %s\n",
	    argv0, strerror(errno));
    return;
  }
  tty = ttyname(fd);
  if (tty) {
    fprintf(stderr, "%s: \tttyname(): %s\n", argv0, tty);
  }
  fprintf(stderr, "%s: \tSize: %dx%d\n", argv0, w.ws_row, w.ws_col);
}

/**
 * close three fds, and make sure not to double-close in case they are the
 * same.
 *
 */
static int
do_close3(int fd0, int fd1, int fd2)
{
  int ret = 0;
  
  ret += do_close(fd0);
  if (fd0 != fd1) {
    ret += do_close(fd1);
  }

  if ((fd2 != fd0) && (fd2 != fd1)) {
    ret += do_close(fd2);
  }
  return ret;
}

/**
 *
 *
 */
static void
do_fdset(fd_set *fds, int fd, int *fdmax)
{
  if (0 > fd) {
    return;
  }
  FD_SET(fd, fds);
  *fdmax = *fdmax > fd ? *fdmax : fd;
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
 *
 */
static void
print_ttyname(const char *fdname, int fdm, int fds)
{
  char *tty;
#ifdef CONSTANT_PTSMASTER
  if (verbose) {
    printf("%s: %s pty master name: %s\n", argv0, fdname, CONSTANT_PTSMASTER);
  }
#else
  if (!(tty = ttyname(fdm))) {
    fprintf(stderr, "%s: %s ttyname(master=%d) failed: %d %s\n",
	    argv0, fdname, fdm, errno, strerror(errno));
  } else if (verbose) {
    fprintf(stderr, "%s: %s pty master name: %s\n", argv0, fdname, tty);
  }
#endif
  if (!(tty = ttyname(fds))) {
    fprintf(stderr, "%s: %s ttyname(slave=%d) failed: %d %s\n",
	    argv0, fdname, fds, errno, strerror(errno));
  } else if (verbose) {
    fprintf(stderr, "%s: %s pty slave name: %s\n", argv0, fdname, tty);
  }
}

/**
 * Set up stdout/stderr and exec subprocess
 *
 * @param   fd:   stdin/stdout fd
 * @param   efd:  stderr fd
 * @param   argv: argv of subprocess
 *
 * does not return. Runs execvp() or exit()
 */
static void
child(int fdi, int fdo, int fde, char **argv)
{
  int fdt;

  /* get a potential terminal to do login_tty() on */
  fdt = fdo;
  if (!isatty(fdt) && isatty(fdi)) {
    fdt = fdi;
  }
  if (!isatty(fdt) && isatty(fde)) {
    fdt = fde;
  }

  /* login_tty() */
  if (isatty(fdt)) {
    if (0 > (fdt = dup(fdt))) {
      fprintf(stderr, "%s: dup(%d) failed: %s\n",
	      argv0, fdt, strerror(errno));
      exit(1);
    }
    if (0 > login_tty(fdt)) {
      fprintf(stderr, "%s: login_tty(%d) failed: %s\n",
	      argv0, fdt, strerror(errno));
      exit(1);
    }
  }

  /* login_tty() screwed up our fds, set them right again */
  if (-1 == dup2(fdi, STDIN_FILENO)) {
    fprintf(stderr, "%s: dup2(%d, stdin) failed: %s\n",
	    argv0, fdi, strerror(errno));
    exit(1);
  }
  if (-1 == dup2(fdo, STDOUT_FILENO)) {
    fprintf(stderr, "%s: dup2(%d, stdout) failed: %s\n",
	    argv0, fdo, strerror(errno));
    exit(1);
  }
  if (-1 == dup2(fde, STDERR_FILENO)) {
    fprintf(stderr, "%s: dup2(%d, stderr) failed: %s\n",
	    argv0, fde, strerror(errno));
    exit(1);
  }

  do_close3(fdi, fdo, fde);
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

  n = read(fdin, buf, sizeof(buf)-1);
  if (verbose > 1) {
    fprintf(stderr, "%s: read(%d): %d (errno=%s)\n", argv0, fdin, n,
	    strerror(errno));
  }
  if (!n) {
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
      fprintf(stderr, "%s: Internal error: read() got errno %d (%s)\n",
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
static void
setup_pty(const char *prefix, const char *postfix,
	  int realttyfd, 
	  int *s01m, int *s01s)
{
  /* set up winsize */
  struct winsize *wsp;
  struct termios *tiop;
  
  wsp = alloca(sizeof(struct winsize));
  if (0 > ioctl(realttyfd, TIOCGWINSZ, wsp)) {
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
  if (0 > tcgetattr(realttyfd, tiop)) {
    /* if parent terminal (if even a terminal at all) won't give up info
     * on terminal, neither will we.
     */
    tiop = 0;
  }
    
  if (-1 == openpty(s01m, s01s, NULL, tiop, wsp)) {
    fprintf(stderr, "%s: openpty() failed: %s\n", argv0, strerror(errno));
    exit(1);
  }
}

/**
 *
 */
int
main(int argc, char **argv)
{
  int c;
  int ptym_in = -1, ptys_in = -1;
  int ptym_out = -1, ptys_out = -1;
  int child_stdin, child_stdout, child_stderr;
  int ind_stdin, ind_stdout, ind_stderr;
  char *prefix = "  ";
  char *eprefix = ">>";
  char *postfix = "";
  char *epostfix = "";
  int emptyline = 1;
  int eemptyline = 1;
  int childpid;
  int stdin_fileno = STDIN_FILENO;

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

  /* create communication pipes (stderr is always in a pipe) */
  {
    int pip_stdin[2];
    int pip_stdout[2];

    if (isatty(STDIN_FILENO)) {
      setup_pty(prefix, postfix, STDIN_FILENO, &ptym_in, &ptys_in);
    }
    
    /* only allocate a new pty if stdout is not the same terminal as stdin */
    if (isatty(STDOUT_FILENO)) {
      if (0 <= ptym_in) {
	char *ttyin, *ttyout;
	ttyin = strdup(ttyname(STDIN_FILENO));
	ttyout = strdup(ttyname(STDOUT_FILENO));
	if (!strcmp(ttyin, ttyout)) {
	  ptym_out = ptym_in;
	  ptys_out = ptys_in;
	}
      }
      if (0 > ptym_out) {
	setup_pty(prefix, postfix, STDOUT_FILENO, &ptym_out, &ptys_out);
      }
    }

    if (isatty(STDIN_FILENO)) {
      child_stdin = ptys_in;
      ind_stdin = ptym_in;
    } else {
      if (-1 == pipe(pip_stdin)) {
	fprintf(stderr, "%s: pipe() failed: %s\n", argv[0], strerror(errno));
	exit(1);
      }
      child_stdin = pip_stdin[0];
      ind_stdin = pip_stdin[1];
    }

    if (isatty(STDOUT_FILENO)) {
      child_stdout = ptys_out;
      ind_stdout = ptym_out;
    } else {
      if (0 > pipe(pip_stdout)) {
	fprintf(stderr, "%s: pipe() failed: %s\n", argv[0], strerror(errno));
	exit(1);
      }
      child_stdout = pip_stdout[1];
      ind_stdout = pip_stdout[0];
    }
  }

  /* print tty name, if we're using ptys  */
  if (0 <= ptym_in) {
    print_ttyname("stdin", ptym_in, ptys_in);
  }
  if (0 <= ptym_out) {
    print_ttyname("stdout", ptym_out, ptys_out);
  }

  /* create stderr pipe */
  {
    int es[2];
    if (-1 == pipe(es)) {
      fprintf(stderr, "%s: pipe() failed: %s\n", argv[0], strerror(errno));
      exit(1);
    }
    child_stderr = es[1];
    ind_stderr = es[0];
  }

  switch ((childpid = fork())) {
  case 0:
    do_close3(ind_stdin, ind_stdout, ind_stderr);
    child(child_stdin, child_stdout, child_stderr, &argv[optind]);
  case -1:
    fprintf(stderr, "%s: fork() failed: %s\n", argv[0], strerror(errno));
    exit(1);
  }
  do_close3(child_stdin, child_stdout, child_stderr);

  if (verbose > 1) {
    fprintf(stderr, "%s: childpid: %d\n", argv[0], childpid);
    terminfo(0);
    terminfo(1);
    terminfo(2);
  }
  /* Raw stdin */
  if (tcgetattr(stdin_fileno, &orig_stdin_tio)) {
    /* if we can't get stdin attrs, don't even try to set them */
  } else {
    struct termios tio;

    orig_stdin_tio_ok = 1;

    if (!tcgetattr(stdin_fileno, &tio)) {
      tio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|IXON);
      if (isatty(STDOUT_FILENO)) {
	tio.c_lflag &= ~(ECHO|ECHONL);
	tio.c_iflag &= ~(INLCR|IGNCR|ICRNL);
	tio.c_lflag &= ~(ICANON);
      }
      tio.c_lflag &= ~(ISIG|IEXTEN);

      /* change to 8bit? */
      if (0) {
	tio.c_cflag &= ~(CSIZE | PARENB);
	tio.c_cflag |= CS8;
      }

      tio.c_cc[VMIN]  = 1;
      tio.c_cc[VTIME] = 0;
      
      if (tcsetattr(stdin_fileno, TCSADRAIN, &tio)) {
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

    /*
     * done when both channels to/from child are closed
     */
    if (isatty(stdin_fileno)) {
      if (ind_stdin == -1
	  && ind_stdout == -1
	  && ind_stderr == -1) {
	break;
      }
    } else {
      if (stdin_fileno == -1
	  && ind_stdin == -1
	  && ind_stdout == -1
	  && ind_stderr == -1) {
	break;
      }
    }

    FD_ZERO(&fds);
    do_fdset(&fds, ind_stdout, &fdmax);
    do_fdset(&fds, ind_stderr, &fdmax);
    do_fdset(&fds, stdin_fileno, &fdmax);
    do_fdset(&fds, ind_stdin, &fdmax);

    if (verbose > 1) {
      fprintf(stderr, "%s: select(%d %d %d %d)\n", argv0,
	      ind_stdin,
	      ind_stdout,
	      ind_stderr,
	      stdin_fileno);
    }
    n = select(fdmax + 1, &fds, NULL, NULL, NULL);
    
    if (verbose > 1) {
      fprintf(stderr, "%s: select(): %d\n", argv0, n);
      if (ind_stdin != -1 && FD_ISSET(ind_stdin, &fds)) {
	fprintf(stderr,"%s: \tfd: ind_stdin (%d) %d readable\n",
		argv0,ind_stdin, isatty(ind_stdin));
      }
      if (ind_stdout != -1 && FD_ISSET(ind_stdout, &fds)) {
	fprintf(stderr,"%s: \tfd: ind_stdout (%d) readable\n",
		argv0,ind_stdout);
      }
      if (ind_stderr != -1 && FD_ISSET(ind_stderr, &fds)) {
	fprintf(stderr,"%s: \tfd: ind_stderr (%d) readable\n",
		argv0,ind_stderr);
      }
      if (stdin_fileno != -1 && FD_ISSET(stdin_fileno, &fds)) {
	fprintf(stderr, "%s: \tfd: stdin_fileno (%d) readable\n",argv0,
		stdin_fileno);
      }
    }
    if (0 > n) {
      fprintf(stderr, "%s: select(): %s\n", argv0, strerror(errno));
      continue;
    }

    if (ind_stdin != ind_stdout
	&& isatty(stdin_fileno) && !isatty(ind_stdin)) {
      do_close(ind_stdin);
      ind_stdin = -1;
    }
    
    /* if stdin != stdout then echo anything read from stdin to stdout */
    if ((-1 < ind_stdin)
	&& isatty(ind_stdin)
	&& (ind_stdin != ind_stdout)
	&& FD_ISSET(ind_stdin, &fds)) {
      if (verbose > 1) {
	fprintf(stderr, "%s: read()ing ind_stdin\n", argv0);
      }
      if (isatty(ind_stdout)) {
	if (process(ind_stdin, STDOUT_FILENO, prefix, postfix, &emptyline)) {
	  ind_stdin = -1;
	}
      } else {
	char buf[128];
	/* read and discard */
	if (0 >= read(ind_stdin, buf, sizeof(buf))) {
	  do_close(ind_stdin);
	  ind_stdin = -1;
	}
      }
    }

    if (-1 < ind_stdout && FD_ISSET(ind_stdout, &fds)) {
      if (verbose > 1) {
	fprintf(stderr, "%s: read()ing ind_stdout\n", argv0);
      }
      if (process(ind_stdout, STDOUT_FILENO, prefix, postfix,&emptyline)) {
	if (ind_stdin == ind_stdout) {
	  ind_stdin = -1;
	}
	ind_stdout = -1;
      }
    }
    if (verbose > 1) {
      fprintf(stderr, "%s: \tdone read()ing ind_stdout\n", argv0);
    }

    if (-1 < ind_stderr && FD_ISSET(ind_stderr, &fds)) {
      if (verbose > 1) {
	fprintf(stderr, "%s: read()ing ind_stderr\n", argv0);
      }
      if (process(ind_stderr, STDERR_FILENO, eprefix, epostfix, &eemptyline)) {
	ind_stderr = -1;
      }
    }

    if (-1 < stdin_fileno && FD_ISSET(stdin_fileno, &fds)) {
      char buf[128];
      ssize_t n;
      if (verbose > 1) {
	fprintf(stderr, "%s: read()ing stdin_fileno\n", argv0);
      }
      n = read(stdin_fileno, buf, sizeof(buf));
      if (0 > n) {
	fprintf(stderr, "%s: read(stdin_fileno): %d %s",
		argv0, errno, strerror(errno));
	reset_stdin_terminal();
	exit(1);
      } else if (!n) {
	stdin_fileno = -1;
	/* Note: is this right even for terminals */
	do_close(ind_stdin);
	ind_stdin = -1;
      } else {
	/* FIXME: this should be nonblocking to not deadlock with child */
	ssize_t nw = safe_write(ind_stdin, buf, n);
	if (nw != n) {
	  fprintf(stderr, "%s: write(ind -> child stdin): %d %s\n",
		  argv0, errno, strerror(errno));
	  reset_stdin_terminal();
	  exit(1);
	}
      }
    }
  }

  if (verbose > 1) {
    fprintf(stderr, "%s: resetting terminal\n", argv0);
  }
  reset_stdin_terminal();

  {
    int status;
    if (verbose > 1) {
      fprintf(stderr, "%s: waitpid(%d)\n", argv0, childpid);
    }
    if (-1 == waitpid(childpid, &status, 0)) {
      fprintf(stderr, "%s: waitpid(%d): %d %s", argv0,
	      childpid, errno, strerror(errno));
      status = 1;
    }
    if (verbose > 1) {
      fprintf(stderr, "%s: exiting\n", argv0);
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
