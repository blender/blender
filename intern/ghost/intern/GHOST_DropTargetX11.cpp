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

/** \file ghost/intern/GHOST_DropTargetX11.cpp
 *  \ingroup GHOST
 */

#include "GHOST_DropTargetX11.h"
#include "GHOST_Debug.h"

#include <ctype.h>
#include <assert.h>

bool GHOST_DropTargetX11::m_xdndInitialized = false;
DndClass GHOST_DropTargetX11::m_dndClass;
Atom *GHOST_DropTargetX11::m_dndTypes = NULL;
Atom *GHOST_DropTargetX11::m_dndActions = NULL;
const char *GHOST_DropTargetX11::m_dndMimeTypes[] = {"url/url", "text/uri-list", "text/plain", "application/octet-stream"};
int GHOST_DropTargetX11::m_refCounter = 0;

#define dndTypeURLID           0
#define dndTypeURIListID       1
#define dndTypePlainTextID     2
#define dndTypeOctetStreamID   3

#define dndTypeURL          m_dndTypes[dndTypeURLID]
#define dndTypeURIList      m_dndTypes[dndTypeURIListID]
#define dndTypePlainText    m_dndTypes[dndTypePlainTextID]
#define dndTypeOctetStream  m_dndTypes[dndTypeOctetStreamID]

void GHOST_DropTargetX11::Initialize(void)
{
	Display *display = m_system->getXDisplay();
	int dndTypesCount = sizeof(m_dndMimeTypes) / sizeof(char *);
	int counter;

	xdnd_init(&m_dndClass, display);

	m_dndTypes = new Atom[dndTypesCount + 1];
	XInternAtoms(display, (char **)m_dndMimeTypes, dndTypesCount, 0, m_dndTypes);
	m_dndTypes[dndTypesCount] = 0;

	m_dndActions = new Atom[8];
	counter = 0;

	m_dndActions[counter++] = m_dndClass.XdndActionCopy;
	m_dndActions[counter++] = m_dndClass.XdndActionMove;

#if 0 /* Not supported yet */
	dndActions[counter++] = dnd->XdndActionLink;
	dndActions[counter++] = dnd->XdndActionAsk;
	dndActions[counter++] = dnd->XdndActionPrivate;
	dndActions[counter++] = dnd->XdndActionList;
	dndActions[counter++] = dnd->XdndActionDescription;
#endif

	m_dndActions[counter++] = 0;
}

void GHOST_DropTargetX11::Uninitialize(void)
{
	xdnd_shut(&m_dndClass);
}

GHOST_DropTargetX11::GHOST_DropTargetX11(GHOST_WindowX11 *window, GHOST_SystemX11 *system)
	:
	m_window(window),
	m_system(system)
{
	if (!m_xdndInitialized) {
		Initialize();
		m_xdndInitialized = true;
		GHOST_PRINT("XDND initialized\n");
	}

	Window wnd = window->getXWindow();

	xdnd_set_dnd_aware(&m_dndClass, wnd, 0);
	xdnd_set_type_list(&m_dndClass, wnd, m_dndTypes);

	m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
	m_refCounter++;
}

GHOST_DropTargetX11::~GHOST_DropTargetX11()
{
	m_refCounter--;
	if (m_refCounter == 0) {
		Uninitialize();
		m_xdndInitialized = false;
		GHOST_PRINT("XDND uninitialized\n");
	}
}

/* based on a code from Saul Rennison
 * http://stackoverflow.com/questions/2673207/c-c-url-decode-library */

typedef enum DecodeState_e {
	STATE_SEARCH = 0, ///< searching for an ampersand to convert
	STATE_CONVERTING  ///< convert the two proceeding characters from hex
} DecodeState_e;

void GHOST_DropTargetX11::UrlDecode(char *decodedOut, int bufferSize, const char *encodedIn)
{
	unsigned int i;
	unsigned int len = strlen(encodedIn);
	DecodeState_e state = STATE_SEARCH;
	int j, asciiCharacter;
	char tempNumBuf[3] = {0};
	bool bothDigits = true;

	memset(decodedOut, 0, bufferSize);

	for (i = 0; i < len; ++i) {
		switch (state) {
			case STATE_SEARCH:
				if (encodedIn[i] != '%') {
					strncat(decodedOut, &encodedIn[i], 1);
					assert(strlen(decodedOut) < bufferSize);
					break;
				}

				/* We are now converting */
				state = STATE_CONVERTING;
				break;

			case STATE_CONVERTING:
				bothDigits = true;

				/* Create a buffer to hold the hex. For example, if %20, this
				 * buffer would hold 20 (in ASCII) */
				memset(tempNumBuf, 0, sizeof(tempNumBuf));

				/* Conversion complete (i.e. don't convert again next iter) */
				state = STATE_SEARCH;

				strncpy(tempNumBuf, &encodedIn[i], 2);

				/* Ensure both characters are hexadecimal */

				for (j = 0; j < 2; ++j) {
					if (!isxdigit(tempNumBuf[j]))
						bothDigits = false;
				}

				if (!bothDigits)
					break;

				/* Convert two hexadecimal characters into one character */
				sscanf(tempNumBuf, "%x", &asciiCharacter);

				/* Ensure we aren't going to overflow */
				assert(strlen(decodedOut) < bufferSize);

				/* Concatenate this character onto the output */
				strncat(decodedOut, (char *)&asciiCharacter, 1);

				/* Skip the next character */
				i++;
				break;
		}
	}
}

char *GHOST_DropTargetX11::FileUrlDecode(char *fileUrl)
{
	if (!strncpy(fileUrl, "file://", 7) == 0) {
		/* assume one character of encoded URL can be expanded to 4 chars max */
		int decodedSize = 4 * strlen(fileUrl) + 1;
		char *decodedPath = (char *)malloc(decodedSize);

		UrlDecode(decodedPath, decodedSize, fileUrl + 7);

		return decodedPath;
	}

	return NULL;
}

void *GHOST_DropTargetX11::getURIListGhostData(unsigned char *dropBuffer, int dropBufferSize)
{
	GHOST_TStringArray *strArray = NULL;
	int totPaths = 0, curLength = 0;

	/* count total number of file pathes in buffer */
	for (int i = 0; i <= dropBufferSize; i++) {
		if (dropBuffer[i] == 0 || dropBuffer[i] == '\n' || dropBuffer[i] == '\r') {
			if (curLength) {
				totPaths++;
				curLength = 0;
			}
		}
		else curLength++;
	}

	strArray = (GHOST_TStringArray *)malloc(sizeof(GHOST_TStringArray));
	strArray->count = 0;
	strArray->strings = (GHOST_TUns8 **)malloc(totPaths * sizeof(GHOST_TUns8 *));

	curLength = 0;
	for (int i = 0; i <= dropBufferSize; i++) {
		if (dropBuffer[i] == 0 || dropBuffer[i] == '\n' || dropBuffer[i] == '\r') {
			if (curLength) {
				char *curPath = (char *)malloc(curLength + 1);
				char *decodedPath;

				strncpy(curPath, (char *)dropBuffer + i - curLength, curLength);
				curPath[curLength] = 0;

				decodedPath = FileUrlDecode(curPath);
				if (decodedPath) {
					strArray->strings[strArray->count] = (GHOST_TUns8 *)decodedPath;
					strArray->count++;
				}

				free(curPath);
				curLength = 0;
			}
		}
		else curLength++;
	}

	return strArray;
}

void *GHOST_DropTargetX11::getGhostData(Atom dropType, unsigned char *dropBuffer, int dropBufferSize)
{
	void *data = NULL;
	unsigned char *tmpBuffer = (unsigned char *)malloc(dropBufferSize + 1);
	bool needsFree = true;

	/* ensure NULL-terminator */
	memcpy(tmpBuffer, dropBuffer, dropBufferSize);
	tmpBuffer[dropBufferSize] = 0;

	if (dropType == dndTypeURIList) {
		m_draggedObjectType = GHOST_kDragnDropTypeFilenames;
		data = getURIListGhostData(tmpBuffer, dropBufferSize);
	}
	else if (dropType == dndTypeURL) {
		/* need to be tested */
		char *decodedPath = FileUrlDecode((char *)tmpBuffer);

		if (decodedPath) {
			m_draggedObjectType = GHOST_kDragnDropTypeString;
			data = decodedPath;
		}
	}
	else if (dropType == dndTypePlainText || dropType == dndTypeOctetStream) {
		m_draggedObjectType = GHOST_kDragnDropTypeString;
		data = tmpBuffer;
		needsFree = false;
	}
	else {
		m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
	}

	if (needsFree)
		free(tmpBuffer);

	return data;
}

bool GHOST_DropTargetX11::GHOST_HandleClientMessage(XEvent *event)
{
	Atom dropType;
	unsigned char *dropBuffer;
	int dropBufferSize, dropX, dropY;

	if (xdnd_get_drop(m_system->getXDisplay(), event, m_dndTypes, m_dndActions,
	                  &dropBuffer, &dropBufferSize, &dropType, &dropX, &dropY))
	{
		void *data = getGhostData(dropType, dropBuffer, dropBufferSize);

		if (data)
			m_system->pushDragDropEvent(GHOST_kEventDraggingDropDone, m_draggedObjectType, m_window, dropX, dropY, data);

		free(dropBuffer);

		m_draggedObjectType = GHOST_kDragnDropTypeUnknown;

		return true;
	}

	return false;
}
