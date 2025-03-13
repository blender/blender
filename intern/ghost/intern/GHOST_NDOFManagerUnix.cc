/* SPDX-FileCopyrightText: 2011-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_NDOFManagerUnix.hh"
#include "GHOST_System.hh"

/* Logging, use `ghost.ndof.unix.*` prefix. */
#include "CLG_log.h"

#include <cstdio>
#include <spnav.h>
#include <unistd.h>

static const char *spnav_sock_path = "/var/run/spnav.sock";

static CLG_LogRef LOG_NDOF_UNIX = {"ghost.ndof.unix"};
#define LOG (&LOG_NDOF_UNIX)

GHOST_NDOFManagerUnix::GHOST_NDOFManagerUnix(GHOST_System &sys)
    : GHOST_NDOFManager(sys), available_(false)
{
  if (access(spnav_sock_path, F_OK) != 0) {
    CLOG_INFO(LOG, 1, "'spacenavd' not found at \"%s\"", spnav_sock_path);
  }
  else if (spnav_open() != -1) {
    CLOG_INFO(LOG, 1, "'spacenavd' found at\"%s\"", spnav_sock_path);
    available_ = true;

    /* determine exactly which device (if any) is plugged in */

#define MAX_LINE_LENGTH 100

    /* look for USB devices with Logitech or 3Dconnexion's vendor ID */
    FILE *command_output = popen("lsusb | grep '046d:\\|256f:'", "r");
    if (command_output) {
      char line[MAX_LINE_LENGTH] = {0};
      while (fgets(line, MAX_LINE_LENGTH, command_output)) {
        ushort vendor_id = 0, product_id = 0;
        if (sscanf(line, "Bus %*d Device %*d: ID %hx:%hx", &vendor_id, &product_id) == 2) {
          if (setDevice(vendor_id, product_id)) {
            break; /* stop looking once the first 3D mouse is found */
          }
        }
      }
      pclose(command_output);
    }
  }
}

GHOST_NDOFManagerUnix::~GHOST_NDOFManagerUnix()
{
  if (available_) {
    spnav_close();
  }
}

bool GHOST_NDOFManagerUnix::available()
{
  return available_;
}

/**
 * Workaround for a problem where we don't enter the #GHOST_kFinished state,
 * this causes any proceeding event to have a very high `dt` (time delta),
 * many seconds for eg, causing the view to jump.
 *
 * this workaround expects continuous events, if we miss a motion event,
 * immediately send a dummy event with no motion to ensure the finished state is reached.
 */
#define USE_FINISH_GLITCH_WORKAROUND
/* TODO: make this available on all platforms */

#ifdef USE_FINISH_GLITCH_WORKAROUND
static bool motion_test_prev = false;
static uint64_t motion_test_prev_time = 0;
/* NOTE(ideasman42): It's important not to generate zero-motion event immediately
 * because the SPNAV API may not have had a chance to generate events since the last
 * call to this function.
 * In my tests values between 50-100ms work well, opt for the larger value
 * to avoid false positives:
 * - Releasing the device can cause subtle wobble generating start/finish events.
 * - Subtle motion can *sometimes* cause start/finish events too.
 *
 * Furthermore, waiting a 10/th of a second for the "finish" event is fast enough
 * that any motion occurring in a shorter time-span can be considered "continuous". */
#  define MOTION_TEST_IDLE_MS 100
#endif /* USE_FINISH_GLITCH_WORKAROUND */

bool GHOST_NDOFManagerUnix::processEvents()
{
  bool anyProcessed = false;

  if (available_) {
    spnav_event e;

#ifdef USE_FINISH_GLITCH_WORKAROUND
    bool motion_test = false;
#endif

    while (spnav_poll_event(&e)) {
      switch (e.type) {
        case SPNAV_EVENT_MOTION: {
          /* convert to blender view coords */
          uint64_t now = system_.getMilliSeconds();
          const int t[3] = {int(e.motion.x), int(e.motion.y), int(-e.motion.z)};
          const int r[3] = {int(-e.motion.rx), int(-e.motion.ry), int(e.motion.rz)};

          updateTranslation(t, now);
          updateRotation(r, now);
#ifdef USE_FINISH_GLITCH_WORKAROUND
          motion_test = true;
          motion_test_prev_time = now;
#endif
          break;
        }
        case SPNAV_EVENT_BUTTON:
          uint64_t now = system_.getMilliSeconds();
          updateButton(e.button.bnum, e.button.press, now);
          break;
      }
      anyProcessed = true;
    }

#ifdef USE_FINISH_GLITCH_WORKAROUND
    if (motion_test_prev == true && motion_test == false) {
      const uint64_t now = system_.getMilliSeconds();
      GHOST_ASSERT(motion_test_prev_time < now, "Invalid time offset");
      if ((now - motion_test_prev_time) < MOTION_TEST_IDLE_MS) {
        /* Re-run this check next time `processEvents` is called. */
        motion_test = true;
      }
      else {
        const int v[3] = {0, 0, 0};

        updateTranslation(v, now);
        updateRotation(v, now);

        anyProcessed = true;
      }
    }
    motion_test_prev = motion_test;
#endif /* USE_FINISH_GLITCH_WORKAROUND */
  }

  return anyProcessed;
}

#undef USE_FINISH_GLITCH_WORKAROUND
