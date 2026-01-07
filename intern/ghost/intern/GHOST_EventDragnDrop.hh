/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_Event.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

/**
 * Drag & drop event
 *
 * The dragging sequence is performed in four phases:
 *
 * - Start sequence (GHOST_kEventDraggingEntered) that tells
 *   a drag & drop operation has started.
 *   Already gives the object data type, and the entering mouse location
 *
 * - Update mouse position (GHOST_kEventDraggingUpdated) sent upon each mouse move until the
 *   drag & drop operation stops, to give the updated mouse position.
 *   Useful to highlight a potential destination, and update the status
 *   (through GHOST_setAcceptDragOperation) telling if the object can be dropped at the current
 *   cursor position.
 *
 * - Abort drag & drop sequence (#GHOST_kEventDraggingExited)
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
 * - UTF8 string.
 * - array of strings representing filenames (#GHOST_TStringArray).
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
                       GHOST_TDragnDropDataPtr data)
      : GHOST_Event(time, type, window)
  {
    dragn_drop_event_data_.x = x;
    dragn_drop_event_data_.y = y;
    dragn_drop_event_data_.dataType = dataType;
    dragn_drop_event_data_.data = data;
    data_ = &dragn_drop_event_data_;
  }

  ~GHOST_EventDragnDrop() override
  {
    /* Free the dropped object data. */
    if (dragn_drop_event_data_.data == nullptr) {
      return;
    }

    switch (dragn_drop_event_data_.dataType) {
      case GHOST_kDragnDropTypeBitmap:
        blender::IMB_freeImBuf((blender::ImBuf *)dragn_drop_event_data_.data);
        break;
      case GHOST_kDragnDropTypeFilenames: {
        GHOST_TStringArray *strArray = (GHOST_TStringArray *)dragn_drop_event_data_.data;
        int i;

        for (i = 0; i < strArray->count; i++) {
          free(strArray->strings[i]);
        }

        free(strArray->strings);
        free(strArray);
        break;
      }
      case GHOST_kDragnDropTypeString:
        free(dragn_drop_event_data_.data);
        break;

      default:
        break;
    }
  }

 protected:
  /** The x,y-coordinates of the cursor position. */
  GHOST_TEventDragnDropData dragn_drop_event_data_;
};
