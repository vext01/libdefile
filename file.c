/*
 * Copyright (c) 2011, Edd Barrett <vext01@gmail.com>
 * Copyright (c) 2011, Christiano F. Haesbaert <haesbaert@haesbaert.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "file.h"

void __dead		 usage(void);
struct df_file		*df_open(const char *);
void			 df_state_init_files(int, char **);
int			 df_check(struct df_file *);
int			 df_check_fs(struct df_file *);
int			 df_check_magic(struct df_file *);
struct df_match		*df_match_add(struct df_file *, enum match_class,
    const char *, ...);

extern char	*__progname;
struct df_state df_state;

void __dead
usage(void)
{
	fprintf(stderr, "usage: [-Ls] %s file [file...]\n", __progname);
	exit(1);
}

/*
 * Opens all files and pushes into a TAILQ
 * Also opens magic
 * Lib
 */
void
df_state_init_files(int argc, char **argv)
{
	struct df_file	*df;
	int		 i;

	TAILQ_INIT(&df_state.df_files);
	for (i = 0; i < argc; i++) {
		/* XXX we can't bail out, fs may match  */
		if ((df = df_open(argv[i])) == NULL)
			err(1, "df_open: %s", argv[i]);
		TAILQ_INSERT_TAIL(&df_state.df_files, df, entry);
	}
	/* XXX we can't bail out, other classes may match */
	df_state.magic_file = fopen(MAGIC, "r");
	if (df_state.magic_file == NULL)
		err(1, "df_open: %s", MAGIC);
}

/* Lib */
struct df_file *
df_open(const char *filename)
{
	struct df_file *df;

	if ((df = calloc(1, sizeof(*df))) == NULL)
		goto err;

	if (strlcpy(df->filename, filename, sizeof(df->filename)) >=
	    sizeof(df->filename)) {
		errno = ENAMETOOLONG;
		goto err;
	}

	df->file = fopen(df->filename, "r");
	if (df->file == NULL)
		goto err;

	TAILQ_INIT(&df->df_matches);

	/* success */
	return (df);
err:
	if (df->file)
		fclose(df->file);
	if (df)
		free(df);

	return (NULL);
}

/*
 * Search for matches in magic
 */
int
df_check_magic(struct df_file *df)
{
	size_t linelen;
	char *line, *p, **ap;
	struct df_parser dp;

	/* No magic file, so no matches */
	if (df_state.magic_file == NULL)
		return (0);
	/* If file is empty, no matches */
	if (df->sb.st_size == 0)
		return (0);
	/* We'll reparse the file */
	rewind(df_state.magic_file);
	bzero(&dp, sizeof(dp));
	/* Parser state */
	dp.magic_file = df_state.magic_file;
	dp.level      = -1;
	dp.lineno     = 0;
	/* Get a line */
	while (!feof(df_state.magic_file)) {
		if ((line = fparseln(df_state.magic_file, &linelen, &dp.lineno,
		    NULL, 0)) == NULL) {
			if (ferror(df_state.magic_file)) {
				warn("magic file");
				return (-1);
			} else
				continue;
		}
		p = line;
		if (*p == 0) 
			goto nextline;
		/* Break The Line !, Guano Apes rules */
		for (ap = dp.argv; ap < &dp.argv[3] &&
			 (*ap = strsep(&p, " \t")) != NULL;) {
			if (**ap != 0)
				ap++;
		}
		*ap	   = NULL;
		/* Get the remainder of the line */
		dp.argv[3] = p;
		dp.argv[4] = NULL;

	nextline:
		free(line);
	}
	
	return (0);
}

/*
 * Search for matches in filesystem goo.
 */
int
df_check_fs(struct df_file *df)
{
	char		 buf[MAXPATHLEN];
	int		 n;
	struct df_file	*df2;

	if (lstat(df->filename, &df->sb) == -1) {
		warn("stat: %s", df->filename);
		return (-1);
	}
	if (S_ISLNK(df->sb.st_mode)) {
		bzero(buf, sizeof(buf));
		n = readlink(df->filename, buf, sizeof(buf));
		if (n == -1) {
			warn("unreadable symlink `%s'", df->filename);
			return (-1);
		}
		if (df_state.check_flags & CHK_FOLLOWSYMLINKS) {
			if ((df2 = df_open(buf)) == NULL) {
				warn("can't follow symlink `%s'", buf);
				return (-1);
			}
			/* Our next df will be the followed symlink */
			TAILQ_INSERT_AFTER(&df_state.df_files, df, df2, entry);
			return (0);
		}
		df_match_add(df, MC_FS, "symbolic link to `%s'", buf);
		return (0);
	}
	if (df->sb.st_mode & S_ISUID)
		df_match_add(df, MC_FS, "setuid");
	if (df->sb.st_mode & S_ISGID)
		df_match_add(df, MC_FS, "setgid");
	if (df->sb.st_mode & S_ISVTX)
		df_match_add(df, MC_FS, "sticky");
	if (S_ISDIR(df->sb.st_mode))
		df_match_add(df, MC_FS, "directory");
	if (df_state.check_flags & CHK_NOSPECIAL)
		goto ordinary;
	if (S_ISCHR(df->sb.st_mode)) {
		df_match_add(df, MC_FS, "character special");
		return (0);
	}
	if (S_ISBLK(df->sb.st_mode)) {
		df_match_add(df, MC_FS, "block special");
		return (0);
	}
	if (S_ISFIFO(df->sb.st_mode)) {
		df_match_add(df, MC_FS, "fifo (named pipe)");
		return (0);
	}
	/* TODO DOOR ? */
	if (S_ISSOCK(df->sb.st_mode)) {
		df_match_add(df, MC_FS, "socket");
		return (0);
	}
ordinary:
	if (df->sb.st_size == 0)
		df_match_add(df, MC_FS, "empty");

	return (0);
}

/*
 * Builds and adds a match at the end of df->df_matches
 */
struct df_match *
df_match_add(struct df_file *df, enum match_class mc, const char *desc, ...)
{
	struct df_match *dm;
	va_list		 ap;

	if ((dm = calloc(1, sizeof(*dm))) == NULL)
		err(1, "calloc"); /* XXX */
	dm->class = mc;
	va_start(ap, desc);
	if (vsnprintf(dm->desc, sizeof(dm->desc), desc, ap) >=
	    (int)sizeof(dm->desc))
		err(1, "vsnprintf");
	va_end(ap);
	TAILQ_INSERT_TAIL(&df->df_matches, dm, entry);

	return (dm);
}

/*
 * Check
 */
int
df_check(struct df_file *df)
{
	struct df_match *dm;

	if (df_check_fs(df) == -1)
		return (-1);
	(void)df_check_magic(df);

	if (!TAILQ_EMPTY(&df->df_matches))
		printf("%s: ", df->filename);
	TAILQ_FOREACH(dm, &df->df_matches, entry)
		printf("%s ", dm->desc);
	if (!TAILQ_EMPTY(&df->df_matches))
		printf("\n");

	return (0);
}

int
main(int argc, char **argv)
{
	struct df_file	*df;
	int		 ch;

	while ((ch = getopt(argc, argv, "Ls")) != -1) {
		switch (ch) {
		case 's':	/* Treat file devices as ordinary files */
			df_state.check_flags |= CHK_NOSPECIAL;
			break;
		case 'L':
			df_state.check_flags |= CHK_FOLLOWSYMLINKS;
			break;
		default:
			usage();
			break;	/* NOTREACHED */
		}
	}
	argv += optind;
	argc -= optind;
	if (argc == 0)
		usage();

	df_state_init_files(argc, argv);
	TAILQ_FOREACH(df, &df_state.df_files, entry)
		(void)df_check(df);

	return (EXIT_SUCCESS);
}
