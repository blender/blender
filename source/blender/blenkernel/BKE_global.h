/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 * \section aboutglobal Global settings
 *   Global settings, handles, pointers. This is the root for finding
 *   any data in Blender. This block is not serialized, but built anew
 *   for every fresh Blender run.
 */

#include "BLI_utildefines.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Main;

typedef struct Global {

  /** Active pointers. */
  struct Main *main;

  /** Strings: last saved */
  char ima[1024], lib[1024]; /* 1024 = FILE_MAX */

  /** When set: `G_MAIN->name` contains valid relative base path. */
  bool relbase_valid;
  bool file_loaded;
  bool save_over;

  /** Strings of recent opened files. */
  struct ListBase recent_files;

  /** Has escape been pressed or Ctrl+C pressed in background mode, used for render quit. */
  bool is_break;

  bool background;
  bool factory_startup;

  short moving;

  /** To indicate render is busy, prevent render-window events etc. */
  bool is_rendering;

  /**
   * Debug value, can be set from the UI and python, used for testing nonstandard features.
   * DO NOT abuse it with generic checks like `if (G.debug_value > 0)`. Do not use it as bitflags.
   * Only precise specific values should be checked for, to avoid unpredictable side-effects.
   * Please document here the value(s) you are using (or a range of values reserved to some area).
   *   * -16384 and below: Reserved for python (add-ons) usage.
   *   *     -1: Disable faster motion paths computation (since 08/2018).
   *   * 1 - 30: EEVEE debug/stats values (01/2018).
   *   *    101: Enable UI debug drawing of fullscreen area's corner widget (10/2014).
   *   *    666: Use quicker batch delete for outliners' delete hierarchy (01/2019).
   *   *    777: Enable UI node panel's sockets polling (11/2011).
   *   *    799: Enable some mysterious new depsgraph behavior (05/2015).
   *   *   1112: Disable new Cloth internal springs handling (09/2014).
   *   *   1234: Disable new dyntopo code fixing skinny faces generation (04/2015).
   *   *   3001: Enable additional Fluid modifier (Mantaflow) options (02/2020).
   *   *   4000: Line Art state output and debugging logs (03/2021).
   *   * 16384 and above: Reserved for python (add-ons) usage.
   */
  short debug_value;

  /** Saved to the blend file as #FileGlobal.globalf,
   * however this is now only used for runtime options. */
  int f;

  struct {
    /** Logging vars (different loggers may use). */
    int level;
    /** FILE handle or use stderr (we own this so close when done). */
    void *file;
  } log;

  /** debug flag, #G_DEBUG, #G_DEBUG_PYTHON & friends, set python or command line args */
  int debug;

  /** This variable is written to / read from #FileGlobal.fileflags */
  int fileflags;

  /** Message to use when auto execution fails. */
  char autoexec_fail[200];
} Global;

/* **************** GLOBAL ********************* */

/** #Global.f */
enum {
  G_FLAG_RENDER_VIEWPORT = (1 << 0),
  G_FLAG_PICKSEL = (1 << 2),
  /** Support simulating events (for testing). */
  G_FLAG_EVENT_SIMULATE = (1 << 3),
  G_FLAG_USERPREF_NO_SAVE_ON_EXIT = (1 << 4),

  G_FLAG_SCRIPT_AUTOEXEC = (1 << 13),
  /** When this flag is set ignore the prefs #USER_SCRIPT_AUTOEXEC_DISABLE. */
  G_FLAG_SCRIPT_OVERRIDE_PREF = (1 << 14),
  G_FLAG_SCRIPT_AUTOEXEC_FAIL = (1 << 15),
  G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET = (1 << 16),
};

/** Don't overwrite these flags when reading a file. */
#define G_FLAG_ALL_RUNTIME \
  (G_FLAG_SCRIPT_AUTOEXEC | G_FLAG_SCRIPT_OVERRIDE_PREF | G_FLAG_EVENT_SIMULATE | \
   G_FLAG_USERPREF_NO_SAVE_ON_EXIT)

/** Flags to read from blend file. */
#define G_FLAG_ALL_READFILE 0

/** #Global.debug */
enum {
  G_DEBUG = (1 << 0), /* general debug flag, print more info in unexpected cases */
  G_DEBUG_FFMPEG = (1 << 1),
  G_DEBUG_PYTHON = (1 << 2),                /* extra python info */
  G_DEBUG_EVENTS = (1 << 3),                /* input/window/screen events */
  G_DEBUG_HANDLERS = (1 << 4),              /* events handling */
  G_DEBUG_WM = (1 << 5),                    /* operator, undo */
  G_DEBUG_JOBS = (1 << 6),                  /* jobs time profiling */
  G_DEBUG_FREESTYLE = (1 << 7),             /* freestyle messages */
  G_DEBUG_DEPSGRAPH_BUILD = (1 << 8),       /* depsgraph construction messages */
  G_DEBUG_DEPSGRAPH_EVAL = (1 << 9),        /* depsgraph evaluation messages */
  G_DEBUG_DEPSGRAPH_TAG = (1 << 10),        /* depsgraph tagging messages */
  G_DEBUG_DEPSGRAPH_TIME = (1 << 11),       /* depsgraph timing statistics and messages */
  G_DEBUG_DEPSGRAPH_NO_THREADS = (1 << 12), /* single threaded depsgraph */
  G_DEBUG_DEPSGRAPH_PRETTY = (1 << 13),     /* use pretty colors in depsgraph messages */
  G_DEBUG_DEPSGRAPH_UUID = (1 << 14),       /* use pretty colors in depsgraph messages */
  G_DEBUG_DEPSGRAPH = (G_DEBUG_DEPSGRAPH_BUILD | G_DEBUG_DEPSGRAPH_EVAL | G_DEBUG_DEPSGRAPH_TAG |
                       G_DEBUG_DEPSGRAPH_TIME | G_DEBUG_DEPSGRAPH_UUID),
  G_DEBUG_SIMDATA = (1 << 15),               /* sim debug data display */
  G_DEBUG_GPU = (1 << 16),                   /* gpu debug */
  G_DEBUG_IO = (1 << 17),                    /* IO Debugging (for Collada, ...)*/
  G_DEBUG_GPU_FORCE_WORKAROUNDS = (1 << 18), /* force gpu workarounds bypassing detections. */
  G_DEBUG_XR = (1 << 19),                    /* XR/OpenXR messages */
  G_DEBUG_XR_TIME = (1 << 20),               /* XR/OpenXR timing messages */

  G_DEBUG_GHOST = (1 << 21), /* Debug GHOST module. */
};

#define G_DEBUG_ALL \
  (G_DEBUG | G_DEBUG_FFMPEG | G_DEBUG_PYTHON | G_DEBUG_EVENTS | G_DEBUG_WM | G_DEBUG_JOBS | \
   G_DEBUG_FREESTYLE | G_DEBUG_DEPSGRAPH | G_DEBUG_IO | G_DEBUG_GHOST)

/** #Global.fileflags */
enum {
  G_FILE_AUTOPACK = (1 << 0),
  G_FILE_COMPRESS = (1 << 1),

  // G_FILE_DEPRECATED_9 = (1 << 9),
  G_FILE_NO_UI = (1 << 10),

  /* Bits 11 to 22 (inclusive) are deprecated & need to be cleared */

  /**
   * On read, use #FileGlobal.filename instead of the real location on-disk,
   * needed for recovering temp files so relative paths resolve.
   *
   * \note In some ways it would be nicer to make this an argument passed to file loading.
   * In practice this means recover needs to be passed around to too many low level functions,
   * so keep this as a flag.
   */
  G_FILE_RECOVER_READ = (1 << 23),
  /**
   * On write, assign use #FileGlobal.filename, otherwise leave it blank,
   * needed so files can be recovered at their original locations.
   *
   * \note only #BLENDER_QUIT_FILE and auto-save files include recovery information.
   * As users/developers may not want their paths exposed in publicly distributed files.
   */
  G_FILE_RECOVER_WRITE = (1 << 24),
  /** BMesh option to save as older mesh format */
  /* #define G_FILE_MESH_COMPAT       (1 << 26) */
  /* #define G_FILE_GLSL_NO_ENV_LIGHTING (1 << 28) */ /* deprecated */
};

/**
 * Run-time only #G.fileflags which are never read or written to/from Blend files.
 * This means we can change the values without worrying about do-versions.
 */
#define G_FILE_FLAG_ALL_RUNTIME (G_FILE_NO_UI | G_FILE_RECOVER_READ | G_FILE_RECOVER_WRITE)

/** ENDIAN_ORDER: indicates what endianness the platform where the file was written had. */
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#  error Either __BIG_ENDIAN__ or __LITTLE_ENDIAN__ must be defined.
#endif

#define L_ENDIAN 1
#define B_ENDIAN 0

#ifdef __BIG_ENDIAN__
#  define ENDIAN_ORDER B_ENDIAN
#else
#  define ENDIAN_ORDER L_ENDIAN
#endif

/** #Global.moving, signals drawing in (3d) window to denote transform */
enum {
  G_TRANSFORM_OBJ = (1 << 0),
  G_TRANSFORM_EDIT = (1 << 1),
  G_TRANSFORM_SEQ = (1 << 2),
  G_TRANSFORM_FCURVES = (1 << 3),
  G_TRANSFORM_WM = (1 << 4),
};

/** Defined in blender.c */
extern Global G;

/**
 * Stupid macro to hide the few *valid* usages of `G.main` (from startup/exit code e.g.),
 * helps with cleanup task.
 */
#define G_MAIN (G).main

#ifdef __cplusplus
}
#endif
