
#ifndef _SDL_events_h
#define _SDL_events_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_keyboard.h"
#include "SDL_mouse.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "SDL_quit.h"
#include "SDL_gesture.h"
#include "SDL_touch.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDL_RELEASED    0
#define SDL_PRESSED 1

typedef enum
{
    SDL_FIRSTEVENT     = 0,

    SDL_QUIT           = 0x100,

    SDL_APP_TERMINATING,
    SDL_APP_LOWMEMORY,
    SDL_APP_WILLENTERBACKGROUND,
    SDL_APP_DIDENTERBACKGROUND,
    SDL_APP_WILLENTERFOREGROUND,
    SDL_APP_DIDENTERFOREGROUND,

    SDL_WINDOWEVENT    = 0x200,
    SDL_SYSWMEVENT,

    SDL_KEYDOWN        = 0x300,
    SDL_KEYUP,
    SDL_TEXTEDITING,
    SDL_TEXTINPUT,

    SDL_MOUSEMOTION    = 0x400,
    SDL_MOUSEBUTTONDOWN,
    SDL_MOUSEBUTTONUP,
    SDL_MOUSEWHEEL,

    SDL_JOYAXISMOTION  = 0x600,
    SDL_JOYBALLMOTION,
    SDL_JOYHATMOTION,
    SDL_JOYBUTTONDOWN,
    SDL_JOYBUTTONUP,
    SDL_JOYDEVICEADDED,
    SDL_JOYDEVICEREMOVED,

    SDL_CONTROLLERAXISMOTION  = 0x650,
    SDL_CONTROLLERBUTTONDOWN,
    SDL_CONTROLLERBUTTONUP,
    SDL_CONTROLLERDEVICEADDED,
    SDL_CONTROLLERDEVICEREMOVED,
    SDL_CONTROLLERDEVICEREMAPPED,

    SDL_FINGERDOWN      = 0x700,
    SDL_FINGERUP,
    SDL_FINGERMOTION,

    SDL_DOLLARGESTURE   = 0x800,
    SDL_DOLLARRECORD,
    SDL_MULTIGESTURE,

    SDL_CLIPBOARDUPDATE = 0x900,

    SDL_DROPFILE        = 0x1000,

    SDL_USEREVENT    = 0x8000,

    SDL_LASTEVENT    = 0xFFFF
} SDL_EventType;

typedef struct SDL_CommonEvent
{
    Uint32 type;
    Uint32 timestamp;
} SDL_CommonEvent;

typedef struct SDL_WindowEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 event;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint32 data1;
    Sint32 data2;
} SDL_WindowEvent;

typedef struct SDL_KeyboardEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint8 state;
    Uint8 repeat;
    Uint8 padding2;
    Uint8 padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

#define SDL_TEXTEDITINGEVENT_TEXT_SIZE (32)

typedef struct SDL_TextEditingEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    char text[SDL_TEXTEDITINGEVENT_TEXT_SIZE];
    Sint32 start;
    Sint32 length;
} SDL_TextEditingEvent;

#define SDL_TEXTINPUTEVENT_TEXT_SIZE (32)

typedef struct SDL_TextInputEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    char text[SDL_TEXTINPUTEVENT_TEXT_SIZE];
} SDL_TextInputEvent;

typedef struct SDL_MouseMotionEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Uint32 state;
    Sint32 x;
    Sint32 y;
    Sint32 xrel;
    Sint32 yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Uint8 button;
    Uint8 state;
    Uint8 padding1;
    Uint8 padding2;
    Sint32 x;
    Sint32 y;
} SDL_MouseButtonEvent;

typedef struct SDL_MouseWheelEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Uint32 which;
    Sint32 x;
    Sint32 y;
} SDL_MouseWheelEvent;

typedef struct SDL_JoyAxisEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
} SDL_JoyAxisEvent;

typedef struct SDL_JoyBallEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 ball;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 xrel;
    Sint16 yrel;
} SDL_JoyBallEvent;

typedef struct SDL_JoyHatEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 hat;
    Uint8 value;
    Uint8 padding1;
    Uint8 padding2;
} SDL_JoyHatEvent;

typedef struct SDL_JoyButtonEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 button;
    Uint8 state;
    Uint8 padding1;
    Uint8 padding2;
} SDL_JoyButtonEvent;

typedef struct SDL_JoyDeviceEvent
{
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
} SDL_JoyDeviceEvent;

typedef struct SDL_ControllerAxisEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 axis;
    Uint8 padding1;
    Uint8 padding2;
    Uint8 padding3;
    Sint16 value;
    Uint16 padding4;
} SDL_ControllerAxisEvent;

typedef struct SDL_ControllerButtonEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_JoystickID which;
    Uint8 button;
    Uint8 state;
    Uint8 padding1;
    Uint8 padding2;
} SDL_ControllerButtonEvent;

typedef struct SDL_ControllerDeviceEvent
{
    Uint32 type;
    Uint32 timestamp;
    Sint32 which;
} SDL_ControllerDeviceEvent;

typedef struct SDL_TouchFingerEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_TouchID touchId;
    SDL_FingerID fingerId;
    float x;
    float y;
    float dx;
    float dy;
    float pressure;
} SDL_TouchFingerEvent;

typedef struct SDL_MultiGestureEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_TouchID touchId;
    float dTheta;
    float dDist;
    float x;
    float y;
    Uint16 numFingers;
    Uint16 padding;
} SDL_MultiGestureEvent;

typedef struct SDL_DollarGestureEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_TouchID touchId;
    SDL_GestureID gestureId;
    Uint32 numFingers;
    float error;
    float x;
    float y;
} SDL_DollarGestureEvent;

typedef struct SDL_DropEvent
{
    Uint32 type;
    Uint32 timestamp;
    char *file;
} SDL_DropEvent;

typedef struct SDL_QuitEvent
{
    Uint32 type;
    Uint32 timestamp;
} SDL_QuitEvent;

typedef struct SDL_OSEvent
{
    Uint32 type;
    Uint32 timestamp;
} SDL_OSEvent;

typedef struct SDL_UserEvent
{
    Uint32 type;
    Uint32 timestamp;
    Uint32 windowID;
    Sint32 code;
    void *data1;
    void *data2;
} SDL_UserEvent;

struct SDL_SysWMmsg;
typedef struct SDL_SysWMmsg SDL_SysWMmsg;

typedef struct SDL_SysWMEvent
{
    Uint32 type;
    Uint32 timestamp;
    SDL_SysWMmsg *msg;
} SDL_SysWMEvent;

typedef union SDL_Event
{
    Uint32 type;
    SDL_CommonEvent common;
    SDL_WindowEvent window;
    SDL_KeyboardEvent key;
    SDL_TextEditingEvent edit;
    SDL_TextInputEvent text;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_MouseWheelEvent wheel;
    SDL_JoyAxisEvent jaxis;
    SDL_JoyBallEvent jball;
    SDL_JoyHatEvent jhat;
    SDL_JoyButtonEvent jbutton;
    SDL_JoyDeviceEvent jdevice;
    SDL_ControllerAxisEvent caxis;
    SDL_ControllerButtonEvent cbutton;
    SDL_ControllerDeviceEvent cdevice;
    SDL_QuitEvent quit;
    SDL_UserEvent user;
    SDL_SysWMEvent syswm;
    SDL_TouchFingerEvent tfinger;
    SDL_MultiGestureEvent mgesture;
    SDL_DollarGestureEvent dgesture;
    SDL_DropEvent drop;

    Uint8 padding[56];
} SDL_Event;

typedef void SDLCALL tSDL_PumpEvents(void);

typedef enum
{
    SDL_ADDEVENT,
    SDL_PEEKEVENT,
    SDL_GETEVENT
} SDL_eventaction;

typedef int SDLCALL tSDL_PeepEvents(SDL_Event * events, int numevents,
                                           SDL_eventaction action,
                                           Uint32 minType, Uint32 maxType);

typedef SDL_bool SDLCALL tSDL_HasEvent(Uint32 type);
typedef SDL_bool SDLCALL tSDL_HasEvents(Uint32 minType, Uint32 maxType);

typedef void SDLCALL tSDL_FlushEvent(Uint32 type);
typedef void SDLCALL tSDL_FlushEvents(Uint32 minType, Uint32 maxType);

typedef int SDLCALL tSDL_PollEvent(SDL_Event * event);

typedef int SDLCALL tSDL_WaitEvent(SDL_Event * event);

typedef int SDLCALL tSDL_WaitEventTimeout(SDL_Event * event,
                                                 int timeout);

typedef int SDLCALL tSDL_PushEvent(SDL_Event * event);

typedef int (SDLCALL * SDL_EventFilter) (void *userdata, SDL_Event * event);

typedef void SDLCALL tSDL_SetEventFilter(SDL_EventFilter filter,
                                                void *userdata);

typedef SDL_bool SDLCALL tSDL_GetEventFilter(SDL_EventFilter * filter,
                                                    void **userdata);

typedef void SDLCALL tSDL_AddEventWatch(SDL_EventFilter filter,
                                               void *userdata);

typedef void SDLCALL tSDL_DelEventWatch(SDL_EventFilter filter,
                                               void *userdata);

typedef void SDLCALL tSDL_FilterEvents(SDL_EventFilter filter,
                                              void *userdata);

#define SDL_QUERY   -1
#define SDL_IGNORE   0
#define SDL_DISABLE  0
#define SDL_ENABLE   1

typedef Uint8 SDLCALL tSDL_EventState(Uint32 type, int state);

#define SDL_GetEventState(type) SDL_EventState(type, SDL_QUERY)

typedef Uint32 SDLCALL tSDL_RegisterEvents(int numevents);

extern tSDL_PumpEvents *SDL_PumpEvents;
extern tSDL_PeepEvents *SDL_PeepEvents;
extern tSDL_HasEvent *SDL_HasEvent;
extern tSDL_HasEvents *SDL_HasEvents;
extern tSDL_FlushEvent *SDL_FlushEvent;
extern tSDL_FlushEvents *SDL_FlushEvents;
extern tSDL_PollEvent *SDL_PollEvent;
extern tSDL_WaitEvent *SDL_WaitEvent;
extern tSDL_WaitEventTimeout *SDL_WaitEventTimeout;
extern tSDL_PushEvent *SDL_PushEvent;
extern tSDL_SetEventFilter *SDL_SetEventFilter;
extern tSDL_GetEventFilter *SDL_GetEventFilter;
extern tSDL_AddEventWatch *SDL_AddEventWatch;
extern tSDL_DelEventWatch *SDL_DelEventWatch;
extern tSDL_FilterEvents *SDL_FilterEvents;
extern tSDL_EventState *SDL_EventState;
extern tSDL_RegisterEvents *SDL_RegisterEvents;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

