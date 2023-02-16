/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file deals with embedding the python interpreter within blender,
 * starting and stopping python and exposing blender/python modules so they can
 * be accesses from scripts.
 */

#include <Python.h>
#include <frameobject.h>

#ifdef WITH_PYTHON_MODULE
#  include "pylifecycle.h" /* For `Py_Version`. */
#endif

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "RNA_types.h"

#include "bpy.h"
#include "bpy_capi_utils.h"
#include "bpy_intern_string.h"
#include "bpy_path.h"
#include "bpy_props.h"
#include "bpy_rna.h"
#include "bpy_traceback.h"

#include "bpy_app_translations.h"

#include "DNA_text_types.h"

#include "BKE_appdir.h"
#include "BKE_context.h"
#include "BKE_global.h" /* only for script checking */
#include "BKE_main.h"
#include "BKE_text.h"

#ifdef WITH_CYCLES
#  include "CCL_api.h"
#endif

#include "BPY_extern.h"
#include "BPY_extern_python.h"
#include "BPY_extern_run.h"

#include "../generic/py_capi_utils.h"

/* inittab initialization functions */
#include "../bmesh/bmesh_py_api.h"
#include "../generic/bgl.h"
#include "../generic/bl_math_py_api.h"
#include "../generic/blf_py_api.h"
#include "../generic/idprop_py_api.h"
#include "../generic/imbuf_py_api.h"
#include "../gpu/gpu_py_api.h"
#include "../mathutils/mathutils.h"

/* Logging types to use anywhere in the Python modules. */

CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_CONTEXT, "bpy.context");
CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_INTERFACE, "bpy.interface");
CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_RNA, "bpy.rna");

/* for internal use, when starting and ending python scripts */

/* In case a python script triggers another python call,
 * stop bpy_context_clear from invalidating. */
static int py_call_level = 0;

/* Set by command line arguments before Python starts. */
static bool py_use_system_env = false;

// #define TIME_PY_RUN /* simple python tests. prints on exit. */

#ifdef TIME_PY_RUN
#  include "PIL_time.h"
static int bpy_timer_count = 0;
static double bpy_timer;         /* time since python starts */
static double bpy_timer_run;     /* time for each python script run */
static double bpy_timer_run_tot; /* accumulate python runs */
#endif

void BPY_context_update(bContext *C)
{
  /* don't do this from a non-main (e.g. render) thread, it can cause a race
   * condition on C->data.recursion. ideal solution would be to disable
   * context entirely from non-main threads, but that's more complicated */
  if (!BLI_thread_is_main()) {
    return;
  }

  BPY_context_set(C);
  BPY_modules_update(); /* can give really bad results if this isn't here */
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
      /* record time from the beginning */
      bpy_timer = PIL_check_seconds_timer();
      bpy_timer_run = bpy_timer_run_tot = 0.0;
    }
    bpy_timer_run = PIL_check_seconds_timer();

    bpy_timer_count++;
#endif
  }
}

void bpy_context_clear(bContext *UNUSED(C), const PyGILState_STATE *gilstate)
{
  py_call_level--;

  if (gilstate) {
    PyGILState_Release(*gilstate);
  }

  if (py_call_level < 0) {
    fprintf(stderr, "ERROR: Python context internal state bug. this should not happen!\n");
  }
  else if (py_call_level == 0) {
    /* XXX: Calling classes currently won't store the context :\,
     * can't set NULL because of this. but this is very flaky still. */
#if 0
    BPY_context_set(NULL);
#endif

#ifdef TIME_PY_RUN
    bpy_timer_run_tot += PIL_check_seconds_timer() - bpy_timer_run;
    bpy_timer_count++;
#endif
  }
}

static void bpy_context_end(bContext *C)
{
  if (UNLIKELY(C == NULL)) {
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
    *dict_p = PyDict_Copy(dict_orig);
  }

  PyObject *dict = *dict_p;
  BLI_assert(PyDict_Check(dict));

  /* Use #PyDict_Pop instead of #PyDict_DelItemString to avoid setting the exception,
   * while supported it's good to avoid for low level functions like this that run often. */
  for (uint i = 0; i < context_members_len; i++) {
    PyObject *key = PyUnicode_FromString(context_members[i]);
    PyObject *item = _PyDict_Pop(dict, key, Py_None);
    Py_DECREF(key);
    Py_DECREF(item);
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
    text->compiled = NULL;

    if (use_gil) {
      PyGILState_Release(gilstate);
    }
  }
}

void BPY_modules_update(void)
{
#if 0 /* slow, this runs all the time poll, draw etc 100's of time a sec. */
  PyObject *mod = PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
  PyModule_AddObject(mod, "data", BPY_rna_module());
  PyModule_AddObject(mod, "types", BPY_rna_types()); /* This does not need updating. */
#endif

  /* refreshes the main struct */
  BPY_update_rna_module();
}

bContext *BPY_context_get(void)
{
  return bpy_context_module->ptr.data;
}

void BPY_context_set(bContext *C)
{
  bpy_context_module->ptr.data = (void *)C;
}

#ifdef WITH_FLUID
/* defined in manta module */
extern PyObject *Manta_initPython(void);
#endif

#ifdef WITH_AUDASPACE_PY
/* defined in AUD_C-API.cpp */
extern PyObject *AUD_initPython(void);
#endif

#ifdef WITH_CYCLES
/* defined in cycles module */
static PyObject *CCL_initPython(void)
{
  return (PyObject *)CCL_python_module_init();
}
#endif

static struct _inittab bpy_internal_modules[] = {
    {"mathutils", PyInit_mathutils},
#if 0
    {"mathutils.geometry", PyInit_mathutils_geometry},
    {"mathutils.noise", PyInit_mathutils_noise},
    {"mathutils.kdtree", PyInit_mathutils_kdtree},
#endif
    {"_bpy_path", BPyInit__bpy_path},
    {"bgl", BPyInit_bgl},
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
    {NULL, NULL},
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
static void pystatus_exit_on_error(PyStatus status)
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

  /* #PyPreConfig (early-configuration). */
  {
    PyPreConfig preconfig;
    PyStatus status;

    /* To narrow down reports where the systems Python is inexplicably used, see: #98131. */
    CLOG_INFO(
        BPY_LOG_INTERFACE,
        2,
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

    /* Force `utf-8` on all platforms, since this is what's used for Blender's internal strings,
     * providing consistent encoding behavior across all Blender installations.
     *
     * This also uses the `surrogateescape` error handler ensures any unexpected bytes are escaped
     * instead of raising an error.
     *
     * Without this `sys.getfilesystemencoding()` and `sys.stdout` for example may be set to ASCII
     * or some other encoding - where printing some `utf-8` values will raise an error.
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

    PyConfig_InitPythonConfig(&config);

    /* Suppress error messages when calculating the module search path.
     * While harmless, it's noisy. */
    config.pathconfig_warnings = 0;

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
              program_path, sizeof(program_path), PY_MAJOR_VERSION, PY_MINOR_VERSION)) {
        status = PyConfig_SetBytesString(&config, &config.executable, program_path);
        pystatus_exit_on_error(status);
        has_python_executable = true;
      }
      else {
        /* Set to `sys.executable = None` below (we can't do before Python is initialized). */
        fprintf(stderr,
                "Unable to find the python binary, "
                "the multiprocessing module may not be functional!\n");
      }
    }

    /* Allow to use our own included Python. `py_path_bundle` may be NULL. */
    {
      const char *py_path_bundle = BKE_appdir_folder_id(BLENDER_SYSTEM_PYTHON, NULL);
      if (py_path_bundle != NULL) {

#  ifdef __APPLE__
        /* Mac-OS allows file/directory names to contain `:` character
         * (represented as `/` in the Finder) but current Python lib (as of release 3.1.1)
         * doesn't handle these correctly. */
        if (strchr(py_path_bundle, ':')) {
          fprintf(stderr,
                  "Warning! Blender application is located in a path containing ':' or '/' chars\n"
                  "This may make python import function fail\n");
        }
#  endif /* __APPLE__ */

        status = PyConfig_SetBytesString(&config, &config.home, py_path_bundle);
        pystatus_exit_on_error(status);
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

#else
  (void)argc;
  (void)argv;

  /* must run before python initializes */
  /* broken in py3.3, load explicitly below */
  // PyImport_ExtendInittab(bpy_internal_modules);
#endif

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
        PyErr_Clear();
      }
      // Py_DECREF(mod); /* ideally would decref, but in this case we never want to free */
    }
  }
#endif

  /* Run first, initializes RNA types. */
  BPY_rna_init();

  /* Defines `bpy.*` and lets us import it. */
  BPy_init_modules(C);

  pyrna_alloc_types();

#ifndef WITH_PYTHON_MODULE
  /* py module runs atexit when bpy is freed */
  BPY_atexit_register(); /* this can init any time */

  /* Free the lock acquired (implicitly) when Python is initialized. */
  PyEval_ReleaseThread(PyGILState_GetThisThreadState());

#endif

#ifdef WITH_PYTHON_MODULE
  /* Disable all add-ons at exit, not essential, it just avoids resource leaks, see #71362. */
  BPY_run_string_eval(C,
                      (const char *[]){"atexit", "addon_utils", NULL},
                      "atexit.register(addon_utils.disable_all)");
#endif
}

void BPY_python_end(void)
{
  // fprintf(stderr, "Ending Python!\n");
  PyGILState_STATE gilstate;

  /* finalizing, no need to grab the state, except when we are a module */
  gilstate = PyGILState_Ensure();

  /* Frees the python-driver name-space & cached data. */
  BPY_driver_exit();

  /* Clear Python values in the context so freeing the context after Python exits doesn't crash. */
  bpy_context_end(BPY_context_get());

  /* Decrement user counts of all callback functions. */
  BPY_rna_props_clear_all();

  /* free other python data. */
  pyrna_free_types();

  BPY_rna_exit();

  /* clear all python data from structs */

  bpy_intern_string_exit();

  /* bpy.app modules that need cleanup */
  BPY_app_translations_end();

#ifndef WITH_PYTHON_MODULE
  BPY_atexit_unregister(); /* without this we get recursive calls to WM_exit */

  Py_Finalize();

  (void)gilstate;
#else
  PyGILState_Release(gilstate);
#endif

#ifdef TIME_PY_RUN
  /* measure time since py started */
  bpy_timer = PIL_check_seconds_timer() - bpy_timer;

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

  // fprintf(stderr, "Ending Python Done!\n");

#endif
}

void BPY_python_reset(bContext *C)
{
  /* unrelated security stuff */
  G.f &= ~(G_FLAG_SCRIPT_AUTOEXEC_FAIL | G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET);
  G.autoexec_fail[0] = '\0';

  BPY_driver_reset();
  BPY_app_handlers_reset(false);
  BPY_modules_load_user(C);
}

void BPY_python_use_system_env(void)
{
  BLI_assert(!Py_IsInitialized());
  py_use_system_env = true;
}

void BPY_python_backtrace(FILE *fp)
{
  fputs("\n# Python backtrace\n", fp);

  /* Can happen in rare cases. */
  if (!_PyThreadState_UncheckedGet()) {
    return;
  }
  PyFrameObject *frame;
  if (!(frame = PyEval_GetFrame())) {
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
    pyrna_invalidate(pyob_ptr);
  }
  PyGILState_Release(gilstate);
}

void BPY_modules_load_user(bContext *C)
{
  PyGILState_STATE gilstate;
  Main *bmain = CTX_data_main(C);
  Text *text;

  /* can happen on file load */
  if (bmain == NULL) {
    return;
  }

  /* update pointers since this can run from a nested script
   * on file load */
  if (py_call_level) {
    BPY_context_update(C);
  }

  bpy_context_set(C, &gilstate);

  for (text = bmain->texts.first; text; text = text->id.next) {
    if (text->flags & TXT_ISSCRIPT) {
      if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC)) {
        if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET)) {
          G.f |= G_FLAG_SCRIPT_AUTOEXEC_FAIL;
          BLI_snprintf(G.autoexec_fail, sizeof(G.autoexec_fail), "Text '%s'", text->id.name + 2);

          printf("scripts disabled for \"%s\", skipping '%s'\n",
                 BKE_main_blendfile_path(bmain),
                 text->id.name + 2);
        }
      }
      else {
        BPY_run_text(C, text, NULL, false);

        /* Check if the script loaded a new file. */
        if (bmain != CTX_data_main(C)) {
          break;
        }
      }
    }
  }
  bpy_context_clear(C, &gilstate);
}

int BPY_context_member_get(bContext *C, const char *member, bContextDataResult *result)
{
  PyGILState_STATE gilstate;
  const bool use_gil = !PyC_IsInterpreterActive();

  PyObject *pyctx;
  PyObject *item;
  PointerRNA *ptr = NULL;
  bool done = false;

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  pyctx = (PyObject *)CTX_py_dict_get(C);
  item = PyDict_GetItemString(pyctx, member);

  if (item == NULL) {
    /* pass */
  }
  else if (item == Py_None) {
    done = true;
  }
  else if (BPy_StructRNA_Check(item)) {
    ptr = &(((BPy_StructRNA *)item)->ptr);

    // result->ptr = ((BPy_StructRNA *)item)->ptr;
    CTX_data_pointer_set_ptr(result, ptr);
    CTX_data_type_set(result, CTX_DATA_TYPE_POINTER);
    done = true;
  }
  else if (PySequence_Check(item)) {
    PyObject *seq_fast = PySequence_Fast(item, "bpy_context_get sequence conversion");
    if (seq_fast == NULL) {
      PyErr_Print();
      PyErr_Clear();
    }
    else {
      const int len = PySequence_Fast_GET_SIZE(seq_fast);
      PyObject **seq_fast_items = PySequence_Fast_ITEMS(seq_fast);
      int i;

      for (i = 0; i < len; i++) {
        PyObject *list_item = seq_fast_items[i];

        if (BPy_StructRNA_Check(list_item)) {
#if 0
          CollectionPointerLink *link = MEM_callocN(sizeof(CollectionPointerLink),
                                                    "bpy_context_get");
          link->ptr = ((BPy_StructRNA *)item)->ptr;
          BLI_addtail(&result->list, link);
#endif
          ptr = &(((BPy_StructRNA *)list_item)->ptr);
          CTX_data_list_add_ptr(result, ptr);
        }
        else {
          CLOG_INFO(BPY_LOG_CONTEXT,
                    1,
                    "'%s' list item not a valid type in sequence type '%s'",
                    member,
                    Py_TYPE(item)->tp_name);
        }
      }
      Py_DECREF(seq_fast);
      CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
      done = true;
    }
  }

  if (done == false) {
    if (item) {
      CLOG_INFO(BPY_LOG_CONTEXT, 1, "'%s' not a valid type", member);
    }
    else {
      CLOG_INFO(BPY_LOG_CONTEXT, 1, "'%s' not found", member);
    }
  }
  else {
    CLOG_INFO(BPY_LOG_CONTEXT, 2, "'%s' found", member);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  return done;
}

#ifdef WITH_PYTHON_MODULE
/* TODO: reloading the module isn't functional at the moment. */

static void bpy_module_free(void *mod);

/* Defined in 'creator.c' when building as a Python module. */
extern int main_python_enter(int argc, const char **argv);
extern void main_python_exit(void);

static struct PyModuleDef bpy_proxy_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy",
    /*m_doc*/ NULL,
    /*m_size*/ 0,
    /*m_methods*/ NULL,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ bpy_module_free,
};

typedef struct {
  PyObject_HEAD
  /* Type-specific fields go here. */
  PyObject *mod;
} dealloc_obj;

/* call once __file__ is set */
static void bpy_module_delay_init(PyObject *bpy_proxy)
{
  const int argc = 1;
  const char *argv[2];

  /* updating the module dict below will lose the reference to __file__ */
  PyObject *filepath_obj = PyModule_GetFilenameObject(bpy_proxy);

  const char *filepath_rel = PyUnicode_AsUTF8(filepath_obj); /* can be relative */
  char filepath_abs[1024];

  BLI_strncpy(filepath_abs, filepath_rel, sizeof(filepath_abs));
  BLI_path_abs_from_cwd(filepath_abs, sizeof(filepath_abs));
  Py_DECREF(filepath_obj);

  argv[0] = filepath_abs;
  argv[1] = NULL;

  // printf("module found %s\n", argv[0]);

  main_python_enter(argc, argv);

  /* initialized in BPy_init_modules() */
  PyDict_Update(PyModule_GetDict(bpy_proxy), PyModule_GetDict(bpy_package_py));
}

/**
 * Raise an error and return false if the Python version used to compile Blender
 * isn't compatible with the interpreter loading the `bpy` module.
 */
static bool bpy_module_ensure_compatible_version(void)
{
  /* First check the Python version used matches the major version that Blender was built with.
   * While this isn't essential, the error message in this case may be cryptic and misleading.
   * NOTE: using `Py_LIMITED_API` would remove the need for this, in practice it's
   * unlikely Blender will ever used the limited API though. */
#  if PY_VERSION_HEX >= 0x030b0000 /* Python 3.11 & newer. */
  const uint version_runtime = Py_Version;
#  else
  uint version_runtime;
  {
    uint version_runtime_major = 0, version_runtime_minor = 0;
    const char *version_str = Py_GetVersion();
    if (sscanf(version_str, "%u.%u.", &version_runtime_major, &version_runtime_minor) != 2) {
      /* Should never happen, raise an error to ensure this check never fails silently. */
      PyErr_Format(PyExc_ImportError, "Failed to extract the version from \"%s\"", version_str);
      return false;
    }
    version_runtime = (version_runtime_major << 24) | (version_runtime_minor << 16);
  }
#  endif

  uint version_compile_major = PY_VERSION_HEX >> 24;
  uint version_compile_minor = ((PY_VERSION_HEX & 0x00ff0000) >> 16);
  uint version_runtime_major = version_runtime >> 24;
  uint version_runtime_minor = ((version_runtime & 0x00ff0000) >> 16);
  if ((version_compile_major != version_runtime_major) ||
      (version_compile_minor != version_runtime_minor)) {
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

/* use our own dealloc so we can free a property if we use one */
static void dealloc_obj_dealloc(PyObject *self)
{
  bpy_module_delay_init(((dealloc_obj *)self)->mod);

  /* NOTE: for subclassed PyObjects we can't just call PyObject_DEL() directly or it will crash. */
  dealloc_obj_Type.tp_free(self);
}

PyMODINIT_FUNC PyInit_bpy(void);

PyMODINIT_FUNC PyInit_bpy(void)
{
  if (!bpy_module_ensure_compatible_version()) {
    return NULL; /* The error has been set. */
  }

  PyObject *bpy_proxy = PyModule_Create(&bpy_proxy_def);

  /* Problem:
   * 1) this init function is expected to have a private member defined - `md_def`
   *    but this is only set for C defined modules (not py packages)
   *    so we can't return 'bpy_package_py' as is.
   *
   * 2) there is a 'bpy' C module for python to load which is basically all of blender,
   *    and there is `scripts/bpy/__init__.py`,
   *    we may end up having to rename this module so there is no naming conflict here eg:
   *    'from blender import bpy'
   *
   * 3) we don't know the filepath at this point, workaround by assigning a dummy value
   *    which calls back when its freed so the real loading can take place.
   */

  /* assign an object which is freed after __file__ is assigned */
  dealloc_obj *dob;

  /* assign dummy type */
  dealloc_obj_Type.tp_name = "dealloc_obj";
  dealloc_obj_Type.tp_basicsize = sizeof(dealloc_obj);
  dealloc_obj_Type.tp_dealloc = dealloc_obj_dealloc;
  dealloc_obj_Type.tp_flags = Py_TPFLAGS_DEFAULT;

  if (PyType_Ready(&dealloc_obj_Type) < 0) {
    return NULL;
  }

  dob = (dealloc_obj *)dealloc_obj_Type.tp_alloc(&dealloc_obj_Type, 0);
  dob->mod = bpy_proxy;                                       /* borrow */
  PyModule_AddObject(bpy_proxy, "__file__", (PyObject *)dob); /* borrow */

  return bpy_proxy;
}

static void bpy_module_free(void *UNUSED(mod))
{
  main_python_exit();
}

#endif

bool BPY_string_is_keyword(const char *str)
{
  /* list is from...
   * ", ".join(['"%s"' % kw for kw in  __import__("keyword").kwlist])
   */
  const char *kwlist[] = {
      "False", "None",     "True",  "and",    "as",   "assert", "async",  "await",    "break",
      "class", "continue", "def",   "del",    "elif", "else",   "except", "finally",  "for",
      "from",  "global",   "if",    "import", "in",   "is",     "lambda", "nonlocal", "not",
      "or",    "pass",     "raise", "return", "try",  "while",  "with",   "yield",    NULL,
  };

  for (int i = 0; kwlist[i]; i++) {
    if (STREQ(str, kwlist[i])) {
      return true;
    }
  }

  return false;
}

/* EVIL: define `text.c` functions here (declared in `BKE_text.h`). */
int text_check_identifier_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier((char)ch)) || Py_UNICODE_ISALNUM(ch);
}

int text_check_identifier_nodigit_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier_nodigit((char)ch)) || Py_UNICODE_ISALPHA(ch);
}
