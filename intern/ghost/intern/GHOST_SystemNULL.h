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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemNULL.h
 *  \ingroup GHOST
 * Declaration of GHOST_SystemNULL class.
 */

#ifndef __GHOST_SYSTEMNULL_H__
#define __GHOST_SYSTEMNULL_H__

#include "GHOST_System.h"
#include "../GHOST_Types.h"
#include "GHOST_DisplayManagerNULL.h"
#include "GHOST_WindowNULL.h"

class GHOST_WindowNULL;

class GHOST_SystemNULL : public GHOST_System {
public:

	GHOST_SystemNULL() : GHOST_System() { /* nop */ }
	~GHOST_SystemNULL() { /* nop */ }
	bool processEvents(bool waitForEvent) { return false; }
	int toggleConsole(int action) { return 0; }
	GHOST_TSuccess getModifierKeys(GHOST_ModifierKeys& keys) const { return GHOST_kSuccess; }
	GHOST_TSuccess getButtons(GHOST_Buttons& buttons) const { return GHOST_kSuccess; }
	GHOST_TUns8 *getClipboard(bool selection) const { return NULL; }
	void putClipboard(GHOST_TInt8 *buffer, bool selection) const { /* nop */ }
	GHOST_TUns64 getMilliSeconds() const { return 0; }
	GHOST_TUns8 getNumDisplays() const { return GHOST_TUns8(1); }
	GHOST_TSuccess getCursorPosition(GHOST_TInt32& x, GHOST_TInt32& y) const { return GHOST_kFailure; }
	GHOST_TSuccess setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y) { return GHOST_kFailure; }
	void getMainDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const { /* nop */ }
	void getAllDisplayDimensions(GHOST_TUns32& width, GHOST_TUns32& height) const { /* nop */ }

	GHOST_TSuccess init() {
		GHOST_TSuccess success = GHOST_System::init();

		if (success) {
			m_displayManager = new GHOST_DisplayManagerNULL(this);

			if (m_displayManager) {
				return GHOST_kSuccess;
			}
		}

		return GHOST_kFailure;
	}

	GHOST_IWindow *createWindow(
			const STR_String& title,
			GHOST_TInt32 left,
			GHOST_TInt32 top,
			GHOST_TUns32 width,
			GHOST_TUns32 height,
			GHOST_TWindowState state,
			GHOST_TDrawingContextType type,
			bool stereoVisual,
			bool exclusive,
			const GHOST_TUns16 numOfAASamples,
			const GHOST_TEmbedderWindowID parentWindow)
	{
		return new GHOST_WindowNULL(this, title, left, top, width, height, state, parentWindow, type, stereoVisual, 1);
	}
};

#endif  /* __GHOST_SYSTEMNULL_H__ */
