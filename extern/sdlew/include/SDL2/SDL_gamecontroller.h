
#ifndef _SDL_gamecontroller_h
#define _SDL_gamecontroller_h

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_joystick.h"

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

typedef enum
{
    SDL_CONTROLLER_BINDTYPE_NONE = 0,
    SDL_CONTROLLER_BINDTYPE_BUTTON,
    SDL_CONTROLLER_BINDTYPE_AXIS,
    SDL_CONTROLLER_BINDTYPE_HAT
} SDL_GameControllerBindType;

typedef struct SDL_GameControllerButtonBind
{
    SDL_GameControllerBindType bindType;
    union
    {
        int button;
        int axis;
        struct {
            int hat;
            int hat_mask;
        } hat;
    } value;

} SDL_GameControllerButtonBind;

typedef int SDLCALL tSDL_GameControllerAddMapping( const char* mappingString );

typedef char * SDLCALL tSDL_GameControllerMappingForGUID( SDL_JoystickGUID guid );

typedef char * SDLCALL tSDL_GameControllerMapping( SDL_GameController * gamecontroller );

typedef SDL_bool SDLCALL tSDL_IsGameController(int joystick_index);

typedef const char * SDLCALL tSDL_GameControllerNameForIndex(int joystick_index);

typedef SDL_GameController * SDLCALL tSDL_GameControllerOpen(int joystick_index);

typedef const char * SDLCALL tSDL_GameControllerName(SDL_GameController *gamecontroller);

typedef SDL_bool SDLCALL tSDL_GameControllerGetAttached(SDL_GameController *gamecontroller);

typedef SDL_Joystick * SDLCALL tSDL_GameControllerGetJoystick(SDL_GameController *gamecontroller);

typedef int SDLCALL tSDL_GameControllerEventState(int state);

typedef void SDLCALL tSDL_GameControllerUpdate(void);

typedef enum
{
    SDL_CONTROLLER_AXIS_INVALID = -1,
    SDL_CONTROLLER_AXIS_LEFTX,
    SDL_CONTROLLER_AXIS_LEFTY,
    SDL_CONTROLLER_AXIS_RIGHTX,
    SDL_CONTROLLER_AXIS_RIGHTY,
    SDL_CONTROLLER_AXIS_TRIGGERLEFT,
    SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
    SDL_CONTROLLER_AXIS_MAX
} SDL_GameControllerAxis;

typedef SDL_GameControllerAxis SDLCALL tSDL_GameControllerGetAxisFromString(const char *pchString);

extern DECLSPEC const char* SDLCALL SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis axis);

extern DECLSPEC SDL_GameControllerButtonBind SDLCALL
SDL_GameControllerGetBindForAxis(SDL_GameController *gamecontroller,
                                 SDL_GameControllerAxis axis);

extern DECLSPEC Sint16 SDLCALL
SDL_GameControllerGetAxis(SDL_GameController *gamecontroller,
                          SDL_GameControllerAxis axis);

typedef enum
{
    SDL_CONTROLLER_BUTTON_INVALID = -1,
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
    SDL_CONTROLLER_BUTTON_MAX
} SDL_GameControllerButton;

typedef SDL_GameControllerButton SDLCALL tSDL_GameControllerGetButtonFromString(const char *pchString);

extern DECLSPEC const char* SDLCALL SDL_GameControllerGetStringForButton(SDL_GameControllerButton button);

extern DECLSPEC SDL_GameControllerButtonBind SDLCALL
SDL_GameControllerGetBindForButton(SDL_GameController *gamecontroller,
                                   SDL_GameControllerButton button);

typedef Uint8 SDLCALL tSDL_GameControllerGetButton(SDL_GameController *gamecontroller,
                                                          SDL_GameControllerButton button);

typedef void SDLCALL tSDL_GameControllerClose(SDL_GameController *gamecontroller);

extern tSDL_GameControllerAddMapping *SDL_GameControllerAddMapping;
extern tSDL_GameControllerMappingForGUID *SDL_GameControllerMappingForGUID;
extern tSDL_GameControllerMapping *SDL_GameControllerMapping;
extern tSDL_IsGameController *SDL_IsGameController;
extern tSDL_GameControllerNameForIndex *SDL_GameControllerNameForIndex;
extern tSDL_GameControllerOpen *SDL_GameControllerOpen;
extern tSDL_GameControllerName *SDL_GameControllerName;
extern tSDL_GameControllerGetAttached *SDL_GameControllerGetAttached;
extern tSDL_GameControllerGetJoystick *SDL_GameControllerGetJoystick;
extern tSDL_GameControllerEventState *SDL_GameControllerEventState;
extern tSDL_GameControllerUpdate *SDL_GameControllerUpdate;
extern tSDL_GameControllerGetAxisFromString *SDL_GameControllerGetAxisFromString;
extern tSDL_GameControllerGetButtonFromString *SDL_GameControllerGetButtonFromString;
extern tSDL_GameControllerGetButton *SDL_GameControllerGetButton;
extern tSDL_GameControllerClose *SDL_GameControllerClose;

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif

