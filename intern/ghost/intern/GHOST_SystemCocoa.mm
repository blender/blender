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
#include "GHOST_EventDragnDrop.h"

#include "GHOST_TimerManager.h"
#include "GHOST_TimerTask.h"
#include "GHOST_WindowManager.h"
#include "GHOST_WindowCocoa.h"
#include "GHOST_NDOFManager.h"
#include "AssertMacros.h"

#pragma mark KeyMap, mouse converters


/* Keycodes from Carbon include file */
/*  
 *  Summary:
 *    Virtual keycodes
 *  
 *  Discussion:
 *    These constants are the virtual keycodes defined originally in
 *    Inside Mac Volume V, pg. V-191. They identify physical keys on a
 *    keyboard. Those constants with "ANSI" in the name are labeled
 *    according to the key position on an ANSI-standard US keyboard.
 *    For example, kVK_ANSI_A indicates the virtual keycode for the key
 *    with the letter 'A' in the US keyboard layout. Other keyboard
 *    layouts may have the 'A' key label on a different physical key;
 *    in this case, pressing 'A' will generate a different virtual
 *    keycode.
 */
enum {
	kVK_ANSI_A                    = 0x00,
	kVK_ANSI_S                    = 0x01,
	kVK_ANSI_D                    = 0x02,
	kVK_ANSI_F                    = 0x03,
	kVK_ANSI_H                    = 0x04,
	kVK_ANSI_G                    = 0x05,
	kVK_ANSI_Z                    = 0x06,
	kVK_ANSI_X                    = 0x07,
	kVK_ANSI_C                    = 0x08,
	kVK_ANSI_V                    = 0x09,
	kVK_ANSI_B                    = 0x0B,
	kVK_ANSI_Q                    = 0x0C,
	kVK_ANSI_W                    = 0x0D,
	kVK_ANSI_E                    = 0x0E,
	kVK_ANSI_R                    = 0x0F,
	kVK_ANSI_Y                    = 0x10,
	kVK_ANSI_T                    = 0x11,
	kVK_ANSI_1                    = 0x12,
	kVK_ANSI_2                    = 0x13,
	kVK_ANSI_3                    = 0x14,
	kVK_ANSI_4                    = 0x15,
	kVK_ANSI_6                    = 0x16,
	kVK_ANSI_5                    = 0x17,
	kVK_ANSI_Equal                = 0x18,
	kVK_ANSI_9                    = 0x19,
	kVK_ANSI_7                    = 0x1A,
	kVK_ANSI_Minus                = 0x1B,
	kVK_ANSI_8                    = 0x1C,
	kVK_ANSI_0                    = 0x1D,
	kVK_ANSI_RightBracket         = 0x1E,
	kVK_ANSI_O                    = 0x1F,
	kVK_ANSI_U                    = 0x20,
	kVK_ANSI_LeftBracket          = 0x21,
	kVK_ANSI_I                    = 0x22,
	kVK_ANSI_P                    = 0x23,
	kVK_ANSI_L                    = 0x25,
	kVK_ANSI_J                    = 0x26,
	kVK_ANSI_Quote                = 0x27,
	kVK_ANSI_K                    = 0x28,
	kVK_ANSI_Semicolon            = 0x29,
	kVK_ANSI_Backslash            = 0x2A,
	kVK_ANSI_Comma                = 0x2B,
	kVK_ANSI_Slash                = 0x2C,
	kVK_ANSI_N                    = 0x2D,
	kVK_ANSI_M                    = 0x2E,
	kVK_ANSI_Period               = 0x2F,
	kVK_ANSI_Grave                = 0x32,
	kVK_ANSI_KeypadDecimal        = 0x41,
	kVK_ANSI_KeypadMultiply       = 0x43,
	kVK_ANSI_KeypadPlus           = 0x45,
	kVK_ANSI_KeypadClear          = 0x47,
	kVK_ANSI_KeypadDivide         = 0x4B,
	kVK_ANSI_KeypadEnter          = 0x4C,
	kVK_ANSI_KeypadMinus          = 0x4E,
	kVK_ANSI_KeypadEquals         = 0x51,
	kVK_ANSI_Keypad0              = 0x52,
	kVK_ANSI_Keypad1              = 0x53,
	kVK_ANSI_Keypad2              = 0x54,
	kVK_ANSI_Keypad3              = 0x55,
	kVK_ANSI_Keypad4              = 0x56,
	kVK_ANSI_Keypad5              = 0x57,
	kVK_ANSI_Keypad6              = 0x58,
	kVK_ANSI_Keypad7              = 0x59,
	kVK_ANSI_Keypad8              = 0x5B,
	kVK_ANSI_Keypad9              = 0x5C
};

/* keycodes for keys that are independent of keyboard layout*/
enum {
	kVK_Return                    = 0x24,
	kVK_Tab                       = 0x30,
	kVK_Space                     = 0x31,
	kVK_Delete                    = 0x33,
	kVK_Escape                    = 0x35,
	kVK_Command                   = 0x37,
	kVK_Shift                     = 0x38,
	kVK_CapsLock                  = 0x39,
	kVK_Option                    = 0x3A,
	kVK_Control                   = 0x3B,
	kVK_RightShift                = 0x3C,
	kVK_RightOption               = 0x3D,
	kVK_RightControl              = 0x3E,
	kVK_Function                  = 0x3F,
	kVK_F17                       = 0x40,
	kVK_VolumeUp                  = 0x48,
	kVK_VolumeDown                = 0x49,
	kVK_Mute                      = 0x4A,
	kVK_F18                       = 0x4F,
	kVK_F19                       = 0x50,
	kVK_F20                       = 0x5A,
	kVK_F5                        = 0x60,
	kVK_F6                        = 0x61,
	kVK_F7                        = 0x62,
	kVK_F3                        = 0x63,
	kVK_F8                        = 0x64,
	kVK_F9                        = 0x65,
	kVK_F11                       = 0x67,
	kVK_F13                       = 0x69,
	kVK_F16                       = 0x6A,
	kVK_F14                       = 0x6B,
	kVK_F10                       = 0x6D,
	kVK_F12                       = 0x6F,
	kVK_F15                       = 0x71,
	kVK_Help                      = 0x72,
	kVK_Home                      = 0x73,
	kVK_PageUp                    = 0x74,
	kVK_ForwardDelete             = 0x75,
	kVK_F4                        = 0x76,
	kVK_End                       = 0x77,
	kVK_F2                        = 0x78,
	kVK_PageDown                  = 0x79,
	kVK_F1                        = 0x7A,
	kVK_LeftArrow                 = 0x7B,
	kVK_RightArrow                = 0x7C,
	kVK_DownArrow                 = 0x7D,
	kVK_UpArrow                   = 0x7E
};

/* ISO keyboards only*/
enum {
	kVK_ISO_Section               = 0x0A
};

/* JIS keyboards only*/
enum {
	kVK_JIS_Yen                   = 0x5D,
	kVK_JIS_Underscore            = 0x5E,
	kVK_JIS_KeypadComma           = 0x5F,
	kVK_JIS_Eisu                  = 0x66,
	kVK_JIS_Kana                  = 0x68
};


static GHOST_TButtonMask convertButton(int button)
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


#pragma mark defines for 10.6 api not documented in 10.5
#ifndef MAC_OS_X_VERSION_10_6

@interface NSEvent(SnowLeopardEvents)
/* modifier keys currently down.  This returns the state of devices combined
 with synthesized events at the moment, independent of which events
 have been delivered via the event stream. */
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
+ (unsigned int)modifierFlags; //NSUInteger is defined only from 10.5
#else
+ (NSUInteger)modifierFlags;
#endif
@end

#endif


#pragma mark Utility functions

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

#if defined(WITH_QUICKTIME) && !defined(USE_QTKIT)
//Need to place this quicktime function in an ObjC file
//It is used to avoid memory leak when raising the quicktime "compression settings" standard dialog
extern "C" {
	struct bContext;
	struct wmOperator;
	extern int fromcocoa_request_qtcodec_settings(bContext *C, wmOperator *op);


int cocoa_request_qtcodec_settings(bContext *C, wmOperator *op)
{
	int result;
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	result = fromcocoa_request_qtcodec_settings(C, op);
	
	[pool drain];
	return result;
}
};
#endif


#pragma mark Cocoa objects

/**
 * CocoaAppDelegate
 * ObjC object to capture applicationShouldTerminate, and send quit event
 **/
@interface CocoaAppDelegate : NSObject {
	GHOST_SystemCocoa *systemCocoa;
}
- (void)setSystemCocoa:(GHOST_SystemCocoa *)sysCocoa;
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (void)applicationWillTerminate:(NSNotification *)aNotification;
- (void)applicationWillBecomeActive:(NSNotification *)aNotification;
@end

@implementation CocoaAppDelegate : NSObject
-(void)setSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
{
	systemCocoa = sysCocoa;
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	return systemCocoa->handleOpenDocumentRequest(filename);
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

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
	systemCocoa->handleApplicationBecomeActiveEvent();
}
@end



#pragma mark initialization/finalization


GHOST_SystemCocoa::GHOST_SystemCocoa()
{
	m_modifierMask =0;
	m_pressedMouseButtons =0;
	m_cursorDelta_x=0;
	m_cursorDelta_y=0;
	m_outsideLoopEventProcessed = false;
	m_displayManager = new GHOST_DisplayManagerCocoa ();
	GHOST_ASSERT(m_displayManager, "GHOST_SystemCocoa::GHOST_SystemCocoa(): m_displayManager==0\n");
	m_displayManager->initialize();

	//NSEvent timeStamp is given in system uptime, state start date is boot time
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
		
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		if (NSApp == nil) {
			[NSApplication sharedApplication];
			
			if ([NSApp mainMenu] == nil) {
				NSMenu *mainMenubar = [[NSMenu alloc] init];
				NSMenuItem *menuItem;
				NSMenu *windowMenu;
				NSMenu *appMenu;
				
				//Create the application menu
				appMenu = [[NSMenu alloc] initWithTitle:@"Blender"];
				
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
				[NSApp performSelector:@selector(setAppleMenu:) withObject:appMenu]; //Needed for 10.5
				[appMenu release];
				
				//Create the window menu
				windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
				
				menuItem = [windowMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
				[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
				
				[windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
				
				menuItem = [windowMenu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
				[menuItem setKeyEquivalentModifierMask:NSCommandKeyMask];
				
				menuItem = [[NSMenuItem	alloc] init];
				[menuItem setSubmenu:windowMenu];
				
				[mainMenubar addItem:menuItem];
				[menuItem release];
				
				[NSApp setMainMenu:mainMenubar];
				[NSApp setWindowsMenu:windowMenu];
				[windowMenu release];
			}
		}
		if ([NSApp delegate] == nil) {
			CocoaAppDelegate *appDelegate = [[CocoaAppDelegate alloc] init];
			[appDelegate setSystemCocoa:this];
			[NSApp setDelegate:appDelegate];
		}
		
		[NSApp finishLaunching];
				
		[pool drain];
    }
    return success;
}


#pragma mark window management

GHOST_TUns64 GHOST_SystemCocoa::getMilliSeconds() const
{
	//Cocoa equivalent exists in 10.6 ([[NSProcessInfo processInfo] systemUptime])
	struct timeval currentTime;
	
	gettimeofday(&currentTime, NULL);
	
	//Return timestamp of system uptime
	
	return ((currentTime.tv_sec*1000)+(currentTime.tv_usec/1000)-m_start_time);
}


GHOST_TUns8 GHOST_SystemCocoa::getNumDisplays() const
{
	//Note that OS X supports monitor hot plug
	// We do not support multiple monitors at the moment
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	GHOST_TUns8 count = [[NSScreen screens] count];

	[pool drain];
	return count;
}


void GHOST_SystemCocoa::getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	//Get visible frame, that is frame excluding dock and top menu bar
	NSRect frame = [[NSScreen mainScreen] visibleFrame];
	
	//Returns max window contents (excluding title bar...)
	NSRect contentRect = [NSWindow contentRectForFrameRect:frame
												 styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)];
	
	width = contentRect.size.width;
	height = contentRect.size.height;

	[pool drain];
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
	const GHOST_TUns16 numOfAASamples,
	const GHOST_TEmbedderWindowID parentWindow
)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	GHOST_IWindow* window = 0;
	
	//Get the available rect for including window contents
	NSRect frame = [[NSScreen mainScreen] visibleFrame];
	NSRect contentRect = [NSWindow contentRectForFrameRect:frame
												 styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)];
	
	//Ensures window top left is inside this available rect
	left = left > contentRect.origin.x ? left : contentRect.origin.x;
	top = top > contentRect.origin.y ? top : contentRect.origin.y;
	
	window = new GHOST_WindowCocoa (this, title, left, top, width, height, state, type, stereoVisual, numOfAASamples);

    if (window) {
        if (window->getValid()) {
            // Store the pointer to the window 
            GHOST_ASSERT(m_windowManager, "m_windowManager not initialized");
            m_windowManager->addWindow(window);
            m_windowManager->setActiveWindow(window);
			//Need to tell window manager the new window is the active one (Cocoa does not send the event activate upon window creation)
            pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window));
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
	[pool drain];
    return window;
}

GHOST_TSuccess GHOST_SystemCocoa::beginFullScreen(const GHOST_DisplaySetting& setting, GHOST_IWindow** window, const bool stereoVisual)
{	
	GHOST_IWindow* currentWindow = m_windowManager->getActiveWindow();

	*window = currentWindow;
	
	return currentWindow->setState(GHOST_kWindowStateFullScreen);
}

GHOST_TSuccess GHOST_SystemCocoa::endFullScreen(void)
{	
	GHOST_IWindow* currentWindow = m_windowManager->getActiveWindow();
	
	return currentWindow->setState(GHOST_kWindowStateNormal);
}


	
/**
 * @note : returns coordinates in Cocoa screen coordinates
 */
GHOST_TSuccess GHOST_SystemCocoa::getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const
{
    NSPoint mouseLoc = [NSEvent mouseLocation];
	
    // Returns the mouse location in screen coordinates
    x = (GHOST_TInt32)mouseLoc.x;
    y = (GHOST_TInt32)mouseLoc.y;
    return GHOST_kSuccess;
}

/**
 * @note : expect Cocoa screen coordinates
 */
GHOST_TSuccess GHOST_SystemCocoa::setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) const
{
	float xf=(float)x, yf=(float)y;
	GHOST_WindowCocoa* window = (GHOST_WindowCocoa*)m_windowManager->getActiveWindow();
	NSScreen *windowScreen = window->getScreen();
	NSRect screenRect = [windowScreen frame];
	
	//Set position relative to current screen
	xf -= screenRect.origin.x;
	yf -= screenRect.origin.y;
	
	//Quartz Display Services uses the old coordinates (top left origin)
	yf = screenRect.size.height -yf;

	CGDisplayMoveCursorToPoint((CGDirectDisplayID)[[[windowScreen deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue], CGPointMake(xf, yf));

    return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_SystemCocoa::getModifierKeys(GHOST_ModifierKeys& keys) const
{
	keys.set(GHOST_kModifierKeyCommand, (m_modifierMask & NSCommandKeyMask) ? true : false);
	keys.set(GHOST_kModifierKeyLeftAlt, (m_modifierMask & NSAlternateKeyMask) ? true : false);
	keys.set(GHOST_kModifierKeyLeftShift, (m_modifierMask & NSShiftKeyMask) ? true : false);
	keys.set(GHOST_kModifierKeyLeftControl, (m_modifierMask & NSControlKeyMask) ? true : false);
	
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
	bool anyProcessed = false;
	NSEvent *event;
	
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
		 }*/
		
		do {
			NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
			event = [NSApp nextEventMatchingMask:NSAnyEventMask
									   untilDate:[NSDate distantPast]
										  inMode:NSDefaultRunLoopMode
										 dequeue:YES];
			if (event==nil) {
				[pool drain];
				break;
			}
			
			anyProcessed = true;
			
			switch ([event type]) {
				case NSKeyDown:
				case NSKeyUp:
				case NSFlagsChanged:
					handleKeyEvent(event);
					
					/* Support system-wide keyboard shortcuts, like Expos√©, ...) =>included in always NSApp sendEvent */
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
					handleTabletEvent(event,[event type]);
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
			[pool drain];
		} while (event!= nil);		
	//} while (waitForEvent && !anyProcessed); Needed only for timer implementation
	
	if (m_outsideLoopEventProcessed) {
		m_outsideLoopEventProcessed = false;
		return true;
	}
	
    return anyProcessed;
}

//Note: called from NSApplication delegate
GHOST_TSuccess GHOST_SystemCocoa::handleApplicationBecomeActiveEvent()
{
	//Update the modifiers key mask, as its status may have changed when the application was not active
	//(that is when update events are sent to another application)
	unsigned int modifiers;
	GHOST_IWindow* window = m_windowManager->getActiveWindow();

#ifdef MAC_OS_X_VERSION_10_6
	modifiers = [NSEvent modifierFlags];
#else
	//If build against an older SDK, check if running on 10.6 to use the correct function
	if ([NSEvent respondsToSelector:@selector(modifierFlags)]) {
		modifiers = [NSEvent modifierFlags];
	}
	else {
		//TODO: need to find a better workaround for the missing cocoa "getModifierFlag" function in 10.4/10.5
		modifiers = 0;
	}
#endif
	
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
	
	return GHOST_kSuccess;
}

//Note: called from NSWindow delegate
GHOST_TSuccess GHOST_SystemCocoa::handleWindowEvent(GHOST_TEventType eventType, GHOST_WindowCocoa* window)
{
	if (!validWindow(window)) {
		return GHOST_kFailure;
	}
		switch(eventType) 
		{
			case GHOST_kEventWindowClose:
				pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window) );
				break;
			case GHOST_kEventWindowActivate:
				m_windowManager->setActiveWindow(window);
				window->loadCursor(window->getCursorVisibility(), window->getCursorShape());
				pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window) );
				break;
			case GHOST_kEventWindowDeactivate:
				m_windowManager->setWindowInactive(window);
				pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowDeactivate, window) );
				break;
			case GHOST_kEventWindowUpdate:
				pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window) );
				break;
			case GHOST_kEventWindowSize:
				if (!m_ignoreWindowSizedMessages)
				{
					window->updateDrawingContext();
					pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window) );
				}
				break;
			default:
				return GHOST_kFailure;
				break;
		}
	
	m_outsideLoopEventProcessed = true;
	return GHOST_kSuccess;
}

//Note: called from NSWindow subclass
GHOST_TSuccess GHOST_SystemCocoa::handleDraggingEvent(GHOST_TEventType eventType, GHOST_TDragnDropTypes draggedObjectType,
								   GHOST_WindowCocoa* window, int mouseX, int mouseY, void* data)
{
	if (!validWindow(window) && (eventType != GHOST_kEventDraggingDropOnIcon)) {
		return GHOST_kFailure;
	}
	switch(eventType) 
	{
		case GHOST_kEventDraggingEntered:
		case GHOST_kEventDraggingUpdated:
		case GHOST_kEventDraggingExited:
			pushEvent(new GHOST_EventDragnDrop(getMilliSeconds(),eventType,draggedObjectType,window,mouseX,mouseY,NULL));
			break;
			
		case GHOST_kEventDraggingDropDone:
		case GHOST_kEventDraggingDropOnIcon:
		{
			GHOST_TUns8 * temp_buff;
			GHOST_TStringArray *strArray;
			NSArray *droppedArray;
			size_t pastedTextSize;	
			NSString *droppedStr;
			GHOST_TEventDataPtr eventData;
			int i;

			if (!data) return GHOST_kFailure;
			
			switch (draggedObjectType) {
				case GHOST_kDragnDropTypeBitmap:
					//TODO: implement bitmap conversion to a blender friendly format
					return GHOST_kFailure;
					break;
				case GHOST_kDragnDropTypeFilenames:
					droppedArray = (NSArray*)data;
					
					strArray = (GHOST_TStringArray*)malloc(sizeof(GHOST_TStringArray));
					if (!strArray) return GHOST_kFailure;
					
					strArray->count = [droppedArray count];
					if (strArray->count == 0) return GHOST_kFailure;
					
					strArray->strings = (GHOST_TUns8**) malloc(strArray->count*sizeof(GHOST_TUns8*));
					
					for (i=0;i<strArray->count;i++)
					{
						droppedStr = [droppedArray objectAtIndex:i];
						
						pastedTextSize = [droppedStr lengthOfBytesUsingEncoding:NSISOLatin1StringEncoding];
						temp_buff = (GHOST_TUns8*) malloc(pastedTextSize+1); 
					
						if (!temp_buff) {
							strArray->count = i;
							break;
						}
					
						strncpy((char*)temp_buff, [droppedStr cStringUsingEncoding:NSISOLatin1StringEncoding], pastedTextSize);
						temp_buff[pastedTextSize] = '\0';
						
						strArray->strings[i] = temp_buff;
					}

					eventData = (GHOST_TEventDataPtr) strArray;	
					break;
					
				case GHOST_kDragnDropTypeString:
					droppedStr = (NSString*)data;
					pastedTextSize = [droppedStr lengthOfBytesUsingEncoding:NSISOLatin1StringEncoding];
					
					temp_buff = (GHOST_TUns8*) malloc(pastedTextSize+1); 
					
					if (temp_buff == NULL) {
						return GHOST_kFailure;
					}
					
					strncpy((char*)temp_buff, [droppedStr cStringUsingEncoding:NSISOLatin1StringEncoding], pastedTextSize);
					
					temp_buff[pastedTextSize] = '\0';
					
					eventData = (GHOST_TEventDataPtr) temp_buff;
					break;
					
				default:
					return GHOST_kFailure;
					break;
			}
			pushEvent(new GHOST_EventDragnDrop(getMilliSeconds(),eventType,draggedObjectType,window,mouseX,mouseY,eventData));
		}
			break;
		default:
			return GHOST_kFailure;
	}
	m_outsideLoopEventProcessed = true;
	return GHOST_kSuccess;
}


GHOST_TUns8 GHOST_SystemCocoa::handleQuitRequest()
{
	GHOST_Window* window = (GHOST_Window*)m_windowManager->getActiveWindow();
	
	//Discard quit event if we are in cursor grab sequence
	if (window && (window->getCursorGrabMode() != GHOST_kGrabDisable) && (window->getCursorGrabMode() != GHOST_kGrabNormal))
		return GHOST_kExitCancel;
	
	//Check open windows if some changes are not saved
	if (m_windowManager->getAnyModifiedState())
	{
		int shouldQuit = NSRunAlertPanel(@"Exit Blender", @"Some changes have not been saved.\nDo you really want to quit ?",
										 @"Cancel", @"Quit Anyway", nil);
		if (shouldQuit == NSAlertAlternateReturn)
		{
			pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventQuit, NULL) );
			return GHOST_kExitNow;
		} else {
			//Give back focus to the blender window if user selected cancel quit
			NSArray *windowsList = [NSApp orderedWindows];
			if ([windowsList count]) {
				[[windowsList objectAtIndex:0] makeKeyAndOrderFront:nil];
			}
		}

	}
	else {
		pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventQuit, NULL) );
		m_outsideLoopEventProcessed = true;
		return GHOST_kExitNow;
	}
	
	return GHOST_kExitCancel;
}

bool GHOST_SystemCocoa::handleOpenDocumentRequest(void *filepathStr)
{
	NSString *filepath = (NSString*)filepathStr;
	int confirmOpen = NSAlertAlternateReturn;
	NSArray *windowsList;
	
	//Check open windows if some changes are not saved
	if (m_windowManager->getAnyModifiedState())
	{
		confirmOpen = NSRunAlertPanel([NSString stringWithFormat:@"Opening %@",[filepath lastPathComponent]],
										 @"Current document has not been saved.\nDo you really want to proceed?",
										 @"Cancel", @"Open", nil);
	}

	//Give back focus to the blender window
	windowsList = [NSApp orderedWindows];
	if ([windowsList count]) {
		[[windowsList objectAtIndex:0] makeKeyAndOrderFront:nil];
	}

	if (confirmOpen == NSAlertAlternateReturn)
	{
		handleDraggingEvent(GHOST_kEventDraggingDropOnIcon,GHOST_kDragnDropTypeFilenames,NULL,0,0, [NSArray arrayWithObject:filepath]);
		return YES;
	}
	else return NO;
}

GHOST_TSuccess GHOST_SystemCocoa::handleTabletEvent(void *eventPtr, short eventType)
{
	NSEvent *event = (NSEvent *)eventPtr;
	GHOST_IWindow* window = m_windowManager->getActiveWindow();
	
	if (!window) return GHOST_kFailure;
	
	GHOST_TabletData& ct=((GHOST_WindowCocoa*)window)->GetCocoaTabletData();
	
	switch (eventType) {
		case NSTabletPoint:
			ct.Pressure = [event pressure];
			ct.Xtilt = [event tilt].x;
			ct.Ytilt = [event tilt].y;
			break;
		
		case NSTabletProximity:
			ct.Pressure = 0;
			ct.Xtilt = 0;
			ct.Ytilt = 0;
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
    GHOST_Window* window = (GHOST_Window*)m_windowManager->getActiveWindow();
	
	if (!window) {
		return GHOST_kFailure;
	}
	
	switch ([event type])
    {
		case NSLeftMouseDown:
		case NSRightMouseDown:
		case NSOtherMouseDown:
			pushEvent(new GHOST_EventButton([event timestamp]*1000, GHOST_kEventButtonDown, window, convertButton([event buttonNumber])));
			//Handle tablet events combined with mouse events
			switch ([event subtype]) {
				case NX_SUBTYPE_TABLET_POINT:
					handleTabletEvent(eventPtr, NSTabletPoint);
					break;
				case NX_SUBTYPE_TABLET_PROXIMITY:
					handleTabletEvent(eventPtr, NSTabletProximity);
					break;
				default:
					//No tablet event included : do nothing
					break;
			}
			break;
						
		case NSLeftMouseUp:
		case NSRightMouseUp:
		case NSOtherMouseUp:
			pushEvent(new GHOST_EventButton([event timestamp]*1000, GHOST_kEventButtonUp, window, convertButton([event buttonNumber])));
			//Handle tablet events combined with mouse events
			switch ([event subtype]) {
				case NX_SUBTYPE_TABLET_POINT:
					handleTabletEvent(eventPtr, NSTabletPoint);
					break;
				case NX_SUBTYPE_TABLET_PROXIMITY:
					handleTabletEvent(eventPtr, NSTabletProximity);
					break;
				default:
					//No tablet event included : do nothing
					break;
			}
			break;
			
		case NSLeftMouseDragged:
		case NSRightMouseDragged:
		case NSOtherMouseDragged:				
			//Handle tablet events combined with mouse events
			switch ([event subtype]) {
				case NX_SUBTYPE_TABLET_POINT:
					handleTabletEvent(eventPtr, NSTabletPoint);
					break;
				case NX_SUBTYPE_TABLET_PROXIMITY:
					handleTabletEvent(eventPtr, NSTabletProximity);
					break;
				default:
					//No tablet event included : do nothing
					break;
			}
			
		case NSMouseMoved:
				switch (window->getCursorGrabMode()) {
					case GHOST_kGrabHide: //Cursor hidden grab operation : no cursor move
					{
						GHOST_TInt32 x_warp, y_warp, x_accum, y_accum;
						
						window->getCursorGrabInitPos(x_warp, y_warp);
						
						window->getCursorGrabAccum(x_accum, y_accum);
						x_accum += [event deltaX];
						y_accum += -[event deltaY]; //Strange Apple implementation (inverted coordinates for the deltaY) ...
						window->setCursorGrabAccum(x_accum, y_accum);
						
						pushEvent(new GHOST_EventCursor([event timestamp]*1000, GHOST_kEventCursorMove, window, x_warp+x_accum, y_warp+y_accum));
					}
						break;
					case GHOST_kGrabWrap: //Wrap cursor at area/window boundaries
					{
						NSPoint mousePos = [event locationInWindow];
						GHOST_TInt32 x_mouse= mousePos.x;
						GHOST_TInt32 y_mouse= mousePos.y;
						GHOST_TInt32 x_accum, y_accum, x_cur, y_cur;
						GHOST_Rect bounds, windowBounds, correctedBounds;
						
						/* fallback to window bounds */
						if(window->getCursorGrabBounds(bounds)==GHOST_kFailure)
							window->getClientBounds(bounds);
						
						//Switch back to Cocoa coordinates orientation (y=0 at botton,the same as blender internal btw!), and to client coordinates
						window->getClientBounds(windowBounds);
						window->screenToClient(bounds.m_l,bounds.m_b, correctedBounds.m_l, correctedBounds.m_t);
						window->screenToClient(bounds.m_r, bounds.m_t, correctedBounds.m_r, correctedBounds.m_b);
						correctedBounds.m_b = (windowBounds.m_b - windowBounds.m_t) - correctedBounds.m_b;
						correctedBounds.m_t = (windowBounds.m_b - windowBounds.m_t) - correctedBounds.m_t;
						
						//Update accumulation counts
						window->getCursorGrabAccum(x_accum, y_accum);
						x_accum += [event deltaX]-m_cursorDelta_x;
						y_accum += -[event deltaY]-m_cursorDelta_y; //Strange Apple implementation (inverted coordinates for the deltaY) ...
						window->setCursorGrabAccum(x_accum, y_accum);
						
						
						//Warp mouse cursor if needed
						x_mouse += [event deltaX]-m_cursorDelta_x;
						y_mouse += -[event deltaY]-m_cursorDelta_y;
						correctedBounds.wrapPoint(x_mouse, y_mouse, 2);
						
						//Compensate for mouse moved event taking cursor position set into account
						m_cursorDelta_x = x_mouse-mousePos.x;
						m_cursorDelta_y = y_mouse-mousePos.y;
						
						//Set new cursor position
						window->clientToScreen(x_mouse, y_mouse, x_cur, y_cur);
						setCursorPosition(x_cur, y_cur); /* wrap */
						
						//Post event
						window->getCursorGrabInitPos(x_cur, y_cur);
						pushEvent(new GHOST_EventCursor([event timestamp]*1000, GHOST_kEventCursorMove, window, x_cur + x_accum, y_cur + y_accum));
					}
						break;
					default:
					{
						//Normal cursor operation: send mouse position in window
						NSPoint mousePos = [event locationInWindow];
						pushEvent(new GHOST_EventCursor([event timestamp]*1000, GHOST_kEventCursorMove, window, mousePos.x, mousePos.y));
						m_cursorDelta_x=0;
						m_cursorDelta_y=0; //Mouse motion occured between two cursor warps, so we can reset the delta counter
					}
						break;
				}
				break;
			
		case NSScrollWheel:
			{
				GHOST_TInt32 delta;
				
				double deltaF = [event deltaY];
				if (deltaF == 0.0) break; //discard trackpad delta=0 events
				
				delta = deltaF > 0.0 ? 1 : -1;
				pushEvent(new GHOST_EventWheel([event timestamp]*1000, window, delta));
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
	unsigned int modifiers;
	NSString *characters;
	NSData *convertedCharacters;
	GHOST_TKey keyCode;
	unsigned char ascii;
	NSString* charsIgnoringModifiers;

	/* Can happen, very rarely - seems to only be when command-H makes
	 * the window go away and we still get an HKey up. 
	 */
	if (!window) {
		printf("\nW failure");
		return GHOST_kFailure;
	}
	
	switch ([event type]) {
		case NSKeyDown:
		case NSKeyUp:
			charsIgnoringModifiers = [event charactersIgnoringModifiers];
			if ([charsIgnoringModifiers length]>0)
				keyCode = convertKey([event keyCode],
									 [charsIgnoringModifiers characterAtIndex:0]);
			else
				keyCode = convertKey([event keyCode],0);

				
			characters = [event characters];
			if ([characters length]>0) { //Check for dead keys
				//Convert characters to iso latin 1 encoding
				convertedCharacters = [characters dataUsingEncoding:NSISOLatin1StringEncoding];
				if ([convertedCharacters length]>0)
					ascii =((char*)[convertedCharacters bytes])[0];
				else
					ascii = 0; //Character not available in iso latin 1 encoding
			}
			else
				ascii= 0;
			
			if ((keyCode == GHOST_kKeyQ) && (m_modifierMask & NSCommandKeyMask))
				break; //Cmd-Q is directly handled by Cocoa

			if ([event type] == NSKeyDown) {
				pushEvent( new GHOST_EventKey([event timestamp]*1000, GHOST_kEventKeyDown, window, keyCode, ascii) );
				//printf("\nKey pressed keyCode=%u ascii=%i %c",keyCode,ascii,ascii);
			} else {
				pushEvent( new GHOST_EventKey([event timestamp]*1000, GHOST_kEventKeyUp, window, keyCode, ascii) );
			}
			break;
	
		case NSFlagsChanged: 
			modifiers = [event modifierFlags];
			if ((modifiers & NSShiftKeyMask) != (m_modifierMask & NSShiftKeyMask)) {
				pushEvent( new GHOST_EventKey([event timestamp]*1000, (modifiers & NSShiftKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftShift) );
			}
			if ((modifiers & NSControlKeyMask) != (m_modifierMask & NSControlKeyMask)) {
				pushEvent( new GHOST_EventKey([event timestamp]*1000, (modifiers & NSControlKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftControl) );
			}
			if ((modifiers & NSAlternateKeyMask) != (m_modifierMask & NSAlternateKeyMask)) {
				pushEvent( new GHOST_EventKey([event timestamp]*1000, (modifiers & NSAlternateKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyLeftAlt) );
			}
			if ((modifiers & NSCommandKeyMask) != (m_modifierMask & NSCommandKeyMask)) {
				pushEvent( new GHOST_EventKey([event timestamp]*1000, (modifiers & NSCommandKeyMask)?GHOST_kEventKeyDown:GHOST_kEventKeyUp, window, GHOST_kKeyCommand) );
			}
			
			m_modifierMask = modifiers;
			break;
			
		default:
			return GHOST_kFailure;
			break;
	}
	
	return GHOST_kSuccess;
}



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
		[NSArray arrayWithObjects: NSStringPboardType, nil];
	
	NSString *bestType = [[NSPasteboard generalPasteboard]
						  availableTypeFromArray:supportedTypes];
	
	if (bestType == nil) {
		[pool drain];
		return NULL;
	}
	
	NSString * textPasted = [pasteBoard stringForType:NSStringPboardType];

	if (textPasted == nil) {
		[pool drain];
		return NULL;
	}
	
	pastedTextSize = [textPasted lengthOfBytesUsingEncoding:NSISOLatin1StringEncoding];
	
	temp_buff = (GHOST_TUns8*) malloc(pastedTextSize+1); 

	if (temp_buff == NULL) {
		[pool drain];
		return NULL;
	}
	
	strncpy((char*)temp_buff, [textPasted cStringUsingEncoding:NSISOLatin1StringEncoding], pastedTextSize);
	
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
	
	NSArray *supportedTypes = [NSArray arrayWithObject:NSStringPboardType];
	
	[pasteBoard declareTypes:supportedTypes owner:nil];
	
	textToCopy = [NSString stringWithCString:buffer encoding:NSISOLatin1StringEncoding];
	
	[pasteBoard setString:textToCopy forType:NSStringPboardType];
	
	[pool drain];
}
