/*
 * ind
 *
 * By Thomas Habets <thomas@habets.pp.se>
 *
 * $Id: ind.c 2009 2007-11-20 15:15:08Z marvin $
 */
/*
 * (BSD license without advertising clause below)
 *
 * Copyright (c) 2005 Thomas Habets. All rights reserved.
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
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#ifndef FILENO_STDIN
#define STDIN_FILENO    0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO   1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO   2
#endif

/* arbitrary maxlength for prefixes and postfixes */
static const max_indstr_length = 1048576;

static const char *argv0;
static const float version = 0.1f;

/**
 * EINTR-safe close()
 * If close() fails due to anything other than EINTR, the fd is silently
 * leaked.
 *
 * @param   fd:  fd to close
 */
static void
do_close(int fd)
{
  int err;
  do {
    err = close(fd);
  } while ((-1 == err) && (errno = EINTR)); 
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
 * @param   fd:   stdout fd
 * @param   efd:  stderr fd
 * @param   argc: argc of subprocess
 * @param   argv: argv of subprocess
 */
static void
child(int fd, int efd, int argc, char **argv)
{
  if ((-1 == dup2(fd, STDOUT_FILENO))
      || (-1 == dup2(efd, STDERR_FILENO))) {
    fprintf(stderr, "%s: Unable to dup2(): %s", strerror(errno));
  }
  execvp(argv[0],argv);
  fprintf(stderr, "%s: %s: %s\n", argv0, argv[0], strerror(errno));
  exit(1);
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
	 "Format:\n"
	 " Normal text, except:\n"
	 "\t%%%%        Insert %%%%\n"
	 "\t%%c        Insert output from ctime(3) function\n"
	 , version, argv0);
  exit(err);
}

/**
 * just like memchr, but first of any of a set of chars
 *
 * @param  p:       string to search in
 * @param  chars:   characters to look for
 * @param  len:     length of string to search in
 *
 * @return   Pointer to first occurance of any of the characters, or NULL
 *           if not found.
 */
static char *
anychr(const char *p, const char *chars, size_t len)
{
  int c;
  char *ret = NULL;
  char *tmp;
  if (!*chars) {
    return NULL;
  }

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
 * @param   output:  place to store the output string pointer at
 */
static void
format(const char *fmt, char **output)
{
  int len = 0;
  const char *p = fmt;
  char *out;
  char ct[128];
  time_t t;

  while (*p) {
    if (*p == '%') {
      p++;
      switch(*p) {
      case 0:
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
    }
    len++;
    p++;
    if (len >= max_indstr_length) {
      break;
    }
  }
  if (!(*output = (char*)malloc(len+2))) {
    fprintf(stderr, "%s: %s\n", argv0, strerror(errno));
    exit(1);
  }
  out = *output;
  p = fmt;
  while (*p) {
    if (*p == '%') {
      p++;
      switch(*p) {
      case 0:
	break;
      case 'c':
	strcpy(out,ct);
	out = index(out,0);
	break;
      default:
	break;
      }
      p++;
    }
    *out++ = *p;
    p++;
  }
  *out = 0;
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
 * @return        0 on success, !0 on fail
 */
static int
process(int fdin,int fdout,
	const char *pre, int prelen,
	const char *post, int postlen, int *emptyline)
{
  int n;
  char buf[128];

  if (!(n = read(fdin, buf, sizeof(buf)-1))) {
    return 1;
  }

  if (n > 0) {
    char *p = buf;
    char *q;

    while ((q = anychr(p,"\r\n",n))) {
      if (*emptyline) {
	if (0 > safe_write(fdout,pre,prelen)) {
	  return 1;
	}
	*emptyline = 0;
      }
      if (0 > safe_write(fdout,p,q-p)) {
	return 1;
      }
      if (0 > safe_write(fdout,post,postlen)) {
	return 1;
      }
      if (0 > safe_write(fdout,q,1)) {
	return 1;
      }
      *emptyline = 1;
      n-=(q-p+1);
      p=q+1;
    }
    if (n) {
      if (*emptyline) {
	if (0 > safe_write(fdout,pre,prelen)) {
	  return 1;
	}
	*emptyline = 0;
      }
      if (0 > safe_write(fdout,p,n)) {
	return 1;
      }
    }
  }
  return 0;
}

/**
 *
 */
int
main(int argc, char **argv)
{
  int c;
  int s[2];
  int es[2];
  char *prefix = "  ";
  char *eprefix = ">>";
  char *postfix = "";
  char *epostfix = "";
  size_t prefixlen;
  size_t eprefixlen;
  size_t postfixlen;
  size_t epostfixlen;
  unsigned int nclosed = 0;
  int emptyline = 1;
  int eemptyline = 1;
  int err;

  argv0 = argv[0];

  while (-1 != (c = getopt(argc, argv, "+hp:a:P:A:"))) {
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
    default:
      usage(1);
    }
  }

  if (optind >= argc) {
    usage(1);
  }

  prefixlen = strlen(prefix);
  eprefixlen = strlen(eprefix);
  postfixlen = strlen(postfix);
  epostfixlen = strlen(epostfix);

  if (-1 == pipe(s)) {
    fprintf(stderr, "%s: pipe() failed: ", argv[0], strerror(errno));
    exit(1);
  }
  if (-1 == pipe(es)) {
    fprintf(stderr, "%s: pipe() failed: ", argv[0], strerror(errno));
    exit(1);
  }

  switch (fork()) {
  case 0:
    do_close(s[0]);
    child(s[1], es[1], argc, &argv[optind]);
  case -1:
    fprintf(stderr, "%s: fork() failed: ", argv[0], strerror(errno));
    exit(1);
  }
  do_close(s[1]);
  do_close(es[1]);

  for(;;) {
    char *tmp, *ptmp;
    if (nclosed > 1) {
      break;
    }
    format(prefix, &tmp);
    format(postfix, &ptmp);
    if (-1!=s[0] && process(s[0],STDOUT_FILENO, tmp,strlen(tmp),
			    ptmp,strlen(ptmp),&emptyline)) {
      nclosed++;
      s[0] = -1;
    }
    free(tmp);
    free(ptmp);
    format(eprefix, &tmp);
    format(epostfix, &ptmp);
    if (-1!=es[0] && process(es[0],STDERR_FILENO, tmp,strlen(tmp),
			    ptmp,strlen(ptmp), &eemptyline)) {
      nclosed++;
      es[0] = -1;
    }
    free(tmp);
    free(ptmp);
  }
  return 0;
}

/**
 * Local Variables:
 * mode: c
 * c-basic-offset: 2
 * fill-column: 79
 * End:
 */
