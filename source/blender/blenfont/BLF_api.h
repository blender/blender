/**
 * $Id:
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLF_API_H
#define BLF_API_H

/* Read the .Blanguages file, return 1 on success or 0 if fails. */
int BLF_lang_init(void);

/* Free the memory allocate for the .Blanguages. */
void BLF_lang_exit(void);

/* Set the current Language. */
void BLF_lang_set(int id);

/* Return a string with all the Language available. */
char *BLF_lang_pup(void);

/* Return the number of invalid lines in the .Blanguages file,
 * zero means no error found.
 */
int BLF_lang_error(void);

/* Return the code string for the specified language code. */
char *BLF_lang_find_code(short langid);

#if 0

/* Add a path to the font dir paths. */
void BLF_dir_add(const char *path);

/* Remove a path from the font dir paths. */
void BLF_dir_rem(const char *path);

/* Return an array with all the font dir (this can be used for filesel) */
char **BLF_dir_get(int *ndir);

/* Free the data return by BLF_dir_get. */
void BLF_dir_free(char **dirs, int count);

#endif /* zero!! */

#endif /* BLF_API_H */
