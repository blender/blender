/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/* NOTE(@ideasman42): 2023-04-07, see: !106608.
 * Even though Ubuntu's Unity (desktop-environment)
 * is no longer in widespread use, DBUS API's defined by: `libunity.so`
 * are still supported by some KDE/GNOME docks (as close as it gets to a standard it seems).
 * So when `libunity.so` is available, it can still be used to update a progress bar.
 *
 * Over time `libunity.so` has become less common on the Linux Desktop.
 * (it's no longer installed by default on most distributions, or even easily available).
 * So long term Blender should be updated to talk to DBUS directly,
 * perhaps as a dynamically loaded library which is used when available.
 *
 * This would allow X11/WAYLAND to share their task-bar implementation too. */

#include "GHOST_TaskbarX11.hh"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

using unity_get_entry_t = void *(*)(const char *);
using unity_set_progress_t = void (*)(void *, double);
using unity_set_progress_visible_t = void (*)(void *, int);
using unity_event_loop_t = int (*)(void *, int);

static unity_get_entry_t unity_get_entry;
static unity_set_progress_t unity_set_progress;
static unity_set_progress_visible_t unity_set_progress_visible;
static unity_event_loop_t unity_event_loop;

static bool libunity_initialized = false;
static bool libunity_available = false;
static void *libunity_handle = nullptr;

void GHOST_TaskBarX11::free()
{
  if (libunity_handle) {
    dlclose(libunity_handle);
    libunity_handle = nullptr;
  }
}

bool GHOST_TaskBarX11::init()
{
  if (libunity_initialized) {
    return libunity_available;
  }

  libunity_initialized = true;

  const char *libunity_names[] = {
      "libunity.so.4", "libunity.so.6", "libunity.so.9", "libunity.so", nullptr};
  for (int i = 0; libunity_names[i]; i++) {
    libunity_handle = dlopen(libunity_names[i], RTLD_LAZY);
    if (libunity_handle) {
      break;
    }
  }

  if (!libunity_handle) {
    return false;
  }

  unity_get_entry = (unity_get_entry_t)dlsym(libunity_handle,
                                             "unity_launcher_entry_get_for_desktop_id");
  if (!unity_get_entry) {
    fprintf(stderr, "failed to load libunity: %s\n", dlerror());
    return false;
  }
  unity_set_progress = (unity_set_progress_t)dlsym(libunity_handle,
                                                   "unity_launcher_entry_set_progress");
  if (!unity_set_progress) {
    fprintf(stderr, "failed to load libunity: %s\n", dlerror());
    return false;
  }
  unity_set_progress_visible = (unity_set_progress_visible_t)dlsym(
      libunity_handle, "unity_launcher_entry_set_progress_visible");
  if (!unity_set_progress_visible) {
    fprintf(stderr, "failed to load libunity: %s\n", dlerror());
    return false;
  }
  unity_event_loop = (unity_event_loop_t)dlsym(libunity_handle, "g_main_context_iteration");
  if (!unity_event_loop) {
    fprintf(stderr, "failed to load libunity: %s\n", dlerror());
    return false;
  }

  atexit(GHOST_TaskBarX11::free);

  libunity_available = true;
  return true;
}

GHOST_TaskBarX11::GHOST_TaskBarX11(const char *name)
{
  if (GHOST_TaskBarX11::init()) {
    handle = unity_get_entry(name);
  }
  else {
    handle = nullptr;
  }
}

bool GHOST_TaskBarX11::is_valid()
{
  return (handle != nullptr);
}

void GHOST_TaskBarX11::set_progress(double progress)
{
  assert(is_valid());
  unity_set_progress(handle, progress);
}

void GHOST_TaskBarX11::set_progress_enabled(bool enabled)
{
  assert(is_valid());
  unity_set_progress_visible(handle, enabled ? 1 : 0);
  unity_event_loop(nullptr, 0);
}
