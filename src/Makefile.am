bin_PROGRAMS=sudosh sudosh-replay
# Ask glibc for POSIX/BSD interfaces even under -std=c17. Must be on the
# command line: sudosh.c includes system headers before super.h.
AM_CPPFLAGS=-D_DEFAULT_SOURCE
sudosh_SOURCES=sudosh.c rand.c parse.c string.c getopt.h  struct.h  super.h util.c
sudosh_replay_SOURCES=replay.c getopt.c getopt.h string.c parse.c  util.c

sysconf_DATA = sudosh.conf
EXTRA_DIST=sudosh.conf
