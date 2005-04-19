/*
 * ind
 *
 * By Thomas Habets <thomas@habets.pp.se>
 *
 * (setq c-basic-offset 2)
 *
 * $Id: ind.c 1231 2005-04-19 16:58:07Z marvin $
 */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#ifndef TEMP_FAILURE_RETRY
#error "I'm too lazy to re-code that"
#endif

const char *argv0;

/*
 *
 */
static int
safe_write(int fd, const void *buf, size_t len)
{
  int ret;
  int offset = 0;
  
  while(len > 0) {
    ret = TEMP_FAILURE_RETRY(write(fd,
				   (unsigned char *)buf + offset,
				   len));
    
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

/*
 *
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
  exit(0);
}

/*
 *
 */
static void
usage(int err)
{
  printf("usage: %s [ -h ] [ -p <text> ] [ -a <text> ] [ -P <text> ] [ -A <text> ]  \n"
	 "\n"
	 "", argv0);
  exit(err);
}

/*
 * just like memchr, but first of any of a set of chars
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

/*
 *
 */
static void
format(const char *fmt, char **output)
{
  int len;
  
  len = snprintf(NULL, 0, "%s", fmt);
  if (!(*output = malloc(len+1))) {
    fprintf(stderr, "%s: %s\n", argv0, strerror(errno));
    exit(1);
  }
  *output = malloc(len+10);
  snprintf(*output, len+1, "%s", fmt);
}

/*
 * ret 0 on success
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

/*
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

  argv0 = argv[0];

  while (-1 != (c = getopt(argc, argv, "+h:p:a:P:A:"))) {
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
    TEMP_FAILURE_RETRY(close(s[0]));
    child(s[1], es[1], argc, &argv[optind]);
  case -1:
    fprintf(stderr, "%s: fork() failed: ", argv[0], strerror(errno));
    exit(1);
  }
  TEMP_FAILURE_RETRY(close(s[1]));
  TEMP_FAILURE_RETRY(close(es[1]));

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
  exit(0);
}
