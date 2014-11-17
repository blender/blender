
#ifndef _SDL_syswm_h
#define _SDL_syswm_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_version.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SDL_PROTOTYPES_ONLY
struct SDL_SysWMinfo;
#else

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(SDL_VIDEO_DRIVER_X11)
#if defined(__APPLE__) && defined(__MACH__)

#define Cursor X11Cursor
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#if defined(__APPLE__) && defined(__MACH__)

#undef Cursor
#endif

#endif

#if defined(SDL_VIDEO_DRIVER_DIRECTFB)
#include <directfb.h>
#endif

#if defined(SDL_VIDEO_DRIVER_COCOA)
#ifdef __OBJC__
#include <Cocoa/Cocoa.h>
#else
typedef struct _NSWindow NSWindow;
#endif
#endif

#if defined(SDL_VIDEO_DRIVER_UIKIT)
#ifdef __OBJC__
#include <UIKit/UIKit.h>
#else
typedef struct _UIWindow UIWindow;
#endif
#endif

typedef enum
{
    SDL_SYSWM_UNKNOWN,
    SDL_SYSWM_WINDOWS,
    SDL_SYSWM_X11,
    SDL_SYSWM_DIRECTFB,
    SDL_SYSWM_COCOA,
    SDL_SYSWM_UIKIT,
} SDL_SYSWM_TYPE;

struct SDL_SysWMmsg
{
    SDL_version version;
    SDL_SYSWM_TYPE subsystem;
    union
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        struct {
            HWND hwnd;
            UINT msg;
            WPARAM wParam;
            LPARAM lParam;
        } win;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
        struct {
            XEvent event;
        } x11;
#endif
#if defined(SDL_VIDEO_DRIVER_DIRECTFB)
        struct {
            DFBEvent event;
        } dfb;
#endif
#if defined(SDL_VIDEO_DRIVER_COCOA)
        struct
        {

        } cocoa;
#endif
#if defined(SDL_VIDEO_DRIVER_UIKIT)
        struct
        {

        } uikit;
#endif

        int dummy;
    } msg;
};

struct SDL_SysWMinfo
{
    SDL_version version;
    SDL_SYSWM_TYPE subsystem;
    union
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        struct
        {
            HWND window;
        } win;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
        struct
        {
            Display *display;
            Window window;
        } x11;
#endif
#if defined(SDL_VIDEO_DRIVER_DIRECTFB)
        struct
        {
            IDirectFB *dfb;
            IDirectFBWindow *window;
            IDirectFBSurface *surface;
        } dfb;
#endif
#if defined(SDL_VIDEO_DRIVER_COCOA)
        struct
        {
            NSWindow *window;
        } cocoa;
#endif
#if defined(SDL_VIDEO_DRIVER_UIKIT)
        struct
        {
            UIWindow *window;
        } uikit;
#endif

        int dummy;
    } info;
};

#endif

typedef struct SDL_SysWMinfo SDL_SysWMinfo;

typedef SDL_bool SDLCALL tSDL_GetWindowWMInfo(SDL_Window * window,
                                                     SDL_SysWMinfo * info);

extern tSDL_GetWindowWMInfo *SDL_GetWindowWMInfo;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

