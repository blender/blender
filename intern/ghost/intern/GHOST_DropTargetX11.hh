/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include "GHOST_SystemX11.hh"
#include "GHOST_WindowX11.hh"
#include <GHOST_Types.h>

#include "xdnd.h"

class GHOST_DropTargetX11 {
 public:
  /**
   * Constructor
   *
   * \param window: The window to register as drop target.
   * \param system: The associated system.
   */
  GHOST_DropTargetX11(GHOST_WindowX11 *window, GHOST_SystemX11 *system);

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
   * \param dropType: Type of dropped entity.
   * \param dropBuffer: Buffer returned from source application.
   * \param dropBufferSize: Size of returned buffer.
   * \return Pointer to data.
   */
  void *getGhostData(Atom dropType, unsigned char *dropBuffer, int dropBufferSize);

 private:
  /* Internal helper functions */

  /**
   * Initialize XDND and all related X atoms
   */
  void Initialize();

  /**
   * Uninitialized XDND and all related X atoms
   */
  void Uninitialize();

  /**
   * Get data to be passed to event from text/URI-list mime type
   * \param dropBuffer: Buffer returned from source application.
   * \param dropBufferSize: Size of dropped buffer.
   * \return pointer to newly created GHOST data.
   */
  void *getURIListGhostData(unsigned char *dropBuffer, int dropBufferSize);

  /**
   * Fully decode file URL (i.e. converts `file:///a%20b/test` to `/a b/test`)
   * \param fileUrl: - file path URL to be fully decoded.
   * \return decoded file path (result should be free-d).
   */
  char *FileUrlDecode(char *fileUrl);

  /* The associated GHOST_WindowWin32. */
  GHOST_WindowX11 *m_window;
  /* The System. */
  GHOST_SystemX11 *m_system;

  /* Data type of the dragged object */
  GHOST_TDragnDropTypes m_draggedObjectType;

  /* Is drag-and-drop stuff initialized. */
  static bool m_xdndInitialized;

  /* Class holding internal stiff of `xdnd` library. */
  static DndClass m_dndClass;

  /* List of supported types to be dragged into. */
  static Atom *m_dndTypes;

  /* List of supported drag-and-drop actions. */
  static Atom *m_dndActions;

  /* List of supported MIME types to be dragged into. */
  static const char *m_dndMimeTypes[];

  /* Counter of references to global #XDND structures. */
  static int m_refCounter;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_DropTargetX11")
#endif
};
