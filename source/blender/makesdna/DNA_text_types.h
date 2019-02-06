/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
/** \file \ingroup DNA
 *
 * Text blocks used for Python-Scripts, OpenShadingLanguage
 * and arbitrary text data to store in blend files.
 */

#ifndef __DNA_TEXT_TYPES_H__
#define __DNA_TEXT_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"

typedef struct TextLine {
	struct TextLine *next, *prev;

	char *line;
	/** May be NULL if syntax is off or not yet formatted. */
	char *format;
	/** Blen unused. */
	int len, blen;
} TextLine;

typedef struct Text {
	ID id;

	char *name;
	void *compiled;

	int flags, nlines;

	ListBase lines;
	TextLine *curl, *sell;
	int curc, selc;

	double mtime;
} Text;

#define TXT_TABSIZE	4
#define TXT_INIT_UNDO 1024
#define TXT_MAX_UNDO	(TXT_INIT_UNDO*TXT_INIT_UNDO)

/* text flags */
#define TXT_ISDIRTY             (1 << 0)
#define TXT_ISMEM               (1 << 2)
#define TXT_ISEXT               (1 << 3)
#define TXT_ISSCRIPT            (1 << 4) /* used by space handler scriptlinks */
// #define TXT_READONLY            (1 << 8)
// #define TXT_FOLLOW              (1 << 9) /* always follow cursor (console) */
#define TXT_TABSTOSPACES        (1 << 10) /* use space instead of tabs */

#endif  /* __DNA_TEXT_TYPES_H__ */
