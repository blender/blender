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
op_call = ops_module.call
op_as_string = ops_module.as_string
op_get_rna = ops_module.get_rna


class bpy_ops(object):
    '''
    Fake module like class.

     bpy.ops
    '''

    def __getattr__(self, module):
        '''
        gets a bpy.ops submodule
        '''
        if module.startswith('__'):
            raise AttributeError(module)
        return bpy_ops_submodule(module)

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


class bpy_ops_submodule(object):
    '''
    Utility class to fake submodules.

    eg. bpy.ops.object
    '''
    __keys__ = ('module',)

    def __init__(self, module):
        self.module = module

    def __getattr__(self, func):
        '''
        gets a bpy.ops.submodule function
        '''
        if func.startswith('__'):
            raise AttributeError(func)
        return bpy_ops_submodule_op(self.module, func)

    def __dir__(self):

        functions = set()

        module_upper = self.module.upper()

        for id_name in op_dir():
            id_split = id_name.split('_OT_', 1)
            if len(id_split) == 2 and module_upper == id_split[0]:
                functions.add(id_split[1])

        return list(functions)

    def __repr__(self):
        return "<module like class 'bpy.ops.%s'>" % self.module


class bpy_ops_submodule_op(object):
    '''
    Utility class to fake submodule operators.

    eg. bpy.ops.object.somefunc
    '''

    __keys__ = ('module', 'func')

    def _get_doc(self):
        return op_as_string(self.idname())

    __doc__ = property(_get_doc)

    def __init__(self, module, func):
        self.module = module
        self.func = func

    def idname(self):
        # submod.foo -> SUBMOD_OT_foo
        return self.module.upper() + "_OT_" + self.func

    def idname_py(self):
        # submod.foo -> SUBMOD_OT_foo
        return self.module + "." + self.func

    def __call__(self, *args, **kw):

        # Get the operator from blender
        if len(args) > 2:
            raise ValueError("1 or 2 args execution context is supported")

        C_dict = None

        if args:

            C_exec = 'EXEC_DEFAULT'

            if len(args) == 2:
                C_exec = args[0]
                C_dict = args[1]
            else:
                if type(args[0]) != str:
                    C_dict = args[0]
                else:
                    C_exec = args[0]

            if len(args) == 2:
                C_dict = args[1]

            ret = op_call(self.idname_py(), C_dict, kw, C_exec)

        else:
            ret = op_call(self.idname_py(), C_dict, kw)

        if 'FINISHED' in ret:
            import bpy
            scene = bpy.context.scene
            if scene: # None in backgroud mode
                scene.update()
            else:
                for scene in bpy.data.scenes:
                    scene.update()

        return ret

    def get_rna(self):
        '''
        currently only used for 'bl_rna'
        '''
        return op_get_rna(self.idname())

    def __repr__(self): # useful display, repr(op)
        import bpy
        idname = self.idname()
        as_string = op_as_string(idname)
        descr = getattr(bpy.types, idname).bl_rna.description
        return as_string + "\n" + descr

    def __str__(self): # used for print(...)
        return "<function bpy.ops.%s.%s at 0x%x'>" % \
                (self.module, self.func, id(self))

ops_fake_module = bpy_ops()
