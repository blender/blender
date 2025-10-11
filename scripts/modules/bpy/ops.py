# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# for slightly faster access
from _bpy import ops as _ops_module

# op_add = _ops_module.add
_op_dir = _ops_module.dir
_op_create_function = _ops_module.create_function

_ModuleType = type(_ops_module)


# -----------------------------------------------------------------------------
# Sub-Module Access

def _bpy_ops_submodule__getattr__(module, func):
    # Return a `BPyOpsCallable` object that bypasses Python `__call__` overhead
    # for improved operator execution performance.
    if func.startswith("__"):
        raise AttributeError(func)
    return _op_create_function(module, func)


def _bpy_ops_submodule__dir__(module):
    functions = set()
    module_upper = module.upper()

    for id_name in _op_dir():
        id_split = id_name.split("_OT_", 1)
        if len(id_split) == 2 and module_upper == id_split[0]:
            functions.add(id_split[1])

    return list(functions)


def _bpy_ops_submodule(module):
    result = _ModuleType("bpy.ops." + module)
    result.__getattr__ = lambda func: _bpy_ops_submodule__getattr__(module, func)
    result.__dir__ = lambda: _bpy_ops_submodule__dir__(module)
    return result


# -----------------------------------------------------------------------------
# Module Access

def __getattr__(module):
    # Return a value from `bpy.ops.{module}`.
    if module.startswith("__"):
        raise AttributeError(module)
    return _bpy_ops_submodule(module)


def __dir__():
    submodules = set()
    for id_name in _op_dir():
        id_split = id_name.split("_OT_", 1)

        if len(id_split) == 2:
            submodules.add(id_split[0].lower())
        else:
            submodules.add(id_split[0])

    return list(submodules)
