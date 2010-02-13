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
#ifndef __KX_BLENDERINPUTDEVICE
#define __KX_BLENDERINPUTDEVICE

#ifdef WIN32
#pragma warning(disable : 4786)  // shut off 255 char limit debug template warning
#endif

#include <map>

#include "wm_event_types.h"
#include "WM_types.h"
#include "SCA_IInputDevice.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/**
 Base Class for Blender specific inputdevices. Blender specific inputdevices are used when the gameengine is running in embedded mode instead of standalone mode.
*/
class BL_BlenderInputDevice : public SCA_IInputDevice                                                               
{
	// this map is Blender specific: a conversion between blender and ketsji enums
	std::map<int,KX_EnumInputs> m_reverseKeyTranslateTable;
public:
	BL_BlenderInputDevice()                                                                                    
		{                                                                                                          
			
			/* The reverse table. In order to not confuse ourselves, we      */
			/* immediately convert all events that come in to KX codes.      */
			m_reverseKeyTranslateTable[LEFTMOUSE			] =	KX_LEFTMOUSE		;
			m_reverseKeyTranslateTable[MIDDLEMOUSE			] =	KX_MIDDLEMOUSE		;
			m_reverseKeyTranslateTable[RIGHTMOUSE			] =	KX_RIGHTMOUSE		;
			m_reverseKeyTranslateTable[WHEELUPMOUSE			] =	KX_WHEELUPMOUSE		;
			m_reverseKeyTranslateTable[WHEELDOWNMOUSE		] =	KX_WHEELDOWNMOUSE	;
			m_reverseKeyTranslateTable[MOUSEX			] =	KX_MOUSEX		;
			m_reverseKeyTranslateTable[MOUSEY			] =	KX_MOUSEY		;
                                                                                                                   
			// TIMERS                                                                                                  

			m_reverseKeyTranslateTable[TIMER0                           ] =	KX_TIMER0                  ;                  
			m_reverseKeyTranslateTable[TIMER1                           ] =	KX_TIMER1                  ;                  
			m_reverseKeyTranslateTable[TIMER2                           ] = KX_TIMER2                  ;                  
                                                                                                                   
			// SYSTEM 
#if 0			
			/* **** XXX **** */
			m_reverseKeyTranslateTable[KEYBD                            ] = KX_KEYBD                   ;                  
			m_reverseKeyTranslateTable[RAWKEYBD                         ] = KX_RAWKEYBD                ;                  
			m_reverseKeyTranslateTable[REDRAW                           ] = KX_REDRAW                  ;                  
			m_reverseKeyTranslateTable[INPUTCHANGE                      ] = KX_INPUTCHANGE             ;                  
			m_reverseKeyTranslateTable[QFULL                            ] = KX_QFULL                   ;                  
			m_reverseKeyTranslateTable[WINFREEZE                        ] = KX_WINFREEZE               ;                  
			m_reverseKeyTranslateTable[WINTHAW                          ] = KX_WINTHAW                 ;                  
			m_reverseKeyTranslateTable[WINCLOSE                         ] = KX_WINCLOSE                ;                  
			m_reverseKeyTranslateTable[WINQUIT                          ] = KX_WINQUIT                 ;                  
			m_reverseKeyTranslateTable[Q_FIRSTTIME                      ] = KX_Q_FIRSTTIME             ;                  
			/* **** XXX **** */
#endif                                                                                                                   
			// standard keyboard                                                                                       
                                                                                                                   
			m_reverseKeyTranslateTable[AKEY                             ] = KX_AKEY                    ;                  
			m_reverseKeyTranslateTable[BKEY                             ] = KX_BKEY                    ;                  
			m_reverseKeyTranslateTable[CKEY                             ] = KX_CKEY                    ;                  
			m_reverseKeyTranslateTable[DKEY                             ] = KX_DKEY                    ;                  
			m_reverseKeyTranslateTable[EKEY                             ] = KX_EKEY                    ;                  
			m_reverseKeyTranslateTable[FKEY                             ] = KX_FKEY                    ;                  
			m_reverseKeyTranslateTable[GKEY                             ] = KX_GKEY                    ;                  
//XXX clean up
#ifdef WIN32
#define HKEY	'h'
#endif
			m_reverseKeyTranslateTable[HKEY                             ] = KX_HKEY                    ;                  
//XXX clean up
#ifdef WIN32
#undef HKEY
#endif
			m_reverseKeyTranslateTable[IKEY                             ] = KX_IKEY                    ;                  
			m_reverseKeyTranslateTable[JKEY                             ] = KX_JKEY                    ;                  
			m_reverseKeyTranslateTable[KKEY                             ] = KX_KKEY                    ;                  
			m_reverseKeyTranslateTable[LKEY                             ] = KX_LKEY                    ;                  
			m_reverseKeyTranslateTable[MKEY                             ] = KX_MKEY                    ;                  
			m_reverseKeyTranslateTable[NKEY                             ] = KX_NKEY                    ;                  
			m_reverseKeyTranslateTable[OKEY                             ] = KX_OKEY                    ;                  
			m_reverseKeyTranslateTable[PKEY                             ] = KX_PKEY                    ;                  
			m_reverseKeyTranslateTable[QKEY                             ] = KX_QKEY                    ;                  
			m_reverseKeyTranslateTable[RKEY                             ] = KX_RKEY                    ;                  
			m_reverseKeyTranslateTable[SKEY                             ] = KX_SKEY                    ;                  
			m_reverseKeyTranslateTable[TKEY                             ] =	KX_TKEY                    ;                  
			m_reverseKeyTranslateTable[UKEY                             ] = KX_UKEY                    ;                  
			m_reverseKeyTranslateTable[VKEY                             ] = KX_VKEY                    ;                  
			m_reverseKeyTranslateTable[WKEY                             ] = KX_WKEY                    ;                  
			m_reverseKeyTranslateTable[XKEY                             ] = KX_XKEY                    ;                  
			m_reverseKeyTranslateTable[YKEY                             ] = KX_YKEY                    ;                  
			m_reverseKeyTranslateTable[ZKEY                             ] = KX_ZKEY                    ;                  
                                                                                                                   
			m_reverseKeyTranslateTable[ZEROKEY		                ] = KX_ZEROKEY		        ;                  
			m_reverseKeyTranslateTable[ONEKEY		                ] = KX_ONEKEY		        ;                  
			m_reverseKeyTranslateTable[TWOKEY		                ] = KX_TWOKEY		        ;                  
			m_reverseKeyTranslateTable[THREEKEY                     ] = KX_THREEKEY                ;                  
			m_reverseKeyTranslateTable[FOURKEY		                ] = KX_FOURKEY		        ;                  
			m_reverseKeyTranslateTable[FIVEKEY		                ] = KX_FIVEKEY		        ;                  
			m_reverseKeyTranslateTable[SIXKEY		                ] = KX_SIXKEY		        ;                  
			m_reverseKeyTranslateTable[SEVENKEY                         ] = KX_SEVENKEY                ;                  
			m_reverseKeyTranslateTable[EIGHTKEY                         ] = KX_EIGHTKEY                ;                  
			m_reverseKeyTranslateTable[NINEKEY		                ] = KX_NINEKEY		        ;                  
	                                                                                                           
			m_reverseKeyTranslateTable[CAPSLOCKKEY                      ] = KX_CAPSLOCKKEY             ;                  
	                                                        
			m_reverseKeyTranslateTable[LEFTCTRLKEY	                ] = KX_LEFTCTRLKEY	        ;                  
			m_reverseKeyTranslateTable[LEFTALTKEY 		        ] = KX_LEFTALTKEY 		;                  
			m_reverseKeyTranslateTable[RIGHTALTKEY 	                ] = KX_RIGHTALTKEY 	        ;                  
			m_reverseKeyTranslateTable[RIGHTCTRLKEY 	                ] = KX_RIGHTCTRLKEY 	        ;                  
			m_reverseKeyTranslateTable[RIGHTSHIFTKEY	                ] = KX_RIGHTSHIFTKEY	        ;                  
			m_reverseKeyTranslateTable[LEFTSHIFTKEY                     ] = KX_LEFTSHIFTKEY            ;                  
	                                                                                                           
			m_reverseKeyTranslateTable[ESCKEY                           ] = KX_ESCKEY                  ;                  
			m_reverseKeyTranslateTable[TABKEY                           ] = KX_TABKEY                  ;                  
			m_reverseKeyTranslateTable[RETKEY                           ] = KX_RETKEY                  ;                  
			m_reverseKeyTranslateTable[SPACEKEY                         ] = KX_SPACEKEY                ;                  
			m_reverseKeyTranslateTable[LINEFEEDKEY		        ] = KX_LINEFEEDKEY		;                  
			m_reverseKeyTranslateTable[BACKSPACEKEY                     ] = KX_BACKSPACEKEY            ;                  
			m_reverseKeyTranslateTable[DELKEY                           ] = KX_DELKEY                  ;                  
			m_reverseKeyTranslateTable[SEMICOLONKEY                     ] = KX_SEMICOLONKEY            ;                  
			m_reverseKeyTranslateTable[PERIODKEY		        ] = KX_PERIODKEY		;                  
			m_reverseKeyTranslateTable[COMMAKEY		                ] = KX_COMMAKEY		;                  
			m_reverseKeyTranslateTable[QUOTEKEY		                ] = KX_QUOTEKEY		;                  
			m_reverseKeyTranslateTable[ACCENTGRAVEKEY	                ] = KX_ACCENTGRAVEKEY	        ;                  
			m_reverseKeyTranslateTable[MINUSKEY		                ] = KX_MINUSKEY		;                  
			m_reverseKeyTranslateTable[SLASHKEY		                ] = KX_SLASHKEY		;                  
			m_reverseKeyTranslateTable[BACKSLASHKEY                     ] = KX_BACKSLASHKEY            ;                  
			m_reverseKeyTranslateTable[EQUALKEY		                ] = KX_EQUALKEY		;                  
			m_reverseKeyTranslateTable[LEFTBRACKETKEY	                ] = KX_LEFTBRACKETKEY	        ;                  
			m_reverseKeyTranslateTable[RIGHTBRACKETKEY	                ] = KX_RIGHTBRACKETKEY	        ;                  
	                                                                                                           
			m_reverseKeyTranslateTable[LEFTARROWKEY                     ] = KX_LEFTARROWKEY            ;                  
			m_reverseKeyTranslateTable[DOWNARROWKEY                     ] = KX_DOWNARROWKEY            ;                  
			m_reverseKeyTranslateTable[RIGHTARROWKEY	                ] = KX_RIGHTARROWKEY	        ;                  
			m_reverseKeyTranslateTable[UPARROWKEY		        ] = KX_UPARROWKEY		;                  
                                                                                                                   
			m_reverseKeyTranslateTable[PAD2	                        ] = KX_PAD2	                ;                  
			m_reverseKeyTranslateTable[PAD4	                        ] = KX_PAD4	                ;                  
			m_reverseKeyTranslateTable[PAD6	                        ] = KX_PAD6	                ;                  
			m_reverseKeyTranslateTable[PAD8	                        ] = KX_PAD8	                ;                  
	                                                                                                           
			m_reverseKeyTranslateTable[PAD1	                        ] = KX_PAD1	                ;                  
			m_reverseKeyTranslateTable[PAD3	                        ] = KX_PAD3	                ;                  
			m_reverseKeyTranslateTable[PAD5	                        ] = KX_PAD5	                ;                  
			m_reverseKeyTranslateTable[PAD7	                        ] = KX_PAD7	                ;                  
			m_reverseKeyTranslateTable[PAD9	                        ] = KX_PAD9	                ;                  

			m_reverseKeyTranslateTable[PADPERIOD                        ] = KX_PADPERIOD               ;                  
			m_reverseKeyTranslateTable[PADSLASHKEY                    ] = KX_PADSLASHKEY           ;                  
			m_reverseKeyTranslateTable[PADASTERKEY                      ] = KX_PADASTERKEY             ;                  
	                                                                                                           
	                                                                                                           
			m_reverseKeyTranslateTable[PAD0	                        ] = KX_PAD0	                ;                  
			m_reverseKeyTranslateTable[PADMINUS                         ] = KX_PADMINUS                ;                  
			m_reverseKeyTranslateTable[PADENTER                         ] = KX_PADENTER                ;                  
			m_reverseKeyTranslateTable[PADPLUSKEY                       ] = KX_PADPLUSKEY              ;                  
	                                                                                                           
	                                                                                                           
			m_reverseKeyTranslateTable[F1KEY                            ] = KX_F1KEY                   ;                  
			m_reverseKeyTranslateTable[F2KEY                            ] = KX_F2KEY                   ;                  
			m_reverseKeyTranslateTable[F3KEY                            ] = KX_F3KEY                   ;                  
			m_reverseKeyTranslateTable[F4KEY                            ] = KX_F4KEY                   ;                  
			m_reverseKeyTranslateTable[F5KEY                            ] = KX_F5KEY                   ;                  
			m_reverseKeyTranslateTable[F6KEY                            ] = KX_F6KEY                   ;                  
			m_reverseKeyTranslateTable[F7KEY                            ] = KX_F7KEY                   ;                  
			m_reverseKeyTranslateTable[F8KEY                            ] = KX_F8KEY                   ;                  
			m_reverseKeyTranslateTable[F9KEY                            ] = KX_F9KEY                   ;                  
			m_reverseKeyTranslateTable[F10KEY                           ] = KX_F10KEY                  ;                  
			m_reverseKeyTranslateTable[F11KEY      ] = KX_F11KEY                  ;                  
			m_reverseKeyTranslateTable[F12KEY      ] = KX_F12KEY                  ;                  
	                                                                                                           
			m_reverseKeyTranslateTable[PAUSEKEY    ] = KX_PAUSEKEY                ;                  
			m_reverseKeyTranslateTable[INSERTKEY   ] = KX_INSERTKEY               ;                  
			m_reverseKeyTranslateTable[HOMEKEY     ] = KX_HOMEKEY                 ;                  
			m_reverseKeyTranslateTable[PAGEUPKEY   ] = KX_PAGEUPKEY               ;                  
			m_reverseKeyTranslateTable[PAGEDOWNKEY ] = KX_PAGEDOWNKEY             ;                  
			m_reverseKeyTranslateTable[ENDKEY      ] = KX_ENDKEY                  ;                  

		                                                                                                   
		}                                                                                                          

	virtual ~BL_BlenderInputDevice()
		{

		}
                                                                                                                   
	 KX_EnumInputs ToNative(unsigned short incode) {
		return m_reverseKeyTranslateTable[incode];
	}

	virtual bool	IsPressed(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
//	virtual const SCA_InputEvent&	GetEventValue(SCA_IInputDevice::KX_EnumInputs inputcode)=0;
	virtual bool	ConvertBlenderEvent(unsigned short incode,short val)=0;

	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_BlenderInputDevice"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};                                                                                                                 
#endif //__KX_BLENDERINPUTDEVICE

