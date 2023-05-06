/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_DropTargetWin32.hh"
#include "GHOST_Debug.hh"
#include <shellapi.h>

#include "utf_winfunc.h"
#include "utfconv.h"

#ifdef WITH_GHOST_DEBUG
/* utility */
void printLastError(void);
#endif /* WITH_GHOST_DEBUG */

GHOST_DropTargetWin32::GHOST_DropTargetWin32(GHOST_WindowWin32 *window, GHOST_SystemWin32 *system)
    : m_window(window), m_system(system)
{
  m_cRef = 1;
  m_hWnd = window->getHWND();
  m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
}

GHOST_DropTargetWin32::~GHOST_DropTargetWin32() {}

/*
 * IUnknown::QueryInterface
 */
HRESULT __stdcall GHOST_DropTargetWin32::QueryInterface(REFIID riid, void **ppv_obj)
{

  if (!ppv_obj) {
    return E_INVALIDARG;
  }
  *ppv_obj = NULL;

  if (riid == IID_IUnknown || riid == IID_IDropTarget) {
    AddRef();
    *ppv_obj = (void *)this;
    return S_OK;
  }
  *ppv_obj = NULL;
  return E_NOINTERFACE;
}

/*
 * IUnknown::AddRef
 */

ULONG __stdcall GHOST_DropTargetWin32::AddRef(void)
{
  return ::InterlockedIncrement(&m_cRef);
}

/*
 * IUnknown::Release
 */
ULONG __stdcall GHOST_DropTargetWin32::Release(void)
{
  ULONG refs = ::InterlockedDecrement(&m_cRef);

  if (refs == 0) {
    delete this;
    return 0;
  }
  else {
    return refs;
  }
}

/*
 * Implementation of IDropTarget::DragEnter
 */
HRESULT __stdcall GHOST_DropTargetWin32::DragEnter(IDataObject *p_data_object,
                                                   DWORD /*grf_key_state*/,
                                                   POINTL pt,
                                                   DWORD *pdw_effect)
{
  /* We accept all drop by default. */
  m_window->setAcceptDragOperation(true);
  *pdw_effect = DROPEFFECT_NONE;

  m_draggedObjectType = getGhostType(p_data_object);
  m_system->pushDragDropEvent(
      GHOST_kEventDraggingEntered, m_draggedObjectType, m_window, pt.x, pt.y, NULL);
  return S_OK;
}

/*
 * Implementation of IDropTarget::DragOver
 */
HRESULT __stdcall GHOST_DropTargetWin32::DragOver(DWORD /*grf_key_state*/,
                                                  POINTL pt,
                                                  DWORD *pdw_effect)
{
  if (m_window->canAcceptDragOperation()) {
    *pdw_effect = allowedDropEffect(*pdw_effect);
  }
  else {
    *pdw_effect = DROPEFFECT_NONE;
    /* XXX Uncomment to test drop. Drop will not be called if `pdw_effect == DROPEFFECT_NONE`. */
    // *pdw_effect = DROPEFFECT_COPY;
  }
  m_system->pushDragDropEvent(
      GHOST_kEventDraggingUpdated, m_draggedObjectType, m_window, pt.x, pt.y, NULL);
  return S_OK;
}

/*
 * Implementation of IDropTarget::DragLeave
 */
HRESULT __stdcall GHOST_DropTargetWin32::DragLeave(void)
{
  m_system->pushDragDropEvent(
      GHOST_kEventDraggingExited, m_draggedObjectType, m_window, 0, 0, NULL);
  m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
  return S_OK;
}

/* Implementation of IDropTarget::Drop
 * This function will not be called if pdw_effect is set to DROPEFFECT_NONE in
 * the implementation of IDropTarget::DragOver
 */
HRESULT __stdcall GHOST_DropTargetWin32::Drop(IDataObject *p_data_object,
                                              DWORD /*grf_key_state*/,
                                              POINTL pt,
                                              DWORD *pdw_effect)
{
  void *data = getGhostData(p_data_object);
  if (m_window->canAcceptDragOperation()) {
    *pdw_effect = allowedDropEffect(*pdw_effect);
  }
  else {
    *pdw_effect = DROPEFFECT_NONE;
  }
  if (data) {
    m_system->pushDragDropEvent(
        GHOST_kEventDraggingDropDone, m_draggedObjectType, m_window, pt.x, pt.y, data);
  }
  m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
  return S_OK;
}

/*
 * Helpers
 */

DWORD GHOST_DropTargetWin32::allowedDropEffect(DWORD dw_allowed)
{
  DWORD dw_effect = DROPEFFECT_NONE;
  if (dw_allowed & DROPEFFECT_COPY) {
    dw_effect = DROPEFFECT_COPY;
  }
  return dw_effect;
}

GHOST_TDragnDropTypes GHOST_DropTargetWin32::getGhostType(IDataObject *p_data_object)
{
  /* Text
   * NOTE: Unicode text is available as CF_TEXT too, the system can do the
   * conversion, but we do the conversion our self with #WC_NO_BEST_FIT_CHARS.
   */
  FORMATETC fmtetc = {CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  if (p_data_object->QueryGetData(&fmtetc) == S_OK) {
    return GHOST_kDragnDropTypeString;
  }

  /* Files-names. */
  fmtetc.cfFormat = CF_HDROP;
  if (p_data_object->QueryGetData(&fmtetc) == S_OK) {
    return GHOST_kDragnDropTypeFilenames;
  }

  return GHOST_kDragnDropTypeUnknown;
}

void *GHOST_DropTargetWin32::getGhostData(IDataObject *p_data_object)
{
  GHOST_TDragnDropTypes type = getGhostType(p_data_object);
  switch (type) {
    case GHOST_kDragnDropTypeFilenames:
      return getDropDataAsFilenames(p_data_object);
    case GHOST_kDragnDropTypeString:
      return getDropDataAsString(p_data_object);
    case GHOST_kDragnDropTypeBitmap:
      // return getDropDataAsBitmap(p_data_object);
      break;
    default:
#ifdef WITH_GHOST_DEBUG
      ::printf("\nGHOST_kDragnDropTypeUnknown");
#endif /* WITH_GHOST_DEBUG */
      return NULL;
  }
  return NULL;
}

void *GHOST_DropTargetWin32::getDropDataAsFilenames(IDataObject *p_data_object)
{
  GHOST_TStringArray *str_array = NULL;
  FORMATETC fmtetc = {CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};

  /* Check if data-object supplies the format we want.
   * Double checking here, first in #getGhostType. */
  if (p_data_object->QueryGetData(&fmtetc) == S_OK) {
    STGMEDIUM stgmed;
    if (p_data_object->GetData(&fmtetc, &stgmed) == S_OK) {
      const HDROP hdrop = (HDROP)::GlobalLock(stgmed.hGlobal);

      const uint totfiles = ::DragQueryFileW(hdrop, -1, NULL, 0);
      if (totfiles) {
        str_array = (GHOST_TStringArray *)::malloc(sizeof(GHOST_TStringArray));
        str_array->count = 0;
        str_array->strings = (uint8_t **)::malloc(totfiles * sizeof(uint8_t *));

        for (uint nfile = 0; nfile < totfiles; nfile++) {
          WCHAR fpath[MAX_PATH];
          if (::DragQueryFileW(hdrop, nfile, fpath, MAX_PATH) > 0) {
            char *temp_path;
            if (!(temp_path = alloc_utf_8_from_16(fpath, 0))) {
              /* Just ignore paths that could not be converted verbatim. */
              continue;
            }
            str_array->strings[str_array->count++] = (uint8_t *)temp_path;
          }
        }
      }
      /* Free up memory. */
      ::GlobalUnlock(stgmed.hGlobal);
      ::ReleaseStgMedium(&stgmed);
    }
  }
  return str_array;
}

void *GHOST_DropTargetWin32::getDropDataAsString(IDataObject *p_data_object)
{
  char *tmp_string;
  FORMATETC fmtetc = {CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
  STGMEDIUM stgmed;

  /* Try unicode first.
   * Check if data-object supplies the format we want. */
  if (p_data_object->QueryGetData(&fmtetc) == S_OK) {
    if (p_data_object->GetData(&fmtetc, &stgmed) == S_OK) {
      LPCWSTR wstr = (LPCWSTR)::GlobalLock(stgmed.hGlobal);

      tmp_string = alloc_utf_8_from_16((wchar_t *)wstr, 0);

      /* Free memory. */
      ::GlobalUnlock(stgmed.hGlobal);
      ::ReleaseStgMedium(&stgmed);

#ifdef WITH_GHOST_DEBUG
      if (tmp_string) {
        ::printf("\n<converted droped unicode string>\n%s\n</droped converted unicode string>\n",
                 tmp_string);
      }
#endif /* WITH_GHOST_DEBUG */
      return tmp_string;
    }
  }

  fmtetc.cfFormat = CF_TEXT;

  if (p_data_object->QueryGetData(&fmtetc) == S_OK) {
    if (p_data_object->GetData(&fmtetc, &stgmed) == S_OK) {
      char *str = (char *)::GlobalLock(stgmed.hGlobal);

      tmp_string = (char *)::malloc(::strlen(str) + 1);
      if (tmp_string) {
        ::strcpy(tmp_string, str);
      }
      /* Free memory. */
      ::GlobalUnlock(stgmed.hGlobal);
      ::ReleaseStgMedium(&stgmed);

      return tmp_string;
    }
  }

  return NULL;
}

int GHOST_DropTargetWin32::WideCharToANSI(LPCWSTR in, char *&out)
{
  int size;
  out = NULL; /* caller should free if != NULL */

  /* Get the required size. */
  size = ::WideCharToMultiByte(CP_ACP,     /* System Default Codepage */
                               0x00000400, /* WC_NO_BEST_FIT_CHARS */
                               in,
                               -1, /* -1 null terminated, makes output null terminated too. */
                               NULL,
                               0,
                               NULL,
                               NULL);

  if (!size) {
#ifdef WITH_GHOST_DEBUG
    ::printLastError();
#endif /* WITH_GHOST_DEBUG */
    return 0;
  }

  out = (char *)::malloc(size);
  if (!out) {
    ::printf("\nmalloc failed!!!");
    return 0;
  }

  size = ::WideCharToMultiByte(CP_ACP, 0x00000400, in, -1, (LPSTR)out, size, NULL, NULL);

  if (!size) {
#ifdef WITH_GHOST_DEBUG
    ::printLastError();
#endif /* WITH_GHOST_DEBUG */
    ::free(out);
    out = NULL;
  }
  return size;
}

#ifdef WITH_GHOST_DEBUG
void printLastError(void)
{
  LPTSTR s;
  DWORD err;

  err = GetLastError();
  if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    err,
                    0,
                    (LPTSTR)&s,
                    0,
                    NULL))
  {
    printf("\nLastError: (%d) %s\n", int(err), s);
    LocalFree(s);
  }
}
#endif /* WITH_GHOST_DEBUG */
