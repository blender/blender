
#ifndef _SDL_video_h
#define _SDL_video_h

#include "SDL_stdinc.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_surface.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    Uint32 format;
    int w;
    int h;
    int refresh_rate;
    void *driverdata;
} SDL_DisplayMode;

typedef struct SDL_Window SDL_Window;

typedef enum
{
    SDL_WINDOW_FULLSCREEN = 0x00000001,
    SDL_WINDOW_OPENGL = 0x00000002,
    SDL_WINDOW_SHOWN = 0x00000004,
    SDL_WINDOW_HIDDEN = 0x00000008,
    SDL_WINDOW_BORDERLESS = 0x00000010,
    SDL_WINDOW_RESIZABLE = 0x00000020,
    SDL_WINDOW_MINIMIZED = 0x00000040,
    SDL_WINDOW_MAXIMIZED = 0x00000080,
    SDL_WINDOW_INPUT_GRABBED = 0x00000100,
    SDL_WINDOW_INPUT_FOCUS = 0x00000200,
    SDL_WINDOW_MOUSE_FOCUS = 0x00000400,
    SDL_WINDOW_FULLSCREEN_DESKTOP = ( SDL_WINDOW_FULLSCREEN | 0x00001000 ),
    SDL_WINDOW_FOREIGN = 0x00000800
} SDL_WindowFlags;

#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000
#define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X)  (SDL_WINDOWPOS_UNDEFINED_MASK|(X))
#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)
#define SDL_WINDOWPOS_ISUNDEFINED(X)    \
            (((X)&0xFFFF0000) == SDL_WINDOWPOS_UNDEFINED_MASK)

#define SDL_WINDOWPOS_CENTERED_MASK    0x2FFF0000
#define SDL_WINDOWPOS_CENTERED_DISPLAY(X)  (SDL_WINDOWPOS_CENTERED_MASK|(X))
#define SDL_WINDOWPOS_CENTERED         SDL_WINDOWPOS_CENTERED_DISPLAY(0)
#define SDL_WINDOWPOS_ISCENTERED(X)    \
            (((X)&0xFFFF0000) == SDL_WINDOWPOS_CENTERED_MASK)

typedef enum
{
    SDL_WINDOWEVENT_NONE,
    SDL_WINDOWEVENT_SHOWN,
    SDL_WINDOWEVENT_HIDDEN,
    SDL_WINDOWEVENT_EXPOSED,
    SDL_WINDOWEVENT_MOVED,
    SDL_WINDOWEVENT_RESIZED,
    SDL_WINDOWEVENT_SIZE_CHANGED,
    SDL_WINDOWEVENT_MINIMIZED,
    SDL_WINDOWEVENT_MAXIMIZED,
    SDL_WINDOWEVENT_RESTORED,
    SDL_WINDOWEVENT_ENTER,
    SDL_WINDOWEVENT_LEAVE,
    SDL_WINDOWEVENT_FOCUS_GAINED,
    SDL_WINDOWEVENT_FOCUS_LOST,
    SDL_WINDOWEVENT_CLOSE
} SDL_WindowEventID;

typedef void *SDL_GLContext;

typedef enum
{
    SDL_GL_RED_SIZE,
    SDL_GL_GREEN_SIZE,
    SDL_GL_BLUE_SIZE,
    SDL_GL_ALPHA_SIZE,
    SDL_GL_BUFFER_SIZE,
    SDL_GL_DOUBLEBUFFER,
    SDL_GL_DEPTH_SIZE,
    SDL_GL_STENCIL_SIZE,
    SDL_GL_ACCUM_RED_SIZE,
    SDL_GL_ACCUM_GREEN_SIZE,
    SDL_GL_ACCUM_BLUE_SIZE,
    SDL_GL_ACCUM_ALPHA_SIZE,
    SDL_GL_STEREO,
    SDL_GL_MULTISAMPLEBUFFERS,
    SDL_GL_MULTISAMPLESAMPLES,
    SDL_GL_ACCELERATED_VISUAL,
    SDL_GL_RETAINED_BACKING,
    SDL_GL_CONTEXT_MAJOR_VERSION,
    SDL_GL_CONTEXT_MINOR_VERSION,
    SDL_GL_CONTEXT_EGL,
    SDL_GL_CONTEXT_FLAGS,
    SDL_GL_CONTEXT_PROFILE_MASK,
    SDL_GL_SHARE_WITH_CURRENT_CONTEXT
} SDL_GLattr;

typedef enum
{
    SDL_GL_CONTEXT_PROFILE_CORE           = 0x0001,
    SDL_GL_CONTEXT_PROFILE_COMPATIBILITY  = 0x0002,
    SDL_GL_CONTEXT_PROFILE_ES             = 0x0004
} SDL_GLprofile;

typedef enum
{
    SDL_GL_CONTEXT_DEBUG_FLAG              = 0x0001,
    SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG = 0x0002,
    SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG      = 0x0004,
    SDL_GL_CONTEXT_RESET_ISOLATION_FLAG    = 0x0008
} SDL_GLcontextFlag;

typedef int SDLCALL tSDL_GetNumVideoDrivers(void);

typedef const char * SDLCALL tSDL_GetVideoDriver(int index);

typedef int SDLCALL tSDL_VideoInit(const char *driver_name);

typedef void SDLCALL tSDL_VideoQuit(void);

typedef const char * SDLCALL tSDL_GetCurrentVideoDriver(void);

typedef int SDLCALL tSDL_GetNumVideoDisplays(void);

typedef const char * SDLCALL tSDL_GetDisplayName(int displayIndex);

typedef int SDLCALL tSDL_GetDisplayBounds(int displayIndex, SDL_Rect * rect);

typedef int SDLCALL tSDL_GetNumDisplayModes(int displayIndex);

typedef int SDLCALL tSDL_GetDisplayMode(int displayIndex, int modeIndex,
                                               SDL_DisplayMode * mode);

typedef int SDLCALL tSDL_GetDesktopDisplayMode(int displayIndex, SDL_DisplayMode * mode);

typedef int SDLCALL tSDL_GetCurrentDisplayMode(int displayIndex, SDL_DisplayMode * mode);

typedef SDL_DisplayMode * SDLCALL tSDL_GetClosestDisplayMode(int displayIndex, const SDL_DisplayMode * mode, SDL_DisplayMode * closest);

typedef int SDLCALL tSDL_GetWindowDisplayIndex(SDL_Window * window);

typedef int SDLCALL tSDL_SetWindowDisplayMode(SDL_Window * window,
                                                     const SDL_DisplayMode
                                                         * mode);

typedef int SDLCALL tSDL_GetWindowDisplayMode(SDL_Window * window,
                                                     SDL_DisplayMode * mode);

typedef Uint32 SDLCALL tSDL_GetWindowPixelFormat(SDL_Window * window);

typedef SDL_Window * SDLCALL tSDL_CreateWindow(const char *title,
                                                      int x, int y, int w,
                                                      int h, Uint32 flags);

typedef SDL_Window * SDLCALL tSDL_CreateWindowFrom(const void *data);

typedef Uint32 SDLCALL tSDL_GetWindowID(SDL_Window * window);

typedef SDL_Window * SDLCALL tSDL_GetWindowFromID(Uint32 id);

typedef Uint32 SDLCALL tSDL_GetWindowFlags(SDL_Window * window);

typedef void SDLCALL tSDL_SetWindowTitle(SDL_Window * window,
                                                const char *title);

typedef const char * SDLCALL tSDL_GetWindowTitle(SDL_Window * window);

typedef void SDLCALL tSDL_SetWindowIcon(SDL_Window * window,
                                               SDL_Surface * icon);

extern DECLSPEC void* SDLCALL SDL_SetWindowData(SDL_Window * window,
                                                const char *name,
                                                void *userdata);

typedef void * SDLCALL tSDL_GetWindowData(SDL_Window * window,
                                                const char *name);

typedef void SDLCALL tSDL_SetWindowPosition(SDL_Window * window,
                                                   int x, int y);

typedef void SDLCALL tSDL_GetWindowPosition(SDL_Window * window,
                                                   int *x, int *y);

typedef void SDLCALL tSDL_SetWindowSize(SDL_Window * window, int w,
                                               int h);

typedef void SDLCALL tSDL_GetWindowSize(SDL_Window * window, int *w,
                                               int *h);

typedef void SDLCALL tSDL_SetWindowMinimumSize(SDL_Window * window,
                                                      int min_w, int min_h);

typedef void SDLCALL tSDL_GetWindowMinimumSize(SDL_Window * window,
                                                      int *w, int *h);

typedef void SDLCALL tSDL_SetWindowMaximumSize(SDL_Window * window,
                                                      int max_w, int max_h);

typedef void SDLCALL tSDL_GetWindowMaximumSize(SDL_Window * window,
                                                      int *w, int *h);

typedef void SDLCALL tSDL_SetWindowBordered(SDL_Window * window,
                                                   SDL_bool bordered);

typedef void SDLCALL tSDL_ShowWindow(SDL_Window * window);

typedef void SDLCALL tSDL_HideWindow(SDL_Window * window);

typedef void SDLCALL tSDL_RaiseWindow(SDL_Window * window);

typedef void SDLCALL tSDL_MaximizeWindow(SDL_Window * window);

typedef void SDLCALL tSDL_MinimizeWindow(SDL_Window * window);

typedef void SDLCALL tSDL_RestoreWindow(SDL_Window * window);

typedef int SDLCALL tSDL_SetWindowFullscreen(SDL_Window * window,
                                                    Uint32 flags);

typedef SDL_Surface * SDLCALL tSDL_GetWindowSurface(SDL_Window * window);

typedef int SDLCALL tSDL_UpdateWindowSurface(SDL_Window * window);

typedef int SDLCALL tSDL_UpdateWindowSurfaceRects(SDL_Window * window,
                                                         const SDL_Rect * rects,
                                                         int numrects);

typedef void SDLCALL tSDL_SetWindowGrab(SDL_Window * window,
                                               SDL_bool grabbed);

typedef SDL_bool SDLCALL tSDL_GetWindowGrab(SDL_Window * window);

typedef int SDLCALL tSDL_SetWindowBrightness(SDL_Window * window, float brightness);

typedef float SDLCALL tSDL_GetWindowBrightness(SDL_Window * window);

typedef int SDLCALL tSDL_SetWindowGammaRamp(SDL_Window * window,
                                                   const Uint16 * red,
                                                   const Uint16 * green,
                                                   const Uint16 * blue);

typedef int SDLCALL tSDL_GetWindowGammaRamp(SDL_Window * window,
                                                   Uint16 * red,
                                                   Uint16 * green,
                                                   Uint16 * blue);

typedef void SDLCALL tSDL_DestroyWindow(SDL_Window * window);

typedef SDL_bool SDLCALL tSDL_IsScreenSaverEnabled(void);

typedef void SDLCALL tSDL_EnableScreenSaver(void);

typedef void SDLCALL tSDL_DisableScreenSaver(void);

typedef int SDLCALL tSDL_GL_LoadLibrary(const char *path);

typedef void * SDLCALL tSDL_GL_GetProcAddress(const char *proc);

typedef void SDLCALL tSDL_GL_UnloadLibrary(void);

typedef SDL_bool SDLCALL tSDL_GL_ExtensionSupported(const char
                                                           *extension);

typedef int SDLCALL tSDL_GL_SetAttribute(SDL_GLattr attr, int value);

typedef int SDLCALL tSDL_GL_GetAttribute(SDL_GLattr attr, int *value);

typedef SDL_GLContext SDLCALL tSDL_GL_CreateContext(SDL_Window *
                                                           window);

typedef int SDLCALL tSDL_GL_MakeCurrent(SDL_Window * window,
                                               SDL_GLContext context);

extern DECLSPEC SDL_Window* SDLCALL SDL_GL_GetCurrentWindow(void);

typedef SDL_GLContext SDLCALL tSDL_GL_GetCurrentContext(void);

typedef int SDLCALL tSDL_GL_SetSwapInterval(int interval);

typedef int SDLCALL tSDL_GL_GetSwapInterval(void);

typedef void SDLCALL tSDL_GL_SwapWindow(SDL_Window * window);

typedef void SDLCALL tSDL_GL_DeleteContext(SDL_GLContext context);

extern tSDL_GetNumVideoDrivers *SDL_GetNumVideoDrivers;
extern tSDL_GetVideoDriver *SDL_GetVideoDriver;
extern tSDL_VideoInit *SDL_VideoInit;
extern tSDL_VideoQuit *SDL_VideoQuit;
extern tSDL_GetCurrentVideoDriver *SDL_GetCurrentVideoDriver;
extern tSDL_GetNumVideoDisplays *SDL_GetNumVideoDisplays;
extern tSDL_GetDisplayName *SDL_GetDisplayName;
extern tSDL_GetDisplayBounds *SDL_GetDisplayBounds;
extern tSDL_GetNumDisplayModes *SDL_GetNumDisplayModes;
extern tSDL_GetDisplayMode *SDL_GetDisplayMode;
extern tSDL_GetDesktopDisplayMode *SDL_GetDesktopDisplayMode;
extern tSDL_GetCurrentDisplayMode *SDL_GetCurrentDisplayMode;
extern tSDL_GetClosestDisplayMode *SDL_GetClosestDisplayMode;
extern tSDL_GetWindowDisplayIndex *SDL_GetWindowDisplayIndex;
extern tSDL_SetWindowDisplayMode *SDL_SetWindowDisplayMode;
extern tSDL_GetWindowDisplayMode *SDL_GetWindowDisplayMode;
extern tSDL_GetWindowPixelFormat *SDL_GetWindowPixelFormat;
extern tSDL_CreateWindow *SDL_CreateWindow;
extern tSDL_CreateWindowFrom *SDL_CreateWindowFrom;
extern tSDL_GetWindowID *SDL_GetWindowID;
extern tSDL_GetWindowFromID *SDL_GetWindowFromID;
extern tSDL_GetWindowFlags *SDL_GetWindowFlags;
extern tSDL_SetWindowTitle *SDL_SetWindowTitle;
extern tSDL_GetWindowTitle *SDL_GetWindowTitle;
extern tSDL_SetWindowIcon *SDL_SetWindowIcon;
extern tSDL_GetWindowData *SDL_GetWindowData;
extern tSDL_SetWindowPosition *SDL_SetWindowPosition;
extern tSDL_GetWindowPosition *SDL_GetWindowPosition;
extern tSDL_SetWindowSize *SDL_SetWindowSize;
extern tSDL_GetWindowSize *SDL_GetWindowSize;
extern tSDL_SetWindowMinimumSize *SDL_SetWindowMinimumSize;
extern tSDL_GetWindowMinimumSize *SDL_GetWindowMinimumSize;
extern tSDL_SetWindowMaximumSize *SDL_SetWindowMaximumSize;
extern tSDL_GetWindowMaximumSize *SDL_GetWindowMaximumSize;
extern tSDL_SetWindowBordered *SDL_SetWindowBordered;
extern tSDL_ShowWindow *SDL_ShowWindow;
extern tSDL_HideWindow *SDL_HideWindow;
extern tSDL_RaiseWindow *SDL_RaiseWindow;
extern tSDL_MaximizeWindow *SDL_MaximizeWindow;
extern tSDL_MinimizeWindow *SDL_MinimizeWindow;
extern tSDL_RestoreWindow *SDL_RestoreWindow;
extern tSDL_SetWindowFullscreen *SDL_SetWindowFullscreen;
extern tSDL_GetWindowSurface *SDL_GetWindowSurface;
extern tSDL_UpdateWindowSurface *SDL_UpdateWindowSurface;
extern tSDL_UpdateWindowSurfaceRects *SDL_UpdateWindowSurfaceRects;
extern tSDL_SetWindowGrab *SDL_SetWindowGrab;
extern tSDL_GetWindowGrab *SDL_GetWindowGrab;
extern tSDL_SetWindowBrightness *SDL_SetWindowBrightness;
extern tSDL_GetWindowBrightness *SDL_GetWindowBrightness;
extern tSDL_SetWindowGammaRamp *SDL_SetWindowGammaRamp;
extern tSDL_GetWindowGammaRamp *SDL_GetWindowGammaRamp;
extern tSDL_DestroyWindow *SDL_DestroyWindow;
extern tSDL_IsScreenSaverEnabled *SDL_IsScreenSaverEnabled;
extern tSDL_EnableScreenSaver *SDL_EnableScreenSaver;
extern tSDL_DisableScreenSaver *SDL_DisableScreenSaver;
extern tSDL_GL_LoadLibrary *SDL_GL_LoadLibrary;
extern tSDL_GL_GetProcAddress *SDL_GL_GetProcAddress;
extern tSDL_GL_UnloadLibrary *SDL_GL_UnloadLibrary;
extern tSDL_GL_ExtensionSupported *SDL_GL_ExtensionSupported;
extern tSDL_GL_SetAttribute *SDL_GL_SetAttribute;
extern tSDL_GL_GetAttribute *SDL_GL_GetAttribute;
extern tSDL_GL_CreateContext *SDL_GL_CreateContext;
extern tSDL_GL_MakeCurrent *SDL_GL_MakeCurrent;
extern tSDL_GL_GetCurrentContext *SDL_GL_GetCurrentContext;
extern tSDL_GL_SetSwapInterval *SDL_GL_SetSwapInterval;
extern tSDL_GL_GetSwapInterval *SDL_GL_GetSwapInterval;
extern tSDL_GL_SwapWindow *SDL_GL_SwapWindow;
extern tSDL_GL_DeleteContext *SDL_GL_DeleteContext;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

