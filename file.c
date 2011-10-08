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

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "file.h"

void __dead	 usage(void);
struct df_file	*df_open(const char *);
void		 df_state_init(int, char **);
int		 df_check_match(struct df_file *);
int		 df_check_match_fs(struct df_file *);
int		 df_check_match_magic(struct df_file *);
struct df_match *df_match_add(struct df_file *, enum match_class,
    const char *);

extern char	*__progname;
struct df_state df_state;

void __dead
usage(void)
{
	fprintf(stderr, "usage: %s file [file...]\n", __progname);
	exit(1);
}

/*
 * Initializes the world
 *
 * Opens all files and pushes into a TAILQ
 * Also opens magic
 * Lib
 */
void
df_state_init(int argc, char **argv)
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

	df->filename = strdup(filename);
	if (df->filename == NULL)
		goto err;

	df->file = fopen(df->filename, "r");
	if (df->file == NULL)
		goto err;

	TAILQ_INIT(&df->df_matches);

	/* success */
	return (df);
err:
	if (df->filename)
		free(df->filename);
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
df_check_match_magic(struct df_file *df)
{
	int matches = 0;

	if (!df_state.magic_file)
		return (0);

	rewind(df_state.magic_file);

	return (matches);

}

/*
 * Search for matches in filesystem goo.
 */
int
df_check_match_fs(struct df_file *df)
{
	struct stat	sb;

	if (stat(df->filename, &sb) == -1) {
		warn("stat: %s", df->filename);
		return (-1);
	}
	if (sb.st_mode & S_ISUID)
		df_match_add(df, MC_FS, "setuid");
	if (sb.st_mode & S_ISGID)
		df_match_add(df, MC_FS, "setgid");
	if (sb.st_mode & S_ISVTX)
		df_match_add(df, MC_FS, "sticky");
	if (sb.st_mode & S_IFDIR)
		df_match_add(df, MC_FS, "directory");

	return (0);
}

/*
 * Builds and adds a match at the end of df->df_matches
 */
struct df_match *
df_match_add(struct df_file *df, enum match_class mc, const char *desc)
{
	struct df_match *dm;

	if ((dm = calloc(1, sizeof(*dm))) == NULL)
		err(1, "calloc"); /* XXX */
	dm->class = mc;
	dm->desc  = desc;
	TAILQ_INSERT_TAIL(&df->df_matches, dm, entry);

	return (dm);
}

/*
 * Check
 */
int
df_check_match(struct df_file *df)
{
	struct df_match *dm;

	(void)df_check_match_fs(df);
	(void)df_check_match_magic(df);

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

	if (argc < 2)
		usage();

	argc--;
	argv++;
	df_state_init(argc, argv);

	TAILQ_FOREACH(df, &df_state.df_files, entry)
		df_check_match(df);

	return (EXIT_SUCCESS);
}
