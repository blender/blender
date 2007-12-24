/**
 * $Id: BIF_fsmenu.h 11920 2007-09-02 17:25:03Z elubie $
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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * 
 */

#ifndef BSE_FSMENU_H
#define BSE_FSMENU_H

	/** Returns the number of entries in the Fileselect Menu */
int		fsmenu_get_nentries		(void);

	/** Returns true if the fsmenu entry at @a index exists and
	 * is a seperator.
	 */
int	fsmenu_is_entry_a_seperator	(int index);

	/** Returns the fsmenu entry at @a index (or NULL if a bad index)
     * or a seperator.
	 */
char*	fsmenu_get_entry		(int index);

	/** Returns a new menu description string representing the
	 * fileselect menu. Should be free'd with MEM_freeN.
	 */
char*	fsmenu_build_menu		(void);

	/** Append a seperator to the FSMenu, inserts always follow the
	 * last seperator.
	 */
void	fsmenu_append_separator	(void);

	/** Inserts a new fsmenu entry with the given @a path.
	 * Duplicate entries are not added.
	 * @param sorted Should entry be inserted in sorted order?
	 */
void	fsmenu_insert_entry		(char *path, int sorted, short save);

	/** Removes the fsmenu entry at the given @a index. */
void	fsmenu_remove_entry		(int index);

	/** saves the 'favourites' to the specified file */
void	fsmenu_write_file(const char *filename);

	/** Free's all the memory associated with the fsmenu */
void	fsmenu_free				(void);

#endif

