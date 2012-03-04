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

/** \file SCA_Joystick.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_JOYSTICK_H__
#define __SCA_JOYSTICK_H__

#include "SCA_JoystickDefines.h"
#ifdef WITH_SDL
#  include "SDL.h"
#endif

/**
 * Basic Joystick class
 * I will make this class a singleton because there should be only one joystick
 * even if there are more than one scene using it and count how many scene are using it.
 * The underlying joystick should only be removed when the last scene is removed
 */

class SCA_Joystick

{
	static SCA_Joystick *m_instance[JOYINDEX_MAX];
	static int m_joynum;
	static int m_refCount;

	class PrivateData;
#ifdef WITH_SDL
	PrivateData		*m_private;
#endif
	int				m_joyindex;

	/** 
	 *support for JOYAXIS_MAX axes (in pairs)
	 */
	int m_axis_array[JOYAXIS_MAX];

	/** 
	 *support for JOYHAT_MAX hats (each is a direction)
	 */
	int m_hat_array[JOYHAT_MAX];	
	
	/**
	 * Precision or range of the axes
	 */
	int 			m_prec;

	/**
	 * max # of buttons avail
	*/
	
	int 			m_axismax;
	int 			m_buttonmax;
	int 			m_hatmax;
	
	/** is the joystick initialized ?*/
	bool			m_isinit;

	
	/** is triggered for each event type */
	bool			m_istrig_axis;
	bool			m_istrig_button;
	bool			m_istrig_hat;

#ifdef WITH_SDL
	/**
	 * event callbacks
	 */
	void OnAxisMotion(SDL_Event *sdl_event);
	void OnHatMotion(SDL_Event *sdl_event);
	void OnButtonUp(SDL_Event *sdl_event);
	void OnButtonDown(SDL_Event *sdl_event);
	void OnNothing(SDL_Event *sdl_event);
#if 0 /* not used yet */
	void OnBallMotion(SDL_Event *sdl_event){}
#endif
		
#endif /* WITH_SDL */
	/**
	 * Open the joystick
	 */
	bool CreateJoystickDevice(void);

	/**
	 * Close the joystick
	 */
	void DestroyJoystickDevice(void);

	/**
	 * fills the axis member values
	 */
	void pFillButtons(void);

	/**
	 * returns m_axis_array
	 */

	int pAxisTest(int axisnum);
	/**
	 * returns m_axis_array
	 */
	int pGetAxis(int axisnum, int udlr);

	SCA_Joystick(short int index);

	~SCA_Joystick();
	
public:

	static SCA_Joystick *GetInstance( short int joyindex );
	static void HandleEvents( void );
	void ReleaseInstance();
	

	/*
	 */
	bool aAxisPairIsPositive(int axis);
	bool aAxisPairDirectionIsPositive(int axis, int dir); /* function assumes joysticks are in axis pairs */
	bool aAxisIsPositive(int axis_single); /* check a single axis only */

	bool aAnyButtonPressIsPositive(void);
	bool aButtonPressIsPositive(int button);
	bool aButtonReleaseIsPositive(int button);
	bool aHatIsPositive(int hatnum, int dir);

	/**
	 * precision is default '3200' which is overridden by input
	 */

	void cSetPrecision(int val);

	int GetAxisPosition(int index){
		return m_axis_array[index];
	}

	int GetHat(int index){
		return m_hat_array[index];
	}

	int GetThreshold(void){
		return m_prec;
	}

	bool IsTrigAxis(void){
		return m_istrig_axis;
	}
	
	bool IsTrigButton(void){
		return m_istrig_button;
	}
	
	bool IsTrigHat(void){
		return m_istrig_hat;
	}

	/**
	 * returns the # of...
	 */

	int GetNumberOfAxes(void);
	int GetNumberOfButtons(void);
	int GetNumberOfHats(void);
	
	/**
	 * Test if the joystick is connected
	 */
	int Connected(void);
};

#endif

