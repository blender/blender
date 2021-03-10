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
 * Chris Keith, Chris Want, Ken Hughes, Campbell Barton
 */

/** \file
 * \ingroup pythonintern
 *
 * This file deals with embedding the python interpreter within blender,
 * starting and stopping python and exposing blender/python modules so they can
 * be accesses from scripts.
 */

#include <Python.h>
#include <frameobject.h>

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

/* use for updating while a python script runs - in case of file load */
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

/* context should be used but not now because it causes some bugs */
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
    /* XXX - Calling classes currently wont store the context :\,
     * cant set NULL because of this. but this is very flakey still. */
#if 0
    BPY_context_set(NULL);
#endif

#ifdef TIME_PY_RUN
    bpy_timer_run_tot += PIL_check_seconds_timer() - bpy_timer_run;
    bpy_timer_count++;
#endif
  }
}

/**
 * Use for `CTX_*_set(..)` functions need to set values which are later read back as expected.
 * In this case we don't want the Python context to override the values as it causes problems
 * see T66256.
 *
 * \param dict_p: A pointer to #bContext.data.py_context so we can assign a new value.
 * \param dict_orig: The value of #bContext.data.py_context_orig to check if we need to copy.
 *
 * \note Typically accessed via #BPY_context_dict_clear_members macro.
 */
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

/**
 * Needed so the #Main pointer in `bpy.data` doesn't become out of date.
 */
void BPY_modules_update(void)
{
#if 0 /* slow, this runs all the time poll, draw etc 100's of time a sec. */
  PyObject *mod = PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
  PyModule_AddObject(mod, "data", BPY_rna_module());
  PyModule_AddObject(mod, "types", BPY_rna_types()); /* atm this does not need updating */
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

#ifdef WITH_AUDASPACE
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
#ifdef WITH_AUDASPACE
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

/* call BPY_context_set first */
void BPY_python_start(bContext *C, int argc, const char **argv)
{
#ifndef WITH_PYTHON_MODULE

  /* #PyPreConfig (early-configuration).  */
  {
    PyPreConfig preconfig;
    PyStatus status;

    if (py_use_system_env) {
      PyPreConfig_InitPythonConfig(&preconfig);
    }
    else {
      /* Only use the systems environment variables and site when explicitly requested.
       * Since an incorrect 'PYTHONPATH' causes difficult to debug errors, see: T72807.
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

    /* When using the system's Python, allow the site-directory as well. */
    config.user_site_directory = py_use_system_env;

    /* While `sys.argv` is set, we don't want Python to interpret it. */
    config.parse_argv = 0;
    status = PyConfig_SetBytesArgv(&config, argc, (char *const *)argv);
    pystatus_exit_on_error(status);

    /* Needed for Python's initialization for portable Python installations.
     * We could use #Py_SetPath, but this overrides Python's internal logic
     * for calculating it's own module search paths.
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

  /* bpy.* and lets us import it */
  BPy_init_modules(C);

  pyrna_alloc_types();

#ifndef WITH_PYTHON_MODULE
  /* py module runs atexit when bpy is freed */
  BPY_atexit_register(); /* this can init any time */

  /* Free the lock acquired (implicitly) when Python is initialized. */
  PyEval_ReleaseThread(PyGILState_GetThisThreadState());

#endif

#ifdef WITH_PYTHON_MODULE
  /* Disable all add-ons at exit, not essential, it just avoids resource leaks, see T71362. */
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

  /* free other python data. */
  pyrna_free_types();

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
  PyThreadState *tstate = PyGILState_GetThisThreadState();
  if (tstate != NULL && tstate->frame != NULL) {
    PyFrameObject *frame = tstate->frame;
    do {
      const int line = PyCode_Addr2Line(frame->f_code, frame->f_lasti);
      const char *filename = PyUnicode_AsUTF8(frame->f_code->co_filename);
      const char *funcname = PyUnicode_AsUTF8(frame->f_code->co_name);
      fprintf(fp, "  File \"%s\", line %d in %s\n", filename, line, funcname);
    } while ((frame = frame->f_back));
  }
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
    if (text->flags & TXT_ISSCRIPT && BLI_path_extension_check(text->id.name + 2, ".py")) {
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
    CTX_data_pointer_set(result, ptr->owner_id, ptr->type, ptr->data);
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
          CTX_data_list_add(result, ptr->owner_id, ptr->type, ptr->data);
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
      CLOG_INFO(BPY_LOG_CONTEXT, 1, "'%s' not found\n", member);
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
/* TODO, reloading the module isn't functional at the moment. */

static void bpy_module_free(void *mod);

/* Defined in 'creator.c' when building as a Python module. */
extern int main_python_enter(int argc, const char **argv);
extern void main_python_exit(void);

static struct PyModuleDef bpy_proxy_def = {
    PyModuleDef_HEAD_INIT,
    "bpy",           /* m_name */
    NULL,            /* m_doc */
    0,               /* m_size */
    NULL,            /* m_methods */
    NULL,            /* m_reload */
    NULL,            /* m_traverse */
    NULL,            /* m_clear */
    bpy_module_free, /* m_free */
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
  PyObject *filename_obj = PyModule_GetFilenameObject(bpy_proxy);

  const char *filename_rel = PyUnicode_AsUTF8(filename_obj); /* can be relative */
  char filename_abs[1024];

  BLI_strncpy(filename_abs, filename_rel, sizeof(filename_abs));
  BLI_path_abs_from_cwd(filename_abs, sizeof(filename_abs));
  Py_DECREF(filename_obj);

  argv[0] = filename_abs;
  argv[1] = NULL;

  // printf("module found %s\n", argv[0]);

  main_python_enter(argc, argv);

  /* initialized in BPy_init_modules() */
  PyDict_Update(PyModule_GetDict(bpy_proxy), PyModule_GetDict(bpy_package_py));
}

static void dealloc_obj_dealloc(PyObject *self);

static PyTypeObject dealloc_obj_Type;

/* use our own dealloc so we can free a property if we use one */
static void dealloc_obj_dealloc(PyObject *self)
{
  bpy_module_delay_init(((dealloc_obj *)self)->mod);

  /* Note, for subclassed PyObjects we cant just call PyObject_DEL() directly or it will crash */
  dealloc_obj_Type.tp_free(self);
}

PyMODINIT_FUNC PyInit_bpy(void);

PyMODINIT_FUNC PyInit_bpy(void)
{
  PyObject *bpy_proxy = PyModule_Create(&bpy_proxy_def);

  /* Problem:
   * 1) this init function is expected to have a private member defined - 'md_def'
   *    but this is only set for C defined modules (not py packages)
   *    so we cant return 'bpy_package_py' as is.
   *
   * 2) there is a 'bpy' C module for python to load which is basically all of blender,
   *    and there is scripts/bpy/__init__.py,
   *    we may end up having to rename this module so there is no naming conflict here eg:
   *    'from blender import bpy'
   *
   * 3) we don't know the filename at this point, workaround by assigning a dummy value
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

/**
 * Avoids duplicating keyword list.
 */
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

/* EVIL, define text.c functions here... */
/* BKE_text.h */
int text_check_identifier_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier((char)ch)) || Py_UNICODE_ISALNUM(ch);
}

int text_check_identifier_nodigit_unicode(const uint ch)
{
  return (ch < 255 && text_check_identifier_nodigit((char)ch)) || Py_UNICODE_ISALPHA(ch);
}
