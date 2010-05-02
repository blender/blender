/**
 * $Id$
 *
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
#include "GPU_KeyboardDevice.h"

void GPU_KeyboardDevice::register_X_key_down_event(KeySym k)
{
	ConvertEvent(k, 1);
}

void GPU_KeyboardDevice::register_X_key_up_event(KeySym k)
{
	ConvertEvent(k, 0);
}


#define map_x_key_to_kx_key(x,y) m_reverseKeyTranslateTable[x] = y; 

GPU_KeyboardDevice::GPU_KeyboardDevice(void)
{
	unsigned int i = 0;

	// Needed?
	m_reverseKeyTranslateTable.clear();

	for (i = XK_A; i< XK_Z; i++) {
	  	m_reverseKeyTranslateTable[i] 
			= (SCA_IInputDevice::KX_EnumInputs)
			(((unsigned int)SCA_IInputDevice::KX_AKEY) + i - XK_A);
	}

	// Shifted versions: should not occur: KX doesn't distinguish 
	for (i = XK_a; i< XK_z; i++) {
		m_reverseKeyTranslateTable[i] 
			= (SCA_IInputDevice::KX_EnumInputs)
			(((int)SCA_IInputDevice::KX_AKEY) + i - XK_a);
	}

	for (i = XK_0; i< XK_9; i++) {
		m_reverseKeyTranslateTable[i] 
			= (SCA_IInputDevice::KX_EnumInputs)
			(((int)SCA_IInputDevice::KX_ZEROKEY) + i - XK_0);
	}

	for (i = XK_F1; i< XK_F19; i++) {
		m_reverseKeyTranslateTable[i] 
			= (SCA_IInputDevice::KX_EnumInputs)
			(((int)SCA_IInputDevice::KX_F1KEY) + i - XK_F1);
	}

	// the remainder:
	map_x_key_to_kx_key(XK_BackSpace,	SCA_IInputDevice::KX_BACKSPACEKEY);
	map_x_key_to_kx_key(XK_Tab,      	SCA_IInputDevice::KX_TABKEY);
	map_x_key_to_kx_key(XK_Return,   	SCA_IInputDevice::KX_RETKEY);
	map_x_key_to_kx_key(XK_Escape,   	SCA_IInputDevice::KX_ESCKEY);
	map_x_key_to_kx_key(XK_space,    	SCA_IInputDevice::KX_SPACEKEY);
	
	map_x_key_to_kx_key(XK_Shift_L,  	SCA_IInputDevice::KX_LEFTSHIFTKEY);
	map_x_key_to_kx_key(XK_Shift_R,  	SCA_IInputDevice::KX_RIGHTSHIFTKEY);
	map_x_key_to_kx_key(XK_Control_L,	SCA_IInputDevice::KX_LEFTCTRLKEY);
	map_x_key_to_kx_key(XK_Control_R,	SCA_IInputDevice::KX_RIGHTCTRLKEY);
	map_x_key_to_kx_key(XK_Alt_L,	 	SCA_IInputDevice::KX_LEFTALTKEY);
	map_x_key_to_kx_key(XK_Alt_R,	 	SCA_IInputDevice::KX_RIGHTALTKEY);

	map_x_key_to_kx_key(XK_Insert,	 	SCA_IInputDevice::KX_INSERTKEY);
	map_x_key_to_kx_key(XK_Delete,	 	SCA_IInputDevice::KX_DELKEY);
	map_x_key_to_kx_key(XK_Home,	 	SCA_IInputDevice::KX_HOMEKEY);
	map_x_key_to_kx_key(XK_End,		    SCA_IInputDevice::KX_ENDKEY);
	map_x_key_to_kx_key(XK_Page_Up,	    SCA_IInputDevice::KX_PAGEUPKEY);
	map_x_key_to_kx_key(XK_Page_Down, 	SCA_IInputDevice::KX_PAGEDOWNKEY);

	map_x_key_to_kx_key(XK_Left,		SCA_IInputDevice::KX_LEFTARROWKEY);
	map_x_key_to_kx_key(XK_Right,		SCA_IInputDevice::KX_RIGHTARROWKEY);
	map_x_key_to_kx_key(XK_Up,			SCA_IInputDevice::KX_UPARROWKEY);
	map_x_key_to_kx_key(XK_Down,		SCA_IInputDevice::KX_DOWNARROWKEY);

	map_x_key_to_kx_key(XK_KP_0,	 	SCA_IInputDevice::KX_PAD0);
	map_x_key_to_kx_key(XK_KP_1,	 	SCA_IInputDevice::KX_PAD1);
	map_x_key_to_kx_key(XK_KP_2,	 	SCA_IInputDevice::KX_PAD2);
	map_x_key_to_kx_key(XK_KP_3,	 	SCA_IInputDevice::KX_PAD3);
	map_x_key_to_kx_key(XK_KP_4,	 	SCA_IInputDevice::KX_PAD4);
	map_x_key_to_kx_key(XK_KP_5,	 	SCA_IInputDevice::KX_PAD5);
	map_x_key_to_kx_key(XK_KP_6,	 	SCA_IInputDevice::KX_PAD6);
	map_x_key_to_kx_key(XK_KP_7,	 	SCA_IInputDevice::KX_PAD7);
	map_x_key_to_kx_key(XK_KP_8,	 	SCA_IInputDevice::KX_PAD8);
	map_x_key_to_kx_key(XK_KP_9,	 	SCA_IInputDevice::KX_PAD9);
	map_x_key_to_kx_key(XK_KP_Decimal,	SCA_IInputDevice::KX_PADPERIOD);

	map_x_key_to_kx_key(XK_KP_Insert, 	SCA_IInputDevice::KX_INSERTKEY);
	map_x_key_to_kx_key(XK_KP_End,	 	SCA_IInputDevice::KX_ENDKEY);
	map_x_key_to_kx_key(XK_KP_Down,	 	SCA_IInputDevice::KX_DOWNARROWKEY);
	map_x_key_to_kx_key(XK_KP_Page_Down,SCA_IInputDevice::KX_PAGEDOWNKEY);
	map_x_key_to_kx_key(XK_KP_Left,	 	SCA_IInputDevice::KX_LEFTARROWKEY);
	map_x_key_to_kx_key(XK_KP_Right,	SCA_IInputDevice::KX_RIGHTARROWKEY);
	map_x_key_to_kx_key(XK_KP_Home,	 	SCA_IInputDevice::KX_HOMEKEY);
	map_x_key_to_kx_key(XK_KP_Up,	 	SCA_IInputDevice::KX_UPARROWKEY);
	map_x_key_to_kx_key(XK_KP_Page_Up,	SCA_IInputDevice::KX_PAGEUPKEY);
	map_x_key_to_kx_key(XK_KP_Delete,	SCA_IInputDevice::KX_DELKEY);

	map_x_key_to_kx_key(XK_KP_Enter,	SCA_IInputDevice::KX_PADENTER);
	map_x_key_to_kx_key(XK_KP_Add,		SCA_IInputDevice::KX_PADPLUSKEY);
	map_x_key_to_kx_key(XK_KP_Subtract,	SCA_IInputDevice::KX_PADMINUS);
	map_x_key_to_kx_key(XK_KP_Multiply,	SCA_IInputDevice::KX_PADASTERKEY);
	map_x_key_to_kx_key(XK_KP_Divide,	SCA_IInputDevice::KX_PADSLASHKEY);

	map_x_key_to_kx_key(XK_Caps_Lock,	SCA_IInputDevice::KX_CAPSLOCKKEY);
		
}
