/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Event.hh"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

/**
 * Drag & drop event
 *
 * The dragging sequence is performed in four phases:
 *
 * - Start sequence (GHOST_kEventDraggingEntered) that tells
 *   a drag'n'drop operation has started.
 *   Already gives the object data type, and the entering mouse location
 *
 * - Update mouse position (GHOST_kEventDraggingUpdated) sent upon each mouse move until the
 *   drag'n'drop operation stops, to give the updated mouse position.
 *   Useful to highlight a potential destination, and update the status
 *   (through GHOST_setAcceptDragOperation) telling if the object can be dropped at the current
 *   cursor position.
 *
 * - Abort drag'n'drop sequence (GHOST_kEventDraggingExited)
 *   sent when the user moved the mouse outside the window.
 *
 * - Send the dropped data (GHOST_kEventDraggingDropDone)
 *
 * - Outside of the normal sequence, dropped data can be sent (GHOST_kEventDraggingDropOnIcon).
 *   This can happen when the user drops an object on the application icon.
 *   (Also used in OSX to pass the filename of the document the user doubled-clicked in the finder)
 *
 * Note that the mouse positions are given in Blender coordinates (y=0 at bottom)
 *
 * Currently supported object types:
 * - UTF-8 string.
 * - array of strings representing filenames (GHOST_TStringArray).
 * - bitmap #ImBuf.
 */
class GHOST_EventDragnDrop : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param time: The time this event was generated.
   * \param type: The type of this event.
   * \param dataType: The type of the drop candidate object.
   * \param window: The window where the event occurred.
   * \param x: The x-coordinate of the location the cursor was at the time of the event.
   * \param y: The y-coordinate of the location the cursor was at the time of the event.
   * \param data: The "content" dropped in the window.
   */
  GHOST_EventDragnDrop(uint64_t time,
                       GHOST_TEventType type,
                       GHOST_TDragnDropTypes dataType,
                       GHOST_IWindow *window,
                       int x,
                       int y,
                       GHOST_TEventDataPtr data)
      : GHOST_Event(time, type, window)
  {
    m_dragnDropEventData.x = x;
    m_dragnDropEventData.y = y;
    m_dragnDropEventData.dataType = dataType;
    m_dragnDropEventData.data = data;
    m_data = &m_dragnDropEventData;
  }

  ~GHOST_EventDragnDrop()
  {
    /* Free the dropped object data. */
    if (m_dragnDropEventData.data == NULL)
      return;

    switch (m_dragnDropEventData.dataType) {
      case GHOST_kDragnDropTypeBitmap:
        IMB_freeImBuf((ImBuf *)m_dragnDropEventData.data);
        break;
      case GHOST_kDragnDropTypeFilenames: {
        GHOST_TStringArray *strArray = (GHOST_TStringArray *)m_dragnDropEventData.data;
        int i;

        for (i = 0; i < strArray->count; i++)
          free(strArray->strings[i]);

        free(strArray->strings);
        free(strArray);
      } break;
      case GHOST_kDragnDropTypeString:
        free(m_dragnDropEventData.data);
        break;

      default:
        break;
    }
  }

 protected:
  /** The x,y-coordinates of the cursor position. */
  GHOST_TEventDragnDropData m_dragnDropEventData;
};
