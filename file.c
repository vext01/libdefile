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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"

void __dead		 usage(void);
struct df_file		*df_open(const char *);
void			 df_state_init_files(int, char **);
int			 df_check_match(struct df_file *);
int			 df_check_match_fs(struct df_file *);
int			 df_check_match_magic(struct df_file *);
struct df_match		*df_match_add(struct df_file *, enum match_class,
    const char *);

/* prototype magic matching */
struct df_magic_match		*df_next_magic_candidate(void);
struct df_magic_match_field	*df_parse_magic_line(char *line);

extern char	*__progname;
struct df_state df_state;

void __dead
usage(void)
{
	fprintf(stderr, "usage: %s file [file...]\n", __progname);
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

struct df_magic_match_field *
df_parse_magic_line(char *line)
{
	/* strsep, put tokens into df_magic_match_field struct XXX */
	return (NULL);
}

/*
 * parses the next potential match from magic db and returns in a struct
 */
#define DF_MAX_MAGIC_LINE		256
struct df_magic_match *
df_next_magic_candidate(void)
{
	char			 line[DF_MAX_MAGIC_LINE];
	int			 first_rec = 1;
	struct df_magic_match	*dm;

	dm = calloc(1, sizeof(*dm));
	if (!dm)
		err(1, "df_next_magic_candidate: calloc");
	TAILQ_INIT(&dm->df_fields);

	while (1) {
		if (fgets(line, DF_MAX_MAGIC_LINE, df_state.magic_file) == NULL)
			break;

		/* skip blank lines and comments */
		if ((strlen(line) <= 1) || (line[0] == '#'))
			continue;

		if ((!first_rec) && (line[0] != '>')) {
			/* woops, beginning of next match, go back */
			fseek(df_state.magic_file, -(strlen(line)), SEEK_CUR);
			break;
		}

		/* XXX field struct into dm */
		df_parse_magic_line(line);
		first_rec = 0;
	}

	if (feof(df_state.magic_file)) {
		free(dm);
		return (NULL); /* no more */
	}

	if (ferror(df_state.magic_file))
		err(1, "df_next_magic_candidate");

	return (dm);
}

/*
 * Search for matches in magic
 */
int
df_check_match_magic(struct df_file *df)
{
	int			matches = 0;
	struct df_magic_match	*dm;

	if (!df_state.magic_file)
		return (0);

	rewind(df_state.magic_file);
	while((dm = df_next_magic_candidate()) != NULL) {
		/* decide if it is a match XXX */
		free(dm);
	}

	return (matches);
}

/*
 * Search for matches in filesystem goo.
 */
int
df_check_match_fs(struct df_file *df)
{
	struct stat	sb;
	char		buf[MAXPATHLEN];

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
	if (S_ISDIR(sb.st_mode))
		df_match_add(df, MC_FS, "directory");
	if (df_state.check_flags & CHK_NOSPECIAL)
		goto ordinary;
	if (S_ISCHR(sb.st_mode)) {
		df_match_add(df, MC_FS, "character special");
		return (0);
	}
	if (S_ISBLK(sb.st_mode)) {
		df_match_add(df, MC_FS, "block special");
		return (0);
	}
	if (S_ISFIFO(sb.st_mode)) {
		df_match_add(df, MC_FS, "fifo (named pipe)");
		return (0);
	}
	/* TODO DOOR ? */
	if (S_ISLNK(sb.st_mode)) {
		if (readlink(df->filename, buf, sizeof(buf)) == -1) {
			warn("unreadable symlink `%s'");
			return (-1);
		}
		buf[sizeof(buf) - 1] = 0; /* readlink does not terminate */
		/* TODO */
	}
	if (S_ISSOCK(sb.st_mode)) {
		df_match_add(df, MC_FS, "socket");
		return (0);
	}
ordinary:
	if (sb.st_size == 0)
		df_match_add(df, MC_FS, "empty");

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
	int		 ch;

	while ((ch = getopt(argc, argv, "s")) != -1) {
		switch (ch) {
		case 's':	/* Treat file devices as ordinary files */
			df_state.check_flags |= CHK_NOSPECIAL;
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
		df_check_match(df);

	return (EXIT_SUCCESS);
}
