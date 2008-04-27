/**
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* Based on ghash, difference is ghash is not a fixed size,
 * so for BPath we dont need to malloc  */

struct BPathIteratorSeqData {
	int totseq;
	int seq;
	struct Sequence **seqar; /* Sequence */
};

struct BPathIterator {
	char*	path;
	char*	lib;
	char*	name;
	void*	data;
	int		len;
	int		type;
	
	void (*setpath_callback)(struct BPathIterator *, char *);
	void (*getpath_callback)(struct BPathIterator *, char *);
	
	/* only for seq data */
	struct BPathIteratorSeqData seqdata;
};

void			BLI_bpathIterator_init				(struct BPathIterator *bpi);
void			BLI_bpathIterator_free				(struct BPathIterator *bpi);
char*			BLI_bpathIterator_getLib			(struct BPathIterator *bpi);
char*			BLI_bpathIterator_getName			(struct BPathIterator *bpi);
int				BLI_bpathIterator_getType			(struct BPathIterator *bpi);
int				BLI_bpathIterator_getPathMaxLen		(struct BPathIterator *bpi);
void			BLI_bpathIterator_step				(struct BPathIterator *bpi);
int				BLI_bpathIterator_isDone			(struct BPathIterator *bpi);
void			BLI_bpathIterator_getPath			(struct BPathIterator *bpi, char *path);
void			BLI_bpathIterator_getPathExpanded	(struct BPathIterator *bpi, char *path_expanded);
void			BLI_bpathIterator_setPath			(struct BPathIterator *bpi, char *path);

/* high level funcs */

/* creates a text file with missing files if there are any */
void checkMissingFiles(char *txtname );
void makeFilesRelative(char *txtname, int *tot, int *changed, int *failed, int *linked);
void makeFilesAbsolute(char *txtname, int *tot, int *changed, int *failed, int *linked);
void findMissingFiles(char *str);
