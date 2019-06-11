/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup GHOST
 */

#ifndef __GHOST_DROPTARGETWIN32_H__
#define __GHOST_DROPTARGETWIN32_H__

#include <string.h>
#include <GHOST_Types.h>
#include "GHOST_WindowWin32.h"
#include "GHOST_SystemWin32.h"

class GHOST_DropTargetWin32 : public IDropTarget {
 public:
  /* IUnknownd implementation.
   * Enables clients to get pointers to other interfaces on a given object
   * through the QueryInterface method, and manage the existence of the object
   * through the AddRef and Release methods. All other COM interfaces are
   * inherited, directly or indirectly, from IUnknown. Therefore, the three
   * methods in IUnknown are the first entries in the VTable for every interface.
   */
  HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObj);
  ULONG __stdcall AddRef(void);
  ULONG __stdcall Release(void);

  /* IDropTarget implementation
   * + The IDropTarget interface is one of the interfaces you implement to
   *   provide drag-and-drop operations in your application. It contains methods
   *   used in any application that can be a target for data during a
   *   drag-and-drop operation. A drop-target application is responsible for:
   *
   *  - Determining the effect of the drop on the target application.
   *  - Incorporating any valid dropped data when the drop occurs.
   *  - Communicating target feedback to the source so the source application
   *    can provide appropriate visual feedback such as setting the cursor.
   *  - Implementing drag scrolling.
   *  - Registering and revoking its application windows as drop targets.
   *
   * The IDropTarget interface contains methods that handle all these
   * responsibilities except registering and revoking the application window
   * as a drop target, for which you must call the RegisterDragDrop and the
   * RevokeDragDrop functions.
   */

  HRESULT __stdcall DragEnter(IDataObject *pDataObject,
                              DWORD grfKeyState,
                              POINTL pt,
                              DWORD *pdwEffect);
  HRESULT __stdcall DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);
  HRESULT __stdcall DragLeave(void);
  HRESULT __stdcall Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

  /**
   * Constructor
   * With the modifier keys, we want to distinguish left and right keys.
   * Sometimes this is not possible (Windows ME for instance). Then, we want
   * events generated for both keys.
   * \param window    The window to register as drop target.
   * \param system    The associated system.
   */
  GHOST_DropTargetWin32(GHOST_WindowWin32 *window, GHOST_SystemWin32 *system);

  /**
   * Destructor
   * Do NOT destroy directly. Use Release() instead to make COM happy.
   */
  ~GHOST_DropTargetWin32();

 private:
  /* Internal helper functions */

  /**
   * Base the effect on those allowed by the drop-source.
   * \param dwAllowed Drop sources allowed drop effect.
   * \return The allowed drop effect.
   */
  DWORD allowedDropEffect(DWORD dwAllowed);

  /**
   * Query DataObject for the data types it supports.
   * \param pDataObject Pointer to the DataObject.
   * \return GHOST data type.
   */
  GHOST_TDragnDropTypes getGhostType(IDataObject *pDataObject);

  /**
   * Get data to pass in event.
   * It checks the type and calls specific functions for each type.
   * \param pDataObject Pointer to the DataObject.
   * \return Pointer to data.
   */
  void *getGhostData(IDataObject *pDataObject);

  /**
   * Allocate data as file array to pass in event.
   * \param pDataObject Pointer to the DataObject.
   * \return Pointer to data.
   */
  void *getDropDataAsFilenames(IDataObject *pDataObject);

  /**
   * Allocate data as string to pass in event.
   * \param pDataObject Pointer to the DataObject.
   * \return Pointer to data.
   */
  void *getDropDataAsString(IDataObject *pDataObject);

  /**
   * Convert Unicode to ANSI, replacing uncomfortable chars with '?'.
   * The ANSI codepage is the system default codepage,
   * and can change from system to system.
   * \param in LPCWSTR.
   * \param out char *. Is set to NULL on failure.
   * \return 0 on failure. Else the size of the string including '\0'.
   */
  int WideCharToANSI(LPCWSTR in, char *&out);

  /* Private member variables */
  /* COM reference count. */
  LONG m_cRef;
  /* Handle of the associated window. */
  HWND m_hWnd;
  /* The associated GHOST_WindowWin32. */
  GHOST_WindowWin32 *m_window;
  /* The System. */
  GHOST_SystemWin32 *m_system;
  /* Data type of the dragged object */
  GHOST_TDragnDropTypes m_draggedObjectType;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("GHOST:GHOST_DropTargetWin32")
#endif
};

#endif  // __GHOST_DROPTARGETWIN32_H__
