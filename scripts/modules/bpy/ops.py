# SPDX-FileCopyrightText: 2009-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

# for slightly faster access
from _bpy import ops as _ops_module

# op_add = _ops_module.add
_op_dir = _ops_module.dir
_op_poll = _ops_module.poll
_op_call = _ops_module.call
_op_as_string = _ops_module.as_string
_op_get_rna_type = _ops_module.get_rna_type
_op_get_bl_options = _ops_module.get_bl_options

_ModuleType = type(_ops_module)


# -----------------------------------------------------------------------------
# Callable Operator Wrapper

class _BPyOpsSubModOp:
    """
    Utility class to fake submodule operators.

    eg. bpy.ops.object.somefunc
    """

    __slots__ = ("_module", "_func")

    def _get_doc(self):
        idname = self.idname()
        sig = _op_as_string(self.idname())
        # XXX You never quite know what you get from bpy.types,
        # with operators... Operator and OperatorProperties
        # are shadowing each other, and not in the same way for
        # native ops and py ones! See #39158.
        # op_class = getattr(bpy.types, idname)
        op_class = _op_get_rna_type(idname)
        descr = op_class.description
        return "%s\n%s" % (sig, descr)

    @staticmethod
    def _parse_args(args):
        C_exec = 'EXEC_DEFAULT'
        C_undo = False

        is_exec = is_undo = False

        for arg in args:
            if is_exec is False and isinstance(arg, str):
                if is_undo is True:
                    raise ValueError("string arg must come before the boolean")
                C_exec = arg
                is_exec = True
            elif is_undo is False and isinstance(arg, int):
                C_undo = arg
                is_undo = True
            else:
                raise ValueError("1-2 args execution context is supported")

        return C_exec, C_undo

    @staticmethod
    def _view_layer_update(context):
        view_layer = context.view_layer
        if view_layer:  # None in background mode
            view_layer.update()
        else:
            import bpy
            for scene in bpy.data.scenes:
                for view_layer in scene.view_layers:
                    view_layer.update()

    __doc__ = property(_get_doc)

    def __init__(self, module, func):
        self._module = module
        self._func = func

    def poll(self, *args):
        C_exec, _C_undo = _BPyOpsSubModOp._parse_args(args)
        return _op_poll(self.idname_py(), C_exec)

    def idname(self):
        # submod.foo -> SUBMOD_OT_foo
        return self._module.upper() + "_OT_" + self._func

    def idname_py(self):
        return self._module + "." + self._func

    def __call__(self, *args, **kw):
        import bpy
        context = bpy.context

        # Get the operator from blender
        wm = context.window_manager

        # Run to account for any RNA values the user changes.
        # NOTE: We only update active view-layer, since that's what
        # operators are supposed to operate on. There might be some
        # corner cases when operator need a full scene update though.
        _BPyOpsSubModOp._view_layer_update(context)

        if args:
            C_exec, C_undo = _BPyOpsSubModOp._parse_args(args)
            ret = _op_call(self.idname_py(), kw, C_exec, C_undo)
        else:
            ret = _op_call(self.idname_py(), kw)

        if 'FINISHED' in ret and context.window_manager == wm:
            _BPyOpsSubModOp._view_layer_update(context)

        return ret

    def get_rna_type(self):
        """Internal function for introspection"""
        return _op_get_rna_type(self.idname())

    @property
    def bl_options(self):
        return _op_get_bl_options(self.idname())

    def __repr__(self):  # useful display, repr(op)
        return _op_as_string(self.idname())

    def __str__(self):  # used for print(...)
        return ("<function bpy.ops.%s.%s at 0x%x'>" %
                (self._module, self._func, id(self)))


# -----------------------------------------------------------------------------
# Sub-Module Access

def _bpy_ops_submodule__getattr__(module, func):
    # Return a value from `bpy.ops.{module}.{func}`
    if func.startswith("__"):
        raise AttributeError(func)
    return _BPyOpsSubModOp(module, func)


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
