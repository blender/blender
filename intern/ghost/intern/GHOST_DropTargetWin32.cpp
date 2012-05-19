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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_DropTargetWin32.cpp
 *  \ingroup GHOST
 */

 
#include "GHOST_Debug.h"
#include "GHOST_DropTargetWin32.h"
#include <ShellApi.h>

#include "utf_winfunc.h"
#include "utfconv.h"

#ifdef GHOST_DEBUG
// utility
void printLastError(void);
#endif // GHOST_DEBUG


GHOST_DropTargetWin32::GHOST_DropTargetWin32(GHOST_WindowWin32 *window, GHOST_SystemWin32 *system)
	:
	m_window(window),
	m_system(system)
{
	m_cRef = 1;
	m_hWnd = window->getHWND();
	m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
	
	// register our window as drop target
	::RegisterDragDrop(m_hWnd, this);
}

GHOST_DropTargetWin32::~GHOST_DropTargetWin32()
{
	::RevokeDragDrop(m_hWnd);
}


/* 
 *  IUnknown::QueryInterface
 */
HRESULT __stdcall GHOST_DropTargetWin32::QueryInterface(REFIID riid, void **ppvObj)
{

	if (!ppvObj)
		return E_INVALIDARG;
	*ppvObj = NULL;

	if (riid == IID_IUnknown || riid == IID_IDropTarget)
	{
		AddRef();
		*ppvObj = (void *)this;
		return S_OK;
	}
	else {
		*ppvObj = 0;
		return E_NOINTERFACE;
	}
}


/* 
 *	IUnknown::AddRef 
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
		
	if (refs == 0)
	{
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
HRESULT __stdcall GHOST_DropTargetWin32::DragEnter(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	// we accept all drop by default
	m_window->setAcceptDragOperation(true);
	*pdwEffect = DROPEFFECT_NONE;
	
	m_draggedObjectType = getGhostType(pDataObject);
	m_system->pushDragDropEvent(GHOST_kEventDraggingEntered, m_draggedObjectType, m_window, pt.x, pt.y, NULL);
	return S_OK;
}

/* 
 * Implementation of IDropTarget::DragOver
 */
HRESULT __stdcall GHOST_DropTargetWin32::DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	if (m_window->canAcceptDragOperation())
	{
		*pdwEffect = allowedDropEffect(*pdwEffect);
	}
	else {
		*pdwEffect = DROPEFFECT_NONE;
		//*pdwEffect = DROPEFFECT_COPY; // XXX Uncomment to test drop. Drop will not be called if pdwEffect == DROPEFFECT_NONE.
	}
	m_system->pushDragDropEvent(GHOST_kEventDraggingUpdated, m_draggedObjectType, m_window, pt.x, pt.y, NULL);
	return S_OK;
}

/* 
 * Implementation of IDropTarget::DragLeave
 */
HRESULT __stdcall GHOST_DropTargetWin32::DragLeave(void)
{
	m_system->pushDragDropEvent(GHOST_kEventDraggingExited, m_draggedObjectType, m_window, 0, 0, NULL);
	m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
	return S_OK;
}

/* Implementation of IDropTarget::Drop
 * This function will not be called if pdwEffect is set to DROPEFFECT_NONE in 
 * the implementation of IDropTarget::DragOver
 */
HRESULT __stdcall GHOST_DropTargetWin32::Drop(IDataObject *pDataObject, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect)
{
	void *data = getGhostData(pDataObject);
	if (m_window->canAcceptDragOperation())
	{
		*pdwEffect = allowedDropEffect(*pdwEffect);

	}
	else {
		*pdwEffect = DROPEFFECT_NONE;
	}
	if (data)
		m_system->pushDragDropEvent(GHOST_kEventDraggingDropDone, m_draggedObjectType, m_window, pt.x, pt.y, data);
		
	m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
	return S_OK;
}

/* 
 * Helpers
 */
 
DWORD GHOST_DropTargetWin32::allowedDropEffect(DWORD dwAllowed)
{
	DWORD dwEffect = DROPEFFECT_NONE;
	if (dwAllowed & DROPEFFECT_COPY)
		dwEffect = DROPEFFECT_COPY;

	return dwEffect;
}

GHOST_TDragnDropTypes GHOST_DropTargetWin32::getGhostType(IDataObject *pDataObject)
{
	/* Text
	 * Note: Unicode text is aviable as CF_TEXT too, the system can do the 
	 * conversion, but we do the conversion ourself with WC_NO_BEST_FIT_CHARS.
	 */
	FORMATETC fmtetc = { CF_TEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	if (pDataObject->QueryGetData(&fmtetc) == S_OK)
	{
		return GHOST_kDragnDropTypeString;
	}

	// Filesnames
	fmtetc.cfFormat = CF_HDROP;
	if (pDataObject->QueryGetData(&fmtetc) == S_OK)
	{
		return GHOST_kDragnDropTypeFilenames;
	}

	return GHOST_kDragnDropTypeUnknown;
}

void *GHOST_DropTargetWin32::getGhostData(IDataObject *pDataObject)
{
	GHOST_TDragnDropTypes type = getGhostType(pDataObject);
	switch (type)
	{
		case GHOST_kDragnDropTypeFilenames:
			return getDropDataAsFilenames(pDataObject);
			break;
		case GHOST_kDragnDropTypeString:
			return getDropDataAsString(pDataObject);
			break;
		case GHOST_kDragnDropTypeBitmap:
			//return getDropDataAsBitmap(pDataObject);
			break;
		default:
#ifdef GHOST_DEBUG
			::printf("\nGHOST_kDragnDropTypeUnknown");
#endif // GHOST_DEBUG
			return NULL;
			break;
	}
	return NULL;
}

void *GHOST_DropTargetWin32::getDropDataAsFilenames(IDataObject *pDataObject)
{
	UINT totfiles, nvalid = 0;
	WCHAR fpath[MAX_PATH];
	char *temp_path;
	GHOST_TStringArray *strArray = NULL;
	FORMATETC fmtetc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgmed;
	HDROP hdrop;

	// Check if dataobject supplies the format we want.
	// Double checking here, first in getGhostType.
	if (pDataObject->QueryGetData(&fmtetc) == S_OK)
	{
		if (pDataObject->GetData(&fmtetc, &stgmed) == S_OK)
		{
			hdrop = (HDROP) ::GlobalLock(stgmed.hGlobal);

			totfiles = ::DragQueryFileW(hdrop, -1, NULL, 0);
			if (!totfiles)
			{
				::GlobalUnlock(stgmed.hGlobal);
				return NULL;
			}

			strArray = (GHOST_TStringArray *) ::malloc(sizeof(GHOST_TStringArray));
			strArray->count = 0;
			strArray->strings = (GHOST_TUns8 **) ::malloc(totfiles * sizeof(GHOST_TUns8 *));

			for (UINT nfile = 0; nfile < totfiles; nfile++)
			{
				if (::DragQueryFileW(hdrop, nfile, fpath, MAX_PATH) > 0)
				{
					if (!(temp_path = alloc_utf_8_from_16(fpath, 0)) )
					{
						continue;
					} 
					// Just ignore paths that could not be converted verbatim.

					strArray->strings[nvalid] = (GHOST_TUns8 *) temp_path;
					strArray->count = nvalid + 1;
					nvalid++;
				}
			}
			// Free up memory.
			::GlobalUnlock(stgmed.hGlobal);
			::ReleaseStgMedium(&stgmed);
			
			return strArray;
		}
	}
	return NULL;
}

void *GHOST_DropTargetWin32::getDropDataAsString(IDataObject *pDataObject)
{
	char *tmp_string;
	FORMATETC fmtetc = { CF_UNICODETEXT, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgmed;

	// Try unicode first.
	// Check if dataobject supplies the format we want.
	if (pDataObject->QueryGetData(&fmtetc) == S_OK)
	{
		if (pDataObject->GetData(&fmtetc, &stgmed) == S_OK)
		{
			LPCWSTR wstr = (LPCWSTR) ::GlobalLock(stgmed.hGlobal);
			if (!(tmp_string = alloc_utf_8_from_16((wchar_t *)wstr, 0)) )
			{
				::GlobalUnlock(stgmed.hGlobal);
				return NULL;
			}
			// Free memory
			::GlobalUnlock(stgmed.hGlobal);
			::ReleaseStgMedium(&stgmed);
#ifdef GHOST_DEBUG
			::printf("\n<converted droped unicode string>\n%s\n</droped converted unicode string>\n", tmp_string);
#endif // GHOST_DEBUG
			return tmp_string;
		}
	}

	fmtetc.cfFormat = CF_TEXT;

	if (pDataObject->QueryGetData(&fmtetc) == S_OK)
	{
		if (pDataObject->GetData(&fmtetc, &stgmed) == S_OK)
		{
			char *str = (char *)::GlobalLock(stgmed.hGlobal);
			
			tmp_string = (char *)::malloc(::strlen(str) + 1);
			if (!tmp_string)
			{
				::GlobalUnlock(stgmed.hGlobal);
				return NULL;
			}

			if (!::strcpy(tmp_string, str) )
			{
				::free(tmp_string);
				::GlobalUnlock(stgmed.hGlobal);
				return NULL;
			}
			// Free memory
			::GlobalUnlock(stgmed.hGlobal);
			::ReleaseStgMedium(&stgmed);

			return tmp_string;
		}
	}
	
	return NULL;
}

int GHOST_DropTargetWin32::WideCharToANSI(LPCWSTR in, char * &out)
{
	int size;
	out = NULL; //caller should free if != NULL

	// Get the required size.
	size = ::WideCharToMultiByte(CP_ACP,        //System Default Codepage
	                             0x00000400,    // WC_NO_BEST_FIT_CHARS
	                             in,
	                             -1,            //-1 null terminated, makes output null terminated too.
	                             NULL,
	                             0,
	                             NULL, NULL
	                             );
	
	if (!size)
	{
#ifdef GHOST_DEBUG
		::printLastError();
#endif // GHOST_DEBUG
		return 0;
	}

	out = (char *)::malloc(size);
	if (!out)
	{
		::printf("\nmalloc failed!!!");
		return 0;
	}

	size = ::WideCharToMultiByte(CP_ACP,
	                             0x00000400,
	                             in,
	                             -1,
	                             (LPSTR) out,
	                             size,
	                             NULL, NULL
	                             );

	if (!size)
	{
#ifdef GHOST_DEBUG
		::printLastError();
#endif //GHOST_DEBUG
		::free(out);
		out = NULL;
	}
	return size;
}

#ifdef GHOST_DEBUG
void printLastError(void)
{
	LPTSTR s;
	DWORD err;

	err = GetLastError();
	if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
	                  FORMAT_MESSAGE_FROM_SYSTEM,
	                  NULL,
	                  err,
	                  0,
	                  (LPTSTR)&s,
	                  0,
	                  NULL)
	    )
	{
		printf("\nLastError: (%d) %s\n", (int)err, s);
		LocalFree(s);
	}
}
#endif // GHOST_DEBUG

