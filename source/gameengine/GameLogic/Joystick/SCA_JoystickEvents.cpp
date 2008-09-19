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



void SCA_Joystick::OnAxisMotion(SDL_Event* sdl_event)
{
	pFillAxes();
	m_axisnum	= sdl_event->jaxis.axis;
	m_axisvalue = sdl_event->jaxis.value;
	m_istrig = 1;
}


void SCA_Joystick::OnHatMotion(SDL_Event* sdl_event)
{
	m_hatdir = sdl_event->jhat.value;
	m_hatnum = sdl_event->jhat.hat;
	m_istrig = 1;
}


void SCA_Joystick::OnButtonUp(SDL_Event* sdl_event)
{
	m_buttonnum = -2;
}


void SCA_Joystick::OnButtonDown(SDL_Event* sdl_event)
{
	m_buttonmax = GetNumberOfButtons();
	if(sdl_event->jbutton.button >= 1 || sdl_event->jbutton.button <= m_buttonmax)
	{
		m_istrig = 1;
		m_buttonnum = sdl_event->jbutton.button;
	}
}


void SCA_Joystick::OnNothing(SDL_Event* sdl_event)
{
	m_istrig = 0;
}

/* only handle events for 1 joystick */

void SCA_Joystick::HandleEvents(void)
{
	SDL_Event		sdl_event;

	if(SDL_PollEvent(&sdl_event))
	{
		/* Note! m_instance[sdl_event.jaxis.which]
		 * will segfault if over JOYINDEX_MAX, not too nice but what are the chances? */
		switch(sdl_event.type)
		{
		case SDL_JOYAXISMOTION:
			SCA_Joystick::m_instance[sdl_event.jaxis.which]->OnAxisMotion(&sdl_event);
			break;
		case SDL_JOYHATMOTION:
			SCA_Joystick::m_instance[sdl_event.jhat.which]->OnHatMotion(&sdl_event);
			break;
		case SDL_JOYBUTTONUP:
			SCA_Joystick::m_instance[sdl_event.jbutton.which]->OnButtonUp(&sdl_event);
			break;
		case SDL_JOYBUTTONDOWN:
			SCA_Joystick::m_instance[sdl_event.jbutton.which]->OnButtonDown(&sdl_event);
			break;
		case SDL_JOYBALLMOTION:
			SCA_Joystick::m_instance[sdl_event.jball.which]->OnBallMotion(&sdl_event);
			break;
		default:
			printf("SCA_Joystick::HandleEvents, Unknown SDL event, this should not happen\n");
			break;
		}
	}
}
