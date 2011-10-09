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

#include <sys/queue.h>

/*
 * Main structure which represents a file to be checked parsed, we have one
 * for each command line argument.
 */
struct df_file {
	TAILQ_ENTRY(df_file) entry;
	TAILQ_HEAD(, df_match) df_matches;
	FILE	*file;		/* File handler */
	char	*filename;	/* File path */
};

/*
 * Main file program state, we have one global for it.
 */
struct df_state {
	TAILQ_HEAD(, df_file) df_files; /* All our jobs */
	FILE	*magic_file;	/* Magic file */
	u_int	 check_flags;	/* Flags regarding file checking */
#define CHK_NOSPECIAL 0x01
};


/*
 * A magic field type.
 * This corresponds to the first field in the magic db file.
 * It indicates how the comparison/search should be performed
 */
enum df_magic_field_type {
	DF_MF_BYTE,
	DF_MF_SHORT,
	DF_MF_LONG,
	DF_MF_QUAD,
	DF_MF_FLOAT,
	DF_MF_DOUBLE,
	DF_MF_STRING,
	DF_MF_PSTRING,
	DF_MF_DATE,
	DF_MF_QDATE,
	DF_MF_LDATE,
	DF_MF_QLDATE,
	DF_MF_BESHORT,
	DF_MF_BELONG,
	DF_MF_BEQUAD,
	DF_MF_BEFLOAT,
	DF_MF_BEDOUBLE,
	DF_MF_BEDATE,
	DF_MF_BEQDATE,
	DF_MF_BELDATE,
	DF_MF_BEQLDATE,
	DF_MF_BESTRING16,
	DF_MF_LESHORT,
	DF_MF_LELONG,
	DF_MF_LEQUAD,
	DF_MF_LEFLOAT,
	DF_MF_LEDOUBLE,
	DF_MF_LEDATE,
	DF_MF_LEQDATE,
	DF_MF_LELDATE,
	DF_MF_LEQLDATE,
	DF_MF_LESTRING16,
	DF_MF_MELONG,
	DF_MF_MEDATE,
	DF_MF_MELDATE,
	DF_MF_REGEX,
	DF_MF_SEARCH,
	DF_MF_DEFAULT
};

/*
 * Represents a field if a potential match from the magic database
 */
struct df_magic_match_field {
	u_int64_t			offset;
	enum df_magic_field_type	type;
};


/*
 * Represents a potential match from the magic database
 */
struct df_magic_match {
	TAILQ_HEAD(, df_magic_field)	df_fields;
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
	const char	*desc;	/* string represtation */
	enum match_class class;	/* df_match_class */
	/* XXX maybe instance in future ? */
};
