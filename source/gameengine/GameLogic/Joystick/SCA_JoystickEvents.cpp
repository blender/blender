/*
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
 * Contributor(s): snailrose.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GameLogic/Joystick/SCA_JoystickEvents.cpp
 *  \ingroup gamelogic
 */

#ifdef WITH_SDL
#  include <SDL.h>
#endif

#include "SCA_Joystick.h"
#include "SCA_JoystickPrivate.h"

#ifdef _MSC_VER
#  include <cstdio> /* printf */
#endif

#ifdef WITH_SDL
void SCA_Joystick::OnAxisMotion(SDL_Event* sdl_event)
{
	if (sdl_event->jaxis.axis >= JOYAXIS_MAX)
		return;
	
	m_axis_array[sdl_event->jaxis.axis] = sdl_event->jaxis.value;
	m_istrig_axis = 1;
}

/* See notes below in the event loop */
void SCA_Joystick::OnHatMotion(SDL_Event* sdl_event)
{
	if (sdl_event->jhat.hat >= JOYHAT_MAX)
		return;

	m_hat_array[sdl_event->jhat.hat] = sdl_event->jhat.value;
	m_istrig_hat = 1;
}

/* See notes below in the event loop */
void SCA_Joystick::OnButtonUp(SDL_Event* sdl_event)
{
	m_istrig_button = 1;
}


void SCA_Joystick::OnButtonDown(SDL_Event* sdl_event)
{
	//if (sdl_event->jbutton.button > m_buttonmax) /* unsigned int so always above 0 */
	//	return;
	// sdl_event->jbutton.button;
	
	m_istrig_button = 1;
}


void SCA_Joystick::OnNothing(SDL_Event* sdl_event)
{
	m_istrig_axis = m_istrig_button = m_istrig_hat = 0;
}

void SCA_Joystick::HandleEvents(void)
{
	SDL_Event		sdl_event;

	if (SDL_PollEvent == (void*)0) {
		return;
	}

	int i;
	for (i=0; i<m_joynum; i++) { /* could use JOYINDEX_MAX but no reason to */
		if (SCA_Joystick::m_instance[i])
			SCA_Joystick::m_instance[i]->OnNothing(&sdl_event);
	}
	
	while (SDL_PollEvent(&sdl_event)) {
		/* Note! m_instance[sdl_event.jaxis.which]
		 * will segfault if over JOYINDEX_MAX, not too nice but what are the chances? */
		
		/* Note!, with buttons, this wont care which button is pressed,
		 * only to set 'm_istrig_button', actual pressed buttons are detected by SDL_JoystickGetButton */
		
		/* Note!, if you manage to press and release a button within 1 logic tick
		 * it wont work as it should */
		
		switch (sdl_event.type) {
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
#if 0	/* Not used yet */
			case SDL_JOYBALLMOTION:
				SCA_Joystick::m_instance[sdl_event.jball.which]->OnBallMotion(&sdl_event);
				break;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 0)
			case SDL_JOYDEVICEADDED:
			case SDL_JOYDEVICEREMOVED:
				/* pass */
				break;
#endif
			default:
				printf("SCA_Joystick::HandleEvents, Unknown SDL event (%d), this should not happen\n", sdl_event.type);
				break;
		}
	}
}
#endif /* WITH_SDL */
