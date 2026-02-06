/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * Python OP-CODE filtering.
 *
 * \note This is in its own file to avoid naming collisions with common ID's such as `SWAP`.
 */

#include <Python.h>

#include "bpy_driver_bytecode.hh"

#define USE_BYTECODE_SECURE

#ifdef USE_BYTECODE_SECURE
#  include <opcode.h>
/* WARNING(@ideasman42): Using `Py_BUILD_CORE` is a last resort,
 * the alternative would be not to inspect OP-CODES at all. */
#  define Py_BUILD_CORE
#  include <internal/pycore_code.h>
#endif

namespace blender {

#ifdef USE_BYTECODE_SECURE

static bool is_opcode_secure(const int opcode)
{
  /*
   * Intentionally Excluded Opcodes
   * ==============================
   *
   * Likely Safe but Unnecessary
   * ---------------------------
   *
   * These opcodes appear safe but are not needed for driver expressions.
   *
   * In the interest of reducing the attack surface, exclude them unless
   * practical use cases are found.
   *
   * - `CALL_INTRINSIC_2`: Used for exception handling (`except*`) and type hints.
   *   Not needed for driver expressions.
   *
   * - `MAKE_FUNCTION`, `LOAD_BUILD_CLASS`: Function/class creation not needed.
   *
   * Known to be Dangerous
   * ---------------------
   *
   * These opcodes are excluded for security reasons:
   *
   * - `DICT_MERGE`, `DICT_UPDATE`: Could potentially be used to manipulate
   *   the namespace via `globals()`, allowing malicious code execution.
   *
   * - `IMPORT_NAME`, `IMPORT_FROM`: Module imports are not allowed.
   *
   * - `STORE_NAME`, `STORE_GLOBAL`, `STORE_ATTR`: Storing to names/globals/attributes
   *   could modify the namespace or objects in unsafe ways.
   *
   * - `LOAD_ATTR`: Attribute access is not allowed as it could access
   *   private/internal attributes.
   */

#  define OK_OP(op) \
    case op: \
      return true;

  switch (opcode) {
    OK_OP(CACHE)
    OK_OP(COPY) /* Ok for short-circuit boolean evaluation (`and`, `or`). */
    OK_OP(POP_TOP)
    OK_OP(PUSH_NULL)
    OK_OP(NOP)
#  if PY_VERSION_HEX >= 0x030e0000
    OK_OP(NOT_TAKEN)
#  endif
    OK_OP(TO_BOOL) /* Ok for boolean conversion in `and`/`or` expressions. */
    OK_OP(UNARY_NEGATIVE)
    OK_OP(UNARY_NOT)
    OK_OP(UNARY_INVERT)
#  if PY_VERSION_HEX < 0x030e0000
    OK_OP(BINARY_SUBSCR) /* Replaced with existing `BINARY_OP`. */
#  endif
    OK_OP(GET_LEN)
    OK_OP(RETURN_VALUE)
    OK_OP(SWAP)
    OK_OP(BUILD_TUPLE)
    OK_OP(BUILD_LIST)
    OK_OP(BUILD_SET)
    OK_OP(BUILD_MAP)
    OK_OP(COMPARE_OP)
    OK_OP(JUMP_FORWARD)
    OK_OP(LOAD_GLOBAL)
    OK_OP(IS_OP)
    OK_OP(CONTAINS_OP)
    OK_OP(BINARY_OP)
    OK_OP(LOAD_FAST)
    OK_OP(LOAD_FAST_AND_CLEAR) /* Ok, optimized variant of `LOAD_FAST`. */
    OK_OP(LOAD_FAST_LOAD_FAST) /* Ok, optimized double `LOAD_FAST`. */
#  if PY_VERSION_HEX >= 0x030e0000
    OK_OP(LOAD_FAST_BORROW)                  /* Ok, optimized variant of `LOAD_FAST`. */
    OK_OP(LOAD_FAST_BORROW_LOAD_FAST_BORROW) /* Ok, optimized double `LOAD_FAST`. */
#  endif
    OK_OP(STORE_FAST)
    OK_OP(STORE_FAST_LOAD_FAST)  /* Ok, optimized `STORE_FAST` + `LOAD_FAST`. */
    OK_OP(STORE_FAST_STORE_FAST) /* Ok, optimized double `STORE_FAST`. */
    OK_OP(DELETE_FAST)
    OK_OP(BUILD_SLICE)
    OK_OP(LOAD_DEREF)
    OK_OP(STORE_DEREF)
    OK_OP(RESUME)
    OK_OP(LIST_EXTEND)
    OK_OP(SET_UPDATE)
/* NOTE(@ideasman42): Don't enable dict manipulation, unless we can prove there is not way it
 * can be used to manipulate the name-space (potentially allowing malicious code). */
#  if 0
    OK_OP(DICT_MERGE)
    OK_OP(DICT_UPDATE)
#  endif

#  if PY_VERSION_HEX < 0x030e0000
    OK_OP(RETURN_CONST)
#  endif
    /* Ok, conditional jumps only affect control flow within the expression. */
    OK_OP(POP_JUMP_IF_FALSE)    /* Used for `and` expressions and `if` conditionals. */
    OK_OP(POP_JUMP_IF_TRUE)     /* Used for `or` expressions. */
    OK_OP(POP_JUMP_IF_NONE)     /* Used for `is not None` conditionals. */
    OK_OP(POP_JUMP_IF_NOT_NONE) /* Used for `is None` conditionals. */
    OK_OP(CALL_INTRINSIC_1)
    /* Special cases. */
    OK_OP(LOAD_CONST) /* Ok because constants are accepted. */
    OK_OP(LOAD_NAME)  /* Ok, because `PyCodeObject.names` is checked. */
#  if PY_VERSION_HEX >= 0x030e0000
    OK_OP(LOAD_SMALL_INT)
#  endif
    OK_OP(CALL)    /* Ok, because we check its "name" before calling. */
    OK_OP(CALL_KW) /* Ok, because it's used for calling functions with keyword arguments. */

    OK_OP(CALL_FUNCTION_EX);

    /* OK because the names are checked. */
    OK_OP(CALL_ALLOC_AND_ENTER_INIT)
    OK_OP(CALL_BOUND_METHOD_EXACT_ARGS)
    OK_OP(CALL_BOUND_METHOD_GENERAL)
    OK_OP(CALL_BUILTIN_CLASS)
    OK_OP(CALL_BUILTIN_FAST)
    OK_OP(CALL_BUILTIN_FAST_WITH_KEYWORDS)
    OK_OP(CALL_BUILTIN_O)
    OK_OP(CALL_ISINSTANCE)
    OK_OP(CALL_LEN)
    OK_OP(CALL_LIST_APPEND)
    OK_OP(CALL_METHOD_DESCRIPTOR_FAST)
    OK_OP(CALL_METHOD_DESCRIPTOR_FAST_WITH_KEYWORDS)
    OK_OP(CALL_METHOD_DESCRIPTOR_NOARGS)
    OK_OP(CALL_METHOD_DESCRIPTOR_O)
    OK_OP(CALL_NON_PY_GENERAL)
    OK_OP(CALL_PY_EXACT_ARGS)
    OK_OP(CALL_PY_GENERAL)
    OK_OP(CALL_STR_1)
    OK_OP(CALL_TUPLE_1)
    OK_OP(CALL_TYPE_1)
  }

#  undef OK_OP
  return false;
}

bool BPY_driver_secure_bytecode_test_ex(PyObject *expr_code,
                                        PyObject *py_namespace_array[],
                                        const bool verbose,
                                        const char *error_prefix)
{
  PyCodeObject *py_code = reinterpret_cast<PyCodeObject *>(expr_code);

  /* Check names. */
  {
    for (int i = 0; i < PyTuple_GET_SIZE(py_code->co_names); i++) {
      PyObject *name = PyTuple_GET_ITEM(py_code->co_names, i);
      const char *name_str = PyUnicode_AsUTF8(name);
      bool contains_name = false;
      for (int j = 0; py_namespace_array[j]; j++) {
        if (PyDict_Contains(py_namespace_array[j], name)) {
          contains_name = true;
          break;
        }
      }

      if ((contains_name == false) || (name_str[0] == '_')) {
        if (verbose) {
          fprintf(stderr,
                  "\t%s: restricted access disallows name '%s', "
                  "enable auto-execution to support\n",
                  error_prefix,
                  name_str);
        }
        return false;
      }
    }
  }

  /* Check opcodes. */
  {
    const _Py_CODEUNIT *codestr;
    Py_ssize_t code_len;

    PyObject *co_code;

    co_code = PyCode_GetCode(py_code);
    if (!co_code) {
      PyErr_Print();
      return false;
    }

    PyBytes_AsStringAndSize(co_code, (char **)&codestr, &code_len);
    code_len /= sizeof(*codestr);
    bool ok = true;

    /* Loop over op-code's, the op-code arguments are ignored. */
    for (Py_ssize_t i = 0; i < code_len; i++) {
      const int opcode = _Py_OPCODE(codestr[i]);
      if (!is_opcode_secure(opcode)) {
        if (verbose) {
          fprintf(stderr,
                  "\t%s: restricted access disallows opcode '%d', "
                  "enable auto-execution to support\n",
                  error_prefix,
                  opcode);
        }
        ok = false;
        break;
      }
    }

    Py_DECREF(co_code);
    if (!ok) {
      return false;
    }
  }

  return true;
}

#endif /* USE_BYTECODE_SECURE */

};  // namespace blender
