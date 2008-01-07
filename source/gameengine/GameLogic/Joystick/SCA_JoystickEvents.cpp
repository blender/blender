/**
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
 * Contributor(s): snailrose.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <SDL.h>
#include "SCA_Joystick.h"
#include "SCA_JoystickPrivate.h"



void SCA_Joystick::OnAxisMotion(void)
{
	pFillAxes();
	m_axisnum	= m_private->m_event.jaxis.axis;
	m_axisvalue = m_private->m_event.jaxis.value;
	m_istrig = 1;
}


void SCA_Joystick::OnHatMotion(void)
{
	m_hatdir = m_private->m_event.jhat.value;
	m_hatnum = m_private->m_event.jhat.hat;
	m_istrig = 1;
}


void SCA_Joystick::OnButtonUp(void)
{
	m_buttonnum = -2;
}


void SCA_Joystick::OnButtonDown(void)
{
	m_buttonmax = GetNumberOfButtons();
	if(m_private->m_event.jbutton.button >= 1 || m_private->m_event.jbutton.button <= m_buttonmax)
	{
		m_istrig = 1;
		m_buttonnum = m_private->m_event.jbutton.button;
	}
}


void SCA_Joystick::OnNothing(void)
{
	m_istrig = 0;
}
