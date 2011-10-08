/*
 * Copyright (c) 2011, Edd Barrett <vext01@gmail.com>
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
void		 df_state_init(void);

extern char *__progname;
struct df_state *df_state;

void __dead
usage(void)
{
	fprintf(stderr, "usage: %s file [file...]\n", __progname);
	exit(1);
}

void
df_state_init(void)
{
	if ((df_state = calloc(1, sizeof(*df_state))) == NULL)
		err(1, "calloc");
	TAILQ_INIT(&df_state->df_files);
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

int
main(int argc, char **argv)
{
	struct df_file	*df;

	if (argc < 2)
		usage();

	df_state_init();
	
	return (EXIT_SUCCESS);	
}

