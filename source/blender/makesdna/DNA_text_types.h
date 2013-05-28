/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/** \file DNA_text_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
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
	char *format; /* may be NULL if syntax is off or not yet formatted */
	int len, blen; /* blen unused */
} TextLine;

typedef struct Text {
	ID id;
	
	char *name;

	int flags, nlines;
	
	ListBase lines;
	TextLine *curl, *sell;
	int curc, selc;
	
	char *undo_buf;
	int undo_pos, undo_len;
	
	void *compiled;
	double mtime;
} Text;

#define TXT_TABSIZE	4
#define TXT_INIT_UNDO 1024
#define TXT_MAX_UNDO	(TXT_INIT_UNDO*TXT_INIT_UNDO)

/* text flags */
#define TXT_ISDIRTY             0x0001
#define TXT_ISMEM               0x0004
#define TXT_ISEXT               0x0008
#define TXT_ISSCRIPT            0x0010 /* used by space handler scriptlinks */
// #define TXT_READONLY            0x0100
// #define TXT_FOLLOW              0x0200 /* always follow cursor (console) */
#define TXT_TABSTOSPACES        0x0400 /* use space instead of tabs */

#endif  /* __DNA_TEXT_TYPES_H__ */
