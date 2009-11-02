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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

# for slightly faster access
from bpy.__ops__ import add        as op_add
from bpy.__ops__ import remove     as op_remove
from bpy.__ops__ import dir        as op_dir
from bpy.__ops__ import call       as op_call
from bpy.__ops__ import as_string  as op_as_string
from bpy.__ops__ import get_rna    as op_get_rna

# Keep in sync with WM_types.h
context_dict = {
    'INVOKE_DEFAULT': 0,
    'INVOKE_REGION_WIN': 1,
    'INVOKE_AREA': 2,
    'INVOKE_SCREEN': 3,
    'EXEC_DEFAULT': 4,
    'EXEC_REGION_WIN': 5,
    'EXEC_AREA': 6,
    'EXEC_SCREEN': 7,
}


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

    def add(self, pyop):
        op_add(pyop)

    def remove(self, pyop):
        op_remove(pyop)

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

    def __init__(self, module, func):
        self.module = module
        self.func = func

    def idname(self):
        # submod.foo -> SUBMOD_OT_foo
        return self.module.upper() + '_OT_' + self.func

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

            try:
                context = context_dict[C_exec]
            except:
                raise ValueError("Expected a single context argument in: " + \
                 str(list(context_dict.keys())))

            if len(args) == 2:
                C_dict = args[1]

            return op_call(self.idname(), C_dict, kw, context)

        else:
            return op_call(self.idname(), C_dict, kw)

    def get_rna(self):
        '''
        currently only used for 'bl_rna'
        '''
        return op_get_rna(self.idname())

    def __repr__(self): # useful display, repr(op)
        return op_as_string(self.idname())

    def __str__(self): # used for print(...)
        return "<function bpy.ops.%s.%s at 0x%x'>" % \
                (self.module, self.func, id(self))

import bpy
bpy.ops = bpy_ops()

# TODO, C macro's cant define settings :|

from bpy.props import *


class MESH_OT_delete_edgeloop(bpy.types.Operator):
    '''Export a single object as a stanford PLY with normals,
    colours and texture coordinates.'''
    bl_idname = "mesh.delete_edgeloop"
    bl_label = "Delete Edge Loop"

    def execute(self, context):
        bpy.ops.tfm.edge_slide(value=1.0)
        bpy.ops.mesh.select_more()
        bpy.ops.mesh.remove_doubles()
        return ('FINISHED',)

rna_path_prop = StringProperty(name="Context Attributes",
        description="rna context string", maxlen=1024, default="")

rna_reverse_prop = BoolProperty(name="Reverse",
        description="Cycle backwards", default=False)


class NullPathMember:
    pass


def context_path_validate(context, path):
    import sys
    try:
        value = eval("context.%s" % path)
    except AttributeError:
        if "'NoneType'" in str(sys.exc_info()[1]):
            # One of the items in the rna path is None, just ignore this
            value = NullPathMember
        else:
            # We have a real error in the rna path, dont ignore that
            raise

    return value


def execute_context_assign(self, context):
    if context_path_validate(context, self.path) == NullPathMember:
        return ('PASS_THROUGH',)

    exec("context.%s=self.value" % self.path)
    return ('FINISHED',)


class WM_OT_context_set_boolean(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_boolean"
    bl_label = "Context Set"

    path = rna_path_prop
    value = BoolProperty(name="Value",
            description="Assignment value", default=True)

    execute = execute_context_assign


class WM_OT_context_set_int(bpy.types.Operator): # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_int"
    bl_label = "Context Set"

    path = rna_path_prop
    value = IntProperty(name="Value", description="Assign value", default=0)

    execute = execute_context_assign


class WM_OT_context_set_float(bpy.types.Operator): # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_int"
    bl_label = "Context Set"

    path = rna_path_prop
    value = FloatProperty(name="Value",
            description="Assignment value", default=0.0)

    execute = execute_context_assign


class WM_OT_context_set_string(bpy.types.Operator): # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_string"
    bl_label = "Context Set"

    path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assign value", maxlen=1024, default="")

    execute = execute_context_assign


class WM_OT_context_set_enum(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_enum"
    bl_label = "Context Set"

    path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assignment value (as a string)",
            maxlen=1024, default="")

    execute = execute_context_assign


class WM_OT_context_toggle(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_toggle"
    bl_label = "Context Toggle"
    path = rna_path_prop

    def execute(self, context):

        if context_path_validate(context, self.path) == NullPathMember:
            return ('PASS_THROUGH',)

        exec("context.%s=not (context.%s)" % (self.path, self.path))
        return ('FINISHED',)


class WM_OT_context_toggle_enum(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_toggle_enum"
    bl_label = "Context Toggle Values"

    path = rna_path_prop
    value_1 = StringProperty(name="Value", \
                description="Toggle enum", maxlen=1024, default="")

    value_2 = StringProperty(name="Value", \
                description="Toggle enum", maxlen=1024, default="")

    def execute(self, context):

        if context_path_validate(context, self.path) == NullPathMember:
            return ('PASS_THROUGH',)

        exec("context.%s = ['%s', '%s'][context.%s!='%s']" % \
            (self.path, self.value_1, self.value_2, self.path, self.value_2))

        return ('FINISHED',)


class WM_OT_context_cycle_int(bpy.types.Operator):
    '''Set a context value. Useful for cycling active material,
    vertex keys, groups' etc.'''
    bl_idname = "wm.context_cycle_int"
    bl_label = "Context Int Cycle"
    path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):

        value = context_path_validate(context, self.path)
        if value == NullPathMember:
            return ('PASS_THROUGH',)

        self.value = value
        if self.reverse:
            self.value -= 1
        else:
            self.value += 1
        execute_context_assign(self, context)

        if self.value != eval("context.%s" % self.path):
            # relies on rna clamping int's out of the range
            if self.reverse:
                self.value = (1 << 32)
            else:
                self.value = - (1 << 32)
            execute_context_assign(self, context)

        return ('FINISHED',)


class WM_OT_context_cycle_enum(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_cycle_enum"
    bl_label = "Context Enum Cycle"

    path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):

        value = context_path_validate(context, self.path)
        if value == NullPathMember:
            return ('PASS_THROUGH',)

        orig_value = value

        # Have to get rna enum values
        rna_struct_str, rna_prop_str = self.path.rsplit('.', 1)
        i = rna_prop_str.find('[')

        # just incse we get "context.foo.bar[0]"
        if i != -1:
            rna_prop_str = rna_prop_str[0:i]

        rna_struct = eval("context.%s.rna_type" % rna_struct_str)

        rna_prop = rna_struct.properties[rna_prop_str]

        if type(rna_prop) != bpy.types.EnumProperty:
            raise Exception("expected an enum property")

        enums = rna_struct.properties[rna_prop_str].items.keys()
        orig_index = enums.index(orig_value)

        # Have the info we need, advance to the next item
        if self.reverse:
            if orig_index == 0:
                advance_enum = enums[-1]
            else:
                advance_enum = enums[orig_index-1]
        else:
            if orig_index == len(enums) - 1:
                advance_enum = enums[0]
            else:
                advance_enum = enums[orig_index + 1]

        # set the new value
        exec("context.%s=advance_enum" % self.path)
        return ('FINISHED',)

doc_id = StringProperty(name="Doc ID",
        description="ID for the documentation", maxlen=1024, default="")

doc_new = StringProperty(name="Doc New",
        description="", maxlen=1024, default="")


class WM_OT_doc_view(bpy.types.Operator):
    '''Load online reference docs'''
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id = doc_id
    _prefix = 'http://www.blender.org/documentation/250PythonDoc'

    def _nested_class_string(self, class_string):
        ls = []
        class_obj = getattr(bpy.types, class_string, None).bl_rna
        while class_obj:
            ls.insert(0, class_obj)
            class_obj = class_obj.nested
        return '.'.join([class_obj.identifier for class_obj in ls])

    def execute(self, context):
        id_split = self.doc_id.split('.')
        if len(id_split) == 1: # rna, class
            url = '%s/bpy.types.%s-class.html' % (self._prefix, id_split[0])
        elif len(id_split) == 2: # rna, class.prop
            class_name, class_prop = id_split

            # It so happens that epydoc nests these
            class_name_full = self._nested_class_string(class_name)

            if hasattr(bpy.types, class_name.upper() + '_OT_' + class_prop):
                url = '%s/bpy.ops.%s-module.html#%s' % \
                        (self._prefix, class_name_full, class_prop)
            else:
                url = '%s/bpy.types.%s-class.html#%s' % \
                        (self._prefix, class_name_full, class_prop)

        else:
            return ('PASS_THROUGH',)

        import webbrowser
        webbrowser.open(url)

        return ('FINISHED',)


class WM_OT_doc_edit(bpy.types.Operator):
    '''Load online reference docs'''
    bl_idname = "wm.doc_edit"
    bl_label = "Edit Documentation"

    doc_id = doc_id
    doc_new = doc_new

    _url = "http://www.mindrones.com/blender/svn/xmlrpc.php"

    def _send_xmlrpc(self, data_dict):
        print("sending data:", data_dict)

        import xmlrpc.client
        user = 'blenderuser'
        pwd = 'blender>user'

        docblog = xmlrpc.client.ServerProxy(self._url)
        docblog.metaWeblog.newPost(1, user, pwd, data_dict, 1)

    def execute(self, context):

        class_name, class_prop = self.doc_id.split('.')

        if not self.doc_new:
            return 'OPERATOR_CANCELLED'

        # check if this is an operator
        op_name = class_name.upper() + '_OT_' + class_prop
        op_class = getattr(bpy.types, op_name, None)

        # Upload this to the web server
        upload = {}

        if op_class:
            rna = op_class.bl_rna
            doc_orig = rna.description
            if doc_orig == self.doc_new:
                return 'OPERATOR_CANCELLED'

            print("op - old:'%s' -> new:'%s'" % (doc_orig, self.doc_new))
            upload["title"] = 'OPERATOR %s:%s' % (self.doc_id, doc_orig)
            upload["description"] = self.doc_new

            self._send_xmlrpc(upload)

        else:
            rna = getattr(bpy.types, class_name).bl_rna
            doc_orig = rna.properties[class_prop].description
            if doc_orig == self.doc_new:
                return 'OPERATOR_CANCELLED'

            print("rna - old:'%s' -> new:'%s'" % (doc_orig, self.doc_new))
            upload["title"] = 'RNA %s:%s' % s(self.doc_id, doc_orig)

        upload["description"] = self.doc_new

        self._send_xmlrpc(upload)

        return ('FINISHED',)

    def invoke(self, context, event):
        wm = context.manager
        wm.invoke_props_popup(self, event)
        return ('RUNNING_MODAL',)


bpy.ops.add(MESH_OT_delete_edgeloop)

bpy.ops.add(WM_OT_context_set_boolean)
bpy.ops.add(WM_OT_context_set_int)
bpy.ops.add(WM_OT_context_set_float)
bpy.ops.add(WM_OT_context_set_string)
bpy.ops.add(WM_OT_context_set_enum)
bpy.ops.add(WM_OT_context_toggle)
bpy.ops.add(WM_OT_context_toggle_enum)
bpy.ops.add(WM_OT_context_cycle_enum)
bpy.ops.add(WM_OT_context_cycle_int)

bpy.ops.add(WM_OT_doc_view)
bpy.ops.add(WM_OT_doc_edit)
