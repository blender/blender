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
 * The Original Code is Copyright (C) 2012 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DropTargetWin32.h
 *  \ingroup GHOST
 */

#ifndef _GHOST_DROP_TARGET_X11_H_
#define _GHOST_DROP_TARGET_X11_H_

#include <GHOST_Types.h>
#include "GHOST_WindowX11.h"
#include "GHOST_SystemX11.h"

#include "xdnd.h"

class GHOST_DropTargetX11
{
public:
	/**
	 * Constructor
	 *
	 * @param window	The window to register as drop target.
	 * @param system	The associated system.
	 */
	GHOST_DropTargetX11(GHOST_WindowX11 * window, GHOST_SystemX11 * system);

	/**
	 * Destructor
	 */
	~GHOST_DropTargetX11();

	/**
	  * Handler of ClientMessage X11 event
	  */
	bool GHOST_HandleClientMessage(XEvent *event);

	/**
	 * Get data to pass in event.
	 * It checks the type and calls specific functions for each type.
	 * @param dropType - type of dropped entity.
	 * @param dropBuffer - buffer returned from source application
	 * @param dropBufferSize - size of returned buffer
	 * @return Pointer to data.
	 */
	void *getGhostData(Atom dropType, unsigned char *dropBuffer, int dropBufferSize);

private:
	/* Internal helper functions */

	/**
	  * Initiailize XDND and all related X atoms
	  */
	void Initialize(void);

	/**
	  * Uninitiailize XDND and all related X atoms
	  */
	void Uninitialize(void);

	/**
	  * Get data to be passed to event from text/uri-list mime type
	  * @param dropBuffer - buffer returned from source application
	  * @param dropBufferSize - size of dropped buffer
	  * @return pointer to newly created GHOST data
	  */
	void * getURIListGhostData(unsigned char *dropBuffer, int dropBufferSize);

	/**
	  * Decode URL (i.e. converts "file:///a%20b/test" to "file:///a b/test")
	  * @param decodedOut - buffer for decoded URL
	  * @param bufferSize - size of output buffer
	  * @param encodedIn - input encoded buffer to be decoded
	  */
	void UrlDecode(char *decodedOut, int bufferSize, const char *encodedIn);

	/**
	  * Fully decode file URL (i.e. converts "file:///a%20b/test" to "/a b/test")
	  * @param fileUrl - file path URL to be fully decoded
	  * @return decoded file path (resutl shold be free-d)
	  */
	char *FileUrlDecode(char *fileUrl);

	/* The associated GHOST_WindowWin32. */
	GHOST_WindowX11 * m_window;
	/* The System. */
	GHOST_SystemX11 * m_system;

	/* Data type of the dragged object */
	GHOST_TDragnDropTypes m_draggedObjectType;

	/* is dnd stuff initialzied */
	static bool m_xdndInitialized;

	/* class holding internal stiff of xdnd library */
	static DndClass m_dndClass;

	/* list of supported types to eb draggeg into */
	static Atom * m_dndTypes;

	/* list of supported dran'n'drop actions */
	static Atom * m_dndActions;

	/* List of supported MIME types to be dragged into */
	static const char *m_dndMimeTypes[];

	/* counter of references to global XDND structures */
	static int m_refCounter;
};

#endif  // _GHOST_DROP_TARGET_X11_H_
