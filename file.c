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
char			*xstrdup(char *);
int			 lookup_mtype(struct df_parser *, char *);
struct df_file		*df_open(const char *);
void			 df_state_init_files(int, char **);
int			 df_check(struct df_file *);
int			 df_check_fs(struct df_file *);
int			 df_check_magic(struct df_file *);
struct df_match		*df_match_add(struct df_file *, enum match_class,
    const char *, ...);
int			 dp_prepare(struct df_parser *);
int			 dp_prepare_moffset(struct df_parser *, const char *);
int			 dp_prepare_mtype(struct df_parser *, char *);
int			 dp_prepare_mdata_numeric(struct df_parser *, char *);

extern char	*malloc_options;
extern char	*__progname;
struct df_state  df_state;
int		 df_debug;

struct {
	int		 mt;
	const char	*str;
	/* a function to call to parse the magic test specification */
	int		(*md_parser)(struct df_parser *, char *);
	/* a function to perform the test itself */
	/* XXX int	(*md_test_handler)... */
} mt_table[] = {
	{ MT_UNKNOWN,	"unknown",	0 },
	{ MT_BYTE,	"byte",		dp_prepare_mdata_numeric },
	{ MT_UBYTE,	"ubyte",	dp_prepare_mdata_numeric },
	{ MT_SHORT,	"short",	dp_prepare_mdata_numeric },
	{ MT_LONG,	"long",		dp_prepare_mdata_numeric },
	{ MT_ULONG,	"ulong",	dp_prepare_mdata_numeric },
	{ MT_QUAD,	"quad",		dp_prepare_mdata_numeric },
	{ MT_FLOAT,	"float",	dp_prepare_mdata_numeric },
	{ MT_DOUBLE,	"double",	dp_prepare_mdata_numeric },
	{ MT_STRING,	"string",	0 },
	{ MT_PSTRING,	"pstring",	0 },
	{ MT_DATE,	"date",		0 },
	{ MT_QDATE,	"qdate",	0 },
	{ MT_LDATE,	"ldate",	0 },
	{ MT_QLDATE,	"qldate",	0 },
	{ MT_BESHORT,	"beshort",	dp_prepare_mdata_numeric },
	{ MT_UBESHORT,	"ubeshort",	dp_prepare_mdata_numeric },
	{ MT_BELONG,	"belong",	dp_prepare_mdata_numeric },
	{ MT_UBELONG,	"ubelong",	dp_prepare_mdata_numeric },
	{ MT_BEQUAD,	"bequad",	dp_prepare_mdata_numeric },
	{ MT_BEFLOAT,	"befloat",	dp_prepare_mdata_numeric },
	{ MT_BEDOUBLE,	"bedouble",	dp_prepare_mdata_numeric },
	{ MT_BEDATE,	"bedate",	0 },
	{ MT_BEQDATE,	"beqdate",	0 },
	{ MT_BELDATE,	"beldate",	0 },
	{ MT_BEQLDATE,	"beqldate",	0 },
	{ MT_BESTRING16,"bestring16",	0 },
	{ MT_LESHORT,	"leshort",	dp_prepare_mdata_numeric },
	{ MT_ULESHORT,	"uleshort",	dp_prepare_mdata_numeric },
	{ MT_LELONG,	"lelong",	dp_prepare_mdata_numeric },
	{ MT_ULELONG,	"ulelong",	dp_prepare_mdata_numeric },
	{ MT_LEQUAD,	"lequad",	dp_prepare_mdata_numeric },
	{ MT_LEFLOAT,	"lefloat",	dp_prepare_mdata_numeric },
	{ MT_LEDOUBLE,	"ledouble",	dp_prepare_mdata_numeric },
	{ MT_LEDATE,	"ledate",	0 },
	{ MT_LEQDATE,	"leqdate",	0 },
	{ MT_LELDATE,	"leldate",	0 },
	{ MT_LEQLDATE,	"leqldate",	0 },
	{ MT_LESTRING16,"lestring16",	0 },
	{ MT_MELONG,	"melong",	dp_prepare_mdata_numeric },
	{ MT_MEDATE,	"medate",	dp_prepare_mdata_numeric },
	{ MT_MELDATE,	"meldate",	dp_prepare_mdata_numeric },
	{ MT_REGEX,	"regex",	0 },
	{ MT_SEARCH,	"search",	0 },
	{ MT_DEFAULT,	"default",	0 },
	{ -1,		NULL,		0 },
};

void __dead
usage(void)
{
	/* XXX the more '-d' specified, the more verbose. How to express this in usage()? */
	fprintf(stderr, "usage: [-dLs] [-f magic] %s file [file...]\n",
	    __progname);
	exit(1);
}

char *
xstrdup(char *old)
{
	char	*p;

	if ((p = strdup(old)) == NULL)
		err(1, "failed to alloc");

	return (p);
}

int
lookup_mtype(struct df_parser *df, char *str)
{
	int i;

	for (i = 0; mt_table[i].mt != -1; i++) {
		if (strcmp(str, mt_table[i].str) == 0) {
			df->mtype = mt_table[i].mt;
			df->mdata_parser = mt_table[i].md_parser;
			DPRINTF(3, "Found mtype: %s: %d %p", str, df->mtype, df->mdata_parser);
			return (0);
		}
	}

	df->mtype = MT_UNKNOWN;
	DPRINTF(3, "Did not find mtype: %s", str);
	return (-1);
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
		/* This duplication is only for debugging purposes */
		dp.line = xstrdup(line);
		p	= line;
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
		/* Convert to something meaningfull */
		if (dp_prepare(&dp) == -1)
			goto nextline;
		DPRINTF(2, "%zd: %5s mlevel = %d moffset = %3lu %7s "
		    "mtype = %d %12s",
		    dp.lineno,
		    dp.argv[0], dp.mlevel, dp.moffset,
		    dp.argv[1], dp.mtype, 
		    dp.argv[2]);
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
 * Parse magic offset field.
 * Eg. '0', '>>>>>(78.l+23)', '>3', ...
 */
int
dp_prepare_moffset(struct df_parser *dp, const char *s)
{
	char *end = NULL;
	const char *cp;
	const char *errstr = NULL;

	cp = s;
	if (cp == NULL)
		goto errorinv;
	/* Check for mimes, skip for now */
	if (*cp == '!') {
		dp->mflags |= MF_MIME;
		return (0);
	}
	/*
	 * Check for an indirect offset, we're parsing something like:
	 * (0x3c.l)
	 * (( x [.[bslBSL]][+-][ y ])
	 */
	/* XXX indirect offsets will modify the string, it should not. */
	if (*cp == '(') {
		if ((end = strchr(cp, ')')) == NULL) {
			warnx("Unclosed paren at line %zd", dp->lineno);
			return (-1);
		}
		*end = 0;	/* terminate */
		dp->mflags |= MF_INDIRECT;
		cp++;		/* Jump over ( */
		/* cp now points to the 0 in (0x3c.l) */
		/*
		 * TODO collect offset at cp here.
		 */
		/* If type not specified, assume long */
		if ((end = strchr(cp, '.')) == NULL)
			dp->moffset_itype = MT_LONG;
		else {
			/* Terminate at dot */
			*end++ = 0;
			/* end now points over the dot */
			switch (*end) {
			case 'c':
			case 'b':
			case 'C':
			case 'B':
				dp->moffset_itype = MT_BYTE;
				break;
			case 'h':
			case 's':
				dp->moffset_itype = MT_LESHORT;
				break;
			case 'l':
				dp->moffset_itype = MT_LELONG;
				break;
			case 'S':
				dp->moffset_itype = MT_BESHORT;
				break;
			case 'L':
				dp->moffset_itype = MT_BELONG;
				break;
			case 'e':
			case 'f':
			case 'g':
				dp->moffset_itype = MT_LEDOUBLE;
				break;
			case 'E':
			case 'F':
			case 'G':
				dp->moffset_itype = MT_BEDOUBLE;
				break;
			default:
				warnx("indirect offset type `%c' "
				    "invalid at line %zd", *cp, dp->lineno);
				return (-1);
				break; /* NOTREACHED */
			}
			end++;
			/* end should be at `)' or `+' or `-' */
			switch (*end) {
			case ')':
				*end = 0; /* Terminate */
				break;
			case '-':
				/* dp->moffset *= -1; */
				/* FALLTHROUGH */
				end++;
			case '+':
				/* TODO collect number */
				break;
			default:
				goto errorinv;
				break; /* NOTREACHED */
			}
			
		}
	}
	if (cp == NULL)
		goto errorinv;
	/* Try hex */
	if (strlen(cp) > 1 && cp[0] == '0' && cp[1] == 'x') {
		errno = 0;
		dp->moffset = strtoll(cp, NULL, 16);
		if (errno) {
			warn("dp_prepare_moffset: strtoll: %s "
			    "line %zd", cp, dp->lineno);
			return (-1);
		}
	}
	dp->moffset = (unsigned long)strtonum(cp, 0,
	    LLONG_MAX, &errstr);
	if (errstr) {
		warn("dp_prepare_moffset: strtonum %s at line %zd",
		    cp, dp->lineno);
		return (-1);
	}

	return (0);

errorinv:
	warnx("dp_prepare_moffset: Invalid offset at line %zd",
	    dp->lineno);

	return (-1);
}

/*
 * Bake dp into something usable.
 */
int
dp_prepare(struct df_parser *dp)
{
	char			*cp;

	/* Reset */
	dp->mlevel	  = 0;
	dp->moffset	  = 0;
	dp->moffset_itype = 0;
	dp->mflags	  = 0;
	dp->mtype	  = MT_UNKNOWN;
	dp->mmask	  = 0;
	dp->d_quad	  = 0;	/* the longest type in the union */
	dp->test_flags	  = 0;
	dp->mdata_parser = 0;
	/* First analyze level and offset */
	cp = dp->argv[0];
	if (*cp == '>') {
		/* Count the > */
		while (cp && *cp == '>') {
			dp->mlevel++;
			cp++;
		}
	}
	/* cp now should point to the start of the offset */
	if (dp_prepare_moffset(dp, cp) == -1)
		return (-1);
	/* We ignore mimes for now */
	if (dp->mflags & MF_MIME) {
		DPRINTF(1, "%zd: mime ignored", dp->lineno);
		goto ignore;
	}
	/* Second, analyze test type */
	if (dp_prepare_mtype(dp, dp->argv[1]) == -1)
		return (-1);
	/* Now test data */
	if (dp->mdata_parser == NULL) {
		warn("%s: no mdata parser for mtype: %d", __func__, dp->mtype);
		return (-1);
	}
	if (dp->mdata_parser(dp, dp->argv[2]) == -1)
		return (-1);
	
	return (0);
ignore:
	return (-1);
}

/*
 * Parse the test type field
 * Eg. 'lelong', 'byte', 'leshort&0x0001', ...
 */
int
dp_prepare_mtype(struct df_parser *dp, char *cp)
{
	char			*mask, *mod;
	const char		*errstr = NULL;

	/* Split mask and test type first */
	cp   = dp->argv[1];
	mask = strchr(cp, '&');
	if (mask != NULL) {
		*mask++ = 0;
		errno  = 0;
		errstr = NULL;
		if (strlen(mask) > 1 && mask[0] == '0' && mask[1] == 'x') {
			/* Hexa */
			dp->mmask = strtoll(mask, NULL, 16);
			if (errno)
				goto badmask;
		} else if (strlen(mask) > 1 && mask[0] == '0') {
			/* Octa */
			dp->mmask = strtoll(mask, NULL, 8);
			if (errno)
				goto badmask;
		} else {
			/* Decimal */
			dp->mmask = strtonum(mask, 0, LLONG_MAX, &errstr);
			if (errstr)
				goto badmask;
		}
		dp->mflags |= MF_MASK;
	}
	/* If no &, check for modifier / as in string/ or search/ */
	if (mask == NULL &&
	    (strncmp(cp, "string", 6) == 0 ||
	    strncmp(cp, "search", 6) == 0)) {
		mod = strchr(cp, '/');
		if (mod != NULL) {
			if (mod[1] == 0)
				goto badmod;
			*mod++ = 0;
			/* TODO collect mod */
		}
	}
	/* Convert the string to something meaningful and decide upon a test handler */
	if ((lookup_mtype(dp, cp) == -1) || (dp->mtype == MT_UNKNOWN)) {
		warnx("dp_prepare: Uknown magic type %s at line %zd", cp, dp->lineno);
		return (-1);
	}

	return (0);

badmod:
	warn("dp_prepare: bad mod %s at line %zd", mod, dp->lineno);
	return (-1);
badmask:
	warn("dp_prepare: bad mask %s at line %zd", mask, dp->lineno);
	return (-1);
}

/*
 * Parse a numeric magic data field
 * Eg. '>0', '0407', '0x84500526'
 */
int
dp_prepare_mdata_numeric(struct df_parser *df, char *cp)
{
	char			*special = "=<>&^~x!";
	int			 ret = -1;

	if (cp == NULL) {
		warn("%s: null magic data", __func__); /* XXX why does this happen? */
		return (-1);
	}

	DPRINTF(2, "Parse numerical magic data: %s", cp);

	/* continue until we have parsed all special prefixes */
	while (strspn(cp, special)) {
		DPRINTF(2, "Found numerical speical prefix: %c", *cp);
		switch (*cp) {
		case '=':
			df->test_flags |= DF_TEST_PFX_EQ;
			break;
		case '<':
			df->test_flags |= DF_TEST_PFX_LT;
			break;
		case '>':
			df->test_flags |= DF_TEST_PFX_GT;
			break;
		case '&':
			df->test_flags |= DF_TEST_PFX_BSET;
			break;
		case '^':
			df->test_flags |= DF_TEST_PFX_BCLR;
			break;
		case '~':
			df->test_flags |= DF_TEST_PFX_BNEG;
			break;
		case 'x':
			df->test_flags |= DF_TEST_PFX_X;
			break;
		case '!':
			df->test_flags |= DF_TEST_PFX_NEG;
			break;
		default:
			/* should not happen */
			warn("%s: unknown special prefix: %c", __func__, *cp);
		};
		cp++;
	}

	/* XXX check for incompatible flag combos */
	/* EQ + LT */
	/* EQ + GT */
	/* GT + LT */
	/* SET + CLR */

	/* XXX store away test data in an "endian certain" mannaer */

	ret = 0;

	return (ret);
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
			errx(1, "this binary was not built with DEBUG");
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
