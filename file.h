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
	const char		*magic_path;	/* Magic file path */
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
enum df_magic_test {
	MT_UNKNOWN,
	MT_BYTE,
	MT_UBYTE,
	MT_SHORT,
	MT_LONG,
	MT_QUAD,
	MT_FLOAT,
	MT_DOUBLE,
	MT_STRING,
	MT_PSTRING,
	MT_DATE,
	MT_QDATE,
	MT_LDATE,
	MT_QLDATE,
	MT_BESHORT,
	MT_UBESHORT,
	MT_BELONG,
	MT_BEQUAD,
	MT_BEFLOAT,
	MT_BEDOUBLE,
	MT_BEDATE,
	MT_BEQDATE,
	MT_BELDATE,
	MT_BEQLDATE,
	MT_BESTRING16,
	MT_LESHORT,
	MT_LELONG,
	MT_LEQUAD,
	MT_LEFLOAT,
	MT_LEDOUBLE,
	MT_LEDATE,
	MT_LEQDATE,
	MT_LELDATE,
	MT_LEQLDATE,
	MT_LESTRING16,
	MT_MELONG,
	MT_MEDATE,
	MT_MELDATE,
	MT_REGEX,
	MT_SEARCH,
	MT_DEFAULT
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

/*
 * The parser state, set every time we parse a new line.
 */
struct df_parser {
	FILE			*magic_file;
	size_t			 lineno; 	/* Current line number */
	char			*line;		/* Current linet */
	int			 level; 	/* Current parser level */
	char			*argv[5];	/* The broken tokens */
	int			 mlevel;	/* Magic level */
	u_long			 moffset;	/* Magic offset */
	enum df_magic_test	 moffset_itype;	/* Indirect type if MF_INDIRECT */
	enum df_magic_test	 mtype; 	/* Magic type */
	u_int64_t		 mmask;		/* Magic mask */
	u_int32_t		 mflags;	/* Magic flags */
#define MF_INDIRECT	0x01	/* Indirect offset (mo) */
#define MF_MASK		0x02	/* Value must be masked (mm is valid) */
	/* the test (d)ata itself */
	union {
		u_int8_t	 d_byte;
		/* native endian */
		int16_t		 d_short;
		int32_t		 d_long;
		int64_t		 d_quad;
		float		 d_float;
		double		 d_double;
		char		*d_string;
		char		*d_pstring;
		int32_t		 d_date;
		int64_t		 d_qdate;
		int32_t		 d_ldate;
		int64_t		 d_qldate;
		/* big endian */
		int16_t		 d_beshort;
		int32_t		 d_belong;
		int64_t		 d_bequad;
		float		 d_befloat;
		double		 d_bedouble;
		int32_t		 d_bedate;
		int64_t		 d_beqdate;
		int32_t		 d_beldate;
		int64_t		 d_beqldate;
		char		*d_bestring16;
		/* little endian */
		int16_t		 d_leshort;
		int32_t		 d_lelong;
		int64_t		 d_lequad;
		float		 d_lefloat;
		double		 d_ledouble;
		int32_t		 d_ledate;
		int64_t		 d_leqdate;
		int32_t		 d_leldate;
		int64_t		 d_leqldate;
		char		*d_lestring16;
		/* middle endian */
		int32_t		 melong;
		int16_t		 medate;
		int32_t		 meldate;
	};
	/* flags, as indicated by test prefixes */
#define DF_TEST_PFX_EQ			(1 << 0)
#define DF_TEST_PFX_LT			(1 << 1)
#define DF_TEST_PFX_GT			(1 << 2)
#define DF_TEST_PFX_BSET		(1 << 3)
#define DF_TEST_PFX_BCLR		(1 << 4)
#define DF_TEST_PFX_BNEG		(1 << 5)
#define DF_TEST_PFX_X			(1 << 6)
#define DF_TEST_PFX_NEG			(1 << 7)
	u_int32_t		  flags;
};

#ifdef DEBUG
#define DPRINTF(lvl, args...)						\
	do {								\
		if (df_debug >= lvl) {					\
			fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
			fprintf(stderr, args);				\
			fprintf(stderr, "\n");				\
		}							\
	} while(0);
#else
#define DPRINTF(fmt, args...)
	/* NOP */
#endif
