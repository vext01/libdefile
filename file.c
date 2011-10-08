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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "file.h"

void __dead	 usage(void);
struct df_file	*df_open(const char *);
void		 df_state_init(int, char **);
char		 df_match(void);
void		 df_match_all_files(void);
char		*df_match_file(struct df_file *df);

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
 */
void
df_state_init(int argc, char **argv)
{
	struct df_file	*df;
	int		 i;

	TAILQ_INIT(&df_state.df_files);
	for (i = 0; i < argc; i++) {
		if ((df = df_open(argv[i])) == NULL)
			err(1, "df_open: %s", argv[i]);
		TAILQ_INSERT_TAIL(&df_state.df_files, df, entry);
	}

	df_state.magic_file = fopen(MAGIC, "r");
	if (df_state.magic_file == NULL)
		err(1, "df_open: %s", MAGIC);
}

/* Lib entry point */
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
 *
 * I guess this will return a textual representation? XXX
 */
char *
df_match_file(struct df_file *df)
{
	char		*ret = "XXX";

	if (!df_state.magic_file)
		return (NULL);

	rewind(df_state.magic_file);

	return (ret);

}

/*
 * Try to match every file that was passed on cmd line
 */
void
df_match_all_files()
{
	struct df_file		*f;
	char			*ident;

	TAILQ_FOREACH(f, &df_state.df_files, entry) {
		ident = df_match_file(f);
		if (!ident)
			printf("%s: failed to identify\n", f->filename);
		else
			printf("%s: %s\n", f->filename, ident);
	}
}

int
main(int argc, char **argv)
{
	if (argc < 2)
		usage();

	argc--;
	argv++;
	df_state_init(argc, argv);

	df_match_all_files();

	return (EXIT_SUCCESS);	
}

