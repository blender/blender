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
 * Contributor(s):	Maarten Gribnau 05/2001
 *					Damien Plisson 09/2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#import <Cocoa/Cocoa.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "GHOST_SystemCocoa.h"

#include "GHOST_DisplayManagerCocoa.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventWheel.h"
#include "GHOST_EventNDOF.h"

#include "GHOST_TimerManager.h"
#include "GHOST_TimerTask.h"
#include "GHOST_WindowManager.h"
#include "GHOST_WindowCocoa.h"
#include "GHOST_NDOFManager.h"
#include "AssertMacros.h"

#pragma mark KeyMap, mouse converters

//TODO: remove (kept as reminder to implement window events)
/*
const EventTypeSpec	kEvents[] =
{
	{ kEventClassAppleEvent, kEventAppleEvent },

//	{ kEventClassApplication, kEventAppActivated },
//	{ kEventClassApplication, kEventAppDeactivated },
	
	{ kEventClassKeyboard, kEventRawKeyDown },
	{ kEventClassKeyboard, kEventRawKeyRepeat },
	{ kEventClassKeyboard, kEventRawKeyUp },
	{ kEventClassKeyboard, kEventRawKeyModifiersChanged },
	
	{ kEventClassMouse, kEventMouseDown },
	{ kEventClassMouse, kEventMouseUp },
	{ kEventClassMouse, kEventMouseMoved },
	{ kEventClassMouse, kEventMouseDragged },
	{ kEventClassMouse, kEventMouseWheelMoved },
	
	{ kEventClassWindow, kEventWindowClickZoomRgn } ,  // for new zoom behaviour  
	{ kEventClassWindow, kEventWindowZoom },  // for new zoom behaviour  
	{ kEventClassWindow, kEventWindowExpand } ,  // for new zoom behaviour 
	{ kEventClassWindow, kEventWindowExpandAll },  // for new zoom behaviour 

	{ kEventClassWindow, kEventWindowClose },
	{ kEventClassWindow, kEventWindowActivated },
	{ kEventClassWindow, kEventWindowDeactivated },
	{ kEventClassWindow, kEventWindowUpdate },
	{ kEventClassWindow, kEventWindowBoundsChanged },
	
	{ kEventClassBlender, kEventBlenderNdofAxis },
	{ kEventClassBlender, kEventBlenderNdofButtons }
	
	
	
};*/

static GHOST_TButtonMask convertButton(EventMouseButton button)
{
	switch (button) {
		case 0:
			return GHOST_kButtonMaskLeft;
		case 1:
			return GHOST_kButtonMaskRight;
		case 2:
			return GHOST_kButtonMaskMiddle;
		case 3:
			return GHOST_kButtonMaskButton4;
		case 4:
			return GHOST_kButtonMaskButton5;
		default:
			return GHOST_kButtonMaskLeft;
	}
}

/**
 * Converts Mac rawkey codes (same for Cocoa & Carbon)
 * into GHOST key codes
 * @param rawCode The raw physical key code
 * @param recvChar the character ignoring modifiers (except for shift)
 * @return Ghost key code
 */
static GHOST_TKey convertKey(int rawCode, unichar recvChar) 
{	
	
	//printf("\nrecvchar %c 0x%x",recvChar,recvChar);
	switch (rawCode) {
		/*Physical keycodes not used due to map changes in int'l keyboards
		case kVK_ANSI_A:	return GHOST_kKeyA;
		case kVK_ANSI_B:	return GHOST_kKeyB;
		case kVK_ANSI_C:	return GHOST_kKeyC;
		case kVK_ANSI_D:	return GHOST_kKeyD;
		case kVK_ANSI_E:	return GHOST_kKeyE;
		case kVK_ANSI_F:	return GHOST_kKeyF;
		case kVK_ANSI_G:	return GHOST_kKeyG;
		case kVK_ANSI_H:	return GHOST_kKeyH;
		case kVK_ANSI_I:	return GHOST_kKeyI;
		case kVK_ANSI_J:	return GHOST_kKeyJ;
		case kVK_ANSI_K:	return GHOST_kKeyK;
		case kVK_ANSI_L:	return GHOST_kKeyL;
		case kVK_ANSI_M:	return GHOST_kKeyM;
		case kVK_ANSI_N:	return GHOST_kKeyN;
		case kVK_ANSI_O:	return GHOST_kKeyO;
		case kVK_ANSI_P:	return GHOST_kKeyP;
		case kVK_ANSI_Q:	return GHOST_kKeyQ;
		case kVK_ANSI_R:	return GHOST_kKeyR;
		case kVK_ANSI_S:	return GHOST_kKeyS;
		case kVK_ANSI_T:	return GHOST_kKeyT;
		case kVK_ANSI_U:	return GHOST_kKeyU;
		case kVK_ANSI_V:	return GHOST_kKeyV;
		case kVK_ANSI_W:	return GHOST_kKeyW;
		case kVK_ANSI_X:	return GHOST_kKeyX;
		case kVK_ANSI_Y:	return GHOST_kKeyY;
		case kVK_ANSI_Z:	return GHOST_kKeyZ;*/
		
		/* Numbers keys mapped to handle some int'l keyboard (e.g. French)*/
		case kVK_ISO_Section: return	GHOST_kKeyUnknown;
		case kVK_ANSI_1:	return GHOST_kKey1;
		case kVK_ANSI_2:	return GHOST_kKey2;
		case kVK_ANSI_3:	return GHOST_kKey3;
		case kVK_ANSI_4:	return GHOST_kKey4;
		case kVK_ANSI_5:	return GHOST_kKey5;
		case kVK_ANSI_6:	return GHOST_kKey6;
		case kVK_ANSI_7:	return GHOST_kKey7;
		case kVK_ANSI_8:	return GHOST_kKey8;
		case kVK_ANSI_9:	return GHOST_kKey9;
		case kVK_ANSI_0:	return GHOST_kKey0;
	
		case kVK_ANSI_Keypad0:			return GHOST_kKeyNumpad0;
		case kVK_ANSI_Keypad1:			return GHOST_kKeyNumpad1;
		case kVK_ANSI_Keypad2:			return GHOST_kKeyNumpad2;
		case kVK_ANSI_Keypad3:			return GHOST_kKeyNumpad3;
		case kVK_ANSI_Keypad4:			return GHOST_kKeyNumpad4;
		case kVK_ANSI_Keypad5:			return GHOST_kKeyNumpad5;
		case kVK_ANSI_Keypad6:			return GHOST_kKeyNumpad6;
		case kVK_ANSI_Keypad7:			return GHOST_kKeyNumpad7;
		case kVK_ANSI_Keypad8:			return GHOST_kKeyNumpad8;
		case kVK_ANSI_Keypad9:			return GHOST_kKeyNumpad9;
		case kVK_ANSI_KeypadDecimal: 	return GHOST_kKeyNumpadPeriod;
		case kVK_ANSI_KeypadEnter:		return GHOST_kKeyNumpadEnter;
		case kVK_ANSI_KeypadPlus:		return GHOST_kKeyNumpadPlus;
		case kVK_ANSI_KeypadMinus:		return GHOST_kKeyNumpadMinus;
		case kVK_ANSI_KeypadMultiply: 	return GHOST_kKeyNumpadAsterisk;
		case kVK_ANSI_KeypadDivide: 	return GHOST_kKeyNumpadSlash;
		case kVK_ANSI_KeypadClear:		return GHOST_kKeyUnknown;

		case kVK_F1:				return GHOST_kKeyF1;
		case kVK_F2:				return GHOST_kKeyF2;
		case kVK_F3:				return GHOST_kKeyF3;
		case kVK_F4:				return GHOST_kKeyF4;
		case kVK_F5:				return GHOST_kKeyF5;
		case kVK_F6:				return GHOST_kKeyF6;
		case kVK_F7:				return GHOST_kKeyF7;
		case kVK_F8:				return GHOST_kKeyF8;
		case kVK_F9:				return GHOST_kKeyF9;
		case kVK_F10:				return GHOST_kKeyF10;
		case kVK_F11:				return GHOST_kKeyF11;
		case kVK_F12:				return GHOST_kKeyF12;
		case kVK_F13:				return GHOST_kKeyF13;
		case kVK_F14:				return GHOST_kKeyF14;
		case kVK_F15:				return GHOST_kKeyF15;
		case kVK_F16:				return GHOST_kKeyF16;
		case kVK_F17:				return GHOST_kKeyF17;
		case kVK_F18:				return GHOST_kKeyF18;
		case kVK_F19:				return GHOST_kKeyF19;
		case kVK_F20:				return GHOST_kKeyF20;
			
		case kVK_UpArrow:			return GHOST_kKeyUpArrow;
		case kVK_DownArrow:			return GHOST_kKeyDownArrow;
		case kVK_LeftArrow:			return GHOST_kKeyLeftArrow;
		case kVK_RightArrow:		return GHOST_kKeyRightArrow;
			
		case kVK_Return:			return GHOST_kKeyEnter;
		case kVK_Delete:			return GHOST_kKeyBackSpace;
		case kVK_ForwardDelete:		return GHOST_kKeyDelete;
		case kVK_Escape:			return GHOST_kKeyEsc;
		case kVK_Tab:				return GHOST_kKeyTab;
		case kVK_Space:				return GHOST_kKeySpace;
			
		case kVK_Home:				return GHOST_kKeyHome;
		case kVK_End:				return GHOST_kKeyEnd;
		case kVK_PageUp:			return GHOST_kKeyUpPage;
		case kVK_PageDown:			return GHOST_kKeyDownPage;
			
		/*case kVK_ANSI_Minus:		return GHOST_kKeyMinus;
		case kVK_ANSI_Equal:		return GHOST_kKeyEqual;
		case kVK_ANSI_Comma:		return GHOST_kKeyComma;
		case kVK_ANSI_Period:		return GHOST_kKeyPeriod;
		case kVK_ANSI_Slash:		return GHOST_kKeySlash;
		case kVK_ANSI_Semicolon: 	return GHOST_kKeySemicolon;
		case kVK_ANSI_Quote:		return GHOST_kKeyQuote;
		case kVK_ANSI_Backslash: 	return GHOST_kKeyBackslash;
		case kVK_ANSI_LeftBracket: 	return GHOST_kKeyLeftBracket;
		case kVK_ANSI_RightBracket:	return GHOST_kKeyRightBracket;
		case kVK_ANSI_Grave:		return GHOST_kKeyAccentGrave;*/
			
		case kVK_VolumeUp:
		case kVK_VolumeDown:
		case kVK_Mute:
			return GHOST_kKeyUnknown;
			
		default:
			/*Then detect on character value for "remappable" keys in int'l keyboards*/
			if ((recvChar >= 'A') && (recvChar <= 'Z')) {
				return (GHOST_TKey) (recvChar - 'A' + GHOST_kKeyA);
			} else if ((recvChar >= 'a') && (recvChar <= 'z')) {
				return (GHOST_TKey) (recvChar - 'a' + GHOST_kKeyA);
			} else
			switch (recvChar) {
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
				default:
					return GHOST_kKeyUnknown;
			}
	}
	return GHOST_kKeyUnknown;
}

/* MacOSX returns a Roman charset with kEventParamKeyMacCharCodes
 * as defined here: http://developer.apple.com/documentation/mac/Text/Text-516.html
 * I am not sure how international this works...
 * For cross-platform convention, we'll use the Latin ascii set instead.
 * As defined at: http://www.ramsch.org/martin/uni/fmi-hp/iso8859-1.html
 * 
 */
static unsigned char convertRomanToLatin(unsigned char ascii)
{

	if(ascii<128) return ascii;
	
	switch(ascii) {
	case 128:	return 142;
	case 129:	return 143;
	case 130:	return 128;
	case 131:	return 201;
	case 132:	return 209;
	case 133:	return 214;
	case 134:	return 220;
	case 135:	return 225;
	case 136:	return 224;
	case 137:	return 226;
	case 138:	return 228;
	case 139:	return 227;
	case 140:	return 229;
	case 141:	return 231;
	case 142:	return 233;
	case 143:	return 232;
	case 144:	return 234;
	case 145:	return 235;
	case 146:	return 237;
	case 147:	return 236;
	case 148:	return 238;
	case 149:	return 239;
	case 150:	return 241;
	case 151:	return 243;
	case 152:	return 242;
	case 153:	return 244;
	case 154:	return 246;
	case 155:	return 245;
	case 156:	return 250;
	case 157:	return 249;
	case 158:	return 251;
	case 159:	return 252;
	case 160:	return 0;
	case 161:	return 176;
	case 162:	return 162;
	case 163:	return 163;
	case 164:	return 167;
	case 165:	return 183;
	case 166:	return 182;
	case 167:	return 223;
	case 168:	return 174;
	case 169:	return 169;
	case 170:	return 174;
	case 171:	return 180;
	case 172:	return 168;
	case 173:	return 0;
	case 174:	return 198;
	case 175:	return 216;
	case 176:	return 0;
	case 177:	return 177;
	case 178:	return 0;
	case 179:	return 0;
	case 180:	return 165;
	case 181:	return 181;
	case 182:	return 0;
	case 183:	return 0;
	case 184:	return 215;
	case 185:	return 0;
	case 186:	return 0;
	case 187:	return 170;
	case 188:	return 186;
	case 189:	return 0;
	case 190:	return 230;
	case 191:	return 248;
	case 192:	return 191;
	case 193:	return 161;
	case 194:	return 172;
	case 195:	return 0;
	case 196:	return 0;
	case 197:	return 0;
	case 198:	return 0;
	case 199:	return 171;
	case 200:	return 187;
	case 201:	return 201;
	case 202:	return 0;
	case 203:	return 192;
	case 204:	return 195;
	case 205:	return 213;
	case 206:	return 0;
	case 207:	return 0;
	case 208:	return 0;
	case 209:	return 0;
	case 210:	return 0;
	
	case 214:	return 247;

	case 229:	return 194;
	case 230:	return 202;
	case 231:	return 193;
	case 232:	return 203;
	case 233:	return 200;
	case 234:	return 205;
	case 235:	return 206;
	case 236:	return 207;
	case 237:	return 204;
	case 238:	return 211;
	case 239:	return 212;
	case 240:	return 0;
	case 241:	return 210;
	case 242:	return 218;
	case 243:	return 219;
	case 244:	return 217;
	case 245:	return 0;
	case 246:	return 0;
	case 247:	return 0;
	case 248:	return 0;
	case 249:	return 0;
	case 250:	return 0;

	
		default: return 0;
	}

}

#define FIRSTFILEBUFLG 512
static bool g_hasFirstFile = false;
static char g_firstFileBuf[512];

//TODO:Need to investigate this. Function called too early in creator.c to have g_hasFirstFile == true
extern "C" int GHOST_HACK_getFirstFile(char buf[FIRSTFILEBUFLG]) { 
	if (g_hasFirstFile) {
		strncpy(buf, g_firstFileBuf, FIRSTFILEBUFLG - 1);
		buf[FIRSTFILEBUFLG - 1] = '\0';
		return 1;
	} else {
		return 0; 
	}
}


#pragma mark Cocoa objects

/**
 * CocoaAppDelegate
 * ObjC object to capture applicationShouldTerminate, and send quit event
 **/
@interface CocoaAppDelegate : NSObject {
	GHOST_SystemCocoa *systemCocoa;
}
-(void)setSystemCocoa:(GHOST_SystemCocoa *)sysCocoa;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (void)applicationWillTerminate:(NSNotification *)aNotification;
@end

@implementation CocoaAppDelegate : NSObject
-(void)setSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
{
	systemCocoa = sysCocoa;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	//TODO: implement graceful termination through Cocoa mechanism to avoid session log off to be cancelled
	//Note that Cmd+Q is already handled by keyhandler
    if (systemCocoa->handleQuitRequest() == GHOST_kExitNow)
		return NSTerminateCancel;//NSTerminateNow;
	else
		return NSTerminateCancel;
}

// To avoid cancelling a log off process, we must use Cocoa termination process
// And this function is the only chance to perform clean up
// So WM_exit needs to be called directly, as the event loop will never run before termination
- (void)applicationWillTerminate:(NSNotification *)aNotification
{
	/*G.afbreek = 0; //Let Cocoa perform the termination at the end
	WM_exit(C);*/
}
@end



#pragma mark initialization/finalization


GHOST_SystemCocoa::GHOST_SystemCocoa()
{
	m_modifierMask =0;
	m_pressedMouseButtons =0;
	m_displayManager = new GHOST_DisplayManagerCocoa ();
	GHOST_ASSERT(m_displayManager, "GHOST_SystemCocoa::GHOST_SystemCocoa(): m_displayManager==0\n");
	m_displayManager->initialize();

	//NSEvent timeStamp is given in system uptime, state start date is boot time
	//FIXME : replace by Cocoa equivalent
	int mib[2];
	struct timeval boottime;
	size_t len;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	len = sizeof(struct timeval);
	
	sysctl(mib, 2, &boottime, &len, NULL, 0);
	m_start_time = ((boottime.tv_sec*1000)+(boottime.tv_usec/1000));
	
	m_ignoreWindowSizedMessages = false;
}

GHOST_SystemCocoa::~GHOST_SystemCocoa()
{
	NSAutoreleasePool* pool = (NSAutoreleasePool *)m_autoReleasePool;
	[pool drain];
}


GHOST_TSuccess GHOST_SystemCocoa::init()
{
	
    GHOST_TSuccess success = GHOST_System::init();
    if (success) {
		//ProcessSerialNumber psn;
		
		//Carbon stuff to move window & menu to foreground
		/*if (!GetCurrentProcess(&psn)) {
			TransformProcessType(&psn, kProcessTransformToForegroundApplication);
			SetFrontProcess(&psn);
		}*/
		
		m_autoReleasePool = [[NSAutoreleasePool alloc] init];
		if (NSApp == nil) {
			[NSApplication sharedApplication];
			
			if ([NSApp mainMenu] == nil) {
				NSMenu *mainMenubar = [[NSMenu alloc] init];
				NSMenuItem *menuItem;
				
				//Create the application menu
				NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"Blender"];
				
				[appMenu addItemWithTitle:@"About Blender" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
				[appMenu addItem:[NSMenuItem separatorItem]];
				
				menuItem = [appMenu addItemWithTitle:@"Hide Blender" action:@selector(hide:) keyEquivalent:@"h"];
				[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
				 
				menuItem = [appMenu addItemWithTitle:@"Hide others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
				[menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask | NSCommandKeyMask)];
				
				[appMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
				
				menuItem = [appMenu addItemWithTitle:@"Quit Blender" action:@selector(terminate:) keyEquivalent:@"q"];
				[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
				
				menuItem = [[NSMenuItem alloc] init];
				[menuItem setSubmenu:appMenu];
				
				[mainMenubar addItem:menuItem];
				[menuItem release];
				[appMenu release];
				
				//Create the window menu
				NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
				
				menuItem = [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
				[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
				
				[windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
				
				menuItem = [[NSMenuItem	alloc] init];
				[menuItem setSubmenu:windowMenu];
				
				[mainMenubar addItem:menuItem];
				[menuItem release];
				
				[NSApp setMainMenu:mainMenubar];
				[NSApp setWindowsMenu:windowMenu];
				[windowMenu release];
			}
			[NSApp finishLaunching];
		}
		if ([NSApp delegate] == nil) {
			CocoaAppDelegate *appDelegate = [[CocoaAppDelegate alloc] init];
			[appDelegate setSystemCocoa:this];
			[NSApp setDelegate:appDelegate];
		}
		
		
		/*
         * Initialize the cursor to the standard arrow shape (so that we can change it later on).
         * This initializes the cursor's visibility counter to 0.
         */
        /*::InitCursor();
		
		MenuRef windMenu;
		::CreateStandardWindowMenu(0, &windMenu);
		::InsertMenu(windMenu, 0);
		::DrawMenuBar();
		
        ::InstallApplicationEventHandler(sEventHandlerProc, GetEventTypeCount(kEvents), kEvents, this, &m_handler);
		
		::AEInstallEventHandler(kCoreEventClass, kAEOpenApplication, sAEHandlerLaunch, (SInt32) this, false);
		::AEInstallEventHandler(kCoreEventClass, kAEOpenDocuments, sAEHandlerOpenDocs, (SInt32) this, false);
		::AEInstallEventHandler(kCoreEventClass, kAEPrintDocuments, sAEHandlerPrintDocs, (SInt32) this, false);
		::AEInstallEventHandler(kCoreEventClass, kAEQuitApplication, sAEHandlerQuit, (SInt32) this, false);
		*/
    }
    return success;
}


#pragma mark window management

GHOST_TUns64 GHOST_SystemCocoa::getMilliSeconds() const
{
	//Cocoa equivalent exists in 10.6 ([[NSProcessInfo processInfo] systemUptime])
	int mib[2];
	struct timeval boottime;
	size_t len;
	
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	len = sizeof(struct timeval);
	
	sysctl(mib, 2, &boottime, &len, NULL, 0);

	return ((boottime.tv_sec*1000)+(boottime.tv_usec/1000));
}


GHOST_TUns8 GHOST_SystemCocoa::getNumDisplays() const
{
	//Note that OS X supports monitor hot plug
	// We do not support multiple monitors at the moment
	return [[NSScreen screens] count];
}


void GHOST_SystemCocoa::getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const
{
	//Get visible frame, that is frame excluding dock and top menu bar
	NSRect frame = [[NSScreen mainScreen] visibleFrame];
	
	//Returns max window contents (excluding title bar...)
	NSRect contentRect = [NSWindow contentRectForFrameRect:frame
												 styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)];
	
	width = contentRect.size.width;
	height = contentRect.size.height;
}


GHOST_IWindow* GHOST_SystemCocoa::createWindow(
	const STR_String& title, 
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	bool stereoVisual,
	const GHOST_TEmbedderWindowID parentWindow
)
{
    GHOST_IWindow* window = 0;
	
	//Get the available rect for including window contents
	NSRect frame = [[NSScreen mainScreen] visibleFrame];
	NSRect contentRect = [NSWindow contentRectForFrameRect:frame
												 styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)];
	
	//Ensures window top left is inside this available rect
	left = left > contentRect.origin.x ? left : contentRect.origin.x;
	top = top > contentRect.origin.y ? top : contentRect.origin.y;
	
	window = new GHOST_WindowCocoa (title, left, top, width, height, state, type);

    if (window) {
        if (window->getValid()) {
            // Store the pointer to the window 
            GHOST_ASSERT(m_windowManager, "m_windowManager not initialized");
            m_windowManager->addWindow(window);
            m_windowManager->setActiveWindow(window);
            pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
        }
        else {
			GHOST_PRINT("GHOST_SystemCocoa::createWindow(): window invalid\n");
            delete window;
            window = 0;
        }
    }
	else {
		GHOST_PRINT("GHOST_SystemCocoa::createWindow(): could not create window\n");
	}
    return window;
}

GHOST_TSuccess GHOST_SystemCocoa::beginFullScreen(const GHOST_DisplaySetting& setting, GHOST_IWindow** window, const bool stereoVisual)
{	
	GHOST_TSuccess success = GHOST_kFailure;

	//TODO: update this method
	// need yo make this Carbon all on 10.5 for fullscreen to work correctly
	CGCaptureAllDisplays();
	
	success = GHOST_System::beginFullScreen( setting, window, stereoVisual);
	
	if( success != GHOST_kSuccess ) {
			// fullscreen failed for other reasons, release
			CGReleaseAllDisplays();	
	}

	return success;
}

GHOST_TSuccess GHOST_SystemCocoa::endFullScreen(void)
{	
	//TODO: update this method
	CGReleaseAllDisplays();
	return GHOST_System::endFullScreen();
}


	

GHOST_TSuccess GHOST_SystemCocoa::getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const
{
    NSPoint mouseLoc = [NSEvent mouseLocation];
	
    // Convert the coordinates to screen coordinates
    x = (GHOST_TInt32)mouseLoc.x;
    y = (GHOST_TInt32)mouseLoc.y;
    return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_SystemCocoa::setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) const
{
	float xf=(float)x, yf=(float)y;

	CGAssociateMouseAndMouseCursorPosition(false);
	CGWarpMouseCursorPosition(CGPointMake(xf, yf));
	CGAssociateMouseAndMouseCursorPosition(true);

    return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_SystemCocoa::getModifierKeys(GHOST_ModifierKeys& keys) const
{
    NSUInteger modifiers = [[NSApp currentEvent] modifierFlags];
	//Direct query to modifierFlags can be used in 10.6

    keys.set(GHOST_kModifierKeyCommand, (modifiers & NSCommandKeyMask) ? true : false);
    keys.set(GHOST_kModifierKeyLeftAlt, (modifiers & NSAlternateKeyMask) ? true : false);
    keys.set(GHOST_kModifierKeyLeftShift, (modifiers & NSShiftKeyMask) ? true : false);
    keys.set(GHOST_kModifierKeyLeftControl, (modifiers & NSControlKeyMask) ? true : false);
	
    return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::getButtons(GHOST_Buttons& buttons) const
{
	buttons.clear();
    buttons.set(GHOST_kButtonMaskLeft, m_pressedMouseButtons & GHOST_kButtonMaskLeft);
	buttons.set(GHOST_kButtonMaskRight, m_pressedMouseButtons & GHOST_kButtonMaskRight);
	buttons.set(GHOST_kButtonMaskMiddle, m_pressedMouseButtons & GHOST_kButtonMaskMiddle);
	buttons.set(GHOST_kButtonMaskButton4, m_pressedMouseButtons & GHOST_kButtonMaskButton4);
	buttons.set(GHOST_kButtonMaskButton5, m_pressedMouseButtons & GHOST_kButtonMaskButton5);
    return GHOST_kSuccess;
}



#pragma mark Event handlers

/**
 * The event queue polling function
 */
bool GHOST_SystemCocoa::processEvents(bool waitForEvent)
{
	NSAutoreleasePool* pool = (NSAutoreleasePool*)m_autoReleasePool;
	//bool anyProcessed = false;
	NSEvent *event;
	
	//Reinit the AutoReleasePool
	//This is not done the typical Cocoa way (init at beginning of loop, and drain at the end)
	//to allow pool to work with other function calls outside this loop (but in same thread)
	[pool drain];
	m_autoReleasePool = [[NSAutoreleasePool alloc] init];
	
	//	SetMouseCoalescingEnabled(false, NULL);
	//TODO : implement timer ??
	
	/*do {
		GHOST_TimerManager* timerMgr = getTimerManager();
		
		 if (waitForEvent) {
		 GHOST_TUns64 next = timerMgr->nextFireTime();
		 double timeOut;
		 
		 if (next == GHOST_kFireTimeNever) {
		 timeOut = kEventDurationForever;
		 } else {
		 timeOut = (double)(next - getMilliSeconds())/1000.0;
		 if (timeOut < 0.0)
		 timeOut = 0.0;
		 }
		 
		 ::ReceiveNextEvent(0, NULL, timeOut, false, &event);
		 }
		 
		 if (timerMgr->fireTimers(getMilliSeconds())) {
		 anyProcessed = true;
		 }
		 
		 //TODO: check fullscreen redrawing issues
		 if (getFullScreen()) {
		 // Check if the full-screen window is dirty
		 GHOST_IWindow* window = m_windowManager->getFullScreenWindow();
		 if (((GHOST_WindowCarbon*)window)->getFullScreenDirty()) {
		 pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window) );
		 anyProcessed = true;
		 }
		 }*/
		
		do {
			event = [NSApp nextEventMatchingMask:NSAnyEventMask
									   untilDate:[NSDate distantPast]
										  inMode:NSDefaultRunLoopMode
										 dequeue:YES];
			if (event==nil)
				break;
			
			//anyProcessed = true;
			
			switch ([event type]) {
				case NSKeyDown:
				case NSKeyUp:
				case NSFlagsChanged:
					handleKeyEvent(event);
					
					/* Support system-wide keyboard shortcuts, like Exposé, ...) =>included in always NSApp sendEvent */
					/*		if (([event modifierFlags] & NSCommandKeyMask) || [event type] == NSFlagsChanged) {
					 [NSApp sendEvent:event];
					 }*/
					break;
					
				case NSLeftMouseDown:
				case NSLeftMouseUp:
				case NSRightMouseDown:
				case NSRightMouseUp:
				case NSMouseMoved:
				case NSLeftMouseDragged:
				case NSRightMouseDragged:
				case NSScrollWheel:
				case NSOtherMouseDown:
				case NSOtherMouseUp:
				case NSOtherMouseDragged:				
					handleMouseEvent(event);
					break;
					
				case NSTabletPoint:
				case NSTabletProximity:
					handleTabletEvent(event);
					break;
					
					/* Trackpad features, will need OS X 10.6 for implementation
					 case NSEventTypeGesture:
					 case NSEventTypeMagnify:
					 case NSEventTypeSwipe:
					 case NSEventTypeRotate:
					 case NSEventTypeBeginGesture:
					 case NSEventTypeEndGesture:
					 break; */
					
					/*Unused events
					 NSMouseEntered       = 8,
					 NSMouseExited        = 9,
					 NSAppKitDefined      = 13,
					 NSSystemDefined      = 14,
					 NSApplicationDefined = 15,
					 NSPeriodic           = 16,
					 NSCursorUpdate       = 17,*/
					
				default:
					break;
			}
			//Resend event to NSApp to ensure Mac wide events are handled
			[NSApp sendEvent:event];
		} while (event!= nil);		
	//} while (waitForEvent && !anyProcessed); Needed only for timer implementation
	
	
    return true; //anyProcessed;
}

//TODO: To be called from NSWindow delegate
GHOST_TSuccess GHOST_SystemCocoa::handleWindowEvent(void *eventPtr)
{
	/*WindowRef windowRef;
	GHOST_WindowCocoa *window;
	
	// Check if the event was send to a GHOST window
	::GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &windowRef);
	window = (GHOST_WindowCarbon*) ::GetWRefCon(windowRef);
	if (!validWindow(window)) {
		return err;
	}

	//if (!getFullScreen()) {
		err = noErr;
		switch([event ]) 
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
			default:
				err = eventNotHandledErr;
				break;
		}
//	}
	//else {
		//window = (GHOST_WindowCarbon*) m_windowManager->getFullScreenWindow();
		//GHOST_PRINT("GHOST_SystemCarbon::handleWindowEvent(): full-screen window event, " << window << "\n");
		//::RemoveEventFromQueue(::GetMainEventQueue(), event);
	//}
	*/
	return GHOST_kSuccess;
}

GHOST_TUns8 GHOST_SystemCocoa::handleQuitRequest()
{
	//Check open windows if some changes are not saved
	if (m_windowManager->getAnyModifiedState())
	{
		int shouldQuit = NSRunAlertPanel(@"Exit Blender", @"Some changes have not been saved. Do you really want to quit ?",
										 @"Cancel", @"Quit anyway", nil);
		if (shouldQuit == NSAlertAlternateReturn)
		{
			pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventQuit, NULL) );
			return GHOST_kExitNow;
		}
	}
	else {
		pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventQuit, NULL) );
		return GHOST_kExitNow;
	}
	
	return GHOST_kExitCancel;
}


GHOST_TSuccess GHOST_SystemCocoa::handleTabletEvent(void *eventPtr)
{
	NSEvent *event = (NSEvent *)eventPtr;
	GHOST_IWindow* window = m_windowManager->getActiveWindow();
	GHOST_TabletData& ct=((GHOST_WindowCocoa*)window)->GetCocoaTabletData();
	NSUInteger tabletEvent;
	
	ct.Pressure = 0;
	ct.Xtilt = 0;
	ct.Ytilt = 0;
	
	//Handle tablet events combined with mouse events
	switch ([event subtype]) {
		case NX_SUBTYPE_TABLET_POINT:
			tabletEvent = NSTabletPoint;
			break;
		case NX_SUBTYPE_TABLET_PROXIMITY:
			tabletEvent = NSTabletProximity;
			break;

		default:
			tabletEvent = [event type];
			break;
	}
	
	switch (tabletEvent) {
		case NSTabletPoint:
			ct.Pressure = [event tangentialPressure];
			ct.Xtilt = [event tilt].x;
			ct.Ytilt = [event tilt].y;
			break;
		
		case NSTabletProximity:
			if ([event isEnteringProximity])
			{
				//pointer is entering tablet area proximity
				switch ([event pointingDeviceType]) {
					case NSPenPointingDevice:
						ct.Active = GHOST_kTabletModeStylus;
						break;
					case NSEraserPointingDevice:
						ct.Active = GHOST_kTabletModeEraser;
						break;
					case NSCursorPointingDevice:
					case NSUnknownPointingDevice:
					default:
						ct.Active = GHOST_kTabletModeNone;
						break;
				}
			} else {
				// pointer is leaving - return to mouse
				ct.Active = GHOST_kTabletModeNone;
			}
			break;
		
		default:
			GHOST_ASSERT(FALSE,"GHOST_SystemCocoa::handleTabletEvent : unknown event received");
			return GHOST_kFailure;
			break;
	}
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_SystemCocoa::handleMouseEvent(void *eventPtr)
{
	NSEvent *event = (NSEvent *)eventPtr;
    GHOST_IWindow* window = m_windowManager->getActiveWindow();
	
	switch ([event type])
    {
		case NSLeftMouseDown:
		case NSRightMouseDown:
		case NSOtherMouseDown:
			if (m_windowManager->getActiveWindow()) {
				pushEvent(new GHOST_EventButton([event timestamp], GHOST_kEventButtonDown, window, convertButton([event buttonNumber])));
			}
			handleTabletEvent(eventPtr);
			break;
						
		case NSLeftMouseUp:
		case NSRightMouseUp:
		case NSOtherMouseUp:
			if (m_windowManager->getActiveWindow()) {
				pushEvent(new GHOST_EventButton([event timestamp], GHOST_kEventButtonUp, window, convertButton([event buttonNumber])));
			}
			handleTabletEvent(eventPtr);
			break;
			
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:				
			handleTabletEvent(eventPtr);
		case NSMouseMoved:
			{
				NSPoint mousePos = [event locationInWindow];
				pushEvent(new GHOST_EventCursor([event timestamp], GHOST_kEventCursorMove, window, mousePos.x, mousePos.y));
				break;
			}
			
		case NSScrollWheel:
			{
				GHOST_TInt32 delta;
				delta = [event deltaY] > 0 ? 1 : -1;
				pushEvent(new GHOST_EventWheel(getMilliSeconds(), window, delta));

			}
			break;
			
		default:
			return GHOST_kFailure;
			break;
		}
	
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_SystemCocoa::handleKeyEvent(void *eventPtr)
{
	NSEvent *event = (NSEvent *)eventPtr;
	GHOST_IWindow* window = m_windowManager->getActiveWindow();
	NSUInteger modifiers;
	NSString *characters;
	GHOST_TKey keyCode;
	unsigned char ascii;

	/* Can happen, very rarely - seems to only be when command-H makes
	 * the window go away and we still get an HKey up. 
	 */
	if (!window) {
		return GHOST_kFailure;
	}
	
	switch ([event type]) {
		case NSKeyDown:
		case NSKeyUp:
			characters = [event characters];
			if ([characters length]) { //Check for dead keys
				keyCode = convertKey([event keyCode],
									 [[event charactersIgnoringModifiers] characterAtIndex:0]);
				ascii= convertRomanToLatin((char)[characters characterAtIndex:0]);
			} else {
				keyCode = convertKey([event keyCode],0);
				ascii= 0;
			}
			
			
			if ((keyCode == GHOST_kKeyQ) && (m_modifierMask & NSCommandKeyMask))
				break; //Cmd-Q is directly handled by Cocoa
			
			if ([event type] == NSKeyDown) {
				pushEvent( new GHOST_EventKey([event timestamp], GHOST_kEventKeyDown, window, keyCode, ascii) );
				//printf("\nKey pressed keyCode=%u ascii=%i %c",keyCode,ascii,ascii);
			} else {
				pushEvent( new GHOST_EventKey([event timestamp], GHOST_kEventKeyUp, window, keyCode, ascii) );
			}
			break;
	
		case NSFlagsChanged: 
			modifiers = [event modifierFlags];
			if ((modifiers & NSShiftKeyMask) != (m_modifierMask & NSShiftKeyMask)) {
				pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & NSShiftKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftShift) );
			}
			if ((modifiers & NSControlKeyMask) != (m_modifierMask & NSControlKeyMask)) {
				pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & NSControlKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftControl) );
			}
			if ((modifiers & NSAlternateKeyMask) != (m_modifierMask & NSAlternateKeyMask)) {
				pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & NSAlternateKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftAlt) );
			}
			if ((modifiers & NSCommandKeyMask) != (m_modifierMask & NSCommandKeyMask)) {
				pushEvent( new GHOST_EventKey(getMilliSeconds(), (modifiers & NSCommandKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyCommand) );
			}
			
			m_modifierMask = modifiers;
			break;
			
		default:
			return GHOST_kFailure;
			break;
	}
	
	return GHOST_kSuccess;
}


/* System wide mouse clicks are handled directly through systematic event forwarding to Cocoa
bool GHOST_SystemCarbon::handleMouseDown(void *eventPtr)
{
	NSEvent *event = (NSEvent *)eventPtr;
	WindowPtr			window;
	short				part;
	BitMap 				screenBits;
    bool 				handled = true;
    GHOST_WindowCarbon* ghostWindow;
    Point 				mousePos = {0 , 0};
	
	::GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, NULL, sizeof(Point), NULL, &mousePos);
	
	part = ::FindWindow(mousePos, &window);
	ghostWindow = (GHOST_WindowCarbon*) ::GetWRefCon(window);
	
	switch (part) {
		case inMenuBar:
			handleMenuCommand(::MenuSelect(mousePos));
			break;
			
		case inDrag:
			// *
			// * The DragWindow() routine creates a lot of kEventWindowBoundsChanged
			// * events. By setting m_ignoreWindowSizedMessages these are suppressed.
			// * @see GHOST_SystemCarbon::handleWindowEvent(EventRef event)
			// *
			// even worse: scale window also generates a load of events, and nothing 
			 //  is handled (read: client's event proc called) until you release mouse (ton) 
			
			GHOST_ASSERT(validWindow(ghostWindow), "GHOST_SystemCarbon::handleMouseDown: invalid window");
			m_ignoreWindowSizedMessages = true;
			::DragWindow(window, mousePos, &GetQDGlobalsScreenBits(&screenBits)->bounds);
			m_ignoreWindowSizedMessages = false;
			
			pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowMove, ghostWindow) );

			break;
		
		case inContent:
			if (window != ::FrontWindow()) {
				::SelectWindow(window);
				//
				// * We add a mouse down event on the newly actived window
				// *		
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
				// todo: add option-close, because itÿs in the HIG
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
				int macState;
				
				macState = ghostWindow->getMac_windowState();
				if ( macState== 0)
					::ZoomWindow(window, part, true);
				else 
					if (macState == 2) { // always ok
							::ZoomWindow(window, part, true);
							ghostWindow->setMac_windowState(1);
					} else { // need to force size again
					//	GHOST_TUns32 scr_x,scr_y; //unused
						Rect outAvailableRect;
						
						ghostWindow->setMac_windowState(2);
						::GetAvailableWindowPositioningBounds ( GetMainDevice(), &outAvailableRect);
						
						//this->getMainDisplayDimensions(scr_x,scr_y);
						::SizeWindow (window, outAvailableRect.right-outAvailableRect.left,outAvailableRect.bottom-outAvailableRect.top-1,false);
						::MoveWindow (window, outAvailableRect.left, outAvailableRect.top,true);
					}
				
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
}*/



#pragma mark Clipboard get/set

GHOST_TUns8* GHOST_SystemCocoa::getClipboard(bool selection) const
{
	GHOST_TUns8 * temp_buff;
	size_t pastedTextSize;	
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
	
	if (pasteBoard == nil) {
		[pool drain];
		return NULL;
	}
	
	NSArray *supportedTypes =
		[NSArray arrayWithObjects: @"public.utf8-plain-text", nil];
	
	NSString *bestType = [[NSPasteboard generalPasteboard]
						  availableTypeFromArray:supportedTypes];
	
	if (bestType == nil) {
		[pool drain];
		return NULL;
	}
	
	NSString * textPasted = [pasteBoard stringForType:@"public.utf8-plain-text"];

	if (textPasted == nil) {
		[pool drain];
		return NULL;
	}
	
	pastedTextSize = [textPasted lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
	
	temp_buff = (GHOST_TUns8*) malloc(pastedTextSize+1); 

	if (temp_buff == NULL) {
		[pool drain];
		return NULL;
	}
	
	strncpy((char*)temp_buff, [textPasted UTF8String], pastedTextSize);
	
	temp_buff[pastedTextSize] = '\0';
	
	[pool drain];

	if(temp_buff) {
		return temp_buff;
	} else {
		return NULL;
	}
}

void GHOST_SystemCocoa::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{
	NSString *textToCopy;
	
	if(selection) {return;} // for copying the selection, used on X11

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		
	NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];
	
	if (pasteBoard == nil) {
		[pool drain];
		return;
	}
	
	NSArray *supportedTypes = [NSArray arrayWithObject:@"public.utf8-plain-text"];
	
	[pasteBoard declareTypes:supportedTypes owner:nil];
	
	textToCopy = [NSString stringWithUTF8String:buffer];
	
	[pasteBoard setString:textToCopy forType:@"public.utf8-plain-text"];
	
	[pool drain];
}

#pragma mark Carbon stuff to remove

#ifdef WITH_CARBON


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
#endif