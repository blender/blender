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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
/**
 * @file	GHOST_EventPrinter.h
 * Declaration of GHOST_EventPrinter class.
 */

#include "GHOST_EventPrinter.h"
#include <iostream>
#include "GHOST_EventKey.h"
#include "GHOST_EventDragnDrop.h"
#include "GHOST_Debug.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


bool GHOST_EventPrinter::processEvent(GHOST_IEvent* event)
{
	bool handled = true;
	
	GHOST_ASSERT(event, "event==0");

	if (event->getType() == GHOST_kEventWindowUpdate) return false;

	std::cout << "GHOST_EventPrinter::processEvent, time: " << (GHOST_TInt32)event->getTime() << ", type: ";
	switch (event->getType()) {
	case GHOST_kEventUnknown:
		std::cout << "GHOST_kEventUnknown"; handled = false;	
		break;

	case GHOST_kEventButtonUp:
		{
		GHOST_TEventButtonData* buttonData = (GHOST_TEventButtonData*)((GHOST_IEvent*)event)->getData();
		std::cout << "GHOST_kEventCursorButtonUp, button: " << buttonData->button;
		}
		break;
	case GHOST_kEventButtonDown:
		{
		GHOST_TEventButtonData* buttonData = (GHOST_TEventButtonData*)((GHOST_IEvent*)event)->getData();
		std::cout << "GHOST_kEventButtonDown, button: " << buttonData->button;
		}
		break;

	case GHOST_kEventWheel:
		{
		GHOST_TEventWheelData* wheelData = (GHOST_TEventWheelData*)((GHOST_IEvent*)event)->getData();
		std::cout << "GHOST_kEventWheel, z: " << wheelData->z;
		}
		break;

	case GHOST_kEventCursorMove:
		{
		GHOST_TEventCursorData* cursorData = (GHOST_TEventCursorData*)((GHOST_IEvent*)event)->getData();
		std::cout << "GHOST_kEventCursorMove, (x,y): (" << cursorData->x << "," << cursorData->y << ")";
		}
		break;

	case GHOST_kEventKeyUp:
		{
		GHOST_TEventKeyData* keyData = (GHOST_TEventKeyData*)((GHOST_IEvent*)event)->getData();
		STR_String str;
		getKeyString(keyData->key, str);
		std::cout << "GHOST_kEventKeyUp, key: " << str.Ptr();
		}
		break;
	case GHOST_kEventKeyDown:
		{
		GHOST_TEventKeyData* keyData = (GHOST_TEventKeyData*)((GHOST_IEvent*)event)->getData();
		STR_String str;
		getKeyString(keyData->key, str);
		std::cout << "GHOST_kEventKeyDown, key: " << str.Ptr();
		}
		break;
			
	case GHOST_kEventDraggingEntered:
		{
			GHOST_TEventDragnDropData* dragnDropData = (GHOST_TEventDragnDropData*)((GHOST_IEvent*)event)->getData();
			std::cout << "GHOST_kEventDraggingEntered, dragged object type : " << dragnDropData->dataType;
			std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
		}
		break;
			
	case GHOST_kEventDraggingUpdated:
		{
			GHOST_TEventDragnDropData* dragnDropData = (GHOST_TEventDragnDropData*)((GHOST_IEvent*)event)->getData();
			std::cout << "GHOST_kEventDraggingUpdated, dragged object type : " << dragnDropData->dataType;
			std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
		}
		break;

	case GHOST_kEventDraggingExited:
		{
			GHOST_TEventDragnDropData* dragnDropData = (GHOST_TEventDragnDropData*)((GHOST_IEvent*)event)->getData();
			std::cout << "GHOST_kEventDraggingExited, dragged object type : " << dragnDropData->dataType;
		}
		break;
	
	case GHOST_kEventDraggingDropDone:
		{
			GHOST_TEventDragnDropData* dragnDropData = (GHOST_TEventDragnDropData*)((GHOST_IEvent*)event)->getData();
			std::cout << "GHOST_kEventDraggingDropDone, dragged object type : " << dragnDropData->dataType;
			std::cout << " mouse at x=" << dragnDropData->x << " y=" << dragnDropData->y;
			switch (dragnDropData->dataType) {
				case GHOST_kDragnDropTypeString:
					std::cout << " string received = " << (char*)dragnDropData->data;
					break;
				case GHOST_kDragnDropTypeFilenames:
				{
					GHOST_TStringArray *strArray = (GHOST_TStringArray*)dragnDropData->data;
					int i;
					std::cout << "\nReceived " << strArray->count << " filenames";
					for (i=0;i<strArray->count;i++)
						std::cout << " Filename #" << i << ": " << strArray->strings[i];
				}
					break;
				default:
					break;
			}
		}
		break;

	case GHOST_kEventDraggingDropOnIcon:
		{
			GHOST_TEventDragnDropData* dragnDropData = (GHOST_TEventDragnDropData*)((GHOST_IEvent*)event)->getData();
			std::cout << "GHOST_kEventDraggingDropOnIcon, dragged object type : " << dragnDropData->dataType;
			switch (dragnDropData->dataType) {
				case GHOST_kDragnDropTypeString:
					std::cout << " string received = " << (char*)dragnDropData->data;
					break;
				case GHOST_kDragnDropTypeFilenames:
				{
					GHOST_TStringArray *strArray = (GHOST_TStringArray*)dragnDropData->data;
					int i;
					std::cout << "\nReceived " << strArray->count << " filenames";
					for (i=0;i<strArray->count;i++)
						std::cout << " Filename #" << i << ": " << strArray->strings[i];
				}
					break;
				default:
					break;
			}
		}
		break;
			
	case GHOST_kEventQuit:
		std::cout << "GHOST_kEventQuit"; 
		break;
	case GHOST_kEventWindowClose:
		std::cout << "GHOST_kEventWindowClose"; 
		break;
	case GHOST_kEventWindowActivate:
		std::cout << "GHOST_kEventWindowActivate"; 
		break;
	case GHOST_kEventWindowDeactivate:
		std::cout << "GHOST_kEventWindowDeactivate"; 
		break;
	case GHOST_kEventWindowUpdate:
		std::cout << "GHOST_kEventWindowUpdate"; 
		break;
	case GHOST_kEventWindowSize:
		std::cout << "GHOST_kEventWindowSize"; 
		break;

	default:
		std::cout << "not found"; handled = false; 
		break;
	}
	std::cout << "\n";
	return handled;
}


void GHOST_EventPrinter::getKeyString(GHOST_TKey key, STR_String& str) const
{
	if ((key >= GHOST_kKeyComma) && (key <= GHOST_kKeyRightBracket)) {
		str = ((char)key);
	} else if ((key >= GHOST_kKeyNumpad0) && (key <= GHOST_kKeyNumpad9)) {
		int number = key - GHOST_kKeyNumpad0;
		STR_String numberStr (number);
		str = "Numpad";
		str += numberStr;
#if defined(__sun__) || defined(__sun)
	} else if (key == 268828432) { /* solaris keyboards are messed up */
		 /* This should really test XK_F11 but that doesn't work */
		str = "F11";
	} else if (key == 268828433) { /* solaris keyboards are messed up */
		 /* This should really test XK_F12 but that doesn't work */
		str = "F12";
#endif
	} else if ((key >= GHOST_kKeyF1) && (key <= GHOST_kKeyF24)) {
		int number = key - GHOST_kKeyF1 + 1;
		STR_String numberStr (number);
		str = "F";
		str += numberStr;
	} else {
		switch (key)
		{
		case GHOST_kKeyBackSpace:
			str = "BackSpace";
			break;
		case GHOST_kKeyTab:
			str = "Tab";
			break;
		case GHOST_kKeyLinefeed:
			str = "Linefeed";
			break;
		case GHOST_kKeyClear:
			str = "Clear";
			break;
		case GHOST_kKeyEnter:
			str = "Enter";
			break;
		case GHOST_kKeyEsc:
			str = "Esc";
			break;
		case GHOST_kKeySpace:
			str = "Space";
			break;
		case GHOST_kKeyQuote:
			str = "Quote";
			break;
		case GHOST_kKeyBackslash:
			str = "\\";
			break;
		case GHOST_kKeyAccentGrave:
			str = "`";
			break;
		case GHOST_kKeyLeftShift:
			str = "LeftShift";
			break;
		case GHOST_kKeyRightShift:
			str = "RightShift";
			break;
		case GHOST_kKeyLeftControl:
			str = "LeftControl";
			break;
		case GHOST_kKeyRightControl:
			str = "RightControl";
			break;
		case GHOST_kKeyLeftAlt:
			str = "LeftAlt";
			break;
		case GHOST_kKeyRightAlt:
			str = "RightAlt";
			break;
		case GHOST_kKeyCommand:
            // APPLE only!
			str = "Command";
			break;
		case GHOST_kKeyGrLess:
            // PC german!
			str = "GrLess";
			break;
		case GHOST_kKeyCapsLock:
			str = "CapsLock";
			break;
		case GHOST_kKeyNumLock:
			str = "NumLock";
			break;
		case GHOST_kKeyScrollLock:
			str = "ScrollLock";
			break;
		case GHOST_kKeyLeftArrow:
			str = "LeftArrow";
			break;
		case GHOST_kKeyRightArrow:
			str = "RightArrow";
			break;
		case GHOST_kKeyUpArrow:
			str = "UpArrow";
			break;
		case GHOST_kKeyDownArrow:
			str = "DownArrow";
			break;
		case GHOST_kKeyPrintScreen:
			str = "PrintScreen";
			break;
		case GHOST_kKeyPause:
			str = "Pause";
			break;
		case GHOST_kKeyInsert:
			str = "Insert";
			break;
		case GHOST_kKeyDelete:
			str = "Delete";
			break;
		case GHOST_kKeyHome:
			str = "Home";
			break;
		case GHOST_kKeyEnd:
			str = "End";
			break;
		case GHOST_kKeyUpPage:
			str = "UpPage";
			break;
		case GHOST_kKeyDownPage:
			str = "DownPage";
			break;
		case GHOST_kKeyNumpadPeriod:
			str = "NumpadPeriod";
			break;
		case GHOST_kKeyNumpadEnter:
			str = "NumpadEnter";
			break;
		case GHOST_kKeyNumpadPlus:
			str = "NumpadPlus";
			break;
		case GHOST_kKeyNumpadMinus:
			str = "NumpadMinus";
			break;
		case GHOST_kKeyNumpadAsterisk:
			str = "NumpadAsterisk";
			break;
		case GHOST_kKeyNumpadSlash:
			str = "NumpadSlash";
			break;
		default:
			str = "unknown";
			break;
		}
	}
}

