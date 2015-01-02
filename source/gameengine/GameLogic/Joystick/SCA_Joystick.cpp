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

/** \file gameengine/GameLogic/Joystick/SCA_Joystick.cpp
 *  \ingroup gamelogic
 */

#ifdef WITH_SDL
#  include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "SCA_Joystick.h"
#include "SCA_JoystickPrivate.h"

#include "BLI_path_util.h"

#ifdef WITH_SDL
#  define SDL_CHECK(x) ((x) != (void *)0)
#endif

SCA_Joystick::SCA_Joystick(short int index)
	:
	m_joyindex(index),
	m_prec(3200),
	m_axismax(-1),
	m_buttonmax(-1),
	m_hatmax(-1),
	m_isinit(0),
	m_istrig_axis(0),
	m_istrig_button(0),
	m_istrig_hat(0)
{
	for (int i=0; i < JOYAXIS_MAX; i++)
		m_axis_array[i] = 0;
	
	for (int i=0; i < JOYHAT_MAX; i++)
		m_hat_array[i] = 0;
	
#ifdef WITH_SDL
	m_private = new PrivateData();
#endif
}


SCA_Joystick::~SCA_Joystick()

{
#ifdef WITH_SDL
	delete m_private;
#endif
}

SCA_Joystick *SCA_Joystick::m_instance[JOYINDEX_MAX];
int SCA_Joystick::m_joynum = 0;
int SCA_Joystick::m_refCount = 0;

SCA_Joystick *SCA_Joystick::GetInstance( short int joyindex )
{
#ifndef WITH_SDL
	return NULL;
#else  /* WITH_SDL */
	if (!SDL_CHECK(SDL_InitSubSystem)) {
		return NULL;
	}
	if (joyindex < 0 || joyindex >= JOYINDEX_MAX) {
		JOYSTICK_ECHO("Error-invalid joystick index: " << joyindex);
		return NULL;
	}

	if (m_refCount == 0) 
	{
		int i;

		/* The video subsystem is required for joystick input to work. However,
		 * when GHOST is running under SDL, video is initialized elsewhere. We
		 * also need to set the videodriver to dummy, and do it here to avoid
		 * interfering with addons that may use SDL too.
		 *
		 * We also init SDL once only. */
#  ifdef WITH_GHOST_SDL
		int success = (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != -1 );
#  else
		/* set and restore environment variable */
		char *videodriver = getenv("SDL_VIDEODRIVER");
		BLI_setenv("SDL_VIDEODRIVER", "dummy");

		int success = (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != -1 );

		BLI_setenv("SDL_VIDEODRIVER", videodriver);
#  endif

		if (!success) {
			JOYSTICK_ECHO("Error-Initializing-SDL: " << SDL_GetError());
			return NULL;
		}
		
		m_joynum = SDL_NumJoysticks();
		
		for (i=0; i<JOYINDEX_MAX; i++) {
			m_instance[i] = new SCA_Joystick(i);
			m_instance[i]->CreateJoystickDevice();
		}
		m_refCount = 1;
	}
	else
	{
		m_refCount++;
	}
	return m_instance[joyindex];
#endif /* WITH_SDL */
}

void SCA_Joystick::ReleaseInstance()
{
	if (--m_refCount == 0)
	{
#ifdef WITH_SDL
		if (!SDL_CHECK(SDL_QuitSubSystem)) {
			return;
		}
		for (int i=0; i<JOYINDEX_MAX; i++) {
			if (m_instance[i]) {
				m_instance[i]->DestroyJoystickDevice();
				delete m_instance[i];
			}
			m_instance[i] = NULL;
		}

		/* The video subsystem is required for joystick input to work. However,
		 * when GHOST is running under SDL, video is freed elsewhere.
		 * Do this once only. */
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
#endif /* WITH_SDL */
	}
}

void SCA_Joystick::cSetPrecision(int val)
{
	m_prec = val;
}


bool SCA_Joystick::aAxisPairIsPositive(int axis)
{
	return (pAxisTest(axis) > m_prec) ? true:false;
}

bool SCA_Joystick::aAxisPairDirectionIsPositive(int axis, int dir)
{

	int res;

	if (dir==JOYAXIS_UP || dir==JOYAXIS_DOWN)
		res = pGetAxis(axis, 1);
	else /* JOYAXIS_LEFT || JOYAXIS_RIGHT */
		res = pGetAxis(axis, 0);
	
	if (dir==JOYAXIS_DOWN || dir==JOYAXIS_RIGHT)
		return (res > m_prec) ? true : false;
	else /* JOYAXIS_UP || JOYAXIS_LEFT */
		return (res < -m_prec) ? true : false;
}

bool SCA_Joystick::aAxisIsPositive(int axis_single)
{
	return abs(m_axis_array[axis_single]) > m_prec ? true:false;
}

bool SCA_Joystick::aAnyButtonPressIsPositive(void)
{
#ifdef WITH_SDL
	if (!SDL_CHECK(SDL_JoystickGetButton)) {
		return false;
	}
	/* this is needed for the "all events" option
	 * so we know if there are no buttons pressed */
	for (int i=0; i<m_buttonmax; i++)
		if (SDL_JoystickGetButton(m_private->m_joystick, i))
			return true;
#endif
	return false;
}

bool SCA_Joystick::aButtonPressIsPositive(int button)
{
#ifndef WITH_SDL
	return false;
#else
	bool result;
	result = SDL_CHECK(SDL_JoystickGetButton) && SDL_JoystickGetButton(m_private->m_joystick, button);
	return result;
#endif
}


bool SCA_Joystick::aButtonReleaseIsPositive(int button)
{
#ifndef WITH_SDL
	return false;
#else
	bool result;
	result = !(SDL_CHECK(SDL_JoystickGetButton) && SDL_JoystickGetButton(m_private->m_joystick, button));
	return result;
#endif
}


bool SCA_Joystick::aHatIsPositive(int hatnum, int dir)
{
	return (GetHat(hatnum)==dir) ? true : false;
}

int SCA_Joystick::GetNumberOfAxes()
{
	return m_axismax;
}


int SCA_Joystick::GetNumberOfButtons()
{
	return m_buttonmax;
}


int SCA_Joystick::GetNumberOfHats()
{
	return m_hatmax;
}

bool SCA_Joystick::CreateJoystickDevice(void)
{
#ifndef WITH_SDL
	m_isinit = true;
	m_axismax = m_buttonmax = m_hatmax = 0;
	return false;
#else /* WITH_SDL */
	if (m_isinit == false && SDL_CHECK(SDL_JoystickOpen)) {
		if (m_joyindex>=m_joynum) {
			/* don't print a message, because this is done anyway */
			//JOYSTICK_ECHO("Joystick-Error: " << SDL_NumJoysticks() << " avaiable joystick(s)");
			
			/* Need this so python args can return empty lists */
			m_axismax = m_buttonmax = m_hatmax = 0;
			return false;
		}

		m_private->m_joystick = SDL_JoystickOpen(m_joyindex);
		SDL_JoystickEventState(SDL_ENABLE);
		m_isinit = true;
		
		JOYSTICK_ECHO("Joystick " << m_joyindex << " initialized");
		
		/* must run after being initialized */
		m_axismax     = SDL_JoystickNumAxes(m_private->m_joystick);
		m_buttonmax   = SDL_JoystickNumButtons(m_private->m_joystick);
		m_hatmax      = SDL_JoystickNumHats(m_private->m_joystick);

		if      (m_axismax > JOYAXIS_MAX) m_axismax = JOYAXIS_MAX;  /* very unlikely */
		else if (m_axismax < 0)           m_axismax = 0;
		
		if      (m_hatmax > JOYHAT_MAX) m_hatmax = JOYHAT_MAX;  /* very unlikely */
		else if (m_hatmax < 0)          m_hatmax = 0;
		
		if (m_buttonmax < 0) m_buttonmax = 0;
		
	}
	return true;
#endif /* WITH_SDL */
}


void SCA_Joystick::DestroyJoystickDevice(void)
{
#ifdef WITH_SDL
	if (m_isinit) {
#if SDL_VERSION_ATLEAST(2, 0, 0)
		if (SDL_CHECK(SDL_JoystickGetAttached) && SDL_JoystickGetAttached(m_private->m_joystick))
#else
		if (SDL_CHECK(SDL_JoystickOpened) && SDL_JoystickOpened(m_joyindex))
#endif
		{
			JOYSTICK_ECHO("Closing-joystick " << m_joyindex);
			SDL_JoystickClose(m_private->m_joystick);
		}
		m_isinit = false;
	}
#endif /* WITH_SDL */
}

int SCA_Joystick::Connected(void)
{
#ifdef WITH_SDL
	if (m_isinit &&
#if SDL_VERSION_ATLEAST(2, 0, 0)
	    SDL_CHECK(SDL_JoystickGetAttached) && SDL_JoystickGetAttached(m_private->m_joystick)
#else
	    SDL_CHECK(SDL_JoystickOpened) && SDL_JoystickOpened(m_joyindex)
#endif
		)
	{
		return 1;
	}
#endif
	return 0;
}

int SCA_Joystick::pGetAxis(int axisnum, int udlr)
{
#ifdef WITH_SDL
	return m_axis_array[(axisnum*2)+udlr];
#endif
	return 0;
}

int SCA_Joystick::pAxisTest(int axisnum)
{
#ifdef WITH_SDL
	/* Use ints instead of shorts here to avoid problems when we get -32768.
	 * When we take the negative of that later, we should get 32768, which is greater
	 * than what a short can hold. In other words, abs(MIN_SHORT) > MAX_SHRT. */
	int i1 = m_axis_array[(axisnum * 2)];
	int i2 = m_axis_array[(axisnum * 2) + 1];
	
	/* long winded way to do:
	 * return max_ff(absf(i1), absf(i2))
	 * ...avoid abs from math.h */
	if (i1 < 0) i1 = -i1;
	if (i2 < 0) i2 = -i2;
	if (i1 <i2) return i2;
	else        return i1;
#else /* WITH_SDL */
	return 0;
#endif /* WITH_SDL */
}

const char *SCA_Joystick::GetName()
{
#ifdef WITH_SDL
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return (SDL_CHECK(SDL_JoystickName)) ? SDL_JoystickName(m_private->m_joystick) : "";
#else
	return (SDL_CHECK(SDL_JoystickName)) ? SDL_JoystickName(m_joyindex) : "";
#endif
#else /* WITH_SDL */
	return "";
#endif /* WITH_SDL */
}
