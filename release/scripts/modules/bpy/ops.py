# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

# for slightly faster access
from _bpy import ops as ops_module

# op_add = ops_module.add
op_dir = ops_module.dir
op_poll = ops_module.poll
op_call = ops_module.call
op_as_string = ops_module.as_string
op_get_rna = ops_module.get_rna
op_get_instance = ops_module.get_instance


class BPyOps(object):
    """
    Fake module like class.

     bpy.ops
    """

    def __getattr__(self, module):
        """
        gets a bpy.ops submodule
        """
        if module.startswith('__'):
            raise AttributeError(module)
        return BPyOpsSubMod(module)

    def __dir__(self):

        submodules = set()

        # add this classes functions
        for id_name in dir(self.__class__):
            if not id_name.startswith('__'):
                submodules.add(id_name)

        for id_name in op_dir():
            id_split = id_name.split('_OT_', 1)

            if len(id_split) == 2:
                submodules.add(id_split[0].lower())
            else:
                submodules.add(id_split[0])

        return list(submodules)

    def __repr__(self):
        return "<module like class 'bpy.ops'>"


class BPyOpsSubMod(object):
    """
    Utility class to fake submodules.

    eg. bpy.ops.object
    """
    __slots__ = ("_module",)

    def __init__(self, module):
        self._module = module

    def __getattr__(self, func):
        """
        gets a bpy.ops.submodule function
        """
        if func.startswith('__'):
            raise AttributeError(func)
        return BPyOpsSubModOp(self._module, func)

    def __dir__(self):

        functions = set()

        module_upper = self._module.upper()

        for id_name in op_dir():
            id_split = id_name.split('_OT_', 1)
            if len(id_split) == 2 and module_upper == id_split[0]:
                functions.add(id_split[1])

        return list(functions)

    def __repr__(self):
        return "<module like class 'bpy.ops.%s'>" % self._module


class BPyOpsSubModOp(object):
    """
    Utility class to fake submodule operators.

    eg. bpy.ops.object.somefunc
    """

    __slots__ = ("_module", "_func")

    def _get_doc(self):
        return op_as_string(self.idname())

    @staticmethod
    def _parse_args(args):
        C_dict = None
        C_exec = 'EXEC_DEFAULT'
        C_undo = False

        is_dict = is_exec = is_undo = False

        for i, arg in enumerate(args):
            if is_dict is False and isinstance(arg, dict):
                if is_exec is True or is_undo is True:
                    raise ValueError("dict arg must come first")
                C_dict = arg
                is_dict = True
            elif is_exec is False and isinstance(arg, str):
                if is_undo is True:
                    raise ValueError("string arg must come before the boolean")
                C_exec = arg
                is_exec = True
            elif is_undo is False and isinstance(arg, int):
                C_undo = arg
                is_undo = True
            else:
                raise ValueError("1-3 args execution context is supported")

        return C_dict, C_exec, C_undo

    @staticmethod
    def _scene_update(context):
        scene = context.scene
        if scene:  # None in background mode
            scene.update()
        else:
            import bpy
            for scene in bpy.data.scenes:
                scene.update()

    __doc__ = property(_get_doc)

    def __init__(self, module, func):
        self._module = module
        self._func = func

    def poll(self, *args):
        C_dict, C_exec, C_undo = BPyOpsSubModOp._parse_args(args)
        return op_poll(self.idname_py(), C_dict, C_exec)

    def idname(self):
        # submod.foo -> SUBMOD_OT_foo
        return self._module.upper() + "_OT_" + self._func

    def idname_py(self):
        # submod.foo -> SUBMOD_OT_foo
        return self._module + "." + self._func

    def __call__(self, *args, **kw):
        import bpy
        context = bpy.context

        # Get the operator from blender
        wm = context.window_manager

        # run to account for any rna values the user changes.
        BPyOpsSubModOp._scene_update(context)

        if args:
            C_dict, C_exec, C_undo = BPyOpsSubModOp._parse_args(args)
            ret = op_call(self.idname_py(), C_dict, kw, C_exec, C_undo)
        else:
            ret = op_call(self.idname_py(), None, kw)

        if 'FINISHED' in ret and context.window_manager == wm:
            BPyOpsSubModOp._scene_update(context)

        return ret

    def get_rna(self):
        """Internal function for introspection"""
        return op_get_rna(self.idname())

    def get_instance(self):
        """Internal function for introspection"""
        return op_get_instance(self.idname())

    def __repr__(self):  # useful display, repr(op)
        # import bpy
        idname = self.idname()
        as_string = op_as_string(idname)
        # XXX You never quite know what you get from bpy.types,
        # with operators... Operator and OperatorProperties
        # are shadowing each other, and not in the same way for
        # native ops and py ones! See T39158.
        # op_class = getattr(bpy.types, idname)
        op_class = op_get_rna(idname)
        descr = op_class.bl_rna.description
        # XXX, workaround for not registering
        # every __doc__ to save time on load.
        if not descr:
            descr = op_class.__doc__
            if not descr:
                descr = ""

        return "# %s\n%s" % (descr, as_string)

    def __str__(self):  # used for print(...)
        return ("<function bpy.ops.%s.%s at 0x%x'>" %
                (self._module, self._func, id(self)))

ops_fake_module = BPyOps()
