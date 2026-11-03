#include "super.h"
#include "struct.h"

#define LL() fprintf(stderr, "[%s, line %i]: ", __FILE__, __LINE__)

struct s_file
{
  FILE *f;
  int fd;
  int line;
  char *ptr;
  char str[BUFSIZ];
};

struct s_session
{
  char to[BUFSIZ];
  char from[BUFSIZ];
  char type[BUFSIZ];
  char randstr[BUFSIZ];
  char date[BUFSIZ];
  char id[BUFSIZ];
  long secs;
  time_t e;
  struct s_session *next;
  struct s_session *prev;
  struct s_file script;
  struct s_file time;
};

typedef struct s_session session;

extern void parse (option *, const char *);

int getopt (int, char *const[], const char *);
extern char *optarg;
extern int optind;

char match[BUFSIZ-1];

char *progname;
session *s_top;
option config_option;

void replay (const char *, const char *, int, int);
void link_session (session *);
void unlink_session (session *);
void count_dur (session *);
char *int2smalltime (int);

session *session_malloc (void);
session *find_session (const char *, const char *, time_t e, const char *);
session *sort_list (session *);
void show_sessions (void);

/* True if NAME ends with SUFFIX. Checks the length first, so a short
 * name cannot make the comparison read before the start of the string. */
static int
has_suffix (const char *name, const char *suffix)
{
  size_t nl = strlen (name), sl = strlen (suffix);

  return nl >= sl && strcmp (name + nl - sl, suffix) == 0;
}

/* A copy of S that is safe to show on a terminal. File names and headers can
 * come from a forged file, so control characters (which could carry escape
 * sequences) become '?'. A few buffers are used in rotation so that one call
 * can show several of these strings. */
static const char *
printable (const char *s)
{
  static char buf[4][BUFSIZ];
  static unsigned int which = 0;
  char *d = buf[which++ % 4];
  size_t i;

  for (i = 0; s[i] != '\0' && i < BUFSIZ - 1; i++)
    d[i] = ((unsigned char) s[i] < 0x20 || (unsigned char) s[i] == 0x7f)
      ? '?' : s[i];
  d[i] = '\0';
  return d;
}

/* True if NAME in the log directory is a regular file. The directory is
 * writable by everyone, so something else may have been put there: opening a
 * FIFO would block forever, and a device or symlink is never something sudosh
 * wrote. Directories are skipped without comment. */
static int
is_regular_file (const char *name)
{
  char path[BUFSIZ];
  struct stat st;

  snprintf (path, sizeof (path), "%.1024s/%s", config_option.logdir, name);
  if (lstat (path, &st) == -1)
    return 0;
  if (S_ISDIR (st.st_mode))
    return 0;
  if (!S_ISREG (st.st_mode))
    {
      fprintf (stderr, "[warning]: skipping %s: not a regular file\n", printable (name));
      return 0;
    }
  return 1;
}

/* True if the file NAME was written by the user named TO. sudosh runs as the
 * user it records, so a genuine recording is owned by that user (or by root,
 * for a setuid install). Anyone can create files in the log directory, so a
 * file owned by somebody else is a forgery. A user that does not exist on this
 * system cannot be checked, and is accepted. */
static int
owner_matches (const char *name, const char *to)
{
  char path[BUFSIZ];
  struct stat st;
  struct passwd *pw;

  snprintf (path, sizeof (path), "%.1024s/%s", config_option.logdir, name);
  if (lstat (path, &st) == -1)
    return 0;
  pw = getpwnam (to);
  if (pw == (struct passwd *) 0 || st.st_uid == 0 || st.st_uid == pw->pw_uid)
    return 1;
  fprintf (stderr, "[warning]: skipping %s: not owned by %s\n", printable (name), printable (to));
  return 0;
}

/* Format E as a date. localtime() returns NULL for a timestamp it cannot
 * represent, which a forged file name can supply, so fall back to a
 * placeholder instead of passing NULL to strftime(). */
static void
format_date (char *dst, size_t size, time_t e)
{
  struct tm *tm = localtime (&e);

  if (tm == (struct tm *) 0
      || strftime (dst, size, "%m/%d/%Y %H:%M:%S", tm) == 0)
    snprintf (dst, size, "%s", "(bad date)");
}

int
main (int argc, char **argv, char **environ)
{
  int c;
  char *p = (char *) 0;
  char sysconfdir[BUFSIZ-1];
  struct dirent *dirp;
  DIR *dp;
  session *scan;
  session *last;

  progname = argv[0];

  if ((p = (char *) strrchr (progname, '/')) != (char *) 0)
    progname = p + 1;


  snprintf (sysconfdir, BUFSIZ - 1, "%s/sudosh.conf", SYSCONFDIR);
  parse (&config_option, sysconfdir);

  snprintf (match, BUFSIZ - 1,
	    "%%128[^%c]%c%%128[^%c]%c%%32[^%c]%c%%ld%c%%64[A-Za-z0-9]",
	    config_option.fdl, config_option.fdl, config_option.fdl,
	    config_option.fdl, config_option.fdl, config_option.fdl,
	    config_option.fdl);

  while (1)
    {
      c = getopt (argc, argv, "d:hVv");

      if (c == EOF)
	break;

      switch (c)
	{
	case 'V':
	case 'v':
	  fprintf (stdout, "%s version %s\n", progname, VERSION);
	  exit (EXIT_SUCCESS);
	  break;
	case 'h':
	  fprintf (stdout, "Usage: %s ID [MULTIPLIER] [MAXWAIT]\n", progname);
	  fprintf (stdout,
		   "Replay %s sessions as they were originally recorded.\n\n",
		   progname);
	  fprintf (stdout,
		   "-d	Specify alternate LOGDIR where sessions are stored.\n");
	  fprintf (stdout, "-h	Show help synopsis.\n");
	  fprintf (stdout, "-v,-V	Show version.\n\n");
	  fprintf (stdout,
		   "ID\n	The session id of which you wish to view.  The format is user-id\n");
	  fprintf (stdout,
		   "	You may obtain a list of ids by just running %s\n\n",
		   progname);
	  fprintf (stdout, "MULTIPLIER\n");
	  fprintf (stdout,
		   "	This is optional.  If you wish to \"speed up\" the playback of the\n");
	  fprintf (stdout,
		   "	session, specify a rate to increase it.  For example if you used\n");
	  fprintf (stdout,
		   "	a multiplier of \"2\" it would playback the session 2x the original\n");
	  fprintf (stdout, "	recording speed.\n\n");
	  fprintf (stdout, "MAXWAIT\n");
	  fprintf (stdout,
		   "	This is optional.  If you do not want to wait during any long pauses\n");
	  fprintf (stdout,
		   "	use this option to specify a maximum wait time.  For example a MAXWAIT\n");
	  fprintf (stdout,
		   "	of 2 would wait a maximum of 2 seconds then continue.\n\n");
	  fprintf (stdout,
		   "Example: how to get a list of sessions to replay.\n");
	  fprintf (stdout, "# %s\n\n", progname);
	  fprintf (stdout,
		   "Example: how to replay a session as it was recorded.\n");
	  fprintf (stdout, "# %s ID\n", progname);
	  fprintf (stdout,
		   "Example: how to replay a session, but just echo the contains to the screen.\n");
	  fprintf (stdout, "# %s ID 0\n\n", progname);
	  fprintf (stdout, "This is %s version %s\n", progname, VERSION);
	  fprintf (stdout, "Report bugs to <%s>\n", PACKAGE_BUGREPORT);
	  exit (EXIT_SUCCESS);
	  break;
	case 'd':
	  snprintf (config_option.logdir, BUFSIZ, "%s", optarg);
	  break;
	default:
	  fprintf (stderr, "%s: unrecognized option `%c'\n", progname, c);
	  fprintf (stderr, "Try `%s -h' for more information.\n", progname);
	  exit (EXIT_FAILURE);
	  break;
	}
    }


  argc -= optind;
  argv += optind;

  if ((dp = opendir (config_option.logdir)) == (DIR *) 0)
    {
      LL ();
      fprintf (stderr, "%s: can't opendir(%s): %s (%i)\n", progname,
	       config_option.logdir, strerror (errno), errno);
      exit (EXIT_FAILURE);
    }

  while ((dirp = readdir (dp)) != (struct dirent *) 0)
    {
      session *s;
      char from[BUFSIZ-1];
      char to[BUFSIZ-1];
      char type[BUFSIZ-1];
      char randstr[BUFSIZ-1];
      time_t e;

      if (strncmp (dirp->d_name, ".", 1) == 0)
	continue;

      if (!is_regular_file (dirp->d_name))
	continue;

      if (has_suffix (dirp->d_name, ".gz")
          || has_suffix (dirp->d_name, ".bz2")
          || has_suffix (dirp->d_name, ".xz"))
        continue; 

      if (sscanf (dirp->d_name, match, from, to, type, &e, randstr) != 5)
	{
	  LL ();
	  fprintf (stderr, "[warning]: invalid file: %s\n", printable (dirp->d_name));
	  continue;
	}

      if (strcmp (type, "script") && strcmp (type, "time")
	  && strcmp (type, "interactive") && strcmp (type, "input"))
	{
	  fprintf (stderr, "[warning]: invalid type (%s): %s\n", printable (type), printable (dirp->d_name));
	  continue;
	}

      if (!owner_matches (dirp->d_name, to))
	continue;

      if (!strcmp (type, "interactive"))
	{
	  char *ptr;
	  FILE *f;
	  s = session_malloc ();

	  s->secs = 0;
	  s->e = e;
	  snprintf (s->script.str, BUFSIZ - 1, "%.1024s/%.128s", config_option.logdir,
		    dirp->d_name);

	  f = fopen (s->script.str, "r");

	  if (f == (FILE *) 0)
	    {
	      /* One unreadable log should not hide the rest of the list. */
	      fprintf (stderr, "[warning]: skipping %.100s: %.100s (%i)\n",
		       printable (s->script.str), strerror (errno), errno); free (s);
	      continue;
	    }

	  if (fgets (s->id, BUFSIZ - 1, f) == NULL)
	    fprintf (stderr, "Bad header in %s\n", printable (s->script.str));
	  else
	    {
	      ptr = strchr (s->id, '\n');
	      if (ptr != (char *) 0)
		*ptr++ = '\0';

	      snprintf (s->from, BUFSIZ, "%s", from);
	      snprintf (s->to, BUFSIZ, "%s", to);
	      snprintf (s->type, BUFSIZ, "%s", type);
	      snprintf (s->randstr, BUFSIZ, "%s", randstr);
	      format_date (s->date, 20, s->e);
	      link_session (s);
	    }
	  fclose (f);
	  continue;
	}

      /* An input log only accompanies a script log; alone it is not a session. */
      if (!strcmp (type, "input"))
        continue;

      s = find_session (from, to, e, randstr);

      if (s == (session *) 0)
      {
        s = session_malloc ();
	  s->secs = 0;

	  if (!strcmp (type, "script"))
	    snprintf (s->script.str, BUFSIZ - 1, "%.1024s/%.128s",
		      config_option.logdir, dirp->d_name);
	  if (!strcmp (type, "time"))
	    {
	      snprintf (s->time.str, BUFSIZ - 1, "%.1024s/%.128s",
			config_option.logdir, dirp->d_name);
	      if (!argc) count_dur (s);
	    }

	  s->e = e;
	  snprintf (s->from, BUFSIZ, "%s", from);
	  snprintf (s->to, BUFSIZ, "%s", to);
	  snprintf (s->type, BUFSIZ, "%s", type);
	  snprintf (s->randstr, BUFSIZ, "%s", randstr);
	  format_date (s->date, 20, s->e);
	  snprintf (s->id, BUFSIZ - 1, "%.128s%c%.128s%c%ld%c%.128s", s->from,
		    config_option.fdl, s->to, config_option.fdl, s->e,
		    config_option.fdl, s->randstr);

	  link_session (s);
	}
      else
	{
	  if (!strcmp (type, "script"))
	    snprintf (s->script.str, BUFSIZ - 1, "%.1024s/%.128s",
		      config_option.logdir, dirp->d_name);
	  if (!strcmp (type, "time"))
	    {
	      snprintf (s->time.str, BUFSIZ - 1, "%.1024s/%.128s",
			config_option.logdir, dirp->d_name);
	      if (!argc) count_dur (s);
	    }
	}
    }

  if (closedir (dp) < 0)
    {
      LL ();
      fprintf (stderr,
	       "%s: can't closedir(%s): %s (%i) .. But teally this isn't fatal, it just wasn't expected.\n",
	       progname, config_option.logdir, strerror (errno), errno);
//      exit(EXIT_FAILURE);
    }

  if (argc)
    {
      char from[BUFSIZ];
      char to[BUFSIZ];
      char randstr[BUFSIZ-1];
      time_t e;
      int div, maxwait;
      session *s;

      snprintf (match, BUFSIZ - 1,
		"%%128[^%c]%c%%128[^%c]%c%%ld%c%%64[A-Za-z0-9]",
		config_option.fdl, config_option.fdl, config_option.fdl,
		config_option.fdl, config_option.fdl);

      if (sscanf (argv[0], match, from, to, &e, randstr) != 4)
	{
	  LL ();
	  fprintf (stderr, "%s: invalid format: %s\n", progname, argv[0]);
	  fprintf (stderr, "%s: the appropiate format is %s\n", progname,
		   match);
	  exit (EXIT_FAILURE);
	}

      s = find_session (from, to, e, randstr);

      if (s == (session *) 0)
	{
	  LL ();
	  fprintf (stderr, "%s: no such session: %s\n", progname, argv[0]);
	  exit (EXIT_FAILURE);
	}

      div = 1;
      maxwait = 1;

      if (argc > 1)
	div = atoi (argv[1]);
      if (argc > 2)
	maxwait = atoi (argv[2]);

      if (div == 0)
	{
	  FILE *f_script = fopen (s->script.str, "r");
	  char buffer[BUFSIZ];
	  size_t got;

	  if (f_script == (FILE *) 0)
	    {
	      LL ();
	      fprintf (stderr, "%s: %s: %s (%i)\n", progname, printable (s->script.str), strerror (errno), errno);
	      exit (EXIT_FAILURE);
	    }

	  while ((got = fread (buffer, 1, BUFSIZ, f_script)) > 0)
	    fwrite (buffer, 1, got, stdout);

	  fclose (f_script);

	}
      else
	replay (s->time.str, s->script.str, div, maxwait);

      exit (EXIT_SUCCESS);
    }

  if (!s_top)
    {
      fprintf (stderr, "No sessions are logged in %s\n",
	       config_option.logdir);
      exit (EXIT_SUCCESS);
    }

  fprintf (stderr, "%-19s %-8s %-12s %-12s %s\n", "Date", "Duration",
	   "From", "To", "ID");
  fprintf (stderr, "%-19s %-8s %-12s %-12s %s\n", "====", "========",
	   "====", "==", "==");

  s_top = sort_list (s_top);
  last = s_top;

  for (scan = s_top; scan; scan = scan->next)
    {
      if ((scan->script.fd = open (scan->script.str, O_RDONLY)) == -1)
	{
	  LL ();
	  fprintf (stderr,
		   "[error]: session %s is missing script information.\n",
		   printable (scan->id)); continue;
	}
      close (scan->script.fd);

      if (!strcmp (scan->type, "interactive"))
	fprintf (stdout, "%-19s %-8s %-12s %-12s %s\n", scan->date,
		 "-c arg", printable (scan->from), printable (scan->to), printable (scan->id));
      else
	fprintf (stdout, "%-19s %-8s %-12s %-12s %s\n", scan->date,
		 int2smalltime (scan->secs), printable (scan->from), printable (scan->to), printable (scan->id));
      last = scan;
    }

  if (s_top && last)
    fprintf (stderr, "\nUsage: %s ID [MULTIPLIER] [MAXWAIT]\n"
	     "See '%s -h' for more help.\n"
	     "Example: %s %s 1 2\n", progname, progname, progname, printable (last->id));
  else
    fprintf (stderr, "\nUsage: %s ID [MULTIPLIER] [MAXWAIT]\n"
	     "See '%s -h' for more help.\n", progname, progname);

  exit (EXIT_SUCCESS);
}

void
replay (const char *time, const char *script, int div, int maxwait)
{
  char read_buffer[BUFSIZ];
  char timebuf[BUFSIZ];
  float ftime = 0;
  int b = 0;
  int r = 0;
  char buffer[BUFSIZ];
  struct s_file s_time;
  struct s_file s_script;
  struct timeval tv;

  if (div < 1)
    div = 1;

  if (maxwait < 1)
    maxwait = 1;

  s_time.f = fopen (time, "r");

  if (s_time.f == (FILE *) 0)
    {
      LL ();
      fprintf (stderr, "%s: %s: %s (%i)\n", progname, printable (time), strerror (errno), errno);
      exit (EXIT_FAILURE);
    }

  if ((s_script.fd = open (script, O_RDONLY)) == -1)
    {
      LL ();
      fprintf (stderr, "%s: %s: %s (%i)\n", progname, printable (script), strerror (errno), errno);
      exit (EXIT_FAILURE);
    }

  for (s_time.line = 1; fgets (buffer, BUFSIZ - 1, s_time.f); s_time.line++)
    {

      if (sscanf (buffer, "%s %i\n", (char *) &timebuf, &b) != 2)
	{			/* find the two numbers */
	  LL ();
	  fprintf (stderr,
		   "[error]: line %i: invalid timing information (!= %%f %%i\\n)\n",
		   s_time.line);
	  exit (EXIT_FAILURE);
	}

      if (sscanf (timebuf, "%f", &ftime) != 1)
	{
	  LL ();
	  fprintf (stderr, "[error]: line %i: invalid float (!= %%f)\n",
		   s_time.line);
	  exit (EXIT_FAILURE);
	}

      ftime = ftime / div;
      /* MAXWAIT is in seconds of playback, so cap after scaling. */
      if (ftime > maxwait)
        ftime = maxwait;

      /* Split the delay into seconds and microseconds directly. Printing it
       * and reading it back with %i treated a leading zero as octal, so a
       * pause of 0.09 s became 0. */
      if (!(ftime > 0))
	ftime = 0;
      tv.tv_sec = (time_t) ftime;
      tv.tv_usec = (long) ((ftime - (float) tv.tv_sec) * 1000000.0f);

      /* Older sudosh wrote -1 when the pty read failed at the end of a
       * session. Those entries carry no data to replay. */
      if (b < 0)
	continue;

      /* sudosh never records more than one buffer at a time, so a larger
       * count is corrupt or hostile and would overflow read_buffer. */
      if (b > (int) sizeof (read_buffer))
	{
	  LL ();
	  fprintf (stderr,
		   "[error]: line %i: wanted to read %i bytes, but the limit is %i.\n",
		   s_time.line, b, (int) sizeof (read_buffer));
	  exit (EXIT_FAILURE);
	}

      memset (read_buffer, '\0', BUFSIZ);
      r = read (s_script.fd, read_buffer, (size_t) b);

      if (r != b)
	{
	  LL ();
	  fprintf (stderr, "[failure]: read %i bytes out of %i.\n", r, b);
	  exit (EXIT_FAILURE);
	}

      select (0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv);
      fwrite (read_buffer, 1, (size_t) b, stdout);
      fflush (stdout);
      memset (read_buffer, '\0', BUFSIZ);
    }
  close (s_script.fd);
  fclose (s_time.f);
  fprintf (stderr, "[info]: EOF\n");
  fflush (stderr);
}

session *
session_malloc (void)
{
  session *s = (session *) malloc (sizeof (session));

  if (!s)
    {
      LL ();
      fprintf (stderr, "%s: session malloc() failed: %s (%i)\n", progname,
	       strerror (errno), errno);
      exit (EXIT_FAILURE);
    }
  else
    memset (s, '\0', sizeof (session));

  return s;
}

session *
find_session (const char *from, const char *to, time_t e, const char *randstr)
{
  session *scan;

  for (scan = s_top; scan; scan = scan->next)
    if (scan->e == e)
      if (!strcmp (from, scan->from))
	if (!strcmp (to, scan->to))
	  if (!strcmp (randstr, scan->randstr))
	    return scan;

  return (session *) 0;
}

session *
find_max (time_t e)
{
  session *scan;

  for (scan = s_top; scan; scan = scan->next)
    if (scan->e == e)
      return scan;

  return (session *) 0;
}

void
link_session (session * s)
{
  s->next = s_top;

  if (s_top)
    s_top->prev = s;
  s_top = s;
}

void
unlink_session (session * s)
{
  if (s->next)
    s->next->prev = s->prev;

  if (s->prev)
    s->prev->next = s->next;
  else
    s_top = s->next;

  s->next = (session *) 0;
  s->prev = (session *) 0;

  return;
}

void
show_sessions (void)
{
  session *scan;

  for (scan = s_top; scan; scan = scan->next)
    {
      fprintf (stderr, "%s\n", scan->from);
    }
}

session *
sort_list (session * list)
{
  session *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;

  if (!list)
    return (session *) 0;

  insize = 1;

  while (1)
    {
      p = list;
      list = (session *) 0;
      tail = (session *) 0;
      nmerges = 0;

      while (p)
	{
	  nmerges++;
	  q = p;
	  psize = 0;

	  for (i = 0; i < insize; i++)
	    {
	      psize++;
	      q = q->next;
	      if (!q)
		break;
	    }

	  qsize = insize;

	  while (psize > 0 || (qsize > 0 && q))
	    {
	      if (psize == 0)
		{
		  e = q;
		  q = q->next;
		  qsize--;
		}
	      else if (qsize == 0 || !q)
		{
		  e = p;
		  p = p->next;
		  psize--;
		}
	      else if (p->e < q->e)
		{
		  e = p;
		  p = p->next;
		  psize--;
		}
	      else
		{
		  e = q;
		  q = q->next;
		  qsize--;
		}

	      if (tail)
		tail->next = e;
	      else
		list = e;
	      tail = e;
	    }
	  p = q;
	}
      tail->next = (session *) 0;

      if (nmerges <= 1)
	return list;

      insize *= 2;
    }
}

void
count_dur (session * s)
{
  struct s_file s_time;
  char buffer[BUFSIZ];
  float t_time = 0;

  /* One unreadable or malformed time file must not stop the whole listing,
   * so report it and leave the duration at 0. */
  s->secs = 0;
  s_time.f = fopen (s->time.str, "r");

  if (s_time.f == (FILE *) 0)
    {
      fprintf (stderr, "[warning]: no duration for %s: %s (%i)\n", printable (s->time.str),
	       strerror (errno), errno);
      return;
    }

  for (s_time.line = 1; fgets (buffer, BUFSIZ - 1, s_time.f); s_time.line++)
    {
      char time[BUFSIZ];
      float ftime = 0;
      int b = 0;

      if (sscanf (buffer, "%s %i\n", (char *) &time, &b) != 2
	  || sscanf (time, "%f", &ftime) != 1)
	{
	  fprintf (stderr, "[warning]: no duration for %s: line %i is not valid timing information\n",
		   printable (s->time.str), s_time.line); fclose (s_time.f);
	  return;
	}

      if (ftime > 0)
	t_time += ftime;
    }

  /* The duration is shown as an int number of seconds. */
  if (!(t_time < 2147483647.0f))
    t_time = 2147483647.0f;
  s->secs = (long) t_time;
  fclose (s_time.f);
}
