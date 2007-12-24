/**
 * $Id: BDR_editmball.h 10893 2007-06-08 14:17:13Z jiri $
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
 */

#ifndef BDR_EDITMBALL_H
#define BDR_EDITMBALL_H

void make_editMball(void);
void load_editMball(void);

/**
 * @attention The argument is discarded. It is there for
 * compatibility.
 */
void add_primitiveMball(int);
void deselectall_mball(void);
void selectinverse_mball(void);
void selectrandom_mball(void);
void mouse_mball(void);
void adduplicate_mball(void);
void delete_mball(void); 
void freeMetaElemlist(struct ListBase *lb);
void undo_push_mball(char *name);
void hide_mball(char hide);
void reveal_mball(void);

#endif /*  BDR_EDITMBALL_H */

