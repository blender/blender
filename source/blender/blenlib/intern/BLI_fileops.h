/**
 * blenlib/BLI_listBase.h    mar 2001 Nzc
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
 *
 * More low-level fileops from Daniel Dunbar. Two functions were also
 * defined in storage.c. These are the old fop_ prefixes. There is
 * definitely some redundancy here!
 * */

#ifndef BLI_FILEOPS_H
#define BLI_FILEOPS_H

char *first_slash(char *string);

/* only for the sane unix world: direct calls to system functions :( */
#ifndef WIN32
void BLI_setCmdCallBack(int (*f)(char*));
#endif

#endif

