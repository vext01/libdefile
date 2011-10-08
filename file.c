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
void		 df_check_fs(struct df_file *);


extern char	*__progname;
struct df_state df_state;

void __dead
usage(void)
{
	fprintf(stderr, "usage: %s file [file...]\n", __progname);
	exit(1);
}

/* Initializes the world */
void
df_state_init(int argc, char **argv)
{
	struct df_file *df;
	int i;

	TAILQ_INIT(&df_state.df_files);
	for (i = 0; i < argc; i++) {
		if ((df = df_open(argv[i])) == NULL)
			err(1, "df_open");
		TAILQ_INSERT_TAIL(&df_state.df_files, df, entry);
	}
}

/* Lib entry point */
struct df_file *
df_open(const char *filename)
{
	struct df_file *df;

	if ((df = calloc(1, sizeof(*df))) == NULL)
		return (NULL);
	df->filename = strdup(filename);
	if (df->filename == NULL) {
		free(df);
		return (NULL);
	}
	df->file = fopen(df->filename, "r");
	if (df->file == NULL) {
		free(df->filename);
		free(df);
		return (NULL);
	}
	/* XXX magic ? */

	return (df);
}

void
df_check_fs(struct df_file *df)
{
	struct stat sb;

	if (stat(df->filename, &sb) == -1)
		err(1, "stat: %s", df->filename);

	if (sb.st_mode & S_ISUID)
		printf("setuid ");
	if (sb.st_mode & S_ISGID)
		printf("setgid ");
	if (sb.st_mode & S_ISVTX)
		printf("sticky ");
}

int
main(int argc, char **argv)
{
	struct df_file *df;
	
	if (argc < 2)
		usage();

	argc--;
	argv++;
	df_state_init(argc, argv);

	TAILQ_FOREACH(df, &df_state.df_files, entry) {
		df_check_fs(df);
		/* df_check_magic(df); */
		/* df_check_mime(df); */
	}

	return (EXIT_SUCCESS);
}
