/**
 * $Id$
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
#ifndef BIF_IMASEL_H
#define BIF_IMASEL_H

struct SpaceImaSel;
struct OneSelectableIma;
struct ScrArea;
struct ImaDir;

void imadir_parent(struct SpaceImaSel *simasel);
int  bitset(int l,  int bit);
void free_sel_ima(struct OneSelectableIma *firstima);

void write_new_pib(struct SpaceImaSel *simasel);
void free_ima_dir(struct ImaDir *firstdir);
void check_for_pib(struct SpaceImaSel *simasel);
void clear_ima_dir(struct SpaceImaSel *simasel);
void check_ima_dir_name(char *dir);
int get_ima_dir(char *dirname, int dtype, int *td, struct ImaDir **first);
void get_next_image(struct SpaceImaSel *simasel);
void get_file_info(struct SpaceImaSel *simasel);
void get_pib_file(struct SpaceImaSel *simasel);
void change_imadir(struct SpaceImaSel *simasel);
void init_imaselspace(struct ScrArea *sa);
void check_imasel_copy(struct SpaceImaSel *simasel);
void free_imasel(struct SpaceImaSel *simasel);

void clever_numbuts_imasel(void);

#endif

