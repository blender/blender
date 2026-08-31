/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup creator
 *
 * Android-specific creator entry point, used when Blender runs inside an APK.
 *
 * Blender is built as a shared library (libblender.so) which SDLActivity loads at startup,
 * calling the main entry point defined below.
 */

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <android/log.h>
#include <unistd.h>

#include <SDL3/SDL_main.h>

#include "creator_intern.h"

namespace blender {
/* Blender's regular `main(..)`, renamed in `creator.cc`. */
int main_android_enter(int argc, const char **argv);
}  // namespace blender

#define BLENDER_LOG_TAG "blender"

/* -------------------------------------------------------------------- */
/** \name Standard Output Redirection to the System Log
 * \{ */

static void stdio_logcat_pump_fn(int read_fd)
{
  char buffer[1024];
  ssize_t size;

  while ((size = read(read_fd, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[size] = '\0';
    __android_log_write(ANDROID_LOG_INFO, BLENDER_LOG_TAG, buffer);
  }

  close(read_fd);
}

static bool redirect_stdio_to_logcat()
{
  int fds[2];
  if (pipe(fds) == -1) {
    return false;
  }

  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  dup2(fds[1], STDOUT_FILENO);
  dup2(fds[1], STDERR_FILENO);
  close(fds[1]);

  std::thread(stdio_logcat_pump_fn, fds[0]).detach();
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Entry Point
 * \{ */

/* Renamed to SDL_main by <SDL3/SDL_main.h>, called by SDLActivity. */
int main(int argc, char *argv[])
{
  if (!redirect_stdio_to_logcat()) {
    __android_log_write(ANDROID_LOG_WARN,
                        BLENDER_LOG_TAG,
                        "creator: Failed to redirect stdio to Android system log.");
  }

  /* Root of the extracted install tree, set by BlenderActivity. */
  const char *extract_dir = getenv("BLENDER_ANDROID_EXTRACT_DIR");
  if (extract_dir == nullptr) {
    printf("creator: BLENDER_ANDROID_EXTRACT_DIR is not set.\n");
    return EXIT_FAILURE;
  }

  /* Override argv[0] (the theorical program name) with a fake <install>/blender for
   * appdir.cc to be able to infer the program_dirname and find the extracted install. */
  const std::string argv0 = std::string(extract_dir) + "/blender";
  argv[0] = const_cast<char *>(argv0.c_str());

  printf("creator: entering Blender main().\n");

  return creator_main(argc, const_cast<const char **>(argv));
}

/** \} */
