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
 */
#define _GNU_SOURCE
#include <dlfcn.h>
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

void syslog(int priority, const char *format, ...)
{
  const char *path = getenv("REGRESS_SYSLOG_LOG");
  char buf[8192];
  va_list ap;
  FILE *f;

  (void)priority;
  if (path && (f = fopen(path, "a")) != NULL) {
    va_start(ap, format);
    vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);
    fprintf(f, "%s\n", buf);
    fclose(f);
  }
}
