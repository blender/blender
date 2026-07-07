/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines the #BPY_driver_exec to execute python driver expressions,
 * called by the animation system, there are also some utility functions
 * to deal with the name-space used for driver execution.
 */

#include <Python.h>

#include "DNA_anim_types.h"

#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BKE_animsys.h"
#include "BKE_fcurve_driver.h"
#include "BKE_global.hh"
#include "BKE_idtype.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "bpy_rna_driver.hh" /* For #pyrna_driver_get_variable_value. */

#include "bpy_intern_string.hh"

#include "bpy_driver.hh"
#include "bpy_driver_bytecode.hh"
#include "bpy_rna.hh"

#include "BPY_extern.hh"

namespace blender {

#define USE_RNA_AS_PYOBJECT

PyObject *bpy_pydriver_Dict = nullptr;

#ifdef USE_BYTECODE_SECURE
static PyObject *bpy_pydriver_Dict__secure_ids = nullptr;
#endif

int bpy_pydriver_create_dict()
{
  PyObject *d, *mod;

  /* Validate name-space for driver evaluation. */
  if (bpy_pydriver_Dict) {
    return -1;
  }

  d = PyDict_New();
  if (d == nullptr) {
    return -1;
  }

  bpy_pydriver_Dict = d;

  /* Import some modules: `builtins`, `bpy`, `math`, `mathutils.noise`. */
  PyDict_SetItemString(d, "__builtins__", PyEval_GetBuiltins());

  mod = PyImport_ImportModule("math");
  if (mod) {
    PyDict_Merge(d, PyModule_GetDict(mod), 0); /* 0 - don't overwrite existing values */
    Py_DECREF(mod);
  }
#ifdef USE_BYTECODE_SECURE
  PyObject *mod_math = mod;
#endif

  /* Add `bpy` to global name-space. */
  mod = PyImport_ImportModuleLevel("bpy", nullptr, nullptr, nullptr, 0);
  if (mod) {
    PyDict_SetItemString(bpy_pydriver_Dict, "bpy", mod);
    Py_DECREF(mod);
  }

  /* Add noise to global name-space. */
  mod = PyImport_ImportModuleLevel("mathutils", nullptr, nullptr, nullptr, 0);
  if (mod) {
    PyObject *modsub = PyDict_GetItemString(PyModule_GetDict(mod), "noise");
    PyDict_SetItemString(bpy_pydriver_Dict, "noise", modsub);
    Py_DECREF(mod);
  }

  /* Add math utility functions. */
  mod = PyImport_ImportModuleLevel("bl_math", nullptr, nullptr, nullptr, 0);
  if (mod) {
    static const char *names[] = {"clamp", "lerp", "smoothstep", nullptr};

    for (const char **pname = names; *pname; ++pname) {
      PyObject *func = PyDict_GetItemString(PyModule_GetDict(mod), *pname);
      PyDict_SetItemString(bpy_pydriver_Dict, *pname, func);
    }

    Py_DECREF(mod);
  }

#ifdef USE_BYTECODE_SECURE
  /* Setup the secure_ids. */
  {
    bpy_pydriver_Dict__secure_ids = PyDict_New();
    const char *secure_ids[] = {
        /* builtins (basic) */
        "all",
        "any",
        "len",
        /* builtins (numeric) */
        "max",
        "min",
        "pow",
        "round",
        "sum",
        /* types */
        "bool",
        "float",
        "int",
        /* bl_math */
        "clamp",
        "lerp",
        "smoothstep",

        nullptr,
    };

    for (int i = 0; secure_ids[i]; i++) {
      PyDict_SetItemString(bpy_pydriver_Dict__secure_ids, secure_ids[i], Py_None);
    }

    /* Add all of `math` functions. */
    if (mod_math != nullptr) {
      PyObject *mod_math_dict = PyModule_GetDict(mod_math);
      PyObject *arg_key, *arg_value;
      Py_ssize_t arg_pos = 0;
      while (PyDict_Next(mod_math_dict, &arg_pos, &arg_key, &arg_value)) {
        const char *arg_str = PyUnicode_AsUTF8(arg_key);
        if (arg_str[0] && arg_str[1] != '_') {
          PyDict_SetItem(bpy_pydriver_Dict__secure_ids, arg_key, Py_None);
        }
      }
    }
  }
#endif /* USE_BYTECODE_SECURE */

  return 0;
}

/**
 * \note this function should do nothing most runs, only when changing frame.
 * Not thread safe but neither is Python.
 */
static struct {
  float evaltime = FLT_MAX;

  /* Borrowed reference to the `self` in `bpy_pydriver_Dict`
   * keep for as long as the same self is used. */
  PyObject *self = nullptr;
  BPy_StructRNA *depsgraph = nullptr;
} g_pydriver_state_prev;

static void bpy_pydriver_namespace_update_frame(const float evaltime)
{
  if (g_pydriver_state_prev.evaltime != evaltime) {
    PyObject *item = PyFloat_FromDouble(evaltime);
    PyDict_SetItem(bpy_pydriver_Dict, bpy_intern_str_frame, item);
    Py_DECREF(item);

    g_pydriver_state_prev.evaltime = evaltime;
  }
}

static void bpy_pydriver_namespace_update_self(PathResolvedRNA *anim_rna)
{
  if ((g_pydriver_state_prev.self == nullptr) ||
      (pyrna_driver_is_equal_anim_rna(anim_rna, g_pydriver_state_prev.self) == false))
  {
    PyObject *item = pyrna_driver_self_from_anim_rna(anim_rna);
    PyDict_SetItem(bpy_pydriver_Dict, bpy_intern_str_self, item);
    Py_DECREF(item);

    g_pydriver_state_prev.self = item;
  }
}

static void bpy_pydriver_namespace_clear_self()
{
  if (g_pydriver_state_prev.self) {
    PyDict_DelItem(bpy_pydriver_Dict, bpy_intern_str_self);

    g_pydriver_state_prev.self = nullptr;
  }
}

static PyObject *bpy_pydriver_depsgraph_as_pyobject(Depsgraph *depsgraph)
{
  PointerRNA depsgraph_ptr = RNA_pointer_create_discrete(nullptr, RNA_Depsgraph, depsgraph);
  return pyrna_struct_CreatePyObject(&depsgraph_ptr);
}

/**
 * Adds a variable `depsgraph` to the name-space. This can then be used to obtain evaluated
 * data-blocks, and the current view layer and scene. See #75553.
 */
static void bpy_pydriver_namespace_update_depsgraph(Depsgraph *depsgraph)
{
  /* This should never happen, but it's probably better to have None in Python
   * than a nullptr-wrapping Depsgraph Python struct. */
  BLI_assert(depsgraph != nullptr);
  if (UNLIKELY(depsgraph == nullptr)) {
    PyDict_SetItem(bpy_pydriver_Dict, bpy_intern_str_depsgraph, Py_None);
    g_pydriver_state_prev.depsgraph = nullptr;
    return;
  }

  if ((g_pydriver_state_prev.depsgraph == nullptr) ||
      (depsgraph != g_pydriver_state_prev.depsgraph->ptr->data))
  {
    PyObject *item = bpy_pydriver_depsgraph_as_pyobject(depsgraph);
    PyDict_SetItem(bpy_pydriver_Dict, bpy_intern_str_depsgraph, item);
    Py_DECREF(item);

    g_pydriver_state_prev.depsgraph = reinterpret_cast<BPy_StructRNA *>(item);
  }
}

void BPY_driver_exit()
{
  if (bpy_pydriver_Dict) { /* Free the global dict used by python-drivers. */
    PyDict_Clear(bpy_pydriver_Dict);
    Py_DECREF(bpy_pydriver_Dict);
    bpy_pydriver_Dict = nullptr;
  }

#ifdef USE_BYTECODE_SECURE
  if (bpy_pydriver_Dict__secure_ids) {
    PyDict_Clear(bpy_pydriver_Dict__secure_ids);
    Py_DECREF(bpy_pydriver_Dict__secure_ids);
    bpy_pydriver_Dict__secure_ids = nullptr;
  }
#endif

  g_pydriver_state_prev.evaltime = FLT_MAX;

  /* Freed when clearing driver dictionary. */
  g_pydriver_state_prev.self = nullptr;
  g_pydriver_state_prev.depsgraph = nullptr;
}

void BPY_driver_reset()
{
  PyGILState_STATE gilstate;
  const bool use_gil = true; /* !PyC_IsInterpreterActive(); */
  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  /* Currently exit/reset are practically the same besides the GIL check. */
  BPY_driver_exit();

  if (use_gil) {
    PyGILState_Release(gilstate);
  }
}

/**
 * Error return function for #BPY_driver_exec.
 *
 * \param anim_rna: Used to show the target when printing the error to give additional context.
 */
static void pydriver_error(ChannelDriver *driver, const PathResolvedRNA *anim_rna)
{
  driver->flag |= DRIVER_FLAG_INVALID; /* Python expression failed. */

  const char *null_str = "<null>";
  const ID *id = anim_rna->ptr.owner_id;
  fprintf(stderr,
          "\n"
          "Error in PyDriver: expression failed: %s\n"
          "For target: (type=%s, name=\"%s\", property=%s, property_index=%d)\n"
          "\n",
          driver->expression,
          id ? BKE_idtype_idcode_to_name(GS(id->name)) : null_str,
          id ? id->name + 2 : null_str,
          anim_rna->prop ? RNA_property_identifier(anim_rna->prop) : null_str,
          anim_rna->prop_index);

  // BPy_errors_to_report(nullptr); /* TODO: reports. */
  PyErr_Print();
}

bool BPY_driver_secure_bytecode_test(PyObject *expr_code,
                                     PyObject *py_namespace,
                                     const bool verbose)
{

  if (!bpy_pydriver_Dict) {
    if (bpy_pydriver_create_dict() != 0) {
      fprintf(stderr, "%s: couldn't create Python dictionary\n", __func__);
      return false;
    }
  }
#ifdef USE_BYTECODE_SECURE
  PyObject *py_namespaces[] = {
      bpy_pydriver_Dict,
      bpy_pydriver_Dict__secure_ids,
      py_namespace,
      nullptr,
  };
  return BPY_driver_secure_bytecode_test_ex(expr_code, py_namespaces, verbose, __func__);
#else
  UNUSED_VARS(expr_code, py_namespace, verbose);
  return false;
#endif
}

float BPY_driver_exec(PathResolvedRNA *anim_rna,
                      ChannelDriver *driver,
                      ChannelDriver *driver_orig,
                      const AnimationEvalContext *anim_eval_context)
{
  /* (old) NOTE: PyGILState_Ensure() isn't always called because python can call
   * the bake operator which intern starts a thread which calls scene update
   * which does a driver update. to avoid a deadlock check #PyC_IsInterpreterActive()
   * if #PyGILState_Ensure() is needed, see #27683.
   *
   * (new) NOTE: checking if python is running is not thread-safe #28114
   * now release the GIL on python operator execution instead, using
   * #PyEval_SaveThread() / #PyEval_RestoreThread() so we don't lock up blender.
   *
   * For copy-on-evaluation we always cache expressions and write errors in the
   * original driver, otherwise these would get freed while editing.
   * Due to the GIL this is thread-safe. */

  PyObject *driver_vars = nullptr;
  PyObject *retval = nullptr;

  /* Speed up by pre-hashing string & avoids re-converting unicode strings for every execution. */
  PyObject *expr_vars;

  PyObject *expr_code;
  PyGILState_STATE gilstate;
  bool use_gil;

  DriverVar *dvar;
  double result = 0.0; /* Default return. */
  const char *expr;
  bool targets_ok = true;
  int i;

  /* Get the python expression to be evaluated. */
  expr = driver_orig->expression;
  if (expr[0] == '\0') {
    return 0.0f;
  }

#ifndef USE_BYTECODE_SECURE
  if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC)) {
    if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET)) {
      G.f |= G_FLAG_SCRIPT_AUTOEXEC_FAIL;
      SNPRINTF_UTF8(G.autoexec_fail, "Driver '%s'", expr);

      printf("skipping driver '%s', automatic scripts are disabled\n", expr);
    }
    driver_orig->flag |= DRIVER_FLAG_PYTHON_BLOCKED;
    return 0.0f;
  }
#else
  bool is_recompile = false;
#endif

  use_gil = true; /* !PyC_IsInterpreterActive(); */

  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  /* Needed since drivers are updated directly after undo where `main` is re-allocated #28807. */
  BPY_update_rna_module();

  /* Initialize global dictionary for Python driver evaluation settings. */
  if (!bpy_pydriver_Dict) {
    if (bpy_pydriver_create_dict() != 0) {
      fprintf(stderr, "%s: couldn't create Python dictionary\n", __func__);
      if (use_gil) {
        PyGILState_Release(gilstate);
      }
      return 0.0f;
    }
  }

  /* Update global name-space. */
  bpy_pydriver_namespace_update_frame(anim_eval_context->eval_time);

  if (driver_orig->flag & DRIVER_FLAG_USE_SELF) {
    bpy_pydriver_namespace_update_self(anim_rna);
  }
  else {
    bpy_pydriver_namespace_clear_self();
  }

  bpy_pydriver_namespace_update_depsgraph(anim_eval_context->depsgraph);

  if (driver_orig->expr_comp == nullptr) {
    driver_orig->flag |= DRIVER_FLAG_RECOMPILE;
  }

  /* Compile the expression first if it hasn't been compiled or needs to be rebuilt. */
  if (driver_orig->flag & DRIVER_FLAG_RECOMPILE) {
    Py_XDECREF(driver_orig->expr_comp);
    driver_orig->expr_comp = PyTuple_New(2);

    expr_code = Py_CompileString(expr, "<bpy driver>", Py_eval_input);
    PyTuple_SET_ITEM(((PyObject *)driver_orig->expr_comp), 0, expr_code);

    driver_orig->flag &= ~DRIVER_FLAG_RECOMPILE;

    /* Maybe this can be removed but for now best keep until were sure. */
    driver_orig->flag |= DRIVER_FLAG_RENAMEVAR;
    driver_orig->flag &= ~DRIVER_FLAG_PYTHON_BLOCKED;

#ifdef USE_BYTECODE_SECURE
    is_recompile = true;
#endif
  }
  else {
    expr_code = PyTuple_GET_ITEM(((PyObject *)driver_orig->expr_comp), 0);
  }

  if (driver_orig->flag & DRIVER_FLAG_RENAMEVAR) {
    /* May not be set. */
    expr_vars = PyTuple_GET_ITEM(((PyObject *)driver_orig->expr_comp), 1);
    Py_XDECREF(expr_vars);

    expr_vars = PyTuple_New(BLI_listbase_count(&driver_orig->variables));
    PyTuple_SET_ITEM(((PyObject *)driver_orig->expr_comp), 1, expr_vars);

    for (dvar = static_cast<DriverVar *>(driver_orig->variables.first), i = 0; dvar;
         dvar = dvar->next)
    {
      PyTuple_SET_ITEM(expr_vars, i++, PyUnicode_FromString(dvar->name));
    }

    driver_orig->flag &= ~DRIVER_FLAG_RENAMEVAR;
  }
  else {
    expr_vars = PyTuple_GET_ITEM(((PyObject *)driver_orig->expr_comp), 1);
  }

  /* Add target values to a dict that will be used as `__locals__` dict. */
  driver_vars = _PyDict_NewPresized(PyTuple_GET_SIZE(expr_vars));
  for (dvar = static_cast<DriverVar *>(driver->variables.first), i = 0; dvar; dvar = dvar->next) {
    PyObject *driver_arg = nullptr;

/* Support for any RNA data. */
#ifdef USE_RNA_AS_PYOBJECT
    if (dvar->type == DVAR_TYPE_SINGLE_PROP) {
      driver_arg = pyrna_driver_get_variable_value(
          anim_eval_context, driver, dvar, &dvar->targets[0]);

      if (driver_arg == nullptr) {
        driver_arg = PyFloat_FromDouble(0.0);
        dvar->curval = 0.0f;
      }
      else {
        /* No need to worry about overflow here, values from RNA are within limits. */
        if (PyFloat_CheckExact(driver_arg)) {
          dvar->curval = float(PyFloat_AsDouble(driver_arg));
        }
        else if (PyLong_CheckExact(driver_arg)) {
          dvar->curval = float(PyLong_AsLong(driver_arg));
        }
        else if (PyBool_Check(driver_arg)) {
          dvar->curval = float(driver_arg == Py_True);
        }
        else {
          dvar->curval = 0.0f;
        }
      }
    }
    else
#endif
    {
      /* Try to get variable value. */
      const float tval = driver_get_variable_value(anim_eval_context, driver, dvar);
      driver_arg = PyFloat_FromDouble(double(tval));
    }

    /* Try to add to dictionary. */
    /* `if (PyDict_SetItemString(driver_vars, dvar->name, driver_arg)) {` */
    if (PyDict_SetItem(driver_vars, PyTuple_GET_ITEM(expr_vars, i++), driver_arg) != -1) {
      /* Pass. */
    }
    else {
      /* This target failed - bad name. */
      if (targets_ok) {
        /* First one, print some extra info for easier identification. */
        fprintf(stderr, "\n%s: Error while evaluating PyDriver:\n", __func__);
        targets_ok = false;
      }

      fprintf(stderr, "\t%s: couldn't add variable '%s' to namespace\n", __func__, dvar->name);
      // BPy_errors_to_report(nullptr); /* TODO: reports. */
      PyErr_Print();
    }
    Py_DECREF(driver_arg);
  }

#ifdef USE_BYTECODE_SECURE
  if (is_recompile && expr_code) {
    if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC)) {
      PyObject *py_namespaces[] = {
          bpy_pydriver_Dict, bpy_pydriver_Dict__secure_ids, driver_vars, nullptr};
      if (!BPY_driver_secure_bytecode_test_ex(
              expr_code,
              py_namespaces,
              /* Always be verbose since this can give hints to why evaluation fails. */
              true,
              __func__))
      {
        if (!(G.f & G_FLAG_SCRIPT_AUTOEXEC_FAIL_QUIET)) {
          G.f |= G_FLAG_SCRIPT_AUTOEXEC_FAIL;
          SNPRINTF_UTF8(G.autoexec_fail, "Driver '%s'", expr);
        }

        Py_DECREF(expr_code);
        expr_code = nullptr;
        PyTuple_SET_ITEM(((PyObject *)driver_orig->expr_comp), 0, nullptr);
        driver_orig->flag |= DRIVER_FLAG_PYTHON_BLOCKED;
      }
    }
  }
#endif /* USE_BYTECODE_SECURE */

#if 0 /* slow, with this can avoid all Py_CompileString above. */
  /* execute expression to get a value */
  retval = PyRun_String(expr, Py_eval_input, bpy_pydriver_Dict, driver_vars);
#else
  /* Evaluate the compiled expression. */
  if (expr_code) {
    retval = PyEval_EvalCode(
        static_cast<PyObject *>(static_cast<void *>(expr_code)), bpy_pydriver_Dict, driver_vars);
  }
#endif

  /* Decref the driver variables first. */
  Py_DECREF(driver_vars);

  /* Process the result. */
  if (retval == nullptr) {
    pydriver_error(driver, anim_rna);
  }
  else {
    if (UNLIKELY((result = PyFloat_AsDouble(retval)) == -1.0 && PyErr_Occurred())) {
      pydriver_error(driver, anim_rna);
      result = 0.0;
    }
    else {
      /* All fine, make sure the "invalid expression" flag is cleared. */
      driver->flag &= ~DRIVER_FLAG_INVALID;
    }
    Py_DECREF(retval);
  }

  if (use_gil) {
    PyGILState_Release(gilstate);
  }

  if (UNLIKELY(!isfinite(result))) {
    fprintf(stderr, "\t%s: driver '%s' evaluates to '%f'\n", __func__, driver->expression, result);
    return 0.0f;
  }

  return float(result);
}

}  // namespace blender
