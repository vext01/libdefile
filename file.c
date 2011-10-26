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

char			*xstrdup(char *old);
void __dead		 usage(void);
int			 str2mt(const char *);
struct df_file		*df_open(const char *);
void			 df_state_init_files(int, char **);
int			 df_check(struct df_file *);
int			 df_check_fs(struct df_file *);
int			 df_check_magic(struct df_file *);
struct df_match		*df_match_add(struct df_file *, enum match_class,
    const char *, ...);
int			 dp_prepare(struct df_parser *);
int			 dp_prepare_mo(struct df_parser *, const char *);

extern char	*malloc_options;
extern char	*__progname;
struct df_state  df_state;
int		 df_debug;

struct {
	int		 mt;
	const char	*str;
} mt_table[] = {
	{ MT_UNKNOWN,	"unknown" },
	{ MT_BYTE,	"byte" },
	{ MT_SHORT,	"short" },
	{ MT_LONG,	"long" },
	{ MT_QUAD,	"quad" },
	{ MT_FLOAT,	"float" },
	{ MT_DOUBLE,	"double" },
	{ MT_STRING,	"string" },
	{ MT_PSTRING,	"pstring" },
	{ MT_DATE,	"date" },
	{ MT_QDATE,	"qdate" },
	{ MT_LDATE,	"ldate" },
	{ MT_QLDATE,	"qldate" },
	{ MT_BESHORT,	"beshort" },
	{ MT_BELONG,	"belong" },
	{ MT_BEQUAD,	"bequad" },
	{ MT_BEFLOAT,	"befloat" },
	{ MT_BEDOUBLE,	"bedouble" },
	{ MT_BEDATE,	"bedate" },
	{ MT_BEQDATE,	"beqdate" },
	{ MT_BELDATE,	"beldate" },
	{ MT_BEQLDATE,	"beqldate" },
	{ MT_BESTRING16,"bestring16" },
	{ MT_LESHORT,	"leshort" },
	{ MT_LELONG,	"lelong" },
	{ MT_LEQUAD,	"lequad" },
	{ MT_LEFLOAT,	"lefloat" },
	{ MT_LEDOUBLE,	"ledouble" },
	{ MT_LEDATE,	"ledate" },
	{ MT_LEQDATE,	"leqdate" },
	{ MT_LELDATE,	"leldate" },
	{ MT_LEQLDATE,	"leqldate" },
	{ MT_LESTRING16,"lestring16" },
	{ MT_MELONG,	"melong" },
	{ MT_MEDATE,	"medate" },
	{ MT_MELDATE,	"meldate" },
	{ MT_REGEX,	"regex" },
	{ MT_SEARCH,	"search" },
	{ MT_DEFAULT,	"default" },
	{ -1,		NULL },
};

char *
xstrdup(char *old)
{
	char	*p;

	if ((p = strdup(old)) == NULL)
		err(1, "failed to alloc");

	return (p);
}

void __dead
usage(void)
{
	/* XXX the more '-d' specified, the more verbose. How to express this in usage()? */
	fprintf(stderr, "usage: [-dLs] [-f magic] %s file [file...]\n",
	    __progname);
	exit(1);
}

int
str2mt(const char *str)
{
	int i;

	for (i = 0; mt_table[i].mt != -1; i++) {
		if (strcmp(str, mt_table[i].str) == 0)
			return (mt_table[i].mt);
	}

	return (MT_UNKNOWN);
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
		if ((df = df_open(argv[i])) == NULL)
			warn("df_open: %s", argv[i]);
		else
			TAILQ_INSERT_TAIL(&df_state.df_files, df, entry);
	}

	df_state.magic_file = fopen(df_state.magic_path, "r");
	if (df_state.magic_file == NULL)
		warn("df_open: %s", df_state.magic_path);
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
	dp.line	      = NULL;
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
		/* This duplication is only for debugging purposes */
		dp.line = xstrdup(line);
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
		/* Convert to something meaningfull */
		if (dp_prepare(&dp) == -1)
			goto nextline;
		DPRINTF(2, "%s: (ml = %d, mo = %lu (%d))",
		    dp.line, dp.ml, dp.mo, dp.mo_itype);
	nextline:
		free(line);
		free(dp.line);
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

/*
 * Prepare magic offset
 */
int
dp_prepare_mo(struct df_parser *dp, const char *s)
{
	char *end = NULL;
	const char *cp = s;
	const char *errstr = NULL;

	if (cp == NULL)
		goto errorinv;
	/*
	 * Check for an indirect offset, we're parsing something like:
	 * (0x3c.l)
	 * (( x [.[bslBSL]][+-][ y ])
	 */
	if (*cp == '(') {
		if ((end = strchr(cp, ')')) == NULL) {
			warnx("Unclosed paren at line %zd", dp->lineno);
			return (-1);
		}
		*end = 0;	/* terminate */
		dp->mflags |= MF_INDIRECT;
		cp++;		/* Jump over ( */
		/* If type not specified, assume long */
		if ((end = strchr(cp, '.')) == NULL)
			dp->mo_itype = MT_LONG;
		else {
			switch (*cp) {
			case 'c':
			case 'b':
			case 'C':
			case 'B':
				dp->mo_itype = MT_BYTE;
				break;
			case 'h':
			case 's':
				dp->mo_itype = MT_LESHORT;
				break;
			case 'l':
				dp->mo_itype = MT_LELONG;
				break;
			case 'S':
				dp->mo_itype = MT_BESHORT;
				break;
			case 'L':
				dp->mo_itype = MT_BELONG;
				break;
			case 'e':
			case 'f':
			case 'g':
				dp->mo_itype = MT_LEDOUBLE;
				break;
			case 'E':
			case 'F':
			case 'G':
				dp->mo_itype = MT_BEDOUBLE;
				break;
			default:
				warnx("indirect offset type `%c' "
				    "invalid at line %zd", *cp, dp->lineno);
				return (-1);
				break; /* NOTREACHED */
			}
		}
	}
	/* TODO handle negative and octal */
	if (cp == NULL)
		goto errorinv;
	/* Try hex */
	if (strlen(cp) > 1 && cp[0] == '0' && cp[1] == 'x') {
		errno = 0;
		dp->mo = strtoll(cp, NULL, 16);
		if (errno) {
			warn("dp_prepare_mo: strtoll: %s "
			    "line %zd", cp, dp->lineno);
			return (-1);
		}
	}
	dp->mo = (unsigned long)strtonum(cp, 0,
	    LLONG_MAX, &errstr);
	if (errstr) {
		warn("dp_prepare_mo: strtonum %s at line %zd",
		    cp, dp->lineno);
		return (-1);
	}

	return (0);

errorinv:
	warnx("dp_prepare_mo: Invalid offset at line %zd",
	    dp->lineno);

	return (-1);
}
/*
 * Bake dp into something usable.
 */
int
dp_prepare(struct df_parser *dp)
{
	char *cp, *mask;
	u_int64_t maskval;

	/* Reset */
	dp->ml = 0;
	dp->mo = 0;
	dp->mt = MT_UNKNOWN;
	/* First analyze level */
	if (*dp->argv[0] == '0')
		dp->ml = 0;
	else if (*dp->argv[0] == '>') {
		cp = dp->argv[0];
		/* Count the > */
		while (cp && *cp == '>') {
			dp->ml++;
			cp++;
		}
		if (dp_prepare_mo(dp, cp) == -1)
			return (-1);
	} else {
		warnx("dp_prepare: unexpected %s", dp->argv[0]);
		return (-1);
	}


	/* Second, analyze test type */
	/* Split mask and test type first */
	cp   = dp->argv[1];
	mask = strchr(cp, ':');
	if (mask != NULL) {
		*mask++ = 0;
		/* Octa TODO */
		/* Hexa */
		if (strlen(mask) > 1 && mask[0] == '0' && mask[1] == 'x') {
			errno = 0;
			maskval = strtoll(mask, NULL, 16);
			if (errno) {
				warn("dp_prepare: %s", mask);
				return (-1);
			}
		}
		/* Decimal TODO */
	}
	/* Convert the string to something meaningful */
	if ((dp->mt = str2mt(cp)) == MT_UNKNOWN) {
		warnx("dp_prepare: Uknown magic type %s at line %zd",
		    cp, dp->lineno);
		return (-1);
	}

	return (0);
}

int
main(int argc, char **argv)
{
	struct df_file	*df;
	int		 ch;

#ifdef DEBUG
	malloc_options = "AFGJPXS";
#endif
	df_state.magic_path = MAGIC;

	while ((ch = getopt(argc, argv, "df:Ls")) != -1) {
		switch (ch) {
		case 'd':
#ifndef DEBUG
			errx(1, "this binary was not built with -DDEBUG");
#endif
			df_debug++;
			break;
		case 'f':
			df_state.magic_path = optarg;
			break;
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
