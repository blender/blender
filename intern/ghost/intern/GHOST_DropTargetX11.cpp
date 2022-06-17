/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2012 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_DropTargetX11.h"
#include "GHOST_Debug.h"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>

bool GHOST_DropTargetX11::m_xdndInitialized = false;
DndClass GHOST_DropTargetX11::m_dndClass;
Atom *GHOST_DropTargetX11::m_dndTypes = nullptr;
Atom *GHOST_DropTargetX11::m_dndActions = nullptr;
const char *GHOST_DropTargetX11::m_dndMimeTypes[] = {
    "url/url", "text/uri-list", "text/plain", "application/octet-stream"};
int GHOST_DropTargetX11::m_refCounter = 0;

#define dndTypeURLID 0
#define dndTypeURIListID 1
#define dndTypePlainTextID 2
#define dndTypeOctetStreamID 3

#define dndTypeURL m_dndTypes[dndTypeURLID]
#define dndTypeURIList m_dndTypes[dndTypeURIListID]
#define dndTypePlainText m_dndTypes[dndTypePlainTextID]
#define dndTypeOctetStream m_dndTypes[dndTypeOctetStreamID]

void GHOST_DropTargetX11::Initialize()
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

void GHOST_DropTargetX11::Uninitialize()
{
  xdnd_shut(&m_dndClass);

  delete[] m_dndActions;
  delete[] m_dndTypes;
}

GHOST_DropTargetX11::GHOST_DropTargetX11(GHOST_WindowX11 *window, GHOST_SystemX11 *system)
    : m_window(window), m_system(system)
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

/* Based on: https://stackoverflow.com/a/2766963/432509 */

using DecodeState_e = enum DecodeState_e {
  /** Searching for an ampersand to convert. */
  STATE_SEARCH = 0,
  /** Convert the two proceeding characters from hex. */
  STATE_CONVERTING
};

void GHOST_DropTargetX11::UrlDecode(char *decodedOut, int bufferSize, const char *encodedIn)
{
  unsigned int i;
  unsigned int len = strlen(encodedIn);
  DecodeState_e state = STATE_SEARCH;
  int j;
  unsigned int asciiCharacter;
  char tempNumBuf[3] = {0};
  bool bothDigits = true;

  memset(decodedOut, 0, bufferSize);

  for (i = 0; i < len; ++i) {
    switch (state) {
      case STATE_SEARCH:
        if (encodedIn[i] != '%') {
          strncat(decodedOut, &encodedIn[i], 1);
          assert((int)strlen(decodedOut) < bufferSize);
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
          if (!isxdigit(tempNumBuf[j])) {
            bothDigits = false;
          }
        }

        if (!bothDigits) {
          break;
        }
        /* Convert two hexadecimal characters into one character */
        sscanf(tempNumBuf, "%x", &asciiCharacter);

        /* Ensure we aren't going to overflow */
        assert((int)strlen(decodedOut) < bufferSize);

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
  if (strncmp(fileUrl, "file://", 7) == 0) {
    /* assume one character of encoded URL can be expanded to 4 chars max */
    int decodedSize = 4 * strlen(fileUrl) + 1;
    char *decodedPath = (char *)malloc(decodedSize);

    UrlDecode(decodedPath, decodedSize, fileUrl + 7);

    return decodedPath;
  }

  return nullptr;
}

void *GHOST_DropTargetX11::getURIListGhostData(unsigned char *dropBuffer, int dropBufferSize)
{
  GHOST_TStringArray *strArray = nullptr;
  int totPaths = 0, curLength = 0;

  /* Count total number of file paths in buffer. */
  for (int i = 0; i <= dropBufferSize; i++) {
    if (dropBuffer[i] == 0 || dropBuffer[i] == '\n' || dropBuffer[i] == '\r') {
      if (curLength) {
        totPaths++;
        curLength = 0;
      }
    }
    else {
      curLength++;
    }
  }

  strArray = (GHOST_TStringArray *)malloc(sizeof(GHOST_TStringArray));
  strArray->count = 0;
  strArray->strings = (uint8_t **)malloc(totPaths * sizeof(uint8_t *));

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
          strArray->strings[strArray->count] = (uint8_t *)decodedPath;
          strArray->count++;
        }

        free(curPath);
        curLength = 0;
      }
    }
    else {
      curLength++;
    }
  }

  return strArray;
}

void *GHOST_DropTargetX11::getGhostData(Atom dropType,
                                        unsigned char *dropBuffer,
                                        int dropBufferSize)
{
  void *data = nullptr;
  unsigned char *tmpBuffer = (unsigned char *)malloc(dropBufferSize + 1);
  bool needsFree = true;

  /* Ensure nil-terminator. */
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

  if (needsFree) {
    free(tmpBuffer);
  }

  return data;
}

bool GHOST_DropTargetX11::GHOST_HandleClientMessage(XEvent *event)
{
  Atom dropType;
  unsigned char *dropBuffer;
  int dropBufferSize, dropX, dropY;

  if (xdnd_get_drop(m_system->getXDisplay(),
                    event,
                    m_dndTypes,
                    m_dndActions,
                    &dropBuffer,
                    &dropBufferSize,
                    &dropType,
                    &dropX,
                    &dropY)) {
    void *data = getGhostData(dropType, dropBuffer, dropBufferSize);

    if (data) {
      m_system->pushDragDropEvent(
          GHOST_kEventDraggingDropDone, m_draggedObjectType, m_window, dropX, dropY, data);
    }

    free(dropBuffer);

    m_draggedObjectType = GHOST_kDragnDropTypeUnknown;

    return true;
  }

  return false;
}
