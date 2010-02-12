/**
 * Interface for input devices. The defines for keyboard/system/mouse events
 * here are for internal use in the KX module.
 *
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

#ifndef KX_INPUTDEVICE_H
#define KX_INPUTDEVICE_H

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class SCA_InputEvent 
{
	
public:
	enum SCA_EnumInputs {
	
		KX_NO_INPUTSTATUS = 0,
		KX_JUSTACTIVATED,
		KX_ACTIVE,
		KX_JUSTRELEASED,
		KX_MAX_INPUTSTATUS
	};

	SCA_InputEvent(SCA_EnumInputs status=KX_NO_INPUTSTATUS,int eventval=0)
		:	m_status(status),
		m_eventval(eventval)
	{

	}

	SCA_EnumInputs m_status;
	int		m_eventval;
};

class SCA_IInputDevice 
{

	
public:

	SCA_IInputDevice();
	virtual ~SCA_IInputDevice();	

	enum KX_EnumInputs {
	
		KX_NOKEY = 0,
	
		// TIMERS 
	
		KX_TIMER0,
		KX_TIMER1,
		KX_TIMER2,
	
		// SYSTEM

		/* Moved to avoid clashes with KX_RETKEY */
		KX_KEYBD,
		KX_RAWKEYBD,
		KX_REDRAW,
		KX_INPUTCHANGE,
		KX_QFULL,
		KX_WINFREEZE,
		KX_WINTHAW,
		/* thaw is 11 */

		/* move past retkey*/
		KX_WINCLOSE = 14,
		KX_WINQUIT,
		KX_Q_FIRSTTIME,
		/* sequence ends on 16 */
	
		// standard keyboard 

		/* Because of the above preamble, KX_BEGINKEY is 15 ! This
		 * means that KX_RETKEY on 13d (0Dh)) will double up with
		 * KX_WINQUIT!  Why is it 13? Because ascii 13d is Ctrl-M aka
		 * CR! Its little brother, LF has 10d (0Ah). This is
		 * dangerous, since the keyboards start scanning at
		 * KX_BEGINKEY. I think the keyboard system should push its
		 * key events instead of demanding the user to poll the
		 * table... But that's for another time... The fix for now is
		 * to move the above system events into a 'safe' (ie. unused)
		 * range. I am loathe to move it away from this 'magical'
		 * coincidence.. it's probably exploited somewhere. I hope the
		 * close and quit events don't mess up 'normal' kb code
		 * scanning.
		 * */
		KX_BEGINKEY = 12,

		KX_RETKEY = 13,
		KX_SPACEKEY = 32,
		KX_PADASTERKEY = 42,
		KX_COMMAKEY = 44,		
		KX_MINUSKEY = 45,		
		KX_PERIODKEY = 46,
		KX_ZEROKEY = 48,
		
		KX_ONEKEY,		// =49
		KX_TWOKEY,		
		KX_THREEKEY,
		KX_FOURKEY,		
		KX_FIVEKEY,		
		KX_SIXKEY,		
		KX_SEVENKEY,
		KX_EIGHTKEY,
		KX_NINEKEY,		// = 57

		KX_AKEY = 97,
		KX_BKEY,
		KX_CKEY,
		KX_DKEY,
		KX_EKEY,
		KX_FKEY,
		KX_GKEY,
		KX_HKEY,
		KX_IKEY,
		KX_JKEY,
		KX_KKEY,
		KX_LKEY,
		KX_MKEY,
		KX_NKEY, // =110
		KX_OKEY,
		KX_PKEY,
		KX_QKEY,
		KX_RKEY,
		KX_SKEY,
		KX_TKEY,
		KX_UKEY,
		KX_VKEY,
		KX_WKEY,
		KX_XKEY, // =120
		KX_YKEY,
		KX_ZKEY, // =122
	
		
		
		KX_CAPSLOCKKEY, // 123
		
		KX_LEFTCTRLKEY,	// 124
		KX_LEFTALTKEY, 		
		KX_RIGHTALTKEY, 	
		KX_RIGHTCTRLKEY, 	
		KX_RIGHTSHIFTKEY,	
		KX_LEFTSHIFTKEY,// 129
		
		KX_ESCKEY, // 130
		KX_TABKEY, //131
		
		
		KX_LINEFEEDKEY,	 // 132	
		KX_BACKSPACEKEY,
		KX_DELKEY,
		KX_SEMICOLONKEY, // 135
		
		
		KX_QUOTEKEY,		//136
		KX_ACCENTGRAVEKEY,	//137
		
		KX_SLASHKEY,		//138
		KX_BACKSLASHKEY,
		KX_EQUALKEY,		
		KX_LEFTBRACKETKEY,	
		KX_RIGHTBRACKETKEY,	// 142
		
		KX_LEFTARROWKEY, // 145
		KX_DOWNARROWKEY,
		KX_RIGHTARROWKEY,	
		KX_UPARROWKEY,		// 148
	
		KX_PAD2	,
		KX_PAD4	,
		KX_PAD6	,
		KX_PAD8	,
		
		KX_PAD1	,
		KX_PAD3	,
		KX_PAD5	,
		KX_PAD7	,
		KX_PAD9	,
		
		KX_PADPERIOD,
		KX_PADSLASHKEY,
		
		
		
		KX_PAD0	,
		KX_PADMINUS,
		KX_PADENTER,
		KX_PADPLUSKEY,
		
		
		KX_F1KEY ,
		KX_F2KEY ,
		KX_F3KEY ,
		KX_F4KEY ,
		KX_F5KEY ,
		KX_F6KEY ,
		KX_F7KEY ,
		KX_F8KEY ,
		KX_F9KEY ,
		KX_F10KEY,
		KX_F11KEY,
		KX_F12KEY,
		
		KX_PAUSEKEY,
		KX_INSERTKEY,
		KX_HOMEKEY ,
		KX_PAGEUPKEY,
		KX_PAGEDOWNKEY,
		KX_ENDKEY,

		// MOUSE
		KX_BEGINMOUSE,
		
		KX_BEGINMOUSEBUTTONS,

		KX_LEFTMOUSE,
		KX_MIDDLEMOUSE,
		KX_RIGHTMOUSE,
		
		KX_ENDMOUSEBUTTONS,
		
		KX_WHEELUPMOUSE,
		KX_WHEELDOWNMOUSE,

		KX_MOUSEX,
		KX_MOUSEY,
	
		KX_ENDMOUSE,



		KX_MAX_KEYS
		
	} ; // enum  


protected:
	/**  
		m_eventStatusTables are two tables that contain current and previous
		status of all events
	*/

	SCA_InputEvent	m_eventStatusTables[2][SCA_IInputDevice::KX_MAX_KEYS];
	/**  
		m_currentTable is index for m_keyStatusTable that toggle between 0 or 1 
	*/
	int				m_currentTable; 
	void			ClearStatusTable(int tableid);

public:
	virtual bool	IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
	virtual const SCA_InputEvent&	GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode);

	/**
	 * Count active events(active and just_activated)
	 */
	virtual int		GetNumActiveEvents();

	/**
	 * Get the number of ramping events (just_activated, just_released)
	 */
	virtual int		GetNumJustEvents();
	
	virtual void		HookEscape();
	
	/* Next frame: we calculate the new key states. This goes as follows:
	*
	* KX_NO_INPUTSTATUS -> KX_NO_INPUTSTATUS
	* KX_JUSTACTIVATED  -> KX_ACTIVE
	* KX_ACTIVE         -> KX_ACTIVE
	* KX_JUSTRELEASED   -> KX_NO_INPUTSTATUS
	*
	* Getting new events provides the
	* KX_NO_INPUTSTATUS->KX_JUSTACTIVATED and
	* KX_ACTIVE->KX_JUSTRELEASED transitions.
	*/
	virtual void	NextFrame();


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_InputEvent"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif	//KX_INPUTDEVICE_H

