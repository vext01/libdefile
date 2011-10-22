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
#include <sys/queue.h>

/*
 * Main structure which represents a file to be checked parsed, we have one
 * for each command line argument.
 */
struct df_file {
	TAILQ_ENTRY(df_file) entry;
	TAILQ_HEAD(, df_match) df_matches;
	FILE		*file;			/* File handler */
	char		 filename[MAXPATHLEN];	/* File path */
	struct stat	 sb;			/* File stat */
};

/*
 * Main file program state, we have one global for it.
 */
struct df_state {
	TAILQ_HEAD(, df_file)	 df_files;	/* All our jobs */
	FILE			*magic_file;	/* Magic file */
	int			 magic_line;	/* Where we are in magic db */
	u_int	 		 check_flags;	/* Checking knobs */
#define CHK_NOSPECIAL		0x01
#define CHK_FOLLOWSYMLINKS	0x02
};

/*
 * A magic test type.
 * This corresponds to the second field in the magic db file.
 * It indicates how the comparison/search should be performed
 */
enum df_magic_test_type {
	MF_BYTE,
	MF_SHORT,
	MF_LONG,
	MF_QUAD,
	MF_FLOAT,
	MF_DOUBLE,
	MF_STRING,
	MF_PSTRING,
	MF_DATE,
	MF_QDATE,
	MF_LDATE,
	MF_QLDATE,
	MF_BESHORT,
	MF_BELONG,
	MF_BEQUAD,
	MF_BEFLOAT,
	MF_BEDOUBLE,
	MF_BEDATE,
	MF_BEQDATE,
	MF_BELDATE,
	MF_BEQLDATE,
	MF_BESTRING16,
	MF_LESHORT,
	MF_LELONG,
	MF_LEQUAD,
	MF_LEFLOAT,
	MF_LEDOUBLE,
	MF_LEDATE,
	MF_LEQDATE,
	MF_LELDATE,
	MF_LEQLDATE,
	MF_LESTRING16,
	MF_MELONG,
	MF_MEDATE,
	MF_MELDATE,
	MF_REGEX,
	MF_SEARCH,
	MF_DEFAULT
};

/*
 * Represents a match in a df_file, a file may have multiple matches.
 */
enum match_class {
	MC_FS,
	MC_MAGIC,
	MC_MIME,
	MC_LANG
};

struct df_match {
	TAILQ_ENTRY(df_match) entry;
	char		 desc[256]; 	/* string represtation */
	enum match_class class;		/* df_match_class */
	/* XXX maybe instance in future ? */
};

struct df_parser {
	FILE	*magic_file;
	size_t	 lineno;
	int	 level;
	char	*line;
	char	*argv[5];
};
