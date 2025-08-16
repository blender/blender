/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_DropTargetX11.hh"
#include "GHOST_Debug.hh"
#include "GHOST_PathUtils.hh"
#include "GHOST_utildefines.hh"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>

bool GHOST_DropTargetX11::xdnd_initialized_ = false;
DndClass GHOST_DropTargetX11::dnd_class_;
Atom *GHOST_DropTargetX11::dnd_types_ = nullptr;
Atom *GHOST_DropTargetX11::dnd_actions_ = nullptr;
const char *GHOST_DropTargetX11::dnd_mime_types_[] = {
    "url/url", "text/uri-list", "text/plain", "application/octet-stream"};
int GHOST_DropTargetX11::ref_counter_ = 0;

#define dndTypeURLID 0
#define dndTypeURIListID 1
#define dndTypePlainTextID 2
#define dndTypeOctetStreamID 3

#define dndTypeURL dnd_types_[dndTypeURLID]
#define dndTypeURIList dnd_types_[dndTypeURIListID]
#define dndTypePlainText dnd_types_[dndTypePlainTextID]
#define dndTypeOctetStream dnd_types_[dndTypeOctetStreamID]

void GHOST_DropTargetX11::Initialize()
{
  Display *display = system_->getXDisplay();
  int dndTypesCount = ARRAY_SIZE(dnd_mime_types_);
  int counter;

  xdnd_init(&dnd_class_, display);

  dnd_types_ = new Atom[dndTypesCount + 1];
  XInternAtoms(display, (char **)dnd_mime_types_, dndTypesCount, 0, dnd_types_);
  dnd_types_[dndTypesCount] = 0;

  dnd_actions_ = new Atom[8];
  counter = 0;

  dnd_actions_[counter++] = dnd_class_.XdndActionCopy;
  dnd_actions_[counter++] = dnd_class_.XdndActionMove;

#if 0 /* Not supported yet */
  dndActions[counter++] = dnd->XdndActionLink;
  dndActions[counter++] = dnd->XdndActionAsk;
  dndActions[counter++] = dnd->XdndActionPrivate;
  dndActions[counter++] = dnd->XdndActionList;
  dndActions[counter++] = dnd->XdndActionDescription;
#endif

  dnd_actions_[counter++] = 0;
}

void GHOST_DropTargetX11::Uninitialize()
{
  xdnd_shut(&dnd_class_);

  delete[] dnd_actions_;
  delete[] dnd_types_;
}

GHOST_DropTargetX11::GHOST_DropTargetX11(GHOST_WindowX11 *window, GHOST_SystemX11 *system)
    : window_(window), system_(system)
{
  if (!xdnd_initialized_) {
    Initialize();
    xdnd_initialized_ = true;
    GHOST_PRINT("XDND initialized\n");
  }

  Window wnd = window->getXWindow();

  xdnd_set_dnd_aware(&dnd_class_, wnd, nullptr);
  xdnd_set_type_list(&dnd_class_, wnd, dnd_types_);

  dragged_object_type_ = GHOST_kDragnDropTypeUnknown;
  ref_counter_++;
}

GHOST_DropTargetX11::~GHOST_DropTargetX11()
{
  ref_counter_--;
  if (ref_counter_ == 0) {
    Uninitialize();
    xdnd_initialized_ = false;
    GHOST_PRINT("XDND uninitialized\n");
  }
}

char *GHOST_DropTargetX11::FileUrlDecode(const char *fileUrl)
{
  if (strncmp(fileUrl, "file://", 7) == 0) {
    const char *file = fileUrl + 7;
    return GHOST_URL_decode_alloc(file, strlen(file));
  }

  return nullptr;
}

void *GHOST_DropTargetX11::getURIListGhostData(const uchar *dropBuffer, int dropBufferSize)
{
  GHOST_TStringArray *strArray = nullptr;
  int totPaths = 0, curLength = 0;

  /* Count total number of file paths in buffer. */
  for (int i = 0; i <= dropBufferSize; i++) {
    if (ELEM(dropBuffer[i], 0, '\n', '\r')) {
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
    if (ELEM(dropBuffer[i], 0, '\n', '\r')) {
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

void *GHOST_DropTargetX11::getGhostData(Atom dropType, const uchar *dropBuffer, int dropBufferSize)
{
  void *data = nullptr;
  uchar *tmpBuffer = (uchar *)malloc(dropBufferSize + 1);
  bool needsFree = true;

  /* Ensure nil-terminator. */
  memcpy(tmpBuffer, dropBuffer, dropBufferSize);
  tmpBuffer[dropBufferSize] = 0;

  if (dropType == dndTypeURIList) {
    dragged_object_type_ = GHOST_kDragnDropTypeFilenames;
    data = getURIListGhostData(tmpBuffer, dropBufferSize);
  }
  else if (dropType == dndTypeURL) {
    /* need to be tested */
    char *decodedPath = FileUrlDecode((const char *)tmpBuffer);

    if (decodedPath) {
      dragged_object_type_ = GHOST_kDragnDropTypeString;
      data = decodedPath;
    }
  }
  else if (ELEM(dropType, dndTypePlainText, dndTypeOctetStream)) {
    dragged_object_type_ = GHOST_kDragnDropTypeString;
    data = tmpBuffer;
    needsFree = false;
  }
  else {
    dragged_object_type_ = GHOST_kDragnDropTypeUnknown;
  }

  if (needsFree) {
    free(tmpBuffer);
  }

  return data;
}

bool GHOST_DropTargetX11::GHOST_HandleClientMessage(XEvent *event)
{
  Atom dropType;
  uchar *dropBuffer;
  int dropBufferSize, dropX, dropY;

  if (xdnd_get_drop(system_->getXDisplay(),
                    event,
                    dnd_types_,
                    dnd_actions_,
                    &dropBuffer,
                    &dropBufferSize,
                    &dropType,
                    &dropX,
                    &dropY))
  {
    void *data = getGhostData(dropType, dropBuffer, dropBufferSize);

    if (data) {
      system_->pushDragDropEvent(
          GHOST_kEventDraggingDropDone, dragged_object_type_, window_, dropX, dropY, data);
    }

    free(dropBuffer);

    dragged_object_type_ = GHOST_kDragnDropTypeUnknown;

    return true;
  }

  return false;
}
