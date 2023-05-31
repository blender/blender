/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup creator
 *
 * Functionality for main() initialization.
 */

struct bArgs;
struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WITH_PYTHON_MODULE

/* creator_args.c */

void main_args_setup(struct bContext *C, struct bArgs *ba);
void main_args_setup_post(struct bContext *C, struct bArgs *ba);

/* creator_signals.c */

void main_signal_setup(void);
void main_signal_setup_background(void);
void main_signal_setup_fpe(void);

#endif /* WITH_PYTHON_MODULE */

/** Shared data for argument handlers to store state in. */
struct ApplicationState {
  struct {
    bool use_crash_handler;
    bool use_abort_handler;
  } signal;

  /* we may want to set different exit codes for other kinds of errors */
  struct {
    unsigned char python;
  } exit_code_on_error;
};
extern struct ApplicationState app_state; /* creator.c */

/**
 * Passes for use by #main_args_setup.
 * Keep in order of execution.
 */
enum {
  /** Run before sub-system initialization. */
  ARG_PASS_ENVIRONMENT = 1,
  /** General settings parsing, also animation player. */
  ARG_PASS_SETTINGS = 2,
  /** Windowing & graphical settings (ignored in background mode). */
  ARG_PASS_SETTINGS_GUI = 3,
  /** Currently use for audio devices. */
  ARG_PASS_SETTINGS_FORCE = 4,

  /**
   * Actions & fall back to loading blend file.
   *
   * \note arguments in the final pass must use #WM_exit instead of `exit()`  environment is
   * properly shut-down (temporary directory deleted, etc).
   */
  ARG_PASS_FINAL = 5,
};

/* for the callbacks: */
#ifndef WITH_PYTHON_MODULE
#  define BLEND_VERSION_FMT "Blender %d.%d.%d"
#  define BLEND_VERSION_ARG (BLENDER_VERSION / 100), (BLENDER_VERSION % 100), BLENDER_VERSION_PATCH
#endif

#ifdef WITH_BUILDINFO_HEADER
#  define BUILD_DATE
#endif

/* From `buildinfo.c`. */
#ifdef BUILD_DATE
extern char build_date[];
extern char build_time[];
extern char build_hash[];
extern unsigned long build_commit_timestamp;

/* TODO(@sergey): ideally size need to be in sync with `buildinfo.c`. */
extern char build_commit_date[16];
extern char build_commit_time[16];

extern char build_branch[];
extern char build_platform[];
extern char build_type[];
extern char build_cflags[];
extern char build_cxxflags[];
extern char build_linkflags[];
extern char build_system[];
#endif /* BUILD_DATE */

#ifdef __cplusplus
}
#endif
