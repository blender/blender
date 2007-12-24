/**
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* Based on ghash, difference is ghash is not a fixed size,
 * so for BPath we dont need to malloc  */

struct BPathIterator {
	char*	path;
	char*	lib;
	char*	name;
	void*	data;
	int		len;
	int		type;
};

void			BLI_bpathIterator_init		(struct BPathIterator *bpi);
char*			BLI_bpathIterator_getPath	(struct BPathIterator *bpi);
char*			BLI_bpathIterator_getLib	(struct BPathIterator *bpi);
char*			BLI_bpathIterator_getName	(struct BPathIterator *bpi);
int				BLI_bpathIterator_getType	(struct BPathIterator *bpi);
int				BLI_bpathIterator_getPathMaxLen(struct BPathIterator *bpi);
void			BLI_bpathIterator_step		(struct BPathIterator *bpi);
int				BLI_bpathIterator_isDone	(struct BPathIterator *bpi);
void			BLI_bpathIterator_copyPathExpanded( struct BPathIterator *bpi, char *path_expanded);

/* high level funcs */

/* creates a text file with missing files if there are any */
struct Text * checkMissingFiles(void);
void makeFilesRelative(int *tot, int *changed, int *failed, int *linked);
void makeFilesAbsolute(int *tot, int *changed, int *failed, int *linked);
void findMissingFiles(char *str);
