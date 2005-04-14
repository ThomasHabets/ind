/*
 * ind
 *
 * By Thomas Habets <thomas@habets.pp.se>
 *
 *
 *
 * $Id: ind.c 1230 2005-04-14 17:45:28Z marvin $
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
  dup2(fd, STDOUT_FILENO);
  dup2(efd, STDERR_FILENO);
  execv("/bin/bash",argv);
  fprintf(stderr, "%s: %s: %s\n", argv0, argv[0], strerror(errno));
  exit(0);
}

/*
 *
 */
static void
usage(int err)
{
  printf("Foo %s\n", argv0);
  exit(err);
}

/*
 *
 */
static void
format(const char *format, char **output)
{
	char *ret = malloc(32);
	int curlen = 32;
	int len;

	len = snprintf(
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

    while ((q = memchr(p,'\n',n))) {
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
      if (0 > safe_write(fdout,"\n",1)) {
	return 1;
      }
      *emptyline = 1;
      n-=(q-p+1);
      p=q+1;
    }
    if (n) {
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

  while (EOF != (c = getopt(argc, argv, "h:p:a:P:A:"))) {
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
    format(prefix, tmp);
    format(postfix, ptmp);
    if (-1!=s[0] && process(s[0],STDOUT_FILENO, tmp,strlen(tmp),
			    ptmp,strlen(ptmp),&emptyline)) {
      nclosed++;
      s[0] = -1;
    }
    free(tmp);
    free(ptmp);
    format(eprefix, tmp);
    format(epostfix, ptmp);
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
