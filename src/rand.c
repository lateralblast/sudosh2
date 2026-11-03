#include "super.h"

/* Seed the generator once per process. Reseeding on every call made all calls
 * within the same microsecond return the same value, so the characters of a
 * session suffix came out in repeated runs. */
int
myrand (void)
{
  static int seeded = 0;
  struct timeval tv;
  unsigned int seed;

  if (!seeded)
    {
      gettimeofday (&tv, (struct timezone *) 0);
      seed =
	(tv.tv_sec % 10000) * 523 + tv.tv_usec * 13 + (getpid () % 1000) * 983;
      srand (seed);
      seeded = 1;
    }

  return rand ();
}

char *
rand2str (size_t len)
{
  static char buf[BUFSIZ];
  char *ptr = buf;
  char *alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  int i;

  if (len == 0)
    return (char *) 0;
  if (len >= BUFSIZ)
    len = BUFSIZ - 1;

  for (i = 0; i < len; i++)
    {
      int j = (myrand () & 0xffff) % (26 + 26 + 10);

      ptr[i] = alphabet[j];
    }
  ptr[len] = '\0';

  return buf;
}
