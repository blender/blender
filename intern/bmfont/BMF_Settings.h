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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
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

#ifndef __BMF_SETTINGS_H
#define __BMF_SETTINGS_H

/* This font is included always */
#define BMF_INCLUDE_HELV10 1

#ifndef BMF_MINIMAL

/* These fonts are included with the minimal setting defined */
#define BMF_INCLUDE_HELV12	1
#define BMF_INCLUDE_HELVB8	1
#define BMF_INCLUDE_HELVB10	1
#define BMF_INCLUDE_HELVB12	1
#define BMF_INCLUDE_HELVB14	1
#define BMF_INCLUDE_SCR12	1
#define BMF_INCLUDE_SCR14	1
#define BMF_INCLUDE_SCR15	1

#else /* BMF_MINIMAL */
#define BMF_INCLUDE_HELV12	0
#define BMF_INCLUDE_HELVB8	0
#define BMF_INCLUDE_HELVB10	0
#define BMF_INCLUDE_HELVB12	0
#define BMF_INCLUDE_HELVB14	0
#define BMF_INCLUDE_SCR12	0
#define BMF_INCLUDE_SCR14	0
#define BMF_INCLUDE_SCR15	0

#endif /* BMF_MINIMAL */

#endif /* __BMF_SETTINGS_H */

