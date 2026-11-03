#include "super.h"

void
mklogdir (char *logdir)
{
  struct stat d;
  mode_t logdir_mode;

  logdir_mode = S_IRUSR | S_IWUSR | S_IXUSR | S_IWGRP | S_IXGRP |
    S_IWOTH | S_IXOTH | S_ISVTX | S_IFDIR;
  /* The sticky bit stops users deleting or renaming each other's logs. */

  /* Make the log dir if it doesn't exist */
  if (stat (logdir, &d) == -1)
    {
      if (mkdir (logdir, logdir_mode) == -1)
	{
	  char str[BUFSIZ];

	  snprintf (str, BUFSIZ - 1, "mkdir(%s)", logdir);
	  perror (str);
	  fprintf (stderr,
		   "Directory %s needs to exist and be created by root with the permissions of 1733\n",
		   logdir);
	  exit (EXIT_FAILURE);
	}
      fprintf (stderr, "[info]: created directory %s\r\n", logdir);
    }

  /* mkdir does not fill in d, so stat the directory that was just made. */
  if (stat (logdir, &d) == -1)
    {
      perror (logdir);
      exit (EXIT_FAILURE);
    }

  /* Never change the permissions of something that is not a directory. */
  if (!S_ISDIR (d.st_mode))
    {
      fprintf (stderr, "logdir '%s' is not a directory.\n", logdir);
      exit (EXIT_FAILURE);
    }

  /* Set perms if they're not what we require */
  if (d.st_mode != logdir_mode)
    {
      if (chmod (logdir, logdir_mode) == -1)
	{
	  char str[BUFSIZ];

	  snprintf (str, BUFSIZ - 1, "chmod(%s)", logdir);
	  perror (str);
	  fprintf (stderr,
		   "Directory %s needs to to have its permissions set to 1733.\n",
		   logdir);
	  exit (EXIT_FAILURE);
	}
      fprintf (stderr, "[info]: chmod 1733 directory %s\n", logdir);
    }
}

void validateLogdir(char *logdir)
{
	struct stat logdir_stat;

	/* check everything at once, attempt to fix if incorrect, die if it cannot be fixed */
	/* Creates a missing logdir, and corrects the permissions of an existing one. */
	mklogdir(logdir);

	if ((stat(logdir, &logdir_stat)) == -1)
	{
		fprintf(stderr, "Fatal error - logdir %s invalid: %s\n", logdir,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!(logdir_stat.st_mode & S_IFDIR))
	{
		fprintf(stderr, "logdir '%s' is not a directory.\n", logdir);
		exit(EXIT_FAILURE);
	}

}
