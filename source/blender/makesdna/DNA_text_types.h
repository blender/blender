/**
 * blenlib/DNA_text_types.h (mar-2001 nzc)
 *	
 * $Id$ 
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#ifndef DNA_TEXT_TYPES_H
#define DNA_TEXT_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

typedef struct TextLine {
	struct TextLine *next, *prev;

	char *line;
	char *format; /* may be NULL if syntax is off or not yet formatted */
	int len, blen; /* blen unused */
} TextLine;

typedef struct TextMarker {
	struct TextMarker *next, *prev;

	int lineno, start, end, pad1; /* line number and start/end character indices */
	
	int group, flags; /* see BKE_text.h for flag defines */
	char color[4], pad[4]; /* draw color of the marker */
} TextMarker;

typedef struct Text {
	ID id;
	
	char *name;

	int flags, nlines;
	
	ListBase lines;
	TextLine *curl, *sell;
	int curc, selc;
	ListBase markers;
	
	char *undo_buf;
	int undo_pos, undo_len;
	
	void *compiled;
	double mtime;
} Text;

	/* TXT_OFFSET used to be 35 when the scrollbar was on the left... */
#define TXT_OFFSET 15
#define TXT_TABSIZE	4
#define TXT_INIT_UNDO 1024
#define TXT_MAX_UNDO	(TXT_INIT_UNDO*TXT_INIT_UNDO)

/* text flags */
#define TXT_ISDIRTY             0x0001
#define TXT_DEPRECATED          0x0004 /* deprecated ISTMP flag */
#define TXT_ISMEM               0x0004
#define TXT_ISEXT               0x0008
#define TXT_ISSCRIPT            0x0010 /* used by space handler scriptlinks */
#define TXT_READONLY            0x0100
#define TXT_FOLLOW              0x0200 /* always follow cursor (console) */
#define TXT_TABSTOSPACES        0x0400 /* use space instead of tabs */

/* format continuation flags */
#define TXT_NOCONT				0x00 /* no continuation */
#define TXT_SNGQUOTSTR			0x01 /* single quotes */
#define TXT_DBLQUOTSTR			0x02 /* double quotes */
#define TXT_TRISTR				0x04 /* triplets of quotes: """ or ''' */
#define TXT_SNGTRISTR			0x05 /*(TXT_TRISTR | TXT_SNGQUOTSTR)*/
#define TXT_DBLTRISTR			0x06 /*(TXT_TRISTR | TXT_DBLQUOTSTR)*/

#endif
