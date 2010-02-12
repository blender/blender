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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include "GHOST_Buttons.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif



GHOST_Buttons::GHOST_Buttons()
{
	clear();
}


bool GHOST_Buttons::get(GHOST_TButtonMask mask) const
{
    switch (mask) {
    case GHOST_kButtonMaskLeft:
        return m_ButtonLeft;
    case GHOST_kButtonMaskMiddle:
        return m_ButtonMiddle;
    case GHOST_kButtonMaskRight:
        return m_ButtonRight;
    default:
        return false;
    }
}

void GHOST_Buttons::set(GHOST_TButtonMask mask, bool down)
{
    switch (mask) {
    case GHOST_kButtonMaskLeft:
        m_ButtonLeft = down; break;
    case GHOST_kButtonMaskMiddle:
        m_ButtonMiddle = down; break;
    case GHOST_kButtonMaskRight:
        m_ButtonRight = down; break;
    default:
        break;
    }
}

void GHOST_Buttons::clear()
{
    m_ButtonLeft = false;
    m_ButtonMiddle = false;
    m_ButtonRight = false;
}

GHOST_Buttons::~GHOST_Buttons() {}
