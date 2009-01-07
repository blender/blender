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
 * Defines the names of the fonts in the library.
 */

#ifndef __BMF_FONTS_H
#define __BMF_FONTS_H

#include "BMF_Settings.h"

typedef enum
{
	BMF_kHelvetica10 = 0,
#if BMF_INCLUDE_HELV12
	BMF_kHelvetica12,
#endif
#if BMF_INCLUDE_HELVB8
	BMF_kHelveticaBold8,
#endif
#if BMF_INCLUDE_HELVB10
	BMF_kHelveticaBold10,
#endif
#if BMF_INCLUDE_HELVB12
	BMF_kHelveticaBold12,
#endif
#if BMF_INCLUDE_HELVB14
	BMF_kHelveticaBold14,
#endif
#if BMF_INCLUDE_SCR12
	BMF_kScreen12,
#endif
#if BMF_INCLUDE_SCR14
	BMF_kScreen14,
#endif
#if BMF_INCLUDE_SCR15
	BMF_kScreen15,
#endif
	BMF_kNumFonts
} BMF_FontType;

typedef struct BMF_Font BMF_Font;

#endif /* __BMF_FONTS_H */

