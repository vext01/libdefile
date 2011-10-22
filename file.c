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

#include "file.h"

void __dead		 usage(void);
struct df_file		*df_open(const char *);
void			 df_state_init_files(int, char **);
int			 df_check(struct df_file *);
int			 df_check_fs(struct df_file *);
int			 df_check_magic(struct df_file *);
struct df_match		*df_match_add(struct df_file *, enum match_class,
    const char *, ...);

/* prototype magic matching */
struct df_magic_match		*df_next_magic_candidate(void);
struct df_magic_match_field	*df_parse_magic_line(char *line,
    struct df_magic_match_field *last_dm);

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

#define DF_NUM_MAGIC_FIELDS			4
#define DF_MIME_TOKEN				"!:mime"

/*
 * Jam a test into a struct
 */
struct df_magic_match_field *
df_parse_magic_line(char *line, struct df_magic_match_field *last_dmf)
{
	char			*tokens[DF_NUM_MAGIC_FIELDS] = {0, 0, 0, 0};
	char			*nxt = line;
	int			 n_tok = 0;
	struct df_magic_match_field	*dmf;

	while ((n_tok < DF_NUM_MAGIC_FIELDS) && (nxt != NULL))
		tokens[n_tok++] = strsep(&nxt, " \t");

	if (n_tok < DF_NUM_MAGIC_FIELDS - 2) /* last 2 fields optional */
		errx(1, "%s: short field count at line %d", MAGIC,
		    df_state.magic_line);

	/*
	 * if it was a mime line, we don't make a new record, but append
	 * mime type info to the last field we found. In this case we return
	 * the same pointer as before to inidicate this was teh case.
	 */
	if (strncmp(tokens[1], DF_MIME_TOKEN, strlen(DF_MIME_TOKEN)) == 0) {
		if (last_dmf == NULL)
			errx(1, "can't append mime info to nothing!");

		last_dmf->mime = strdup(tokens[2]);
		return (last_dmf);
	}

	dmf = calloc(1, sizeof(*dmf));
	if (dmf == NULL)
		err(1, "calloc");

	/* XXX populate */

	return (dmf);
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
	struct df_magic_match_field	*dmf, *last_dmf = NULL;

	dm = calloc(1, sizeof(*dm));
	if (!dm)
		err(1, "df_next_magic_candidate: calloc");
	TAILQ_INIT(&dm->df_fields);

	while (1) {
		if (fgets(line, DF_MAX_MAGIC_LINE,df_state.magic_file) == NULL)
			break;

		df_state.magic_line++;

		/* skip blank lines and comments */
		if ((strlen(line) <= 1) || (line[0] == '#'))
			continue;

		if ((!first_rec) && (line[0] != '>')) {
			/* woops, beginning of next match, go back */
			fseek(df_state.magic_file, -(strlen(line)), SEEK_CUR);
			break;
		}

		/* XXX field struct into dm */
		dmf = df_parse_magic_line(line, last_dmf);

		if (dmf != last_dmf)
			TAILQ_INSERT_TAIL(&dm->df_fields, dmf, entry);

		last_dmf = dmf;
		first_rec = 0;
	}

	if (feof(df_state.magic_file)) {
		free(dm);
		return (NULL); /* no more */
	}

	if (ferror(df_state.magic_file))
		err(1, "%s", __func__);

	return (dm);
}

/*
 * Search for matches in magic
 */
int
df_check_magic(struct df_file *df)
{
	int			matches = 0;
	struct df_magic_match	*dm;

	if (!df_state.magic_file)
		return (0);

	rewind(df_state.magic_file);
	df_state.magic_line = 1;

	while((dm = df_next_magic_candidate()) != NULL) {
		/* decide if it is a match XXX */
		free(dm); /* XXX and the rest */
	}

	return (matches);
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

	(void)df_check_fs(df);
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
		df_check(df);

	return (EXIT_SUCCESS);
}
