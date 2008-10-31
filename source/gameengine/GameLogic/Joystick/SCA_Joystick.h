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

#ifndef _SCA_JOYSTICK_H_
#define _SCA_JOYSTICK_H_

#include "SCA_JoystickDefines.h"
#ifndef DISABLE_SDL
#include "SDL.h"
#endif

/*
 * Basic Joystick class
 * I will make this class a singleton because there should be only one joystick
 * even if there are more than one scene using it and count how many scene are using it.
 * The underlying joystick should only be removed when the last scene is removed
 */

class SCA_Joystick

{
	static SCA_Joystick *m_instance[JOYINDEX_MAX];
	static int m_refCount;

	class PrivateData;
#ifndef DISABLE_SDL
	PrivateData		*m_private;
#endif
	int				m_joyindex;

	/* 
	 *support for 2 axes 
	 */

	int m_axis10,m_axis11;
	int m_axis20,m_axis21;

	/*
	 * Precision or range of the axes
	 */
	int 			m_prec;

	/*
	 * multiple axis values stored here
	 */
	int 			m_axisnum;
	int 			m_axisvalue;

	/*
	 * max # of axes avail
	 */
	/*disabled
	int 			m_axismax;
	*/

	/* 
	 *	button values stored here 
	 */
	int 			m_buttonnum;

	/*
	 * max # of buttons avail
	*/
	
	int 			m_axismax;
	int 			m_buttonmax;
	int 			m_hatmax;
	
	/*
	 * hat values stored here 
	 */
	int 			m_hatnum;
	int 			m_hatdir;

	/*

	 * max # of hats avail
		disabled
	int 			m_hatmax;
	 */
	/* is the joystick initialized ?*/
	bool			m_isinit;

	
	/* is triggered for each event type */
	bool			m_istrig_axis;
	bool			m_istrig_button;
	bool			m_istrig_hat;

#ifndef DISABLE_SDL
	/*
	 * event callbacks
	 */
	void OnAxisMotion(SDL_Event *sdl_event);
	void OnHatMotion(SDL_Event *sdl_event);
	void OnButtonUp(SDL_Event *sdl_event);
	void OnButtonDown(SDL_Event *sdl_event);
	void OnNothing(SDL_Event *sdl_event);
	void OnBallMotion(SDL_Event *sdl_event){}
#endif
	/*
	 * Open the joystick
	 */
	bool CreateJoystickDevice(void);

	/*
	 * Close the joystick
	 */
	void DestroyJoystickDevice(void);

	/*
	 * fills the axis mnember values 
	 */
	void pFillAxes(void);
	void pFillButtons(void);

	/*
	 * returns m_axis10,m_axis11...
	 */

	int pAxisTest(int axisnum);
	/*
	 * returns m_axis10,m_axis11...
	 */
	int pGetAxis(int axisnum, int udlr);

	/*
	 * gets the current hat direction
	 */
	int pGetHat(int direction);

	SCA_Joystick(short int index);

	~SCA_Joystick();
	
public:

	static SCA_Joystick *GetInstance( short int joyindex );
	static void HandleEvents( void );
	void ReleaseInstance();
	

	/*
	 */
	bool aAnyAxisIsPositive(int axis);
	bool aUpAxisIsPositive(int axis);
	bool aDownAxisIsPositive(int axis);
	bool aLeftAxisIsPositive(int axis);
	bool aRightAxisIsPositive(int axis);

	bool aAnyButtonPressIsPositive(void);
	bool aAnyButtonReleaseIsPositive(void);
	bool aButtonPressIsPositive(int button);
	bool aButtonReleaseIsPositive(int button);
	bool aHatIsPositive(int dir);

	/*
	 * precision is default '3200' which is overridden by input
	 */

	void cSetPrecision(int val);

	int GetAxis10(void){

		return m_axis10;

	}

	int GetAxis11(void){
		return m_axis11;
	}

	int GetAxis20(void){
		return m_axis20;
	}

	int GetAxis21(void){
		return m_axis21;
	}

	int GetButton(void){
		return m_buttonnum;
	}

	int GetHat(void){
		return m_hatdir;
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

	/*
	 * returns the # of...
	 */

	int GetNumberOfAxes(void);
	int GetNumberOfButtons(void);
	int GetNumberOfHats(void);
	
	/*
	 * Test if the joystick is connected
	 */
	int Connected(void);
};
#ifndef	DISABLE_SDL
void Joystick_HandleEvents( void );
#endif

#endif

