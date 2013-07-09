/** \file ghost/intern/GHOST_TaskbarWin32.h
 *  \ingroup GHOST
 */
#ifndef __GHOST_TASKBARWIN32_H__
#define __GHOST_TASKBARWIN32_H__

#ifndef WIN32
#error WIN32 only!
#endif // WIN32

#define _WIN32_WINNT 0x501 // require Windows XP or newer
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>


// ITaskbarList, ITaskbarList2 and ITaskbarList3 might be missing, present here in that case.
// Note, ITaskbarList3 is supported only since Windows 7, though. Check for that is done in
// GHOST_WindowWin32
#ifndef __ITaskbarList_INTERFACE_DEFINED__
#define __ITaskbarList_INTERFACE_DEFINED__
extern "C" {const GUID CLSID_TaskbarList = {0x56FDF344, 0xFD6D, 0x11D0, {0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90} };
            const GUID IID_ITaskbarList  = {0x56FDF342, 0xFD6D, 0x11D0, {0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90} }; }
class ITaskbarList : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE HrInit(void) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddTab(HWND hwnd) = 0;
	virtual HRESULT STDMETHODCALLTYPE DeleteTab(HWND hwnd) = 0;
	virtual HRESULT STDMETHODCALLTYPE ActivateTab(HWND hwnd) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetActiveAlt(HWND hwnd) = 0;
};
#endif  /* ITaskbarList */

#ifndef __ITaskbarList2_INTERFACE_DEFINED__
#define __ITaskbarList2_INTERFACE_DEFINED__
extern "C" {const GUID IID_ITaskbarList2 = {0x602D4995, 0xB13A, 0x429b, {0xA6, 0x6E, 0x19, 0x35, 0xE4, 0x4F, 0x43, 0x17} }; }
class ITaskbarList2 : public ITaskbarList
{
public:
	virtual HRESULT STDMETHODCALLTYPE MarkFullscreenWindow(HWND hwnd, BOOL fFullscreen) = 0;
};
#endif  /* ITaskbarList2 */

#ifndef __ITaskbarList3_INTERFACE_DEFINED__
#define __ITaskbarList3_INTERFACE_DEFINED__
typedef enum THUMBBUTTONFLAGS {THBF_ENABLED = 0, THBF_DISABLED = 0x1, THBF_DISMISSONCLICK = 0x2, THBF_NOBACKGROUND = 0x4, THBF_HIDDEN = 0x8, THBF_NONINTERACTIVE = 0x10} THUMBBUTTONFLAGS;
typedef enum THUMBBUTTONMASK {THB_BITMAP = 0x1, THB_ICON = 0x2, THB_TOOLTIP = 0x4, THB_FLAGS = 0x8} THUMBBUTTONMASK;
typedef struct THUMBBUTTON {THUMBBUTTONMASK dwMask; UINT iId; UINT iBitmap; HICON hIcon; WCHAR szTip[260]; THUMBBUTTONFLAGS dwFlags; } THUMBBUTTON;
typedef enum TBPFLAG {TBPF_NOPROGRESS = 0, TBPF_INDETERMINATE = 0x1, TBPF_NORMAL = 0x2, TBPF_ERROR = 0x4, TBPF_PAUSED = 0x8 } TBPFLAG;
#define THBN_CLICKED  0x1800	
extern "C" {const GUID IID_ITaskList3 = { 0xEA1AFB91, 0x9E28, 0x4B86, {0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEF, 0xAF} }; }

class ITaskbarList3 : public ITaskbarList2
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetProgressValue(HWND hwnd, ULONGLONG ullCompleted, ULONGLONG ullTotal) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProgressState(HWND hwnd, TBPFLAG tbpFlags) = 0;
	virtual HRESULT STDMETHODCALLTYPE RegisterTab(HWND hwndTab, HWND hwndMDI) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnregisterTab(HWND hwndTab) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetTabOrder(HWND hwndTab,  HWND hwndInsertBefore) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetTabActive(HWND hwndTab,  HWND hwndMDI, DWORD dwReserved) = 0;
	virtual HRESULT STDMETHODCALLTYPE ThumbBarAddButtons(HWND hwnd, UINT cButtons, THUMBBUTTON *pButton) = 0;
	virtual HRESULT STDMETHODCALLTYPE ThumbBarUpdateButtons(HWND hwnd, UINT cButtons, THUMBBUTTON *pButton) = 0;
	virtual HRESULT STDMETHODCALLTYPE ThumbBarSetImageList(HWND hwnd, HIMAGELIST himl) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOverlayIcon(HWND hwnd, HICON hIcon, LPCWSTR pszDescription) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetThumbnailTooltip(HWND hwnd, LPCWSTR pszTip) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetThumbnailClip(HWND hwnd, RECT *prcClip) = 0;
};
#endif  /* ITaskbarList3 */

#endif /*__GHOST_TASKBARWIN32_H__*/
