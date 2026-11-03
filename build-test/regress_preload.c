/* Test-only overrides for build-test/regress.sh, loaded with LD_PRELOAD into
 * the sudosh process it starts. Each override is inert unless its environment
 * variable is set.
 *
 *   REGRESS_FAKE_UID     stat() reports this owner for paths under /dev/pts/, and
 *                        getpwuid() for this UID returns REGRESS_FAKE_NAME
 *   REGRESS_FAKE_NAME    login name for REGRESS_FAKE_UID; unset means no entry
 *   REGRESS_FAKE_SHELLS  getusershell() lists only /nonexistent/shell
 *   REGRESS_SHORT_WRITE  write() accepts at most one byte per call
 *   REGRESS_SYSLOG_LOG   syslog() appends each formatted message to this file
 *   REGRESS_NOW          time() returns this many seconds since the epoch
 *   REGRESS_EXECL_FAIL   execl() fails with ENOENT
 *   REGRESS_FROZEN_CLOCK gettimeofday() and getpid() always return the same values
 *   REGRESS_PTSNAME_FAIL ptsname() returns NULL
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

int stat(const char *path, struct stat *buf)
{
  static int (*real)(const char *, struct stat *);
  const char *fake = getenv("REGRESS_FAKE_UID");
  int r;

  if (!real)
    real = (int (*)(const char *, struct stat *))dlsym(RTLD_NEXT, "stat");
  r = real(path, buf);
  if (r == 0 && fake && strncmp(path, "/dev/pts/", 9) == 0)
    buf->st_uid = (uid_t)atoi(fake);
  return r;
}

struct passwd *getpwuid(uid_t uid)
{
  static struct passwd *(*real)(uid_t);
  static struct passwd fake;
  const char *fake_uid = getenv("REGRESS_FAKE_UID");
  const char *name = getenv("REGRESS_FAKE_NAME");

  if (fake_uid && uid == (uid_t)atoi(fake_uid)) {
    if (!name)
      return NULL;
    fake.pw_name = (char *)name;
    fake.pw_passwd = (char *)"x";
    fake.pw_uid = uid;
    fake.pw_gid = uid;
    fake.pw_gecos = (char *)"";
    fake.pw_dir = (char *)"/";
    fake.pw_shell = (char *)"/bin/sh";
    return &fake;
  }
  if (!real)
    real = (struct passwd *(*)(uid_t))dlsym(RTLD_NEXT, "getpwuid");
  return real(uid);
}

static const char *fake_shells[] = {"/nonexistent/shell", NULL};
static const char **next_shell = NULL;

void setusershell(void)
{
  static void (*real)(void);

  if (getenv("REGRESS_FAKE_SHELLS")) {
    next_shell = fake_shells;
    return;
  }
  if (!real)
    real = (void (*)(void))dlsym(RTLD_NEXT, "setusershell");
  real();
}

char *getusershell(void)
{
  static char *(*real)(void);

  if (getenv("REGRESS_FAKE_SHELLS")) {
    if (!next_shell)
      next_shell = fake_shells;
    return *next_shell ? (char *)*next_shell++ : NULL;
  }
  if (!real)
    real = (char *(*)(void))dlsym(RTLD_NEXT, "getusershell");
  return real();
}

void endusershell(void)
{
  static void (*real)(void);

  if (getenv("REGRESS_FAKE_SHELLS")) {
    next_shell = NULL;
    return;
  }
  if (!real)
    real = (void (*)(void))dlsym(RTLD_NEXT, "endusershell");
  real();
}

ssize_t write(int fd, const void *buf, size_t n)
{
  static ssize_t (*real)(int, const void *, size_t);

  if (!real)
    real = (ssize_t (*)(int, const void *, size_t))dlsym(RTLD_NEXT, "write");
  if (n > 1 && getenv("REGRESS_SHORT_WRITE"))
    n = 1;
  return real(fd, buf, n);
}

static void record_syslog(const char *format, va_list ap)
{
  const char *path = getenv("REGRESS_SYSLOG_LOG");
  char buf[8192];
  FILE *f;

  if (path && (f = fopen(path, "a")) != NULL) {
    vsnprintf(buf, sizeof buf, format, ap);
    fprintf(f, "%s\n", buf);
    fclose(f);
  }
}

void syslog(int priority, const char *format, ...)
{
  va_list ap;

  (void)priority;
  va_start(ap, format);
  record_syslog(format, ap);
  va_end(ap);
}

/* Optimised, fortified builds call this instead of syslog(). */
void __syslog_chk(int priority, int flag, const char *format, ...)
{
  va_list ap;

  (void)priority;
  (void)flag;
  va_start(ap, format);
  record_syslog(format, ap);
  va_end(ap);
}

time_t time(time_t *t)
{
  static time_t (*real)(time_t *);
  const char *now = getenv("REGRESS_NOW");

  if (now) {
    time_t v = (time_t)atoll(now);

    if (t)
      *t = v;
    return v;
  }
  if (!real)
    real = (time_t (*)(time_t *))dlsym(RTLD_NEXT, "time");
  return real(t);
}

int execl(const char *path, const char *arg, ...)
{
  const char *argv[64];
  va_list ap;
  int n = 0;

  if (getenv("REGRESS_EXECL_FAIL")) {
    errno = ENOENT;
    return -1;
  }
  argv[n++] = arg;
  va_start(ap, arg);
  while (n < 63 && (argv[n] = va_arg(ap, const char *)) != NULL)
    n++;
  va_end(ap);
  argv[n] = NULL;
  return execv(path, (char *const *)argv);
}

int gettimeofday(struct timeval *tv, void *tz)
{
  static int (*real)(struct timeval *, void *);

  if (getenv("REGRESS_FROZEN_CLOCK")) {
    tv->tv_sec = 1700000000;
    tv->tv_usec = 123456;
    return 0;
  }
  if (!real)
    real = (int (*)(struct timeval *, void *))dlsym(RTLD_NEXT, "gettimeofday");
  return real(tv, tz);
}

pid_t getpid(void)
{
  static pid_t (*real)(void);

  if (getenv("REGRESS_FROZEN_CLOCK"))
    return 4242;
  if (!real)
    real = (pid_t (*)(void))dlsym(RTLD_NEXT, "getpid");
  return real();
}

char *ptsname(int fd)
{
  static char *(*real)(int);

  if (getenv("REGRESS_PTSNAME_FAIL"))
    return NULL;
  if (!real)
    real = (char *(*)(int))dlsym(RTLD_NEXT, "ptsname");
  return real(fd);
}
