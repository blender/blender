/* SP{DX-FileCopyrightText: 2023 Blender Authors
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
#  if PY_VERSION_HEX >= 0x030d0000 /* >=3.13 */
/* WARNING(@ideasman42): Using `Py_BUILD_CORE` is a last resort,
 * the alternative would be not to inspect OP-CODES at all. */
#    define Py_BUILD_CORE
#    include <internal/pycore_code.h>
#  endif
#endif

namespace blender {

#ifdef USE_BYTECODE_SECURE

static bool is_opcode_secure(const int opcode)
{
  /* TODO(@ideasman42): Handle intrinsic opcodes (`CALL_INTRINSIC_2`).
   * For Python 3.12. */

#  define OK_OP(op) \
    case op: \
      return true;

  switch (opcode) {
    OK_OP(CACHE)
    OK_OP(POP_TOP)
    OK_OP(PUSH_NULL)
    OK_OP(NOP)
#  if PY_VERSION_HEX >= 0x030e0000
    OK_OP(NOT_TAKEN)
#  endif
#  if PY_VERSION_HEX < 0x030c0000
    OK_OP(UNARY_POSITIVE)
#  endif
    OK_OP(UNARY_NEGATIVE)
    OK_OP(UNARY_NOT)
    OK_OP(UNARY_INVERT)
#  if PY_VERSION_HEX < 0x030e0000
    OK_OP(BINARY_SUBSCR) /* Replaced with existing `BINARY_OP`. */
#  endif
    OK_OP(GET_LEN)
#  if PY_VERSION_HEX < 0x030c0000
    OK_OP(LIST_TO_TUPLE)
#  endif
    OK_OP(RETURN_VALUE)
    OK_OP(SWAP)
    OK_OP(BUILD_TUPLE)
    OK_OP(BUILD_LIST)
    OK_OP(BUILD_SET)
    OK_OP(BUILD_MAP)
    OK_OP(COMPARE_OP)
    OK_OP(JUMP_FORWARD)
#  if PY_VERSION_HEX < 0x030c0000
    OK_OP(JUMP_IF_FALSE_OR_POP)
    OK_OP(JUMP_IF_TRUE_OR_POP)
    OK_OP(POP_JUMP_FORWARD_IF_FALSE)
    OK_OP(POP_JUMP_FORWARD_IF_TRUE)
#  endif
    OK_OP(LOAD_GLOBAL)
    OK_OP(IS_OP)
    OK_OP(CONTAINS_OP)
    OK_OP(BINARY_OP)
    OK_OP(LOAD_FAST)
    OK_OP(STORE_FAST)
    OK_OP(DELETE_FAST)
#  if PY_VERSION_HEX < 0x030c0000
    OK_OP(POP_JUMP_FORWARD_IF_NOT_NONE)
    OK_OP(POP_JUMP_FORWARD_IF_NONE)
#  endif
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

#  if PY_VERSION_HEX < 0x030c0000
    OK_OP(POP_JUMP_BACKWARD_IF_NOT_NONE)
    OK_OP(POP_JUMP_BACKWARD_IF_NONE)
    OK_OP(POP_JUMP_BACKWARD_IF_FALSE)
    OK_OP(POP_JUMP_BACKWARD_IF_TRUE)
#  endif

#  if PY_VERSION_HEX >= 0x030c0000
#    if PY_VERSION_HEX < 0x030e0000
    OK_OP(RETURN_CONST)
#    endif
    OK_OP(POP_JUMP_IF_FALSE)
    OK_OP(CALL_INTRINSIC_1)
#  endif
    /* Special cases. */
    OK_OP(LOAD_CONST) /* Ok because constants are accepted. */
    OK_OP(LOAD_NAME)  /* Ok, because `PyCodeObject.names` is checked. */
#  if PY_VERSION_HEX >= 0x030e0000
    OK_OP(LOAD_SMALL_INT)
#  endif
    OK_OP(CALL) /* Ok, because we check its "name" before calling. */
#  if PY_VERSION_HEX >= 0x030d0000
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
#  else
    OK_OP(KW_NAMES) /* Ok, because it's used for calling functions with keyword arguments. */
#  endif

#  if PY_VERSION_HEX < 0x030c0000
    OK_OP(PRECALL) /* Ok, because it's used for calling. */
#  endif
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
