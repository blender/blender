/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file deals with embedding the python interpreter within blender,
 * starting and stopping python and exposing blender/python modules so they can
 * be accesses from scripts.
 */

#include <Python.h>
#include <frameobject.h>
#include <optional>

#ifdef WITH_PYTHON_MODULE
#  include "pylifecycle.h" /* For `Py_Version`. */
#endif
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "CLG_log.h"

#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#ifdef WITH_PYTHON_MODULE
#  include "BLI_string.h"
#endif

#include "BLT_translation.hh"

#include "RNA_types.hh"

#include "bpy.hh"
#include "bpy_capi_utils.hh"
#include "bpy_intern_string.hh"
#include "bpy_path.hh"
#include "bpy_props.hh"
#include "bpy_rna.hh"

#include "bpy_app_translations.hh"

#include "DNA_text_types.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_global.hh" /* Only for script checking. */
#include "BKE_main.hh"
#include "BKE_text.h"

#ifdef WITH_CYCLES
#  include "CCL_api.h"
#endif

#include "BPY_extern.hh"
#include "BPY_extern_python.hh"
#include "BPY_extern_run.hh"

#include "../generic/py_capi_utils.hh"

/* `inittab` initialization functions. */
#include "../bmesh/bmesh_py_api.hh"
#include "../generic/bl_math_py_api.hh"
#include "../generic/blf_py_api.hh"
#include "../generic/idprop_py_api.hh"
#include "../generic/imbuf_py_api.hh"
#include "../gpu/gpu_py_api.hh"
#include "../mathutils/mathutils.hh"

/* Logging types to use anywhere in the Python modules. */

CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_INTERFACE, "bpy.interface");
CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_RNA, "bpy.rna");

extern CLG_LogRef *BKE_LOG_CONTEXT;

/* For internal use, when starting and ending Python scripts. */

/* In case a Python script triggers another Python call,
 * stop #bpy_context_clear from invalidating. */
static int py_call_level = 0;

/* Set by command line arguments before Python starts. */
static bool py_use_system_env = false;

// #define TIME_PY_RUN /* Simple python tests. prints on exit. */

#ifdef TIME_PY_RUN
#  include "BLI_time.h"
static int bpy_timer_count = 0;
/** Time since python starts. */
static double bpy_timer;
/** Time for each Python script run. */
static double bpy_timer_run;
/** Accumulate Python runs. */
static double bpy_timer_run_tot;
#endif

void BPY_context_update(bContext *C)
{
  /* Don't do this from a non-main (e.g. render) thread, it can cause a race
   * condition on `C->data.recursion`. Ideal solution would be to disable
   * context entirely from non-main threads, but that's more complicated. */
  if (!BLI_thread_is_main()) {
    return;
  }

  BPY_context_set(C);

  /* Can give really bad results if this isn't here. */
  BPY_modules_update();
}

void bpy_context_set(bContext *C, PyGILState_STATE *gilstate)
{
  py_call_level++;

  if (gilstate) {
    *gilstate = PyGILState_Ensure();
  }

  if (py_call_level == 1) {
    BPY_context_update(C);

#ifdef TIME_PY_RUN
    if (bpy_timer_count == 0) {
      /* Record time from the beginning. */
      bpy_timer = BLI_time_now_seconds();
      bpy_timer_run = bpy_timer_run_tot = 0.0;
    }
    bpy_timer_run = BLI_time_now_seconds();

    bpy_timer_count++;
#endif
  }
}

void bpy_context_clear(bContext * /*C*/, const PyGILState_STATE *gilstate)
{
  py_call_level--;

  if (gilstate) {
    PyGILState_Release(*gilstate);
  }

  if (py_call_level < 0) {
    fprintf(stderr, "ERROR: Python context internal state bug. this should not happen!\n");
  }
  else if (py_call_level == 0) {
    /* NOTE: Unfortunately calling classes currently won't store the context.
     * Can't set nullptr because of this - but this is very unreliable still. */
#if 0
    BPY_context_set(nullptr);
#endif

#ifdef TIME_PY_RUN
    bpy_timer_run_tot += BLI_time_now_seconds() - bpy_timer_run;
    bpy_timer_count++;
#endif
  }
}

static void bpy_context_end(bContext *C)
{
  if (UNLIKELY(C == nullptr)) {
    return;
  }
  CTX_wm_operator_poll_msg_clear(C);
}

void BPY_context_dict_clear_members_array(void **dict_p,
                                          void *dict_orig,
                                          const char *context_members[],
                                          uint context_members_len)
{
  PyGILState_STATE gilstate;
  const bool use_gil = !PyC_IsInterpreterActive();
  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  /* Copy on write. */
  if (*dict_p == dict_orig) {
    *dict_p = PyDict_Copy(static_cast<PyObject *>(dict_orig));
  }

  PyObject *dict = static_cast<PyObject *>(*dict_p);
  BLI_assert(PyDict_Check(dict));

  /* Use #PyDict_Pop instead of #PyDict_DelItemString to avoid setting the exception,
   * while supported it's good to avoid for low level functions like this that run often. */
  for (uint i = 0; i < context_members_len; i++) {
    PyObject *key = PyUnicode_FromString(context_members[i]);
    PyObject *item;

#if PY_VERSION_HEX >= 0x030d0000
    switch (PyDict_Pop(dict, key, &item)) {
      case 1: {
        Py_DECREF(item);
        break;
      }
      case -1: {
        /* Not expected, but allow for an error. */
        BLI_assert(false);
        PyErr_Clear();
        break;
      }
    }
#else /* Remove when Python 3.12 support is dropped. */
    item = _PyDict_Pop(dict, key, Py_None);
    Py_DECREF(item);
#endif

    Py_DECREF(key);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }
}

void BPY_text_free_code(Text *text)
{
  if (text->compiled) {
    PyGILState_STATE gilstate;
    const bool use_gil = !PyC_IsInterpreterActive();

    if (use_gil) {
      gilstate = PyGILState_Ensure();
    }

    Py_DECREF((PyObject *)text->compiled);
    text->compiled = nullptr;

    if (use_gil) {
      PyGILState_Release(gilstate);
    }
  }
}

void BPY_modules_update()
{
  /* Correct but slow, this runs all the time operator poll, panel draw etc
   * (100's of time a second). */
#if 0
  PyObject *mod = PyImport_ImportModuleLevel("bpy", nullptr, nullptr, nullptr, 0);
  PyModule_AddObject(mod, "data", BPY_rna_module());
  PyModule_AddObject(mod, "types", BPY_rna_types()); /* This does not need updating. */
#endif

  /* Refreshes the main struct. */
  BPY_update_rna_module();
}

bContext *BPY_context_get()
{
  return static_cast<bContext *>(bpy_context_module->ptr->data);
}

void BPY_context_set(bContext *C)
{
  bpy_context_module->ptr->data = (void *)C;
}

#ifdef WITH_FLUID
/* Defined in `manta` module. */
extern "C" PyObject *Manta_initPython();
#endif

#ifdef WITH_AUDASPACE_PY
/* Defined in `AUD_C-API.cpp`. */
extern "C" PyObject *AUD_initPython();
#endif

#ifdef WITH_CYCLES
/* Defined in `cycles` module. */
static PyObject *CCL_initPython()
{
  return (PyObject *)CCL_python_module_init();
}
#endif

#ifdef WITH_HYDRA
/* Defined in `render_hydra` module. */
PyObject *BPyInit_hydra();
#endif

static _inittab bpy_internal_modules[] = {
    {"mathutils", PyInit_mathutils},
#if 0
    {"mathutils.geometry", PyInit_mathutils_geometry},
    {"mathutils.noise", PyInit_mathutils_noise},
    {"mathutils.kdtree", PyInit_mathutils_kdtree},
#endif
    {"_bpy_path", BPyInit__bpy_path},
    {"blf", BPyInit_blf},
    {"bl_math", BPyInit_bl_math},
    {"imbuf", BPyInit_imbuf},
    {"bmesh", BPyInit_bmesh},
#if 0
    {"bmesh.types", BPyInit_bmesh_types},
    {"bmesh.utils", BPyInit_bmesh_utils},
    {"bmesh.utils", BPyInit_bmesh_geometry},
#endif
#ifdef WITH_FLUID
    {"manta", Manta_initPython},
#endif
#ifdef WITH_AUDASPACE_PY
    {"aud", AUD_initPython},
#endif
#ifdef WITH_CYCLES
    {"_cycles", CCL_initPython},
#endif
    {"gpu", BPyInit_gpu},
    {"idprop", BPyInit_idprop},
#ifdef WITH_HYDRA
    {"_bpy_hydra", BPyInit_hydra},
#endif
    {nullptr, nullptr},
};

#ifndef WITH_PYTHON_MODULE
/**
 * Convenience function for #BPY_python_start.
 *
 * These should happen so rarely that having comprehensive errors isn't needed.
 * For example if `sys.argv` fails to allocate memory.
 *
 * Show an error just to avoid silent failure in the unlikely event something goes wrong,
 * in this case a developer will need to track down the root cause.
 */
static void pystatus_exit_on_error(const PyStatus &status)
{
  if (UNLIKELY(PyStatus_Exception(status))) {
    fputs("Internal error initializing Python!\n", stderr);
    /* This calls `exit`. */
    Py_ExitStatusException(status);
  }
}
#endif

void BPY_python_start(bContext *C, int argc, const char **argv)
{
#ifndef WITH_PYTHON_MODULE
  BLI_assert_msg(Py_IsInitialized() == 0, "Python has already been initialized");

  /* #PyPreConfig (early-configuration). */
  {
    PyPreConfig preconfig;
    PyStatus status;

    /* To narrow down reports where the systems Python is inexplicably used, see: #98131. */
    CLOG_DEBUG(
        BPY_LOG_INTERFACE,
        "Initializing %s support for the systems Python environment such as 'PYTHONPATH' and "
        "the user-site directory.",
        py_use_system_env ? "*with*" : "*without*");

    if (py_use_system_env) {
      PyPreConfig_InitPythonConfig(&preconfig);
    }
    else {
      /* Only use the systems environment variables and site when explicitly requested.
       * Since an incorrect 'PYTHONPATH' causes difficult to debug errors, see: #72807.
       * An alternative to setting `preconfig.use_environment = 0` */
      PyPreConfig_InitIsolatedConfig(&preconfig);
    }

    /* Force UTF8 on all platforms, since this is what's used for Blender's internal strings,
     * providing consistent encoding behavior across all Blender installations.
     *
     * This also uses the `surrogateescape` error handler ensures any unexpected bytes are escaped
     * instead of raising an error.
     *
     * Without this `sys.getfilesystemencoding()` and `sys.stdout` for example may be set to ASCII
     * or some other encoding - where printing some UTF8 values will raise an error.
     *
     * This can cause scripts to fail entirely on some systems.
     *
     * This assignment is the equivalent of enabling the `PYTHONUTF8` environment variable.
     * See `PEP-540` for details on exactly what this changes. */
    preconfig.utf8_mode = true;

    /* Note that there is no reason to call #Py_PreInitializeFromBytesArgs here
     * as this is only used so that command line arguments can be handled by Python itself,
     * not for setting `sys.argv` (handled below). */
    status = Py_PreInitialize(&preconfig);
    pystatus_exit_on_error(status);
  }

  /* Must run before python initializes, but after #PyPreConfig. */
  PyImport_ExtendInittab(bpy_internal_modules);

  /* #PyConfig (initialize Python). */
  {
    PyConfig config;
    PyStatus status;
    bool has_python_executable = false;

    if (py_use_system_env) {
      PyConfig_InitPythonConfig(&config);

      BLI_assert(config.install_signal_handlers);
    }
    else {
      PyConfig_InitIsolatedConfig(&config);
      /* Python's isolated config disables its own signal overrides.
       * While it makes sense not to interfering with other components of the process,
       * the signal handlers are needed for Python's own error handling to work properly.
       * Without this a `SIGPIPE` signal will crash Blender, see: #129657. */
      config.install_signal_handlers = 1;
    }

    /* Suppress error messages when calculating the module search path.
     * While harmless, it's noisy. */
    config.pathconfig_warnings = 0;

    {
      /* NOTE: running scripts directly uses the default behavior *but* the default
       * warning filter doesn't show warnings form module besides `__main__`.
       * Use the default behavior unless debugging Python. See: !139487. */
      bool show_python_warnings = false;

#  ifdef NDEBUG
      show_python_warnings = G.debug & G_DEBUG_PYTHON;
#  else
      /* Always show warnings for debug builds so developers are made aware
       * of outdated API use before any breakages occur. */
      show_python_warnings = true;
#  endif

      if (show_python_warnings) {
        /* Don't overwrite warning settings if they have been set by the environment. */
        if (!(py_use_system_env && BLI_getenv("PYTHONWARNINGS"))) {
          /* Confusingly `default` is not the default.
           * Setting to `default` without any module names shows warnings for all modules.
           * Useful for development since most functionality occurs outside of `__main__`. */
          PyWideStringList_Append(&config.warnoptions, L"default");
        }
      }
    }

    /* Allow the user site directory because this is used
     * when PIP installing packages from Blender, see: #104000.
     *
     * NOTE(@ideasman42): While an argument can be made for isolating Blender's Python
     * from the users home directory entirely, an alternative directory should be used in that
     * case - so PIP can be used to install packages. Otherwise PIP will install packages to a
     * directory which us not in the users `sys.path`, see `site.USER_BASE` for details. */
    // config.user_site_directory = py_use_system_env;

    /* While `sys.argv` is set, we don't want Python to interpret it. */
    config.parse_argv = 0;
    status = PyConfig_SetBytesArgv(&config, argc, (char *const *)argv);
    pystatus_exit_on_error(status);

    /* Needed for Python's initialization for portable Python installations.
     * We could use #Py_SetPath, but this overrides Python's internal logic
     * for calculating its own module search paths.
     *
     * `sys.executable` is overwritten after initialization to the Python binary. */
    {
      const char *program_path = BKE_appdir_program_path();
      status = PyConfig_SetBytesString(&config, &config.program_name, program_path);
      pystatus_exit_on_error(status);
    }

    /* Setting the program name is important so the 'multiprocessing' module
     * can launch new Python instances. */
    {
      char program_path[FILE_MAX];
      if (BKE_appdir_program_python_search(
              program_path, sizeof(program_path), PY_MAJOR_VERSION, PY_MINOR_VERSION))
      {
        status = PyConfig_SetBytesString(&config, &config.executable, program_path);
        pystatus_exit_on_error(status);
        has_python_executable = true;
      }
      else {
        /* Set to `sys.executable = None` below (we can't do before Python is initialized). */
        fprintf(stderr,
                "Unable to find the Python binary, "
                "the multiprocessing module may not be functional!\n");
      }
    }

    /* Allow to use our own included Python. `py_path_bundle` may be nullptr. */
    {
      const std::optional<std::string> py_path_bundle = BKE_appdir_folder_id(BLENDER_SYSTEM_PYTHON,
                                                                             nullptr);
      if (py_path_bundle.has_value()) {

#  ifdef __APPLE__
        /* Mac-OS allows file/directory names to contain `:` character
         * (represented as `/` in the Finder) but current Python lib (as of release 3.1.1)
         * doesn't handle these correctly. */
        if (strchr(py_path_bundle->c_str(), ':')) {
          fprintf(stderr,
                  "Warning! Blender application is located in a path containing ':' or '/' chars\n"
                  "This may make Python import function fail\n");
        }
#  endif /* __APPLE__ */

        status = PyConfig_SetBytesString(&config, &config.home, py_path_bundle->c_str());
        pystatus_exit_on_error(status);

#  ifdef PYTHON_SSL_CERT_FILE
        /* Point to the portable SSL certificate to support HTTPS access, see: #102300. */
        const char *ssl_cert_file_env = "SSL_CERT_FILE";
        if (BLI_getenv(ssl_cert_file_env) == nullptr) {
          const char *ssl_cert_file_suffix = PYTHON_SSL_CERT_FILE;
          char ssl_cert_file[FILE_MAX];
          BLI_path_join(
              ssl_cert_file, sizeof(ssl_cert_file), py_path_bundle->c_str(), ssl_cert_file_suffix);
          BLI_setenv(ssl_cert_file_env, ssl_cert_file);
        }
#  endif /* PYTHON_SSL_CERT_FILE */
      }
      else {
/* Common enough to use the system Python on Linux/Unix, warn on other systems. */
#  if defined(__APPLE__) || defined(_WIN32)
        fprintf(stderr,
                "Bundled Python not found and is expected on this platform "
                "(the 'install' target may have not been built)\n");
#  endif
      }
    }

    /* Initialize Python (also acquires lock). */
    status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);

    pystatus_exit_on_error(status);

    if (!has_python_executable) {
      PySys_SetObject("executable", Py_None);
    }
  }

#  ifdef WITH_FLUID
  /* Required to prevent assertion error, see:
   * https://stackoverflow.com/questions/27844676 */
  Py_DECREF(PyImport_ImportModule("threading"));
#  endif

#else /* WITH_PYTHON_MODULE */
  (void)argc;
  (void)argv;

  /* NOTE(ideasman42): unfortunately the `inittab` can only be used
   * before Python has been initialized.
   * When built as a Python module, Python will have been initialized
   * and using the `inittab` isn't supported.
   * So it's necessary to load all modules as soon as `bpy` is imported. */
  // PyImport_ExtendInittab(bpy_internal_modules);

#endif /* WITH_PYTHON_MODULE */

  bpy_intern_string_init();

#ifdef WITH_PYTHON_MODULE
  {
    /* Manually load all modules */
    struct _inittab *inittab_item;
    PyObject *sys_modules = PyImport_GetModuleDict();

    for (inittab_item = bpy_internal_modules; inittab_item->name; inittab_item++) {
      PyObject *mod = inittab_item->initfunc();
      if (mod) {
        PyDict_SetItemString(sys_modules, inittab_item->name, mod);
      }
      else {
        PyErr_Print();
      }
      // Py_DECREF(mod); /* Ideally would decref, but in this case we never want to free. */
    }
  }
#endif

  /* Run first, initializes RNA types. */
  BPY_rna_init();

  /* Defines `bpy.*` and lets us import it. */
  BPy_init_modules(C);

  pyrna_alloc_types();

#ifndef WITH_PYTHON_MODULE
  /* Python module runs `atexit` when `bpy` is freed. */
  BPY_atexit_register(); /* This can initialize any time. */

  /* Free the lock acquired (implicitly) when Python is initialized. */
  PyEval_ReleaseThread(PyGILState_GetThisThreadState());

#endif

#ifdef WITH_PYTHON_MODULE
  /* Disable all add-ons at exit, not essential, it just avoids resource leaks, see #71362. */
  const char *imports[] = {"atexit", "addon_utils", nullptr};
  BPY_run_string_eval(C, imports, "atexit.register(addon_utils.disable_all)");
#endif
}

void BPY_python_end(const bool do_python_exit)
{
#ifndef WITH_PYTHON_MODULE
  BLI_assert_msg(Py_IsInitialized() != 0, "Python must be initialized");
#endif

  /* Finalizing, no need to grab the state, except when we are a module. */
  PyGILState_STATE gilstate = PyGILState_Ensure();

  /* Frees the Python-driver name-space & cached data. */
  BPY_driver_exit();

  /* Clear Python values in the context so freeing the context after Python exits doesn't crash. */
  bpy_context_end(BPY_context_get());

  /* Decrement user counts of all callback functions. */
  BPY_rna_props_clear_all();

  /* Free other Python data. */
  RNA_bpy_exit();

  BPY_rna_exit();

  /* Clear all Python data from structs. */

  bpy_intern_string_exit();

  /* `bpy.app` modules that need cleanup. */
  BPY_app_translations_end();

#ifndef WITH_PYTHON_MODULE
  /* Without this we get recursive calls to #WM_exit_ex. */
  BPY_atexit_unregister();

  if (do_python_exit) {
    Py_Finalize();
  }
  (void)gilstate;
#else
  PyGILState_Release(gilstate);
  (void)do_python_exit;
#endif

#ifdef TIME_PY_RUN
  /* Measure time since Python started. */
  bpy_timer = BLI_time_now_seconds() - bpy_timer;

  printf("*bpy stats* - ");
  printf("tot exec: %d,  ", bpy_timer_count);
  printf("tot run: %.4fsec,  ", bpy_timer_run_tot);
  if (bpy_timer_count > 0) {
    printf("average run: %.6fsec,  ", (bpy_timer_run_tot / bpy_timer_count));
  }

  if (bpy_timer > 0.0) {
    printf("tot usage %.4f%%", (bpy_timer_run_tot / bpy_timer) * 100.0);
  }

  printf("\n");
#endif
}

void BPY_python_reset(bContext *C)
{
  BLI_assert_msg(Py_IsInitialized() != 0, "Python must be initialized");

  /* Unrelated security stuff. */
  G.f &= ~(G_FLAG_SCRIPT_AUTOEXEC_FAIL | G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET);
  G.autoexec_fail[0] = '\0';

  BPY_driver_reset();
  BPY_app_handlers_reset(false);
  BPY_modules_load_user(C);
}

void BPY_python_use_system_env()
{
  BLI_assert(!Py_IsInitialized());
  py_use_system_env = true;
}

bool BPY_python_use_system_env_get()
{
  return py_use_system_env;
}

void BPY_python_backtrace(FILE *fp)
{
  fputs("\n# Python backtrace\n", fp);

  /* Can happen in rare cases. */
  if (!PyThreadState_GetUnchecked()) {
    return;
  }
  PyFrameObject *frame = PyEval_GetFrame();
  if (frame == nullptr) {
    return;
  }
  do {
    PyCodeObject *code = PyFrame_GetCode(frame);
    const int line = PyFrame_GetLineNumber(frame);
    const char *filepath = PyUnicode_AsUTF8(code->co_filename);
    const char *funcname = PyUnicode_AsUTF8(code->co_name);
    fprintf(fp, "  File \"%s\", line %d in %s\n", filepath, line, funcname);
  } while ((frame = PyFrame_GetBack(frame)));
}

void BPY_DECREF(void *pyob_ptr)
{
  const PyGILState_STATE gilstate = PyGILState_Ensure();
  Py_DECREF((PyObject *)pyob_ptr);
  PyGILState_Release(gilstate);
}

void BPY_DECREF_RNA_INVALIDATE(void *pyob_ptr)
{
  const PyGILState_STATE gilstate = PyGILState_Ensure();
  const bool do_invalidate = (Py_REFCNT((PyObject *)pyob_ptr) > 1);
  Py_DECREF((PyObject *)pyob_ptr);
  if (do_invalidate) {
    pyrna_invalidate(static_cast<BPy_DummyPointerRNA *>(pyob_ptr));
  }
  PyGILState_Release(gilstate);
}

void BPY_modules_load_user(bContext *C)
{
  PyGILState_STATE gilstate;
  Main *bmain = CTX_data_main(C);
  Text *text;

  /* Can happen on file load. */
  if (bmain == nullptr) {
    return;
  }

  /* Update pointers since this can run from a nested script on file load. */
  if (py_call_level) {
    BPY_context_update(C);
  }

  bpy_context_set(C, &gilstate);

  for (text = static_cast<Text *>(bmain->texts.first); text;
       text = static_cast<Text *>(text->id.next))
  {
    if (text->flags & TXT_ISSCRIPT) {
      if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC)) {
        if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET)) {
          G.f |= G_FLAG_SCRIPT_AUTOEXEC_FAIL;
          SNPRINTF_UTF8(G.autoexec_fail, RPT_("Text '%s'"), text->id.name + 2);

          printf("scripts disabled for \"%s\", skipping '%s'\n",
                 BKE_main_blendfile_path(bmain),
                 text->id.name + 2);
        }
      }
      else {
        BPY_run_text(C, text, nullptr, false);

        /* Check if the script loaded a new file. */
        if (bmain != CTX_data_main(C)) {
          break;
        }
      }
    }
  }
  bpy_context_clear(C, &gilstate);
}

/** Helper function for logging context member access errors with both CLI and Python support */
static void bpy_context_log_member_error(const bContext *C, const char *message)
{
  const bool use_logging_info = CLOG_CHECK(BKE_LOG_CONTEXT, CLG_LEVEL_INFO);
  const bool use_logging_member = C && CTX_member_logging_get(C);
  if (!(use_logging_info || use_logging_member)) {
    return;
  }

  std::optional<std::string> python_location = BPY_python_current_file_and_line();
  const char *location = python_location ? python_location->c_str() : "unknown:0";

  if (use_logging_info) {
    CLOG_INFO(BKE_LOG_CONTEXT, "%s: %s", location, message);
  }
  else if (use_logging_member) {
    CLOG_AT_LEVEL_NOCHECK(BKE_LOG_CONTEXT, CLG_LEVEL_INFO, "%s: %s", location, message);
  }
  else {
    BLI_assert_unreachable();
  }
}

bool BPY_context_member_get(bContext *C, const char *member, bContextDataResult *result)
{
  PyGILState_STATE gilstate;
  const bool use_gil = !PyC_IsInterpreterActive();
  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  PyObject *pyctx;
  PyObject *item;
  PointerRNA *ptr = nullptr;
  bool done = false;

  pyctx = (PyObject *)CTX_py_dict_get(C);
  item = PyDict_GetItemString(pyctx, member);

  if (item == nullptr) {
    /* Pass. */
  }
  else if (item == Py_None) {
    done = true;
  }
  else if (BPy_StructRNA_Check(item)) {
    ptr = &reinterpret_cast<BPy_StructRNA *>(item)->ptr.value();

    // result->ptr = ((BPy_StructRNA *)item)->ptr;
    CTX_data_pointer_set_ptr(result, ptr);
    CTX_data_type_set(result, ContextDataType::Pointer);
    done = true;
  }
  else if (PySequence_Check(item)) {
    PyObject *seq_fast = PySequence_Fast(item, "bpy_context_get sequence conversion");
    if (seq_fast == nullptr) {
      PyErr_Print();
    }
    else {
      const int len = PySequence_Fast_GET_SIZE(seq_fast);
      PyObject **seq_fast_items = PySequence_Fast_ITEMS(seq_fast);
      int i;

      for (i = 0; i < len; i++) {
        PyObject *list_item = seq_fast_items[i];

        if (BPy_StructRNA_Check(list_item)) {
          ptr = &reinterpret_cast<BPy_StructRNA *>(list_item)->ptr.value();
          CTX_data_list_add_ptr(result, ptr);
        }
        else {
          /* Log invalid list item type */
          std::string message = std::string("'") + member +
                                "' list item not a valid type in sequence type '" +
                                Py_TYPE(list_item)->tp_name + "'";
          bpy_context_log_member_error(C, message.c_str());
        }
      }
      Py_DECREF(seq_fast);
      CTX_data_type_set(result, ContextDataType::Collection);
      done = true;
    }
  }

  if (done == false) {
    if (item) {
      /* Log invalid member type */
      std::string message = std::string("'") + member + "' not a valid type";
      bpy_context_log_member_error(C, message.c_str());
    }
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  return done;
}

std::optional<std::string> BPY_python_current_file_and_line()
{
  /* Early return if Python is not initialized, usually during startup.
   * This function shouldn't operate if Python isn't initialized yet.
   *
   * In most cases this shouldn't be done, make an exception as it's needed for logging. */
  if (!Py_IsInitialized()) {
    return std::nullopt;
  }

  PyGILState_STATE gilstate;
  const bool use_gil = !PyC_IsInterpreterActive();
  std::optional<std::string> result = std::nullopt;
  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  const char *filename = nullptr;
  int lineno = -1;
  PyC_FileAndNum_Safe(&filename, &lineno);

  if (filename) {
    result = std::string(filename) + ":" + std::to_string(lineno);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }
  return result;
}

#ifdef WITH_PYTHON_MODULE

/* -------------------------------------------------------------------- */
/** \name Detect Exit Singleton
 *
 * Python does not reliably free all modules on exit.
 * This means we can't rely on #PyModuleDef::m_free running to clean-up
 * Blender data when Python exits.
 *
 * However Python *does* reliably clear the modules name-space.
 * Store a singleton in modules which may reference Blender owned memory,
 * calling #main_python_exit once the singleton has been cleared from the
 * name-space of all modules.
 * \{ */

static void main_python_exit_ensure();

static void bpy_detect_exit_singleton_cleanup(PyObject * /*capsule*/)
{
  main_python_exit_ensure();
}

static void bpy_detect_exit_singleton_add_to_module(PyObject *mod)
{
  static PyObject *singleton = nullptr;

  /* Note that Python's API docs state that:
   * - If this capsule will be stored as an attribute of a module,
   *   the name should be specified as `modulename.attributename`.
   * This is ignored here because the capsule is not intended for script author access.
   * It also wouldn't make sense as it is stored in multiple modules. */
  const char *bpy_detect_exit_singleton_id = "_bpy_detect_exit_singleton";
  if (singleton == nullptr) {
    /* This is ignored, but must be non-null,
     * set an address that is non-null and easily identifiable. */
    void *pointer = reinterpret_cast<void *>(uintptr_t(-1));
    singleton = PyCapsule_New(
        pointer, bpy_detect_exit_singleton_id, bpy_detect_exit_singleton_cleanup);
    BLI_assert(singleton);
  }
  else {
    Py_INCREF(singleton);
  }
  PyModule_AddObject(mod, bpy_detect_exit_singleton_id, singleton);
}

/** \} */

/* TODO: reloading the module isn't functional at the moment. */

static void bpy_module_free(void *mod);

/* Defined in 'creator.c' when building as a Python module. */
extern int main_python_enter(int argc, const char **argv);
extern void main_python_exit();

static void main_python_exit_ensure()
{
  static bool exit = false;
  if (exit) {
    return;
  }
  exit = true;
  main_python_exit();
}

static struct PyModuleDef bpy_proxy_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ nullptr,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ bpy_module_free,
};

struct dealloc_obj {
  PyObject_HEAD
  /* Type-specific fields go here. */
  PyObject *mod;
};

/* Call once `__file__` is set. */
static void bpy_module_delay_init(PyObject *bpy_proxy)
{
  const int argc = 1;
  const char *argv[2];

  /* Updating the module dict below will lose the reference to `__file__`. */
  PyObject *filepath_obj = PyModule_GetFilenameObject(bpy_proxy);

  /* The file can be a relative path. */
  const char *filepath_rel = PyUnicode_AsUTF8(filepath_obj);
  char filepath_abs[1024];

  STRNCPY(filepath_abs, filepath_rel);
  BLI_path_abs_from_cwd(filepath_abs, sizeof(filepath_abs));
  Py_DECREF(filepath_obj);

  argv[0] = filepath_abs;
  argv[1] = nullptr;

  main_python_enter(argc, argv);

  /* Initialized in #BPy_init_modules(). */
  PyDict_Update(PyModule_GetDict(bpy_proxy), PyModule_GetDict(bpy_package_py));

  {
    /* Modules which themselves require access to Blender
     * allocated resources to be freed should be included in this list.
     * Once the last module has been cleared, the singleton will be de-allocated
     * which calls #main_python_exit.
     *
     * Note that, other modules can be here as needed. */
    const char *bpy_modules_array[] = {
        "bpy.types",
        /* Not technically required however as this is created early on
         * in Blender's module initialization, it's likely to be cleared later,
         * since module cleanup runs in the reverse of the order added to `sys.modules`. */
        "_bpy",
    };
    PyObject *sys_modules = PyImport_GetModuleDict();
    for (int i = 0; i < ARRAY_SIZE(bpy_modules_array); i++) {
      PyObject *mod = PyDict_GetItemString(sys_modules, bpy_modules_array[i]);
      BLI_assert(mod);
      bpy_detect_exit_singleton_add_to_module(mod);
    }
  }
}

/**
 * Raise an error and return false if the Python version used to compile Blender
 * isn't compatible with the interpreter loading the `bpy` module.
 */
static bool bpy_module_ensure_compatible_version()
{
  /* First check the Python version used matches the major version that Blender was built with.
   * While this isn't essential, the error message in this case may be cryptic and misleading.
   * NOTE: using `Py_LIMITED_API` would remove the need for this, in practice it's
   * unlikely Blender will ever used the limited API though. */
  const uint version_runtime = Py_Version;

  uint version_compile_major = PY_VERSION_HEX >> 24;
  uint version_compile_minor = ((PY_VERSION_HEX & 0x00ff0000) >> 16);
  uint version_runtime_major = version_runtime >> 24;
  uint version_runtime_minor = ((version_runtime & 0x00ff0000) >> 16);
  if ((version_compile_major != version_runtime_major) ||
      (version_compile_minor != version_runtime_minor))
  {
    PyErr_Format(PyExc_ImportError,
                 "The version of \"bpy\" was compiled with: "
                 "(%u.%u) is incompatible with: (%u.%u) used by the interpreter!",
                 version_compile_major,
                 version_compile_minor,
                 version_runtime_major,
                 version_runtime_minor);
    return false;
  }
  return true;
}

static void dealloc_obj_dealloc(PyObject *self);

static PyTypeObject dealloc_obj_Type;

/* Use our own `dealloc` so we can free a property if we use one. */
static void dealloc_obj_dealloc(PyObject *self)
{
  bpy_module_delay_init(((dealloc_obj *)self)->mod);

  /* NOTE: for sub-classed `PyObject` objects
   * we can't call #PyObject_DEL() directly or it will crash. */
  dealloc_obj_Type.tp_free(self);
}

PyMODINIT_FUNC PyInit_bpy();

PyMODINIT_FUNC PyInit_bpy()
{
  if (!bpy_module_ensure_compatible_version()) {
    return nullptr; /* The error has been set. */
  }

  PyObject *bpy_proxy = PyModule_Create(&bpy_proxy_def);

  /* Problem:
   * 1) This initializing function is expected to have a private member defined - `md_def`
   *    but this is only set for CAPI defined modules (not Python packages)
   *    so we can't return `bpy_package_py` as is.
   *
   * 2) There is a `bpy` CAPI module for python to load which is basically all of blender,
   *    and there is `scripts/bpy/__init__.py`,
   *    we may end up having to rename this module so there is no naming conflict here eg:
   *    `from blender import bpy`
   *
   * 3) We don't know the file-path at this point, workaround by assigning a dummy value
   *    which calls back when its freed so the real loading can take place.
   */

  /* Assign an object which is freed after `__file__` is assigned. */
  dealloc_obj *dob;

  /* Assign dummy type. */
  dealloc_obj_Type.tp_name = "dealloc_obj";
  dealloc_obj_Type.tp_basicsize = sizeof(dealloc_obj);
  dealloc_obj_Type.tp_dealloc = dealloc_obj_dealloc;
  dealloc_obj_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  if (PyType_Ready(&dealloc_obj_Type) < 0) {
    return nullptr;
  }

  dob = (dealloc_obj *)dealloc_obj_Type.tp_alloc(&dealloc_obj_Type, 0);
  dob->mod = bpy_proxy;                                       /* borrow */
  PyModule_AddObject(bpy_proxy, "__file__", (PyObject *)dob); /* borrow */

  return bpy_proxy;
}

static void bpy_module_free(void * /*mod*/)
{
  main_python_exit_ensure();
}

#endif

bool BPY_string_is_keyword(const char *str)
{
  /* List is from: `", ".join(['"%s"' % kw for kw in  __import__("keyword").kwlist])`. */
  const char *kwlist[] = {
      "False", "None",     "True",  "and",    "as",   "assert", "async",  "await",    "break",
      "class", "continue", "def",   "del",    "elif", "else",   "except", "finally",  "for",
      "from",  "global",   "if",    "import", "in",   "is",     "lambda", "nonlocal", "not",
      "or",    "pass",     "raise", "return", "try",  "while",  "with",   "yield",    nullptr,
  };

  for (int i = 0; kwlist[i]; i++) {
    if (STREQ(str, kwlist[i])) {
      return true;
    }
  }

  return false;
}

/* -------------------------------------------------------------------- */
/** \name Character Classification
 *
 * Define `text.cc` functions here (declared in `BKE_text.h`),
 * This could be removed if Blender gets its own unicode library.
 * \{ */

int text_check_identifier_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier(char(ch))) || Py_UNICODE_ISALNUM(ch);
}

int text_check_identifier_nodigit_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier_nodigit(char(ch))) || Py_UNICODE_ISALPHA(ch);
}

/** \} */
