/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	May 7, 2001
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "GHOST_SystemCarbon.h"

#include "GHOST_DisplayManagerCarbon.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventWheel.h"
#include "GHOST_TimerManager.h"
#include "GHOST_TimerTask.h"
#include "GHOST_WindowManager.h"
#include "GHOST_WindowCarbon.h"

#define GHOST_KEY_SWITCH(mac, ghost) { case (mac): ghostKey = (ghost); break; }

const EventTypeSpec	kEvents[] =
{
	{ kEventClassAppleEvent, kEventAppleEvent },
	
/*
	{ kEventClassApplication, kEventAppActivated },
	{ kEventClassApplication, kEventAppDeactivated },
*/	

	{ kEventClassKeyboard, kEventRawKeyDown },
	{ kEventClassKeyboard, kEventRawKeyRepeat },
	{ kEventClassKeyboard, kEventRawKeyUp },
	{ kEventClassKeyboard, kEventRawKeyModifiersChanged },
	
	{ kEventClassMouse, kEventMouseDown },
	{ kEventClassMouse, kEventMouseUp },
	{ kEventClassMouse, kEventMouseMoved },
	{ kEventClassMouse, kEventMouseDragged },
	{ kEventClassMouse, kEventMouseWheelMoved },
	
	{ kEventClassWindow, kEventWindowClose },
	{ kEventClassWindow, kEventWindowActivated },
	{ kEventClassWindow, kEventWindowDeactivated },
	{ kEventClassWindow, kEventWindowUpdate },
	{ kEventClassWindow, kEventWindowBoundsChanged }
};

static GHOST_TButtonMask convertButton(EventMouseButton button)
{
	switch (button) {
	case kEventMouseButtonPrimary:
		return GHOST_kButtonMaskLeft;
	case kEventMouseButtonSecondary:
		return GHOST_kButtonMaskRight;
	case kEventMouseButtonTertiary:
	default:
		return GHOST_kButtonMaskMiddle;
	}
}

static GHOST_TKey convertKey(int rawCode) 
{	
		/* This bit of magic converts the rawCode into a virtual
		 * Mac key based on the current keyboard mapping, but
		 * without regard to the modifiers (so we don't get 'a' 
		 * and 'A' for example.
		 */
	UInt32 dummy= 0;
	Handle transData = (Handle) GetScriptManagerVariable(smKCHRCache);
	char vk = KeyTranslate(transData, rawCode, &dummy);
	
		/* Map numpad based on rawcodes first, otherwise they
		 * look like non-numpad events.
		 */
	switch (rawCode) {
	case 82: 	return GHOST_kKeyNumpad0;
	case 83: 	return GHOST_kKeyNumpad1;
	case 84: 	return GHOST_kKeyNumpad2;
	case 85: 	return GHOST_kKeyNumpad3;
	case 86: 	return GHOST_kKeyNumpad4;
	case 87: 	return GHOST_kKeyNumpad5;
	case 88: 	return GHOST_kKeyNumpad6;
	case 89: 	return GHOST_kKeyNumpad7;
	case 91: 	return GHOST_kKeyNumpad8;
	case 92: 	return GHOST_kKeyNumpad9;
	case 65: 	return GHOST_kKeyNumpadPeriod;
	case 76: 	return GHOST_kKeyNumpadEnter;
	case 69: 	return GHOST_kKeyNumpadPlus;
	case 78: 	return GHOST_kKeyNumpadMinus;
	case 67: 	return GHOST_kKeyNumpadAsterisk;
	case 75: 	return GHOST_kKeyNumpadSlash;
	}
	
	if ((vk >= 'a') && (vk <= 'z')) {
		return (GHOST_TKey) (vk - 'a' + GHOST_kKeyA);
	} else if ((vk >= '0') && (vk <= '9')) {
		return (GHOST_TKey) (vk - '0' + GHOST_kKey0);
	} else if (vk==16) {
		switch (rawCode) {
		case 122: 	return GHOST_kKeyF1;
		case 120: 	return GHOST_kKeyF2;
		case 99: 	return GHOST_kKeyF3;
		case 118: 	return GHOST_kKeyF4;
		case 96: 	return GHOST_kKeyF5;
		case 97: 	return GHOST_kKeyF6;
		case 98: 	return GHOST_kKeyF7;
		case 100: 	return GHOST_kKeyF8;
		case 101: 	return GHOST_kKeyF9;
		case 109: 	return GHOST_kKeyF10;
		case 103: 	return GHOST_kKeyF11;
		case 111: 	return GHOST_kKeyF12;  // Never get, is used for ejecting the CD! 
		}
	} else {
		switch (vk) {
		case kUpArrowCharCode: 		return GHOST_kKeyUpArrow;
		case kDownArrowCharCode: 	return GHOST_kKeyDownArrow;
		case kLeftArrowCharCode: 	return GHOST_kKeyLeftArrow;
		case kRightArrowCharCode: 	return GHOST_kKeyRightArrow;

		case kReturnCharCode: 		return GHOST_kKeyEnter;
		case kBackspaceCharCode: 	return GHOST_kKeyBackSpace;
		case kDeleteCharCode:		return GHOST_kKeyDelete;
		case kEscapeCharCode: 		return GHOST_kKeyEsc;
		case kTabCharCode: 			return GHOST_kKeyTab;
		case kSpaceCharCode: 		return GHOST_kKeySpace;

		case kHomeCharCode: 		return GHOST_kKeyHome;
		case kEndCharCode:			return GHOST_kKeyEnd;
		case kPageUpCharCode: 		return GHOST_kKeyUpPage;
		case kPageDownCharCode: 	return GHOST_kKeyDownPage;

		case '-': 	return GHOST_kKeyMinus;
		case '=': 	return GHOST_kKeyEqual;
		case ',': 	return GHOST_kKeyComma;
		case '.': 	return GHOST_kKeyPeriod;
		case '/': 	return GHOST_kKeySlash;
		case ';': 	return GHOST_kKeySemicolon;
		case '\'': 	return GHOST_kKeyQuote;
		case '\\': 	return GHOST_kKeyBackslash;
		case '[': 	return GHOST_kKeyLeftBracket;
		case ']': 	return GHOST_kKeyRightBracket;
		case '`': 	return GHOST_kKeyAccentGrave;
		}
	}
	
	printf("GHOST: unknown key: %d %d\n", vk, rawCode);
	
	return GHOST_kKeyUnknown;
}

/***/

GHOST_SystemCarbon::GHOST_SystemCarbon() :
	m_modifierMask(0)
{
	m_displayManager = new GHOST_DisplayManagerCarbon ();
	GHOST_ASSERT(m_displayManager, "GHOST_SystemCarbon::GHOST_SystemCarbon(): m_displayManager==0\n");
	m_displayManager->initialize();

	UnsignedWide micros;
	::Microseconds(&micros);
	m_start_time = UnsignedWideToUInt64(micros)/1000;
	m_ignoreWindowSizedMessages = false;
}

GHOST_SystemCarbon::~GHOST_SystemCarbon()
{
}


GHOST_TUns64 GHOST_SystemCarbon::getMilliSeconds() const
{
	UnsignedWide micros;
	::Microseconds(&micros);
	UInt64 millis;
	millis = UnsignedWideToUInt64(micros);
	return (millis / 1000) - m_start_time;
}


GHOST_TUns8 GHOST_SystemCarbon::getNumDisplays() const
{
	// We do not support multiple monitors at the moment
	return 1;
}


void GHOST_SystemCarbon::getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const
{
	BitMap screenBits;
    Rect bnds = GetQDGlobalsScreenBits(&screenBits)->bounds;
	width = bnds.right - bnds.left;
	height = bnds.bottom - bnds.top;
}


GHOST_IWindow* GHOST_SystemCarbon::createWindow(
	const STR_String& title, 
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	bool stereoVisual
)
{
    GHOST_IWindow* window = 0;
    window = new GHOST_WindowCarbon (title, left, top, width, height, state, type);
    if (window) {
        if (window->getValid()) {
            // Store the pointer to the window 
            GHOST_ASSERT(m_windowManager, "m_windowManager not initialized");
            m_windowManager->addWindow(window);
            m_windowManager->setActiveWindow(window);
            pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
        }
        else {
			GHOST_PRINT("GHOST_SystemCarbon::createWindow(): window invalid\n");
            delete window;
            window = 0;
        }
    }
	else {
		GHOST_PRINT("GHOST_SystemCarbon::createWindow(): could not create window\n");
	}
    return window;
}


bool GHOST_SystemCarbon::processEvents(bool waitForEvent)
{
	bool anyProcessed = false;
	EventRef event;
	
	do {
		GHOST_TimerManager* timerMgr = getTimerManager();

		if (waitForEvent) {
			GHOST_TUns64 curtime = getMilliSeconds();
			GHOST_TUns64 next = timerMgr->nextFireTime();
			double timeOut;
			
			if (next == GHOST_kFireTimeNever) {
				timeOut = kEventDurationForever;
			} else {
				if (next<=curtime)
					timeOut = 0.0;
				else
					timeOut = (double) (next - getMilliSeconds())/1000.0;
			}
			
			::ReceiveNextEvent(0, NULL, timeOut, false, &event);
		}
		
		if (timerMgr->fireTimers(getMilliSeconds())) {
			anyProcessed = true;
		}

		if (getFullScreen()) {
			// Check if the full-screen window is dirty
			GHOST_IWindow* window = m_windowManager->getFullScreenWindow();
			if (((GHOST_WindowCarbon*)window)->getFullScreenDirty()) {
				pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window) );
				anyProcessed = true;
			}
		}

		while (::ReceiveNextEvent(0, NULL, 0, true, &event)==noErr) {
			OSStatus status= ::SendEventToEventTarget(event, ::GetEventDispatcherTarget());
			if (status==noErr) {
				anyProcessed = true;
			} else {
				UInt32 i= ::GetEventClass(event);
				
					/* Ignore 'cgs ' class, no documentation on what they
					 * are, but we get a lot of them
					 */
				if (i!='cgs ') {
					//printf("Missed - Class: '%.4s', Kind: %d\n", &i, ::GetEventKind(event));
				}
			}
			::ReleaseEvent(event);
		}
	} while (waitForEvent && !anyProcessed);
	
    return anyProcessed;
}
	

GHOST_TSuccess GHOST_SystemCarbon::getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const
{
    Point mouseLoc;
    // Get the position of the mouse in the active port
    ::GetGlobalMouse(&mouseLoc);
    // Convert the coordinates to screen coordinates
    x = (GHOST_TInt32)mouseLoc.h;
    y = (GHOST_TInt32)mouseLoc.v;
    return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_SystemCarbon::setCursorPosition(GHOST_TInt32 /*x*/, GHOST_TInt32 /*y*/) const
{
    // Not supported in Carbon!
    return GHOST_kFailure;
}


GHOST_TSuccess GHOST_SystemCarbon::getModifierKeys(GHOST_ModifierKeys& keys) const
{
    UInt32 modifiers = ::GetCurrentKeyModifiers();

    keys.set(GHOST_kModifierKeyCommand, (modifiers & cmdKey) ? true : false);
    keys.set(GHOST_kModifierKeyLeftAlt, (modifiers & optionKey) ? true : false);
    keys.set(GHOST_kModifierKeyLeftShift, (modifiers & shiftKey) ? true : false);
    keys.set(GHOST_kModifierKeyLeftControl, (modifiers & controlKey) ? true : false);
	
    return GHOST_kSuccess;
}

	/* XXX, incorrect for multibutton mice */
GHOST_TSuccess GHOST_SystemCarbon::getButtons(GHOST_Buttons& buttons) const
{
    Boolean theOnlyButtonIsDown = ::Button();
    buttons.clear();
    buttons.set(GHOST_kButtonMaskLeft, theOnlyButtonIsDown);
    return GHOST_kSuccess;
}

static bool g_hasFirstFile = false;
static char g_firstFileBuf[512];

extern "C" int GHOST_HACK_getFirstFile(char buf[512]) { 
	if (g_hasFirstFile) {
		strcpy(buf, g_firstFileBuf);
		return 1;
	} else {
		return 0; 
	}
}

OSErr GHOST_SystemCarbon::sAEHandlerLaunch(const AppleEvent *event, AppleEvent *reply, SInt32 refCon)
{
	//GHOST_SystemCarbon* sys = (GHOST_SystemCarbon*) refCon;
	
	return noErr;
}

OSErr GHOST_SystemCarbon::sAEHandlerOpenDocs(const AppleEvent *event, AppleEvent *reply, SInt32 refCon)
{
	//GHOST_SystemCarbon* sys = (GHOST_SystemCarbon*) refCon;
	AEDescList docs;
	SInt32 ndocs;
	OSErr err;

	err = AEGetParamDesc(event, keyDirectObject, typeAEList, &docs);
	if (err != noErr)  return err;

	err = AECountItems(&docs, &ndocs);
	if (err==noErr) {
		int i;
	
		for (i=0; i<ndocs; i++) {
			FSSpec fss;
			AEKeyword kwd;
			DescType actType;
			Size actSize;
		
			err = AEGetNthPtr(&docs, i+1, typeFSS, &kwd, &actType, &fss, sizeof(fss), &actSize);
			if (err!=noErr)
				break;
		
			if (i==0) {
				FSRef fsref;
				
				if (FSpMakeFSRef(&fss, &fsref)!=noErr)
					break;
				if (FSRefMakePath(&fsref, (UInt8*) g_firstFileBuf, sizeof(g_firstFileBuf))!=noErr)
					break;

				g_hasFirstFile = true;
			}
		}
	}
	
	AEDisposeDesc(&docs);
	
	return err;
}

OSErr GHOST_SystemCarbon::sAEHandlerPrintDocs(const AppleEvent *event, AppleEvent *reply, SInt32 refCon)
{
	//GHOST_SystemCarbon* sys = (GHOST_SystemCarbon*) refCon;
	
	return noErr;
}

OSErr GHOST_SystemCarbon::sAEHandlerQuit(const AppleEvent *event, AppleEvent *reply, SInt32 refCon)
{
	GHOST_SystemCarbon* sys = (GHOST_SystemCarbon*) refCon;
	
	sys->pushEvent( new GHOST_Event(sys->getMilliSeconds(), GHOST_kEventQuit, NULL) );
	
	return noErr;
}


GHOST_TSuccess GHOST_SystemCarbon::init()
{
    GHOST_TSuccess success = GHOST_System::init();
    if (success) {
		/*
         * Initialize the cursor to the standard arrow shape (so that we can change it later on).
         * This initializes the cursor's visibility counter to 0.
         */
        ::InitCursor();

		MenuRef windMenu;
		::CreateStandardWindowMenu(0, &windMenu);
		::InsertMenu(windMenu, 0);
		::DrawMenuBar();

        ::InstallApplicationEventHandler(sEventHandlerProc, GetEventTypeCount(kEvents), kEvents, this, &m_handler);
		
		::AEInstallEventHandler(kCoreEventClass, kAEOpenApplication, sAEHandlerLaunch, (SInt32) this, false);
		::AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, sAEHandlerOpenDocs, (SInt32) this, false);
		::AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments, sAEHandlerPrintDocs, (SInt32) this, false);
		::AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, sAEHandlerQuit, (SInt32) this, false);
    }
    return success;
}


GHOST_TSuccess GHOST_SystemCarbon::exit()
{
    return GHOST_System::exit();
}


OSStatus GHOST_SystemCarbon::handleWindowEvent(EventRef event)
{
	GHOST_WindowCarbon *window;
	
	if (!getFullScreen()) {
		WindowRef windowref;
		::GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &windowref);
		window = (GHOST_WindowCarbon*) ::GetWRefCon(windowref);
		
		if (validWindow(window)) {
			switch(::GetEventKind(event)) 
			{
				case kEventWindowClose:
					pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window) );
					break;
				case kEventWindowActivated:
					m_windowManager->setActiveWindow(window);
					window->loadCursor(window->getCursorVisibility(), window->getCursorShape());
					pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window) );
					break;
				case kEventWindowDeactivated:
					m_windowManager->setWindowInactive(window);
					pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowDeactivate, window) );
					break;
				case kEventWindowUpdate:
					//if (getFullScreen()) GHOST_PRINT("GHOST_SystemCarbon::handleWindowEvent(): full-screen update event\n");
					pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window) );
					break;
				case kEventWindowBoundsChanged:
					if (!m_ignoreWindowSizedMessages)
					{
						window->updateDrawingContext();
						pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window) );
					}
					break;
			}
		}
	}
	//else {
		//window = (GHOST_WindowCarbon*) m_windowManager->getFullScreenWindow();
		//GHOST_PRINT("GHOST_SystemCarbon::handleWindowEvent(): full-screen window event, " << window << "\n");
		//::RemoveEventFromQueue(::GetMainEventQueue(), event);
	//}
	
	return noErr;
}

OSStatus GHOST_SystemCarbon::handleMouseEvent(EventRef event)
{
	GHOST_IWindow* window = m_windowManager->getActiveWindow();
	UInt32 kind = ::GetEventKind(event);
	
	switch (kind)
    {
		case kEventMouseDown:
		case kEventMouseUp:
				// Handle Mac application responsibilities
			if ((kind == kEventMouseDown) && handleMouseDown(event)) {
				;
			} else {
				GHOST_TEventType type = (kind == kEventMouseDown) ? GHOST_kEventButtonDown : GHOST_kEventButtonUp;
				EventMouseButton button;
				
					/* Window still gets mouse up after command-H */
				if (window) {
					::GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL, sizeof(button), NULL, &button);
					pushEvent(new GHOST_EventButton(getMilliSeconds(), type, window, convertButton(button)));
				}
			}
            break;
			
		case kEventMouseMoved:
        case kEventMouseDragged:
			Point mousePos;
			if (window) {
				::GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &mousePos);
				pushEvent(new GHOST_EventCursor(getMilliSeconds(), GHOST_kEventCursorMove, window, mousePos.h, mousePos.v));
			}
            break;

		case kEventMouseWheelMoved:
			{
				OSStatus status;
				//UInt32 modifiers;
				EventMouseWheelAxis axis;
				SInt32 delta;
				//status = ::GetEventParameter(event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(modifiers), NULL, &modifiers);
				//GHOST_ASSERT(status == noErr, "GHOST_SystemCarbon::handleMouseEvent(): GetEventParameter() failed");
				status = ::GetEventParameter(event, kEventParamMouseWheelAxis, typeMouseWheelAxis, NULL, sizeof(axis), NULL, &axis);
				GHOST_ASSERT(status == noErr, "GHOST_SystemCarbon::handleMouseEvent(): GetEventParameter() failed");
				status = ::GetEventParameter(event, kEventParamMouseWheelDelta, typeLongInteger, NULL, sizeof(delta), NULL, &delta);
				GHOST_ASSERT(status == noErr, "GHOST_SystemCarbon::handleMouseEvent(): GetEventParameter() failed");
				if (axis == kEventMouseWheelAxisY)
				{
					pushEvent(new GHOST_EventWheel(getMilliSeconds(), GHOST_kEventWheel, window, delta));
				}
			}
			break;
		}
	
	return noErr;
}


OSStatus GHOST_SystemCarbon::handleKeyEvent(EventRef event)
{
	GHOST_IWindow* window = m_windowManager->getActiveWindow();
	UInt32 kind = ::GetEventKind(event);
	UInt32 modifiers;
	UInt32 rawCode;
	GHOST_TKey key;
	char ascii;
	
		/* Can happen, very rarely - seems to only be when command-H makes
		 * the window go away and we still get an HKey up. 
		 */
	if (!window) {
		::GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &rawCode);
		key = convertKey(rawCode);
		return noErr;
	}
	
	switch (kind) {
	case kEventRawKeyDown: 
	case kEventRawKeyRepeat: 
	case kEventRawKeyUp: 
		::GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(UInt32), NULL, &rawCode);
		::GetEventParameter(event, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(char), NULL, &ascii);
		key = convertKey(rawCode);
		if (key!=GHOST_kKeyUnknown) {
			GHOST_TEventType type;
			if (kind == kEventRawKeyDown) {
				type = GHOST_kEventKeyDown;
			} else if (kind == kEventRawKeyRepeat) { 
				type = GHOST_kEventKeyDown;  /* XXX, fixme */
			} else {
				type = GHOST_kEventKeyUp;
			}
			pushEvent( new GHOST_EventKey( getMilliSeconds(), type, window, key, ascii) );
		}
		break;

	case kEventRawKeyModifiersChanged: 
			/* ugh */
		::GetEventParameter(event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(UInt32), NULL, &modifiers);
		if ((modifiers & shiftKey) != (m_modifierMask & shiftKey)) {
			pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & shiftKey)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftShift) );
		}
		if ((modifiers & controlKey) != (m_modifierMask & controlKey)) {
			pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & controlKey)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftControl) );
		}
		if ((modifiers & optionKey) != (m_modifierMask & optionKey)) {
			pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & optionKey)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftAlt) );
		}
		if ((modifiers & cmdKey) != (m_modifierMask & cmdKey)) {
			pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & cmdKey)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyCommand) );
		}
		
		m_modifierMask = modifiers;
		break;
	}
	
	return noErr;
}


bool GHOST_SystemCarbon::handleMouseDown(EventRef event)
{
	WindowPtr			window;
	short				part;
	BitMap 				screenBits;
    bool 				handled = true;
    GHOST_IWindow* 		ghostWindow;
    Point 				mousePos = {0 , 0};
	
	::GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &mousePos);
	
	part = ::FindWindow(mousePos, &window);
	ghostWindow = (GHOST_IWindow*) ::GetWRefCon(window);
	
	switch (part) {
		case inMenuBar:
			handleMenuCommand(::MenuSelect(mousePos));
			break;
			
		case inDrag:
			/*
			 * The DragWindow() routine creates a lot of kEventWindowBoundsChanged
			 * events. By setting m_ignoreWindowSizedMessages these are suppressed.
			 * @see GHOST_SystemCarbon::handleWindowEvent(EventRef event)
			 */
			GHOST_ASSERT(validWindow(ghostWindow), "GHOST_SystemCarbon::handleMouseDown: invalid window");
			m_ignoreWindowSizedMessages = true;
			::DragWindow(window, mousePos, &GetQDGlobalsScreenBits(&screenBits)->bounds);
			m_ignoreWindowSizedMessages = false;
			break;
		
		case inContent:
			if (window != ::FrontWindow()) {
				::SelectWindow(window);
				/*
				 * We add a mouse down event on the newly actived window
				 */		
				//GHOST_PRINT("GHOST_SystemCarbon::handleMouseDown(): adding mouse down event, " << ghostWindow << "\n");
				EventMouseButton button;
				::GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL, sizeof(button), NULL, &button);
				pushEvent(new GHOST_EventButton(getMilliSeconds(), GHOST_kEventButtonDown, ghostWindow, convertButton(button)));
			} else {
				handled = false;
			}
			break;
			
		case inGoAway:
			GHOST_ASSERT(ghostWindow, "GHOST_SystemCarbon::handleMouseEvent: ghostWindow==0");
			if (::TrackGoAway(window, mousePos))
			{
				// todo: add option-close, because itÕs in the HIG
				// if (event.modifiers & optionKey) {
					// Close the clean documents, others will be confirmed one by one.
				//}
				// else {
				pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, ghostWindow));
				//}
			}
			break;
			
		case inGrow:
			GHOST_ASSERT(ghostWindow, "GHOST_SystemCarbon::handleMouseEvent: ghostWindow==0");
			::ResizeWindow(window, mousePos, NULL, NULL);
			break;
			
		case inZoomIn:
		case inZoomOut:
			GHOST_ASSERT(ghostWindow, "GHOST_SystemCarbon::handleMouseEvent: ghostWindow==0");
			if (::TrackBox(window, mousePos, part)) {
				::ZoomWindow(window, part, true);
			}
			break;

		default:
			handled = false;
			break;
	}
	
	return handled;
}


bool GHOST_SystemCarbon::handleMenuCommand(GHOST_TInt32 menuResult)
{
	short		menuID;
	short		menuItem;
	UInt32		command;
	bool		handled;
	OSErr		err;
	
	menuID = HiWord(menuResult);
	menuItem = LoWord(menuResult);

	err = ::GetMenuItemCommandID(::GetMenuHandle(menuID), menuItem, &command);

	handled = false;
	
	if (err || command == 0) {
	}
	else {
		switch(command) {
		}
	}

	::HiliteMenu(0);
    return handled;
}

OSStatus GHOST_SystemCarbon::sEventHandlerProc(EventHandlerCallRef handler, EventRef event, void* userData)
{
	GHOST_SystemCarbon* sys = (GHOST_SystemCarbon*) userData;
    OSStatus err = eventNotHandledErr;

    switch (::GetEventClass(event))
    {
		case kEventClassAppleEvent:
			EventRecord eventrec;
			if (ConvertEventRefToEventRecord(event, &eventrec)) {
				err = AEProcessAppleEvent(&eventrec);
			}
			break;
        case kEventClassMouse:
            err = sys->handleMouseEvent(event);
            break;
		case kEventClassWindow:
			err = sys->handleWindowEvent(event);
			break;
		case kEventClassKeyboard:
			err = sys->handleKeyEvent(event);
			break;
    }

    return err;
}
