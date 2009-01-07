/**
 * $Id$
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
 * Copyright (C) 2002 Blender Foundation. All Rights Reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Allows you to determine which fonts to include in the library.
 */

#ifndef __FTF_SETTINGS_H
#define __FTF_SETTINGS_H

#define FTF_BIT(num) ((unsigned int)1 << (num))
#define FTF_NO_TRANSCONV 0
#define FTF_INPUT_SYSTEM_ENCODING FTF_BIT(1)
#define FTF_USE_GETTEXT FTF_BIT(2)
#define FTF_INPUT_UTF8 FTF_BIT(3)
#define FTF_PIXMAPFONT  0
#define FTF_TEXTUREFONT	1

#endif /* __FTF_SETTINGS_H */
