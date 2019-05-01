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

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"

#include "RNA_types.h"

#include "bpy.h"
#include "bpy_rna.h"
#include "bpy_path.h"
#include "bpy_capi_utils.h"
#include "bpy_traceback.h"
#include "bpy_intern_string.h"

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

#include "../generic/py_capi_utils.h"

/* inittab initialization functions */
#include "../generic/bgl.h"
#include "../generic/blf_py_api.h"
#include "../generic/idprop_py_api.h"
#include "../generic/imbuf_py_api.h"
#include "../gpu/gpu_py_api.h"
#include "../bmesh/bmesh_py_api.h"
#include "../mathutils/mathutils.h"

/* Logging types to use anywhere in the Python modules. */
CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_CONTEXT, "bpy.context");
CLG_LOGREF_DECLARE_GLOBAL(BPY_LOG_RNA, "bpy.rna");

/* for internal use, when starting and ending python scripts */

/* In case a python script triggers another python call,
 * stop bpy_context_clear from invalidating. */
static int py_call_level = 0;

// #define TIME_PY_RUN // simple python tests. prints on exit.

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

  BPy_SetContext(C);
  BPY_modules_update(C); /* can give really bad results if this isn't here */
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
void bpy_context_clear(bContext *UNUSED(C), PyGILState_STATE *gilstate)
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
    BPy_SetContext(NULL);
#endif

#ifdef TIME_PY_RUN
    bpy_timer_run_tot += PIL_check_seconds_timer() - bpy_timer_run;
    bpy_timer_count++;
#endif
  }
}

void BPY_text_free_code(Text *text)
{
  if (text->compiled) {
    PyGILState_STATE gilstate;
    bool use_gil = !PyC_IsInterpreterActive();

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

void BPY_modules_update(bContext *C)
{
#if 0 /* slow, this runs all the time poll, draw etc 100's of time a sec. */
  PyObject *mod = PyImport_ImportModuleLevel("bpy", NULL, NULL, NULL, 0);
  PyModule_AddObject(mod, "data", BPY_rna_module());
  PyModule_AddObject(mod, "types", BPY_rna_types()); /* atm this does not need updating */
#endif

  /* refreshes the main struct */
  BPY_update_rna_module();
  if (bpy_context_module) {
    bpy_context_module->ptr.data = (void *)C;
  }
}

void BPY_context_set(bContext *C)
{
  BPy_SetContext(C);
}

/* defined in AUD_C-API.cpp */
extern PyObject *AUD_initPython(void);

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
    {"imbuf", BPyInit_imbuf},
    {"bmesh", BPyInit_bmesh},
#if 0
    {"bmesh.types", BPyInit_bmesh_types},
    {"bmesh.utils", BPyInit_bmesh_utils},
    {"bmesh.utils", BPyInit_bmesh_geometry},
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

/* call BPY_context_set first */
void BPY_python_start(int argc, const char **argv)
{
#ifndef WITH_PYTHON_MODULE
  PyThreadState *py_tstate = NULL;
  const char *py_path_bundle = BKE_appdir_folder_id(BLENDER_SYSTEM_PYTHON, NULL);

  /* not essential but nice to set our name */
  static wchar_t program_path_wchar[FILE_MAX]; /* python holds a reference */
  BLI_strncpy_wchar_from_utf8(
      program_path_wchar, BKE_appdir_program_path(), ARRAY_SIZE(program_path_wchar));
  Py_SetProgramName(program_path_wchar);

  /* must run before python initializes */
  PyImport_ExtendInittab(bpy_internal_modules);

  /* allow to use our own included python */
  PyC_SetHomePath(py_path_bundle);

  /* without this the sys.stdout may be set to 'ascii'
   * (it is on my system at least), where printing unicode values will raise
   * an error, this is highly annoying, another stumbling block for devs,
   * so use a more relaxed error handler and enforce utf-8 since the rest of
   * blender is utf-8 too - campbell */
  Py_SetStandardStreamEncoding("utf-8", "surrogateescape");

  /* Update, Py3.3 resolves attempting to parse non-existing header */
#  if 0
  /* Python 3.2 now looks for '2.xx/python/include/python3.2d/pyconfig.h' to
   * parse from the 'sysconfig' module which is used by 'site',
   * so for now disable site. alternatively we could copy the file. */
  if (py_path_bundle) {
    Py_NoSiteFlag = 1;
  }
#  endif

  Py_FrozenFlag = 1;

  Py_Initialize();

  // PySys_SetArgv(argc, argv);  /* broken in py3, not a huge deal */
  /* sigh, why do python guys not have a (char **) version anymore? */
  {
    int i;
    PyObject *py_argv = PyList_New(argc);
    for (i = 0; i < argc; i++) {
      /* should fix bug #20021 - utf path name problems, by replacing
       * PyUnicode_FromString, with this one */
      PyList_SET_ITEM(py_argv, i, PyC_UnicodeFromByte(argv[i]));
    }

    PySys_SetObject("argv", py_argv);
    Py_DECREF(py_argv);
  }

  /* Initialize thread support (also acquires lock) */
  PyEval_InitThreads();
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
  BPy_init_modules();

  pyrna_alloc_types();

#ifndef WITH_PYTHON_MODULE
  /* py module runs atexit when bpy is freed */
  BPY_atexit_register(); /* this can init any time */

  py_tstate = PyGILState_GetThisThreadState();
  PyEval_ReleaseThread(py_tstate);
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

static void python_script_error_jump_text(struct Text *text)
{
  int lineno;
  int offset;
  python_script_error_jump(text->id.name + 2, &lineno, &offset);
  if (lineno != -1) {
    /* select the line with the error */
    txt_move_to(text, lineno - 1, INT_MAX, false);
    txt_move_to(text, lineno - 1, offset, true);
  }
}

/* super annoying, undo _PyModule_Clear(), bug [#23871] */
#define PYMODULE_CLEAR_WORKAROUND

#ifdef PYMODULE_CLEAR_WORKAROUND
/* bad!, we should never do this, but currently only safe way I could find to keep namespace.
 * from being cleared. - campbell */
typedef struct {
  PyObject_HEAD PyObject *md_dict;
  /* omit other values, we only want the dict. */
} PyModuleObject;
#endif

/* returns a dummy filename for a textblock so we can tell what file a text block comes from */
static void bpy_text_filename_get(char *fn, const Main *bmain, size_t fn_len, const Text *text)
{
  BLI_snprintf(fn, fn_len, "%s%c%s", ID_BLEND_PATH(bmain, &text->id), SEP, text->id.name + 2);
}

static bool python_script_exec(
    bContext *C, const char *fn, struct Text *text, struct ReportList *reports, const bool do_jump)
{
  Main *bmain_old = CTX_data_main(C);
  PyObject *main_mod = NULL;
  PyObject *py_dict = NULL, *py_result = NULL;
  PyGILState_STATE gilstate;

  BLI_assert(fn || text);

  if (fn == NULL && text == NULL) {
    return 0;
  }

  bpy_context_set(C, &gilstate);

  PyC_MainModule_Backup(&main_mod);

  if (text) {
    char fn_dummy[FILE_MAXDIR];
    bpy_text_filename_get(fn_dummy, bmain_old, sizeof(fn_dummy), text);

    if (text->compiled == NULL) { /* if it wasn't already compiled, do it now */
      char *buf;
      PyObject *fn_dummy_py;

      fn_dummy_py = PyC_UnicodeFromByte(fn_dummy);

      buf = txt_to_buf(text);
      text->compiled = Py_CompileStringObject(buf, fn_dummy_py, Py_file_input, NULL, -1);
      MEM_freeN(buf);

      Py_DECREF(fn_dummy_py);

      if (PyErr_Occurred()) {
        if (do_jump) {
          python_script_error_jump_text(text);
        }
        BPY_text_free_code(text);
      }
    }

    if (text->compiled) {
      py_dict = PyC_DefaultNameSpace(fn_dummy);
      py_result = PyEval_EvalCode(text->compiled, py_dict, py_dict);
    }
  }
  else {
    FILE *fp = BLI_fopen(fn, "r");

    if (fp) {
      py_dict = PyC_DefaultNameSpace(fn);

#ifdef _WIN32
      /* Previously we used PyRun_File to run directly the code on a FILE
       * object, but as written in the Python/C API Ref Manual, chapter 2,
       * 'FILE structs for different C libraries can be different and
       * incompatible'.
       * So now we load the script file data to a buffer.
       *
       * Note on use of 'globals()', it's important not copy the dictionary because
       * tools may inspect 'sys.modules["__main__"]' for variables defined in the code
       * where using a copy of 'globals()' causes code execution
       * to leave the main namespace untouched. see: T51444
       *
       * This leaves us with the problem of variables being included,
       * currently this is worked around using 'dict.__del__' it's ugly but works.
       */
      {
        const char *pystring =
            "with open(__file__, 'rb') as f:"
            "exec(compile(f.read(), __file__, 'exec'), globals().__delitem__('f') or globals())";

        fclose(fp);

        py_result = PyRun_String(pystring, Py_file_input, py_dict, py_dict);
      }
#else
      py_result = PyRun_File(fp, fn, Py_file_input, py_dict, py_dict);
      fclose(fp);
#endif
    }
    else {
      PyErr_Format(
          PyExc_IOError, "Python file \"%s\" could not be opened: %s", fn, strerror(errno));
      py_result = NULL;
    }
  }

  if (!py_result) {
    if (text) {
      if (do_jump) {
        /* ensure text is valid before use, the script may have freed its self */
        Main *bmain_new = CTX_data_main(C);
        if ((bmain_old == bmain_new) && (BLI_findindex(&bmain_new->texts, text) != -1)) {
          python_script_error_jump_text(text);
        }
      }
    }
    BPy_errors_to_report(reports);
  }
  else {
    Py_DECREF(py_result);
  }

  if (py_dict) {
#ifdef PYMODULE_CLEAR_WORKAROUND
    PyModuleObject *mmod = (PyModuleObject *)PyDict_GetItem(PyImport_GetModuleDict(),
                                                            bpy_intern_str___main__);
    PyObject *dict_back = mmod->md_dict;
    /* freeing the module will clear the namespace,
     * gives problems running classes defined in this namespace being used later. */
    mmod->md_dict = NULL;
    Py_DECREF(dict_back);
#endif

#undef PYMODULE_CLEAR_WORKAROUND
  }

  PyC_MainModule_Restore(main_mod);

  bpy_context_clear(C, &gilstate);

  return (py_result != NULL);
}

/* Can run a file or text block */
bool BPY_execute_filepath(bContext *C, const char *filepath, struct ReportList *reports)
{
  return python_script_exec(C, filepath, NULL, reports, false);
}

bool BPY_execute_text(bContext *C,
                      struct Text *text,
                      struct ReportList *reports,
                      const bool do_jump)
{
  return python_script_exec(C, NULL, text, reports, do_jump);
}

void BPY_DECREF(void *pyob_ptr)
{
  PyGILState_STATE gilstate = PyGILState_Ensure();
  Py_DECREF((PyObject *)pyob_ptr);
  PyGILState_Release(gilstate);
}

void BPY_DECREF_RNA_INVALIDATE(void *pyob_ptr)
{
  PyGILState_STATE gilstate = PyGILState_Ensure();
  const int do_invalidate = (Py_REFCNT((PyObject *)pyob_ptr) > 1);
  Py_DECREF((PyObject *)pyob_ptr);
  if (do_invalidate) {
    pyrna_invalidate(pyob_ptr);
  }
  PyGILState_Release(gilstate);
}

/**
 * \return success
 */
bool BPY_execute_string_as_number(
    bContext *C, const char *imports[], const char *expr, const bool verbose, double *r_value)
{
  PyGILState_STATE gilstate;
  bool ok = true;

  if (!r_value || !expr) {
    return -1;
  }

  if (expr[0] == '\0') {
    *r_value = 0.0;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsNumber(imports, expr, "<expr as number>", r_value);

  if (ok == false) {
    if (verbose) {
      BPy_errors_to_report_ex(CTX_wm_reports(C), false, false);
    }
    else {
      PyErr_Clear();
    }
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

/**
 * \return success
 */
bool BPY_execute_string_as_string(
    bContext *C, const char *imports[], const char *expr, const bool verbose, char **r_value)
{
  BLI_assert(r_value && expr);
  PyGILState_STATE gilstate;
  bool ok = true;

  if (expr[0] == '\0') {
    *r_value = NULL;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsString(imports, expr, "<expr as str>", r_value);

  if (ok == false) {
    if (verbose) {
      BPy_errors_to_report_ex(CTX_wm_reports(C), false, false);
    }
    else {
      PyErr_Clear();
    }
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

/**
 * Support both int and pointers.
 *
 * \return success
 */
bool BPY_execute_string_as_intptr(
    bContext *C, const char *imports[], const char *expr, const bool verbose, intptr_t *r_value)
{
  BLI_assert(r_value && expr);
  PyGILState_STATE gilstate;
  bool ok = true;

  if (expr[0] == '\0') {
    *r_value = 0;
    return ok;
  }

  bpy_context_set(C, &gilstate);

  ok = PyC_RunString_AsIntPtr(imports, expr, "<expr as intptr>", r_value);

  if (ok == false) {
    if (verbose) {
      BPy_errors_to_report_ex(CTX_wm_reports(C), false, false);
    }
    else {
      PyErr_Clear();
    }
  }

  bpy_context_clear(C, &gilstate);

  return ok;
}

bool BPY_execute_string_ex(bContext *C, const char *imports[], const char *expr, bool use_eval)
{
  BLI_assert(expr);
  PyGILState_STATE gilstate;
  PyObject *main_mod = NULL;
  PyObject *py_dict, *retval;
  bool ok = true;

  if (expr[0] == '\0') {
    return ok;
  }

  bpy_context_set(C, &gilstate);

  PyC_MainModule_Backup(&main_mod);

  py_dict = PyC_DefaultNameSpace("<blender string>");

  if (imports && (!PyC_NameSpace_ImportArray(py_dict, imports))) {
    Py_DECREF(py_dict);
    retval = NULL;
  }
  else {
    retval = PyRun_String(expr, use_eval ? Py_eval_input : Py_file_input, py_dict, py_dict);
  }

  if (retval == NULL) {
    ok = false;
    BPy_errors_to_report(CTX_wm_reports(C));
  }
  else {
    Py_DECREF(retval);
  }

  PyC_MainModule_Restore(main_mod);

  bpy_context_clear(C, &gilstate);

  return ok;
}

bool BPY_execute_string(bContext *C, const char *imports[], const char *expr)
{
  return BPY_execute_string_ex(C, imports, expr, true);
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
        BPY_execute_text(C, text, NULL, false);

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
  bool use_gil = !PyC_IsInterpreterActive();

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
    CTX_data_pointer_set(result, ptr->id.data, ptr->type, ptr->data);
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
      int len = PySequence_Fast_GET_SIZE(seq_fast);
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
          CTX_data_list_add(result, ptr->id.data, ptr->type, ptr->data);
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

  /* updating the module dict below will loose the reference to __file__ */
  PyObject *filename_obj = PyModule_GetFilenameObject(bpy_proxy);

  const char *filename_rel = _PyUnicode_AsString(filename_obj); /* can be relative */
  char filename_abs[1024];

  BLI_strncpy(filename_abs, filename_rel, sizeof(filename_abs));
  BLI_path_cwd(filename_abs, sizeof(filename_abs));
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
int text_check_identifier_unicode(const unsigned int ch)
{
  return (ch < 255 && text_check_identifier((char)ch)) || Py_UNICODE_ISALNUM(ch);
}

int text_check_identifier_nodigit_unicode(const unsigned int ch)
{
  return (ch < 255 && text_check_identifier_nodigit((char)ch)) || Py_UNICODE_ISALPHA(ch);
}
