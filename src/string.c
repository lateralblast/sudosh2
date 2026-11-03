#include "super.h"

void
lc (char *str)
{
  size_t i;

  for (i = 0; str[i]; i++)
    str[i] = tolower (str[i]);
}

char *
int2smalltime (int t)
{
  static char time_string[BUFSIZ];
  size_t used = 0;
  int days, hrs, mins, secs;

  if (t == -1)
    return "Never";

  if (t <= 0)
    return "0s";

  days = t / 86400;
  hrs = (t / 3600) % 24;
  mins = (t / 60) % 60;
  secs = t % 60;
  time_string[0] = '\0';

  /* Show every unit that is not zero, so 3661 seconds is "1h1m1s". */
  if (days)
    used += snprintf (time_string + used, sizeof (time_string) - used, "%dd", days);
  if (hrs)
    used += snprintf (time_string + used, sizeof (time_string) - used, "%dh", hrs);
  if (mins)
    used += snprintf (time_string + used, sizeof (time_string) - used, "%dm", mins);
  if (secs)
    used += snprintf (time_string + used, sizeof (time_string) - used, "%ds", secs);

  return time_string;
}
