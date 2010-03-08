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

import bpy

from bpy.props import *


class MESH_OT_delete_edgeloop(bpy.types.Operator):
    '''Export a single object as a stanford PLY with normals,
    colours and texture coordinates.'''
    bl_idname = "mesh.delete_edgeloop"
    bl_label = "Delete Edge Loop"

    def execute(self, context):
        bpy.ops.transform.edge_slide(value=1.0)
        bpy.ops.mesh.select_more()
        bpy.ops.mesh.remove_doubles()

        return {'FINISHED'}

rna_path_prop = StringProperty(name="Context Attributes",
        description="rna context string", maxlen=1024, default="")

rna_reverse_prop = BoolProperty(name="Reverse",
        description="Cycle backwards", default=False)

rna_relative_prop = BoolProperty(name="Relative",
        description="Apply relative to the current value (delta)",
        default=False)


def context_path_validate(context, path):
    import sys
    try:
        value = eval("context.%s" % path)
    except AttributeError:
        if "'NoneType'" in str(sys.exc_info()[1]):
            # One of the items in the rna path is None, just ignore this
            value = Ellipsis
        else:
            # We have a real error in the rna path, dont ignore that
            raise

    return value


def execute_context_assign(self, context):
    if context_path_validate(context, self.properties.path) is Ellipsis:
        return {'PASS_THROUGH'}

    if getattr(self.properties, "relative", False):
        exec("context.%s+=self.properties.value" % self.properties.path)
    else:
        exec("context.%s=self.properties.value" % self.properties.path)

    return {'FINISHED'}


class WM_OT_context_set_boolean(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_boolean"
    bl_label = "Context Set Boolean"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = BoolProperty(name="Value",
            description="Assignment value", default=True)

    execute = execute_context_assign


class WM_OT_context_set_int(bpy.types.Operator): # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_int"
    bl_label = "Context Set"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = IntProperty(name="Value", description="Assign value", default=0)
    relative = rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_scale_int(bpy.types.Operator): # same as enum
    '''Scale an int context value.'''
    bl_idname = "wm.context_scale_int"
    bl_label = "Context Set"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = FloatProperty(name="Value", description="Assign value", default=1.0)
    always_step = BoolProperty(name="Always Step",
        description="Always adjust the value by a minimum of 1 when 'value' is not 1.0.",
        default=True)

    def execute(self, context):
        if context_path_validate(context, self.properties.path) is Ellipsis:
            return {'PASS_THROUGH'}

        value = self.properties.value
        path = self.properties.path

        if value == 1.0: # nothing to do
            return {'CANCELLED'}

        if getattr(self.properties, "always_step", False):
            if value > 1.0:
                add = "1"
                func = "max"
            else:
                add = "-1"
                func = "min"
            exec("context.%s = %s(round(context.%s * value), context.%s + %s)" % (path, func, path, path, add))
        else:
            exec("context.%s *= value" % self.properties.path)

        return {'FINISHED'}


class WM_OT_context_set_float(bpy.types.Operator): # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_float"
    bl_label = "Context Set Float"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = FloatProperty(name="Value",
            description="Assignment value", default=0.0)
    relative = rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_set_string(bpy.types.Operator): # same as enum
    '''Set a context value.'''
    bl_idname = "wm.context_set_string"
    bl_label = "Context Set String"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assign value", maxlen=1024, default="")

    execute = execute_context_assign


class WM_OT_context_set_enum(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_enum"
    bl_label = "Context Set Enum"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assignment value (as a string)",
            maxlen=1024, default="")

    execute = execute_context_assign


class WM_OT_context_set_value(bpy.types.Operator):
    '''Set a context value.'''
    bl_idname = "wm.context_set_value"
    bl_label = "Context Set Value"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value = StringProperty(name="Value",
            description="Assignment value (as a string)",
            maxlen=1024, default="")

    def execute(self, context):
        if context_path_validate(context, self.properties.path) is Ellipsis:
            return {'PASS_THROUGH'}
        exec("context.%s=%s" % (self.properties.path, self.properties.value))
        return {'FINISHED'}


class WM_OT_context_toggle(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_toggle"
    bl_label = "Context Toggle"
    bl_options = {'UNDO'}

    path = rna_path_prop

    def execute(self, context):

        if context_path_validate(context, self.properties.path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("context.%s=not (context.%s)" %
            (self.properties.path, self.properties.path))

        return {'FINISHED'}


class WM_OT_context_toggle_enum(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_toggle_enum"
    bl_label = "Context Toggle Values"
    bl_options = {'UNDO'}

    path = rna_path_prop
    value_1 = StringProperty(name="Value", \
                description="Toggle enum", maxlen=1024, default="")

    value_2 = StringProperty(name="Value", \
                description="Toggle enum", maxlen=1024, default="")

    def execute(self, context):

        if context_path_validate(context, self.properties.path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("context.%s = ['%s', '%s'][context.%s!='%s']" % \
            (self.properties.path, self.properties.value_1,\
             self.properties.value_2, self.properties.path,
             self.properties.value_2))

        return {'FINISHED'}


class WM_OT_context_cycle_int(bpy.types.Operator):
    '''Set a context value. Useful for cycling active material,
    vertex keys, groups' etc.'''
    bl_idname = "wm.context_cycle_int"
    bl_label = "Context Int Cycle"
    bl_options = {'UNDO'}

    path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):
        path = self.properties.path
        value = context_path_validate(context, path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        if self.properties.reverse:
            value -= 1
        else:
            value += 1

        exec("context.%s=value" % path)

        if value != eval("context.%s" % path):
            # relies on rna clamping int's out of the range
            if self.properties.reverse:
                value = (1 << 32)
            else:
                value = - (1 << 32)

            exec("context.%s=value" % path)

        return {'FINISHED'}


class WM_OT_context_cycle_enum(bpy.types.Operator):
    '''Toggle a context value.'''
    bl_idname = "wm.context_cycle_enum"
    bl_label = "Context Enum Cycle"
    bl_options = {'UNDO'}

    path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):

        value = context_path_validate(context, self.properties.path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        orig_value = value

        # Have to get rna enum values
        rna_struct_str, rna_prop_str = self.properties.path.rsplit('.', 1)
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
        if self.properties.reverse:
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
        exec("context.%s=advance_enum" % self.properties.path)
        return {'FINISHED'}

doc_id = StringProperty(name="Doc ID",
        description="", maxlen=1024, default="", options={'HIDDEN'})

doc_new = StringProperty(name="Edit Description",
        description="", maxlen=1024, default="")


class WM_OT_context_modal_mouse(bpy.types.Operator):
    '''Adjust arbitrary values with mouse input'''
    bl_idname = "wm.context_modal_mouse"
    bl_label = "Context Modal Mouse"

    path_iter = StringProperty(description="The path relative to the context, must point to an iterable.")
    path_item = StringProperty(description="The path from each iterable to the value (int or float)")
    input_scale = FloatProperty(default=0.01, description="Scale the mouse movement by this value before applying the delta")
    invert = BoolProperty(default=False, description="Invert the mouse input")
    initial_x = IntProperty(options={'HIDDEN'})

    def _values_store(self, context):
        path_iter = self.properties.path_iter
        path_item = self.properties.path_item

        self._values = values = {}

        for item in getattr(context, path_iter):
            try:
                value_orig = eval("item." + path_item)
            except:
                continue

            # check this can be set, maybe this is library data.
            try:
                exec("item.%s = %s" % (path_item, value_orig))
            except:
                continue

            values[item] = value_orig

    def _values_delta(self, delta):
        delta *= self.properties.input_scale
        if self.properties.invert:
            delta = - delta

        path_item = self.properties.path_item
        for item, value_orig in self._values.items():
            if type(value_orig) == int:
                exec("item.%s = int(%d)" % (path_item, round(value_orig + delta)))
            else:
                exec("item.%s = %f" % (path_item, value_orig + delta))

    def _values_restore(self):
        path_item = self.properties.path_item
        for item, value_orig in self._values.items():
            exec("item.%s = %s" % (path_item, value_orig))

        self._values.clear()

    def _values_clear(self):
        self._values.clear()

    def modal(self, context, event):
        event_type = event.type

        if event_type == 'MOUSEMOVE':
            delta = event.mouse_x - self.properties.initial_x
            self._values_delta(delta)

        elif 'LEFTMOUSE' == event_type:
            self._values_clear()
            return {'FINISHED'}

        elif event_type in ('RIGHTMOUSE', 'ESC'):
            self._values_restore()
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        self._values_store(context)

        if not self._values:
            self.report({'WARNING'}, "Nothing to operate on: %s[ ].%s" %
                    (self.properties.path_iter, self.properties.path_item))

            return {'CANCELLED'}
        else:
            self.properties.initial_x = event.mouse_x

            context.manager.add_modal_handler(self)
            return {'RUNNING_MODAL'}


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
        id_split = self.properties.doc_id.split('.')
        if len(id_split) == 1: # rna, class
            url = '%s/bpy.types.%s.html' % (self._prefix, id_split[0])
        elif len(id_split) == 2: # rna, class.prop
            class_name, class_prop = id_split

            if hasattr(bpy.types, class_name.upper() + '_OT_' + class_prop):
                url = '%s/bpy.ops.%s.html#bpy.ops.%s.%s' % \
                        (self._prefix, class_name, class_name, class_prop)
            else:
                # It so happens that epydoc nests these, not sphinx
                # class_name_full = self._nested_class_string(class_name)
                url = '%s/bpy.types.%s.html#bpy.types.%s.%s' % \
                        (self._prefix, class_name, class_name, class_prop)

        else:
            return {'PASS_THROUGH'}

        import webbrowser
        webbrowser.open(url)

        return {'FINISHED'}


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

        doc_id = self.properties.doc_id
        doc_new = self.properties.doc_new

        class_name, class_prop = doc_id.split('.')

        if not doc_new:
            return {'RUNNING_MODAL'}

        # check if this is an operator
        op_name = class_name.upper() + '_OT_' + class_prop
        op_class = getattr(bpy.types, op_name, None)

        # Upload this to the web server
        upload = {}

        if op_class:
            rna = op_class.bl_rna
            doc_orig = rna.description
            if doc_orig == doc_new:
                return {'RUNNING_MODAL'}

            print("op - old:'%s' -> new:'%s'" % (doc_orig, doc_new))
            upload["title"] = 'OPERATOR %s:%s' % (doc_id, doc_orig)
            upload["description"] = doc_new

            self._send_xmlrpc(upload)

        else:
            rna = getattr(bpy.types, class_name).bl_rna
            doc_orig = rna.properties[class_prop].description
            if doc_orig == doc_new:
                return {'RUNNING_MODAL'}

            print("rna - old:'%s' -> new:'%s'" % (doc_orig, doc_new))
            upload["title"] = 'RNA %s:%s' % (doc_id, doc_orig)

        upload["description"] = doc_new

        self._send_xmlrpc(upload)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        return wm.invoke_props_popup(self, event)


class WM_OT_reload_scripts(bpy.types.Operator):
    '''Load online reference docs'''
    bl_idname = "wm.reload_scripts"
    bl_label = "Reload Scripts"

    def execute(self, context):
        MOD = type(bpy)
        bpy.utils.load_scripts(True)
        return {'FINISHED'}


import rna_prop_ui

classes = [
    MESH_OT_delete_edgeloop,

    WM_OT_context_set_boolean,
    WM_OT_context_set_int,
    WM_OT_context_scale_int,
    WM_OT_context_set_float,
    WM_OT_context_set_string,
    WM_OT_context_set_enum,
    WM_OT_context_set_value,
    WM_OT_context_toggle,
    WM_OT_context_toggle_enum,
    WM_OT_context_cycle_enum,
    WM_OT_context_cycle_int,
    WM_OT_context_modal_mouse,

    WM_OT_doc_view,
    WM_OT_doc_edit,

    WM_OT_reload_scripts,

    # experemental!
    rna_prop_ui.WM_OT_properties_edit,
    rna_prop_ui.WM_OT_properties_add,
    rna_prop_ui.WM_OT_properties_remove]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()
