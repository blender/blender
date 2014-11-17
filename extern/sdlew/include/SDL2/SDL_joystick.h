
#ifndef _SDL_joystick_h
#define _SDL_joystick_h

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;

typedef struct {
    Uint8 data[16];
} SDL_JoystickGUID;

typedef Sint32 SDL_JoystickID;

typedef int SDLCALL tSDL_NumJoysticks(void);

typedef const char * SDLCALL tSDL_JoystickNameForIndex(int device_index);

typedef SDL_Joystick * SDLCALL tSDL_JoystickOpen(int device_index);

typedef const char * SDLCALL tSDL_JoystickName(SDL_Joystick * joystick);

typedef SDL_JoystickGUID SDLCALL tSDL_JoystickGetDeviceGUID(int device_index);

typedef SDL_JoystickGUID SDLCALL tSDL_JoystickGetGUID(SDL_Joystick * joystick);

extern DECLSPEC void SDL_JoystickGetGUIDString(SDL_JoystickGUID guid, char *pszGUID, int cbGUID);

typedef SDL_JoystickGUID SDLCALL tSDL_JoystickGetGUIDFromString(const char *pchGUID);

typedef SDL_bool SDLCALL tSDL_JoystickGetAttached(SDL_Joystick * joystick);

typedef SDL_JoystickID SDLCALL tSDL_JoystickInstanceID(SDL_Joystick * joystick);

typedef int SDLCALL tSDL_JoystickNumAxes(SDL_Joystick * joystick);

typedef int SDLCALL tSDL_JoystickNumBalls(SDL_Joystick * joystick);

typedef int SDLCALL tSDL_JoystickNumHats(SDL_Joystick * joystick);

typedef int SDLCALL tSDL_JoystickNumButtons(SDL_Joystick * joystick);

typedef void SDLCALL tSDL_JoystickUpdate(void);

typedef int SDLCALL tSDL_JoystickEventState(int state);

typedef Sint16 SDLCALL tSDL_JoystickGetAxis(SDL_Joystick * joystick,
                                                   int axis);

#define SDL_HAT_CENTERED    0x00
#define SDL_HAT_UP      0x01
#define SDL_HAT_RIGHT       0x02
#define SDL_HAT_DOWN        0x04
#define SDL_HAT_LEFT        0x08
#define SDL_HAT_RIGHTUP     (SDL_HAT_RIGHT|SDL_HAT_UP)
#define SDL_HAT_RIGHTDOWN   (SDL_HAT_RIGHT|SDL_HAT_DOWN)
#define SDL_HAT_LEFTUP      (SDL_HAT_LEFT|SDL_HAT_UP)
#define SDL_HAT_LEFTDOWN    (SDL_HAT_LEFT|SDL_HAT_DOWN)

typedef Uint8 SDLCALL tSDL_JoystickGetHat(SDL_Joystick * joystick,
                                                 int hat);

typedef int SDLCALL tSDL_JoystickGetBall(SDL_Joystick * joystick,
                                                int ball, int *dx, int *dy);

typedef Uint8 SDLCALL tSDL_JoystickGetButton(SDL_Joystick * joystick,
                                                    int button);

typedef void SDLCALL tSDL_JoystickClose(SDL_Joystick * joystick);

extern tSDL_NumJoysticks *SDL_NumJoysticks;
extern tSDL_JoystickNameForIndex *SDL_JoystickNameForIndex;
extern tSDL_JoystickOpen *SDL_JoystickOpen;
extern tSDL_JoystickName *SDL_JoystickName;
extern tSDL_JoystickGetDeviceGUID *SDL_JoystickGetDeviceGUID;
extern tSDL_JoystickGetGUID *SDL_JoystickGetGUID;
extern tSDL_JoystickGetGUIDFromString *SDL_JoystickGetGUIDFromString;
extern tSDL_JoystickGetAttached *SDL_JoystickGetAttached;
extern tSDL_JoystickInstanceID *SDL_JoystickInstanceID;
extern tSDL_JoystickNumAxes *SDL_JoystickNumAxes;
extern tSDL_JoystickNumBalls *SDL_JoystickNumBalls;
extern tSDL_JoystickNumHats *SDL_JoystickNumHats;
extern tSDL_JoystickNumButtons *SDL_JoystickNumButtons;
extern tSDL_JoystickUpdate *SDL_JoystickUpdate;
extern tSDL_JoystickEventState *SDL_JoystickEventState;
extern tSDL_JoystickGetAxis *SDL_JoystickGetAxis;
extern tSDL_JoystickGetHat *SDL_JoystickGetHat;
extern tSDL_JoystickGetBall *SDL_JoystickGetBall;
extern tSDL_JoystickGetButton *SDL_JoystickGetButton;
extern tSDL_JoystickClose *SDL_JoystickClose;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

