/* SPDX-FileCopyrightText: 2010-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#define DEBUG_NDOF_DRIVER false

#include "GHOST_NDOFManagerCocoa.hh"
#include "GHOST_SystemCocoa.hh"
#import <Cocoa/Cocoa.h>

#include <dlfcn.h>
#include <stdint.h>

#if DEBUG_NDOF_DRIVER
#  include <cstdio>
#endif

/* Static callback functions need to talk to these objects: */
static GHOST_SystemCocoa *ghost_system = nullptr;
static GHOST_NDOFManager *ndof_manager = nullptr;

static uint16_t clientID = 0;

static bool driver_loaded = false;

/* 3DxMacCore version >= minimal_version is considered "new".
 * It was firstly introduced in 3DxWare v10.8.4 r3716 and can process
 * (not yet documented in SDK manual) kConnexionCmdAppEvent events. */
static NSString *new_driver_minimal_version = @"1.3.4.473";

/* Replicate just enough of the 3Dx API for our uses, not everything the driver provides. */

#define kConnexionClientModeTakeOver 1
#define kConnexionMaskAll 0x3fff
#define kConnexionMaskAxis 0x3f00
#define kConnexionMaskNoButtons 0x0
#define kConnexionMaskAllButtons 0xffffffff
#define kConnexionCmdHandleButtons 2
#define kConnexionCmdHandleAxis 3
#define kConnexionCmdAppSpecific 10
#define kConnexionCmdAppEvent 11
#define kConnexionMsgDeviceState '3dSR'
#define kConnexionCtlGetDeviceID '3did'

#pragma pack(push, 2) /* Just this struct. */
struct ConnexionDeviceState {
  uint16_t version;
  uint16_t client;
  uint16_t command;
  int16_t param;
  int32_t value;
  uint64_t time;
  uint8_t report[8];
  uint16_t appEventPressed;
  int16_t axis[6]; /* TX, TY, TZ, RX, RY, RZ. */
  uint16_t address;
  uint32_t buttons;
};
#pragma pack(pop)

/* Callback functions: */
typedef void (*AddedHandler)(uint32_t);
typedef void (*RemovedHandler)(uint32_t);
typedef void (*MessageHandler)(uint32_t, uint32_t msg_type, void *msg_arg);

/* Driver functions: */
typedef int16_t (*SetConnexionHandlers_ptr)(MessageHandler, AddedHandler, RemovedHandler, bool);
typedef void (*CleanupConnexionHandlers_ptr)();
typedef uint16_t (*RegisterConnexionClient_ptr)(uint32_t signature,
                                                const char *name,
                                                uint16_t mode,
                                                uint32_t mask);
typedef void (*SetConnexionClientButtonMask_ptr)(uint16_t clientID, uint32_t buttonMask);
typedef void (*UnregisterConnexionClient_ptr)(uint16_t clientID);
typedef int16_t (*ConnexionClientControl_ptr)(uint16_t clientID,
                                              uint32_t message,
                                              int32_t param,
                                              int32_t *result);

#define DECLARE_FUNC(name) name##_ptr name = nullptr

DECLARE_FUNC(SetConnexionHandlers);
DECLARE_FUNC(CleanupConnexionHandlers);
DECLARE_FUNC(RegisterConnexionClient);
DECLARE_FUNC(SetConnexionClientButtonMask);
DECLARE_FUNC(UnregisterConnexionClient);
DECLARE_FUNC(ConnexionClientControl);

static void *load_func(void *module, const char *func_name)
{
  void *func = dlsym(module, func_name);

#if DEBUG_NDOF_DRIVER
  if (func) {
    printf("'%s' loaded :D\n", func_name);
  }
  else {
    printf("<!> %s\n", dlerror());
  }
#endif

  return func;
}

#define LOAD_FUNC(name) name = (name##_ptr)load_func(module, #name)

static void *module; /* Handle to the whole driver. */

static bool load_driver_functions()
{
  if (driver_loaded) {
    return true;
  }

  module = dlopen("/Library/Frameworks/3DconnexionClient.framework/3DconnexionClient",
                  RTLD_LAZY | RTLD_LOCAL);

  if (module) {
    LOAD_FUNC(SetConnexionHandlers);

    if (SetConnexionHandlers != nullptr) {
      driver_loaded = true;
    }

    if (driver_loaded) {
      LOAD_FUNC(CleanupConnexionHandlers);
      LOAD_FUNC(RegisterConnexionClient);
      LOAD_FUNC(SetConnexionClientButtonMask);
      LOAD_FUNC(UnregisterConnexionClient);
      LOAD_FUNC(ConnexionClientControl);
    }
  }
#if DEBUG_NDOF_DRIVER
  else {
    printf("<!> %s\n", dlerror());
  }

  printf("loaded: %s\n", driver_loaded ? "YES" : "NO");
#endif

  return driver_loaded;
}

static void unload_driver()
{
  dlclose(module);
}

static void DeviceAdded(uint32_t /*unused*/)
{
#if DEBUG_NDOF_DRIVER
  printf("ndof: device added\n");
#endif

  /* Determine exactly which device is plugged in. */
  int32_t result;
  ConnexionClientControl(clientID, kConnexionCtlGetDeviceID, 0, &result);
  const int16_t vendorID = result >> 16;
  const int16_t productID = result & 0xffff;

  ndof_manager->setDevice(vendorID, productID);
}

static void DeviceRemoved(uint32_t /*unused*/)
{
#if DEBUG_NDOF_DRIVER
  printf("ndof: device removed\n");
#endif
}

static void DeviceEvent(uint32_t /*unused*/, uint32_t msg_type, void *msg_arg)
{
  if (msg_type == kConnexionMsgDeviceState) {
    ConnexionDeviceState *s = (ConnexionDeviceState *)msg_arg;

    /* Device state is broadcast to all clients; only react if sent to us. */
    if (s->client == clientID) {
      /* TODO: is s->time compatible with GHOST timestamps? if so use that instead. */
      const uint64_t now = ghost_system->getMilliSeconds();

      switch (s->command) {
        case kConnexionCmdHandleAxis: {
          /* convert to blender view coordinates. */
          const int t[3] = {s->axis[0], -(s->axis[2]), s->axis[1]};
          const int r[3] = {-(s->axis[3]), s->axis[5], -(s->axis[4])};

          ndof_manager->updateTranslation(t, now);
          ndof_manager->updateRotation(r, now);

          ghost_system->notifyExternalEventProcessed();
          break;
        }
        case kConnexionCmdHandleButtons: {
          const int button_bits = s->buttons;
#ifdef DEBUG_NDOF_BUTTONS
          printf("button bits: 0x%08x\n", button_bits);
#endif
          ndof_manager->updateButtonsBitmask(button_bits, now);
          ghost_system->notifyExternalEventProcessed();
          break;
        }
        case kConnexionCmdAppEvent: {
          const int button_number = s->value;
          const bool pressed = s->appEventPressed;
#ifdef DEBUG_NDOF_BUTTONS
          printf("button number: %d, pressed: %d\n", button_number, pressed);
#endif
          ndof_manager->updateButton(GHOST_NDOF_ButtonT(button_number), pressed, now);
          ghost_system->notifyExternalEventProcessed();
          break;
        }
#if DEBUG_NDOF_DRIVER
        case kConnexionCmdAppSpecific:
          printf("ndof: app-specific command, param = %hd, value = %d\n", s->param, s->value);
          break;

        default:
          printf("ndof: mystery device command %d\n", s->command);
#endif
      }
    }
  }
}

GHOST_NDOFManagerCocoa::GHOST_NDOFManagerCocoa(GHOST_System &sys) : GHOST_NDOFManager(sys)
{
  if (load_driver_functions()) {
    /* Give static functions something to talk to: */
    ghost_system = dynamic_cast<GHOST_SystemCocoa *>(&sys);
    ndof_manager = this;

    const uint16_t error = SetConnexionHandlers(DeviceEvent, DeviceAdded, DeviceRemoved, true);

    if (error) {
#if DEBUG_NDOF_DRIVER
      printf("ndof: error %d while setting up handlers\n", error);
#endif
      return;
    }

    const NSDictionary *dictInfos =
        [NSBundle bundleWithPath:@"/Library/Frameworks/3DconnexionClient.framework"]
            .infoDictionary;
    NSString *strVersion = [dictInfos objectForKey:(NSString *)kCFBundleVersionKey];
    const auto compare = [strVersion compare:new_driver_minimal_version];
    const bool has_new_driver = compare != NSOrderedAscending;

    /* New driver makes use of kConnexionCmdAppEvent events, which require to have all buttons
     * unmasked. Basically, this means that driver consumes all NDOF device input and then sends
     * appropriate app events based on its configuration instead of forwarding raw data to the
     * application. When using an older driver, the old solution with all buttons forwarded
     * (masked) is preferred. */
    const uint32_t client_mask = has_new_driver ? kConnexionMaskAxis : kConnexionMaskAll;
    const uint32_t button_mask = has_new_driver ? kConnexionMaskNoButtons :
                                                  kConnexionMaskAllButtons;

    /* Pascal string *and* a four-letter constant. How old-school. */
    clientID = RegisterConnexionClient(
        'blnd', "\007blender", kConnexionClientModeTakeOver, client_mask);

    SetConnexionClientButtonMask(clientID, button_mask);
  }
}

GHOST_NDOFManagerCocoa::~GHOST_NDOFManagerCocoa()
{
  if (driver_loaded) {
    UnregisterConnexionClient(clientID);
    CleanupConnexionHandlers();
    unload_driver();

    ghost_system = nullptr;
    ndof_manager = nullptr;
  }
}

bool GHOST_NDOFManagerCocoa::available()
{
  return driver_loaded;
}
