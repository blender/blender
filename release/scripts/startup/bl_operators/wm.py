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

# <pep8 compliant>

import bpy
from bpy.types import Menu, Operator
from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       EnumProperty,
                       )

from rna_prop_ui import rna_idprop_ui_prop_get, rna_idprop_ui_prop_clear


class MESH_OT_delete_edgeloop(Operator):
    '''Delete an edge loop by merging the faces on each side to a single face loop'''
    bl_idname = "mesh.delete_edgeloop"
    bl_label = "Delete Edge Loop"

    def execute(self, context):
        if 'FINISHED' in bpy.ops.transform.edge_slide(value=1.0):
            bpy.ops.mesh.select_more()
            bpy.ops.mesh.remove_doubles()
            return {'FINISHED'}

        return {'CANCELLED'}

rna_path_prop = StringProperty(
        name="Context Attributes",
        description="rna context string",
        maxlen=1024,
        )

rna_reverse_prop = BoolProperty(
        name="Reverse",
        description="Cycle backwards",
        default=False,
        )

rna_relative_prop = BoolProperty(
        name="Relative",
        description="Apply relative to the current value (delta)",
        default=False,
        )


def context_path_validate(context, data_path):
    try:
        value = eval("context.%s" % data_path) if data_path else Ellipsis
    except AttributeError as e:
        if str(e).startswith("'NoneType'"):
            # One of the items in the rna path is None, just ignore this
            value = Ellipsis
        else:
            # We have a real error in the rna path, don't ignore that
            raise

    return value


def operator_value_is_undo(value):
    if value in {None, Ellipsis}:
        return False

    # typical properties or objects
    id_data = getattr(value, "id_data", Ellipsis)

    if id_data is None:
        return False
    elif id_data is Ellipsis:
        # handle mathutils types
        id_data = getattr(getattr(value, "owner", None), "id_data", None)

        if id_data is None:
            return False

    # return True if its a non window ID type
    return (isinstance(id_data, bpy.types.ID) and
            (not isinstance(id_data, (bpy.types.WindowManager,
                                      bpy.types.Screen,
                                      bpy.types.Scene,
                                      bpy.types.Brush,
                                      ))))


def operator_path_is_undo(context, data_path):
    # note that if we have data paths that use strings this could fail
    # luckily we don't do this!
    #
    # When we cant find the data owner assume no undo is needed.
    data_path_head = data_path.rpartition(".")[0]

    if not data_path_head:
        return False

    value = context_path_validate(context, data_path_head)

    return operator_value_is_undo(value)


def operator_path_undo_return(context, data_path):
    return {'FINISHED'} if operator_path_is_undo(context, data_path) else {'CANCELLED'}


def operator_value_undo_return(value):
    return {'FINISHED'} if operator_value_is_undo(value) else {'CANCELLED'}


def execute_context_assign(self, context):
    data_path = self.data_path
    if context_path_validate(context, data_path) is Ellipsis:
        return {'PASS_THROUGH'}

    if getattr(self, "relative", False):
        exec("context.%s += self.value" % data_path)
    else:
        exec("context.%s = self.value" % data_path)

    return operator_path_undo_return(context, data_path)


class BRUSH_OT_active_index_set(Operator):
    '''Set active sculpt/paint brush from it's number'''
    bl_idname = "brush.active_index_set"
    bl_label = "Set Brush Number"

    mode = StringProperty(
            name="Mode",
            description="Paint mode to set brush for",
            maxlen=1024,
            )
    index = IntProperty(
            name="Number",
            description="Brush number",
            )

    _attr_dict = {"sculpt": "use_paint_sculpt",
                  "vertex_paint": "use_paint_vertex",
                  "weight_paint": "use_paint_weight",
                  "image_paint": "use_paint_image",
                  }

    def execute(self, context):
        attr = self._attr_dict.get(self.mode)
        if attr is None:
            return {'CANCELLED'}

        toolsettings = context.tool_settings
        for i, brush in enumerate((cur for cur in bpy.data.brushes if getattr(cur, attr))):
            if i == self.index:
                getattr(toolsettings, self.mode).brush = brush
                return {'FINISHED'}

        return {'CANCELLED'}


class WM_OT_context_set_boolean(Operator):
    '''Set a context value'''
    bl_idname = "wm.context_set_boolean"
    bl_label = "Context Set Boolean"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = BoolProperty(
            name="Value",
            description="Assignment value",
            default=True,
            )

    execute = execute_context_assign


class WM_OT_context_set_int(Operator):  # same as enum
    '''Set a context value'''
    bl_idname = "wm.context_set_int"
    bl_label = "Context Set"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = IntProperty(
            name="Value",
            description="Assign value",
            default=0,
            )
    relative = rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_scale_int(Operator):
    '''Scale an int context value'''
    bl_idname = "wm.context_scale_int"
    bl_label = "Context Set"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = FloatProperty(
            name="Value",
            description="Assign value",
            default=1.0,
            )
    always_step = BoolProperty(
            name="Always Step",
            description="Always adjust the value by a minimum of 1 when 'value' is not 1.0",
            default=True,
            )

    def execute(self, context):
        data_path = self.data_path
        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        value = self.value

        if value == 1.0:  # nothing to do
            return {'CANCELLED'}

        if getattr(self, "always_step", False):
            if value > 1.0:
                add = "1"
                func = "max"
            else:
                add = "-1"
                func = "min"
            exec("context.%s = %s(round(context.%s * value), context.%s + %s)" %
                 (data_path, func, data_path, data_path, add))
        else:
            exec("context.%s *= value" % data_path)

        return operator_path_undo_return(context, data_path)


class WM_OT_context_set_float(Operator):  # same as enum
    '''Set a context value'''
    bl_idname = "wm.context_set_float"
    bl_label = "Context Set Float"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = FloatProperty(
            name="Value",
            description="Assignment value",
            default=0.0,
            )
    relative = rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_set_string(Operator):  # same as enum
    '''Set a context value'''
    bl_idname = "wm.context_set_string"
    bl_label = "Context Set String"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = StringProperty(
            name="Value",
            description="Assign value",
            maxlen=1024,
            )

    execute = execute_context_assign


class WM_OT_context_set_enum(Operator):
    '''Set a context value'''
    bl_idname = "wm.context_set_enum"
    bl_label = "Context Set Enum"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = StringProperty(
            name="Value",
            description="Assignment value (as a string)",
            maxlen=1024,
            )

    execute = execute_context_assign


class WM_OT_context_set_value(Operator):
    '''Set a context value'''
    bl_idname = "wm.context_set_value"
    bl_label = "Context Set Value"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = StringProperty(
            name="Value",
            description="Assignment value (as a string)",
            maxlen=1024,
            )

    def execute(self, context):
        data_path = self.data_path
        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}
        exec("context.%s = %s" % (data_path, self.value))
        return operator_path_undo_return(context, data_path)


class WM_OT_context_toggle(Operator):
    '''Toggle a context value'''
    bl_idname = "wm.context_toggle"
    bl_label = "Context Toggle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop

    def execute(self, context):
        data_path = self.data_path

        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("context.%s = not (context.%s)" % (data_path, data_path))

        return operator_path_undo_return(context, data_path)


class WM_OT_context_toggle_enum(Operator):
    '''Toggle a context value'''
    bl_idname = "wm.context_toggle_enum"
    bl_label = "Context Toggle Values"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value_1 = StringProperty(
            name="Value",
            description="Toggle enum",
            maxlen=1024,
            )
    value_2 = StringProperty(
            name="Value",
            description="Toggle enum",
            maxlen=1024,
            )

    def execute(self, context):
        data_path = self.data_path

        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("context.%s = ('%s', '%s')[context.%s != '%s']" %
             (data_path, self.value_1,
              self.value_2, data_path,
              self.value_2,
              ))

        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_int(Operator):
    '''Set a context value. Useful for cycling active material, '''
    '''vertex keys, groups' etc'''
    bl_idname = "wm.context_cycle_int"
    bl_label = "Context Int Cycle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        if self.reverse:
            value -= 1
        else:
            value += 1

        exec("context.%s = value" % data_path)

        if value != eval("context.%s" % data_path):
            # relies on rna clamping integers out of the range
            if self.reverse:
                value = (1 << 31) - 1
            else:
                value = -1 << 31

            exec("context.%s = value" % data_path)

        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_enum(Operator):
    '''Toggle a context value'''
    bl_idname = "wm.context_cycle_enum"
    bl_label = "Context Enum Cycle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        orig_value = value

        # Have to get rna enum values
        rna_struct_str, rna_prop_str = data_path.rsplit('.', 1)
        i = rna_prop_str.find('[')

        # just in case we get "context.foo.bar[0]"
        if i != -1:
            rna_prop_str = rna_prop_str[0:i]

        rna_struct = eval("context.%s.rna_type" % rna_struct_str)

        rna_prop = rna_struct.properties[rna_prop_str]

        if type(rna_prop) != bpy.types.EnumProperty:
            raise Exception("expected an enum property")

        enums = rna_struct.properties[rna_prop_str].enum_items.keys()
        orig_index = enums.index(orig_value)

        # Have the info we need, advance to the next item
        if self.reverse:
            if orig_index == 0:
                advance_enum = enums[-1]
            else:
                advance_enum = enums[orig_index - 1]
        else:
            if orig_index == len(enums) - 1:
                advance_enum = enums[0]
            else:
                advance_enum = enums[orig_index + 1]

        # set the new value
        exec("context.%s = advance_enum" % data_path)
        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_array(Operator):
    '''Set a context array value. '''
    '''Useful for cycling the active mesh edit mode'''
    bl_idname = "wm.context_cycle_array"
    bl_label = "Context Array Cycle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    reverse = rna_reverse_prop

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        def cycle(array):
            if self.reverse:
                array.insert(0, array.pop())
            else:
                array.append(array.pop(0))
            return array

        exec("context.%s = cycle(context.%s[:])" % (data_path, data_path))

        return operator_path_undo_return(context, data_path)


class WM_MT_context_menu_enum(Menu):
    bl_label = ""
    data_path = ""  # BAD DESIGN, set from operator below.

    def draw(self, context):
        data_path = self.data_path
        value = context_path_validate(bpy.context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}
        base_path, prop_string = data_path.rsplit(".", 1)
        value_base = context_path_validate(context, base_path)

        values = [(i.name, i.identifier) for i in value_base.bl_rna.properties[prop_string].enum_items]

        for name, identifier in values:
            props = self.layout.operator("wm.context_set_enum", text=name)
            props.data_path = data_path
            props.value = identifier


class WM_OT_context_menu_enum(Operator):
    bl_idname = "wm.context_menu_enum"
    bl_label = "Context Enum Menu"
    bl_options = {'UNDO', 'INTERNAL'}
    data_path = rna_path_prop

    def execute(self, context):
        data_path = self.data_path
        WM_MT_context_menu_enum.data_path = data_path
        bpy.ops.wm.call_menu(name="WM_MT_context_menu_enum")
        return {'PASS_THROUGH'}


class WM_OT_context_set_id(Operator):
    '''Toggle a context value'''
    bl_idname = "wm.context_set_id"
    bl_label = "Set Library ID"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = StringProperty(
            name="Value",
            description="Assign value",
            maxlen=1024,
            )

    def execute(self, context):
        value = self.value
        data_path = self.data_path

        # match the pointer type from the target property to bpy.data.*
        # so we lookup the correct list.
        data_path_base, data_path_prop = data_path.rsplit(".", 1)
        data_prop_rna = eval("context.%s" % data_path_base).rna_type.properties[data_path_prop]
        data_prop_rna_type = data_prop_rna.fixed_type

        id_iter = None

        for prop in bpy.data.rna_type.properties:
            if prop.rna_type.identifier == "CollectionProperty":
                if prop.fixed_type == data_prop_rna_type:
                    id_iter = prop.identifier
                    break

        if id_iter:
            value_id = getattr(bpy.data, id_iter).get(value)
            exec("context.%s = value_id" % data_path)

        return operator_path_undo_return(context, data_path)


doc_id = StringProperty(
        name="Doc ID",
        maxlen=1024,
        options={'HIDDEN'},
        )

doc_new = StringProperty(
        name="Edit Description",
        maxlen=1024,
        )

data_path_iter = StringProperty(
        description="The data path relative to the context, must point to an iterable")

data_path_item = StringProperty(
        description="The data path from each iterable to the value (int or float)")


class WM_OT_context_collection_boolean_set(Operator):
    '''Set boolean values for a collection of items'''
    bl_idname = "wm.context_collection_boolean_set"
    bl_label = "Context Collection Boolean Set"
    bl_options = {'UNDO', 'REGISTER', 'INTERNAL'}

    data_path_iter = data_path_iter
    data_path_item = data_path_item

    type = EnumProperty(
            name="Type",
            items=(('TOGGLE', "Toggle", ""),
                   ('ENABLE', "Enable", ""),
                   ('DISABLE', "Disable", ""),
                   ),
            )

    def execute(self, context):
        data_path_iter = self.data_path_iter
        data_path_item = self.data_path_item

        items = list(getattr(context, data_path_iter))
        items_ok = []
        is_set = False
        for item in items:
            try:
                value_orig = eval("item." + data_path_item)
            except:
                continue

            if value_orig == True:
                is_set = True
            elif value_orig == False:
                pass
            else:
                self.report({'WARNING'}, "Non boolean value found: %s[ ].%s" %
                            (data_path_iter, data_path_item))
                return {'CANCELLED'}

            items_ok.append(item)

        # avoid undo push when nothing to do
        if not items_ok:
            return {'CANCELLED'}

        if self.type == 'ENABLE':
            is_set = True
        elif self.type == 'DISABLE':
            is_set = False
        else:
            is_set = not is_set

        exec_str = "item.%s = %s" % (data_path_item, is_set)
        for item in items_ok:
            exec(exec_str)

        return operator_value_undo_return(item)


class WM_OT_context_modal_mouse(Operator):
    '''Adjust arbitrary values with mouse input'''
    bl_idname = "wm.context_modal_mouse"
    bl_label = "Context Modal Mouse"
    bl_options = {'GRAB_POINTER', 'BLOCKING', 'UNDO', 'INTERNAL'}

    data_path_iter = data_path_iter
    data_path_item = data_path_item
    header_text = StringProperty(
            name="Header Text",
            description="Text to display in header during scale",
            )

    input_scale = FloatProperty(
            description="Scale the mouse movement by this value before applying the delta",
            default=0.01,
            )
    invert = BoolProperty(
            description="Invert the mouse input",
            default=False,
            )
    initial_x = IntProperty(options={'HIDDEN'})

    def _values_store(self, context):
        data_path_iter = self.data_path_iter
        data_path_item = self.data_path_item

        self._values = values = {}

        for item in getattr(context, data_path_iter):
            try:
                value_orig = eval("item." + data_path_item)
            except:
                continue

            # check this can be set, maybe this is library data.
            try:
                exec("item.%s = %s" % (data_path_item, value_orig))
            except:
                continue

            values[item] = value_orig

    def _values_delta(self, delta):
        delta *= self.input_scale
        if self.invert:
            delta = - delta

        data_path_item = self.data_path_item
        for item, value_orig in self._values.items():
            if type(value_orig) == int:
                exec("item.%s = int(%d)" % (data_path_item, round(value_orig + delta)))
            else:
                exec("item.%s = %f" % (data_path_item, value_orig + delta))

    def _values_restore(self):
        data_path_item = self.data_path_item
        for item, value_orig in self._values.items():
            exec("item.%s = %s" % (data_path_item, value_orig))

        self._values.clear()

    def _values_clear(self):
        self._values.clear()

    def modal(self, context, event):
        event_type = event.type

        if event_type == 'MOUSEMOVE':
            delta = event.mouse_x - self.initial_x
            self._values_delta(delta)
            header_text = self.header_text
            if header_text:
                if len(self._values) == 1:
                    (item, ) = self._values.keys()
                    header_text = header_text % eval("item.%s" % self.data_path_item)
                else:
                    header_text = (self.header_text % delta) + " (delta)"
                context.area.header_text_set(header_text)

        elif 'LEFTMOUSE' == event_type:
            item = next(iter(self._values.keys()))
            self._values_clear()
            context.area.header_text_set()
            return operator_value_undo_return(item)

        elif event_type in {'RIGHTMOUSE', 'ESC'}:
            self._values_restore()
            context.area.header_text_set()
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        self._values_store(context)

        if not self._values:
            self.report({'WARNING'}, "Nothing to operate on: %s[ ].%s" %
                    (self.data_path_iter, self.data_path_item))

            return {'CANCELLED'}
        else:
            self.initial_x = event.mouse_x

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}


class WM_OT_url_open(Operator):
    "Open a website in the web-browser"
    bl_idname = "wm.url_open"
    bl_label = ""

    url = StringProperty(
            name="URL",
            description="URL to open",
            )

    def execute(self, context):
        import webbrowser
        webbrowser.open(self.url)
        return {'FINISHED'}


class WM_OT_path_open(Operator):
    "Open a path in a file browser"
    bl_idname = "wm.path_open"
    bl_label = ""

    filepath = StringProperty(
            subtype='FILE_PATH',
            options={'SKIP_SAVE'},
            )

    def execute(self, context):
        import sys
        import os
        import subprocess

        filepath = self.filepath

        if not filepath:
            self.report({'ERROR'}, "File path was not set")
            return {'CANCELLED'}

        filepath = bpy.path.abspath(filepath)
        filepath = os.path.normpath(filepath)

        if not os.path.exists(filepath):
            self.report({'ERROR'}, "File '%s' not found" % filepath)
            return {'CANCELLED'}

        if sys.platform[:3] == "win":
            subprocess.Popen(["start", filepath], shell=True)
        elif sys.platform == "darwin":
            subprocess.Popen(["open", filepath])
        else:
            try:
                subprocess.Popen(["xdg-open", filepath])
            except OSError:
                # xdg-open *should* be supported by recent Gnome, KDE, Xfce
                pass

        return {'FINISHED'}


class WM_OT_doc_view(Operator):
    '''Load online reference docs'''
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id = doc_id
    if bpy.app.version_cycle == "release":
        _prefix = ("http://www.blender.org/documentation/blender_python_api_%s%s_release" %
                   ("_".join(str(v) for v in bpy.app.version[:2]), bpy.app.version_char))
    else:
        _prefix = ("http://www.blender.org/documentation/blender_python_api_%s" %
                   "_".join(str(v) for v in bpy.app.version))

    def _nested_class_string(self, class_string):
        ls = []
        class_obj = getattr(bpy.types, class_string, None).bl_rna
        while class_obj:
            ls.insert(0, class_obj)
            class_obj = class_obj.nested
        return '.'.join(class_obj.identifier for class_obj in ls)

    def execute(self, context):
        id_split = self.doc_id.split('.')
        if len(id_split) == 1:  # rna, class
            url = '%s/bpy.types.%s.html' % (self._prefix, id_split[0])
        elif len(id_split) == 2:  # rna, class.prop
            class_name, class_prop = id_split

            if hasattr(bpy.types, class_name.upper() + '_OT_' + class_prop):
                url = ("%s/bpy.ops.%s.html#bpy.ops.%s.%s" %
                       (self._prefix, class_name, class_name, class_prop))
            else:

                # detect if this is a inherited member and use that name instead
                rna_parent = getattr(bpy.types, class_name).bl_rna
                rna_prop = rna_parent.properties[class_prop]
                rna_parent = rna_parent.base
                while rna_parent and rna_prop == rna_parent.properties.get(class_prop):
                    class_name = rna_parent.identifier
                    rna_parent = rna_parent.base

                #~ class_name_full = self._nested_class_string(class_name)
                url = ("%s/bpy.types.%s.html#bpy.types.%s.%s" %
                       (self._prefix, class_name, class_name, class_prop))

        else:
            return {'PASS_THROUGH'}

        import webbrowser
        webbrowser.open(url)

        return {'FINISHED'}


class WM_OT_doc_edit(Operator):
    '''Load online reference docs'''
    bl_idname = "wm.doc_edit"
    bl_label = "Edit Documentation"

    doc_id = doc_id
    doc_new = doc_new

    _url = "http://www.mindrones.com/blender/svn/xmlrpc.php"

    def _send_xmlrpc(self, data_dict):
        print("sending data:", data_dict)

        import xmlrpc.client
        user = "blenderuser"
        pwd = "blender>user"

        docblog = xmlrpc.client.ServerProxy(self._url)
        docblog.metaWeblog.newPost(1, user, pwd, data_dict, 1)

    def execute(self, context):

        doc_id = self.doc_id
        doc_new = self.doc_new

        class_name, class_prop = doc_id.split('.')

        if not doc_new:
            self.report({'ERROR'}, "No input given for '%s'" % doc_id)
            return {'CANCELLED'}

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

    def draw(self, context):
        layout = self.layout
        layout.label(text="Descriptor ID: '%s'" % self.doc_id)
        layout.prop(self, "doc_new", text="")

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)


rna_path = StringProperty(
        name="Property Edit",
        description="Property data_path edit",
        maxlen=1024,
        options={'HIDDEN'},
        )

rna_value = StringProperty(
        name="Property Value",
        description="Property value edit",
        maxlen=1024,
        )

rna_property = StringProperty(
        name="Property Name",
        description="Property name edit",
        maxlen=1024,
        )

rna_min = FloatProperty(
        name="Min",
        default=0.0,
        precision=3,
        )

rna_max = FloatProperty(
        name="Max",
        default=1.0,
        precision=3,
        )


class WM_OT_properties_edit(Operator):
    '''Internal use (edit a property data_path)'''
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property"
    bl_options = {'REGISTER'}  # only because invoke_props_popup requires.

    data_path = rna_path
    property = rna_property
    value = rna_value
    min = rna_min
    max = rna_max
    description = StringProperty(
            name="Tip",
            )

    def execute(self, context):
        data_path = self.data_path
        value = self.value
        prop = self.property

        prop_old = getattr(self, "_last_prop", [None])[0]

        if prop_old is None:
            self.report({'ERROR'}, "Direct execution not supported")
            return {'CANCELLED'}

        try:
            value_eval = eval(value)
        except:
            value_eval = value

        # First remove
        item = eval("context.%s" % data_path)

        rna_idprop_ui_prop_clear(item, prop_old)
        exec_str = "del item['%s']" % prop_old
        # print(exec_str)
        exec(exec_str)

        # Reassign
        exec_str = "item['%s'] = %s" % (prop, repr(value_eval))
        # print(exec_str)
        exec(exec_str)
        self._last_prop[:] = [prop]

        prop_type = type(item[prop])

        prop_ui = rna_idprop_ui_prop_get(item, prop)

        if prop_type in {float, int}:
            prop_ui["soft_min"] = prop_ui["min"] = prop_type(self.min)
            prop_ui["soft_max"] = prop_ui["max"] = prop_type(self.max)

        prop_ui['description'] = self.description

        # otherwise existing buttons which reference freed
        # memory may crash blender [#26510]
        # context.area.tag_redraw()
        for win in context.window_manager.windows:
            for area in win.screen.areas:
                area.tag_redraw()

        return {'FINISHED'}

    def invoke(self, context, event):
        data_path = self.data_path

        if not data_path:
            self.report({'ERROR'}, "Data path not set")
            return {'CANCELLED'}

        self._last_prop = [self.property]

        item = eval("context.%s" % data_path)

        # setup defaults
        prop_ui = rna_idprop_ui_prop_get(item, self.property, False)  # don't create
        if prop_ui:
            self.min = prop_ui.get("min", -1000000000)
            self.max = prop_ui.get("max", 1000000000)
            self.description = prop_ui.get("description", "")

        wm = context.window_manager
        return wm.invoke_props_dialog(self)


class WM_OT_properties_add(Operator):
    '''Internal use (edit a property data_path)'''
    bl_idname = "wm.properties_add"
    bl_label = "Add Property"
    bl_options = {'UNDO'}

    data_path = rna_path

    def execute(self, context):
        data_path = self.data_path
        item = eval("context.%s" % data_path)

        def unique_name(names):
            prop = "prop"
            prop_new = prop
            i = 1
            while prop_new in names:
                prop_new = prop + str(i)
                i += 1

            return prop_new

        property = unique_name(item.keys())

        item[property] = 1.0
        return {'FINISHED'}


class WM_OT_properties_context_change(Operator):
    "Change the context tab in a Properties Window"
    bl_idname = "wm.properties_context_change"
    bl_label = ""

    context = StringProperty(
            name="Context",
            maxlen=64,
            )

    def execute(self, context):
        context.space_data.context = self.context
        return {'FINISHED'}


class WM_OT_properties_remove(Operator):
    '''Internal use (edit a property data_path)'''
    bl_idname = "wm.properties_remove"
    bl_label = "Remove Property"
    bl_options = {'UNDO'}

    data_path = rna_path
    property = rna_property

    def execute(self, context):
        data_path = self.data_path
        item = eval("context.%s" % data_path)
        del item[self.property]
        return {'FINISHED'}


class WM_OT_keyconfig_activate(Operator):
    bl_idname = "wm.keyconfig_activate"
    bl_label = "Activate Keyconfig"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )

    def execute(self, context):
        bpy.utils.keyconfig_set(self.filepath)
        return {'FINISHED'}


class WM_OT_appconfig_default(Operator):
    bl_idname = "wm.appconfig_default"
    bl_label = "Default Application Configuration"

    def execute(self, context):
        import os

        context.window_manager.keyconfigs.active = context.window_manager.keyconfigs.default

        filepath = os.path.join(bpy.utils.preset_paths("interaction")[0], "blender.py")

        if os.path.exists(filepath):
            bpy.ops.script.execute_preset(filepath=filepath, menu_idname="USERPREF_MT_interaction_presets")

        return {'FINISHED'}


class WM_OT_appconfig_activate(Operator):
    bl_idname = "wm.appconfig_activate"
    bl_label = "Activate Application Configuration"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )

    def execute(self, context):
        import os
        bpy.utils.keyconfig_set(self.filepath)

        filepath = self.filepath.replace("keyconfig", "interaction")

        if os.path.exists(filepath):
            bpy.ops.script.execute_preset(filepath=filepath, menu_idname="USERPREF_MT_interaction_presets")

        return {'FINISHED'}


class WM_OT_sysinfo(Operator):
    '''Generate System Info'''
    bl_idname = "wm.sysinfo"
    bl_label = "System Info"

    def execute(self, context):
        import sys_info
        sys_info.write_sysinfo(self)
        return {'FINISHED'}


class WM_OT_copy_prev_settings(Operator):
    '''Copy settings from previous version'''
    bl_idname = "wm.copy_prev_settings"
    bl_label = "Copy Previous Settings"

    def execute(self, context):
        import os
        import shutil
        ver = bpy.app.version
        ver_old = ((ver[0] * 100) + ver[1]) - 1
        path_src = bpy.utils.resource_path('USER', ver_old // 100, ver_old % 100)
        path_dst = bpy.utils.resource_path('USER')

        if os.path.isdir(path_dst):
            self.report({'ERROR'}, "Target path %r exists" % path_dst)
        elif not os.path.isdir(path_src):
            self.report({'ERROR'}, "Source path %r exists" % path_src)
        else:
            shutil.copytree(path_src, path_dst, symlinks=True)

            # in 2.57 and earlier windows installers, system scripts were copied
            # into the configuration directory, don't want to copy those
            system_script = os.path.join(path_dst, "scripts/modules/bpy_types.py")
            if os.path.isfile(system_script):
                shutil.rmtree(os.path.join(path_dst, "scripts"))
                shutil.rmtree(os.path.join(path_dst, "plugins"))

            # don't loose users work if they open the splash later.
            if bpy.data.is_saved is bpy.data.is_dirty is False:
                bpy.ops.wm.read_homefile()
            else:
                self.report({'INFO'}, "Reload Start-Up file to restore settings")
            return {'FINISHED'}

        return {'CANCELLED'}


class WM_OT_blenderplayer_start(Operator):
    '''Launch the blender-player with the current blend-file'''
    bl_idname = "wm.blenderplayer_start"
    bl_label = "Start Game In Player"

    def execute(self, context):
        import os
        import sys
        import subprocess

        # these remain the same every execution
        blender_bin_path = bpy.app.binary_path
        blender_bin_dir = os.path.dirname(blender_bin_path)
        ext = os.path.splitext(blender_bin_path)[-1]
        player_path = os.path.join(blender_bin_dir, "blenderplayer" + ext)
        # done static vars

        if sys.platform == "darwin":
            player_path = os.path.join(blender_bin_dir, "../../../blenderplayer.app/Contents/MacOS/blenderplayer")

        if not os.path.exists(player_path):
            self.report({'ERROR'}, "Player path: %r not found" % player_path)
            return {'CANCELLED'}

        filepath = os.path.join(bpy.app.tempdir, "game.blend")
        bpy.ops.wm.save_as_mainfile(filepath=filepath, check_existing=False, copy=True)
        subprocess.call([player_path, filepath])
        return {'FINISHED'}


class WM_OT_keyconfig_test(Operator):
    "Test key-config for conflicts"
    bl_idname = "wm.keyconfig_test"
    bl_label = "Test Key Configuration for Conflicts"

    def execute(self, context):
        from bpy_extras import keyconfig_utils

        wm = context.window_manager
        kc = wm.keyconfigs.default

        if keyconfig_utils.keyconfig_test(kc):
            print("CONFLICT")

        return {'FINISHED'}


class WM_OT_keyconfig_import(Operator):
    "Import key configuration from a python script"
    bl_idname = "wm.keyconfig_import"
    bl_label = "Import Key Configuration..."

    filepath = StringProperty(
            subtype='FILE_PATH',
            default="keymap.py",
            )
    filter_folder = BoolProperty(
            name="Filter folders",
            default=True,
            options={'HIDDEN'},
            )
    filter_text = BoolProperty(
            name="Filter text",
            default=True,
            options={'HIDDEN'},
            )
    filter_python = BoolProperty(
            name="Filter python",
            default=True,
            options={'HIDDEN'},
            )
    keep_original = BoolProperty(
            name="Keep original",
            description="Keep original file after copying to configuration folder",
            default=True,
            )

    def execute(self, context):
        import os
        from os.path import basename
        import shutil

        if not self.filepath:
            self.report({'ERROR'}, "Filepath not set")
            return {'CANCELLED'}

        config_name = basename(self.filepath)

        path = bpy.utils.user_resource('SCRIPTS', os.path.join("presets", "keyconfig"), create=True)
        path = os.path.join(path, config_name)

        try:
            if self.keep_original:
                shutil.copy(self.filepath, path)
            else:
                shutil.move(self.filepath, path)
        except Exception as e:
            self.report({'ERROR'}, "Installing keymap failed: %s" % e)
            return {'CANCELLED'}

        # sneaky way to check we're actually running the code.
        bpy.utils.keyconfig_set(path)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}

# This operator is also used by interaction presets saving - AddPresetBase


class WM_OT_keyconfig_export(Operator):
    "Export key configuration to a python script"
    bl_idname = "wm.keyconfig_export"
    bl_label = "Export Key Configuration..."

    filepath = StringProperty(
            subtype='FILE_PATH',
            default="keymap.py",
            )
    filter_folder = BoolProperty(
            name="Filter folders",
            default=True,
            options={'HIDDEN'},
            )
    filter_text = BoolProperty(
            name="Filter text",
            default=True,
            options={'HIDDEN'},
            )
    filter_python = BoolProperty(
            name="Filter python",
            default=True,
            options={'HIDDEN'},
            )

    def execute(self, context):
        from bpy_extras import keyconfig_utils

        if not self.filepath:
            raise Exception("Filepath not set")

        if not self.filepath.endswith('.py'):
            self.filepath += '.py'

        wm = context.window_manager

        keyconfig_utils.keyconfig_export(wm,
                                         wm.keyconfigs.active,
                                         self.filepath,
                                         )

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_keymap_restore(Operator):
    "Restore key map(s)"
    bl_idname = "wm.keymap_restore"
    bl_label = "Restore Key Map(s)"

    all = BoolProperty(
            name="All Keymaps",
            description="Restore all keymaps to default",
            )

    def execute(self, context):
        wm = context.window_manager

        if self.all:
            for km in wm.keyconfigs.user.keymaps:
                km.restore_to_default()
        else:
            km = context.keymap
            km.restore_to_default()

        return {'FINISHED'}


class WM_OT_keyitem_restore(Operator):
    "Restore key map item"
    bl_idname = "wm.keyitem_restore"
    bl_label = "Restore Key Map Item"

    item_id = IntProperty(
            name="Item Identifier",
            description="Identifier of the item to remove",
            )

    @classmethod
    def poll(cls, context):
        keymap = getattr(context, "keymap", None)
        return keymap

    def execute(self, context):
        km = context.keymap
        kmi = km.keymap_items.from_id(self.item_id)

        if (not kmi.is_user_defined) and kmi.is_user_modified:
            km.restore_item_to_default(kmi)

        return {'FINISHED'}


class WM_OT_keyitem_add(Operator):
    "Add key map item"
    bl_idname = "wm.keyitem_add"
    bl_label = "Add Key Map Item"

    def execute(self, context):
        km = context.keymap

        if km.is_modal:
            km.keymap_items.new_modal("", 'A', 'PRESS')
        else:
            km.keymap_items.new("none", 'A', 'PRESS')

        # clear filter and expand keymap so we can see the newly added item
        if context.space_data.filter_text != "":
            context.space_data.filter_text = ""
            km.show_expanded_items = True
            km.show_expanded_children = True

        return {'FINISHED'}


class WM_OT_keyitem_remove(Operator):
    "Remove key map item"
    bl_idname = "wm.keyitem_remove"
    bl_label = "Remove Key Map Item"

    item_id = IntProperty(
            name="Item Identifier",
            description="Identifier of the item to remove",
            )

    @classmethod
    def poll(cls, context):
        return hasattr(context, "keymap")

    def execute(self, context):
        km = context.keymap
        kmi = km.keymap_items.from_id(self.item_id)
        km.keymap_items.remove(kmi)
        return {'FINISHED'}


class WM_OT_keyconfig_remove(Operator):
    "Remove key config"
    bl_idname = "wm.keyconfig_remove"
    bl_label = "Remove Key Config"

    @classmethod
    def poll(cls, context):
        wm = context.window_manager
        keyconf = wm.keyconfigs.active
        return keyconf and keyconf.is_user_defined

    def execute(self, context):
        wm = context.window_manager
        keyconfig = wm.keyconfigs.active
        wm.keyconfigs.remove(keyconfig)
        return {'FINISHED'}


class WM_OT_operator_cheat_sheet(Operator):
    bl_idname = "wm.operator_cheat_sheet"
    bl_label = "Operator Cheat Sheet"

    def execute(self, context):
        op_strings = []
        tot = 0
        for op_module_name in dir(bpy.ops):
            op_module = getattr(bpy.ops, op_module_name)
            for op_submodule_name in dir(op_module):
                op = getattr(op_module, op_submodule_name)
                text = repr(op)
                if text.split("\n")[-1].startswith("bpy.ops."):
                    op_strings.append(text)
                    tot += 1

            op_strings.append('')

        textblock = bpy.data.texts.new("OperatorList.txt")
        textblock.write('# %d Operators\n\n' % tot)
        textblock.write('\n'.join(op_strings))
        self.report({'INFO'}, "See OperatorList.txt textblock")
        return {'FINISHED'}


# -----------------------------------------------------------------------------
# Addon Operators

class WM_OT_addon_enable(Operator):
    "Enable an addon"
    bl_idname = "wm.addon_enable"
    bl_label = "Enable Addon"

    module = StringProperty(
            name="Module",
            description="Module name of the addon to enable",
            )

    def execute(self, context):
        import addon_utils

        mod = addon_utils.enable(self.module)

        if mod:
            info = addon_utils.module_bl_info(mod)

            info_ver = info.get("blender", (0, 0, 0))

            if info_ver > bpy.app.version:
                self.report({'WARNING'}, ("This script was written Blender "
                                          "version %d.%d.%d and might not "
                                          "function (correctly), "
                                          "though it is enabled") %
                                         info_ver)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class WM_OT_addon_disable(Operator):
    "Disable an addon"
    bl_idname = "wm.addon_disable"
    bl_label = "Disable Addon"

    module = StringProperty(
            name="Module",
            description="Module name of the addon to disable",
            )

    def execute(self, context):
        import addon_utils

        addon_utils.disable(self.module)
        return {'FINISHED'}

class WM_OT_theme_install(Operator):
    "Install a theme"
    bl_idname = "wm.theme_install"
    bl_label  = "Install Theme..."

    overwrite = BoolProperty(
            name="Overwrite",
            description="Remove existing theme file if exists",
            default=True,
            )
    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_folder = BoolProperty(
            name="Filter folders",
            default=True,
            options={'HIDDEN'},
            )
    filter_glob = StringProperty(
            default="*.xml",
            options={'HIDDEN'},
            )

    def execute(self, context):
        import os
        import shutil
        import traceback
        
        xmlfile = self.filepath

        path_themes = bpy.utils.user_resource('SCRIPTS','presets/interface_theme',create=True)

        if not path_themes:
            self.report({'ERROR'}, "Failed to get themes path")
            return {'CANCELLED'}

        path_dest = os.path.join(path_themes, os.path.basename(xmlfile))

        if not self.overwrite:
            if os.path.exists(path_dest):
                self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                return {'CANCELLED'}

        try:
            shutil.copyfile(xmlfile, path_dest)
            bpy.ops.script.execute_preset(filepath=path_dest,menu_idname="USERPREF_MT_interface_theme_presets")

        except:
            traceback.print_exc()
            return {'CANCELLED'}

        return {'FINISHED'}


    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_install(Operator):
    "Install an addon"
    bl_idname = "wm.addon_install"
    bl_label = "Install Addon..."

    overwrite = BoolProperty(
            name="Overwrite",
            description="Remove existing addons with the same ID",
            default=True,
            )
    target = EnumProperty(
            name="Target Path",
            items=(('DEFAULT', "Default", ""),
                   ('PREFS', "User Prefs", "")),
            )

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    filter_folder = BoolProperty(
            name="Filter folders",
            default=True,
            options={'HIDDEN'},
            )
    filter_python = BoolProperty(
            name="Filter python",
            default=True,
            options={'HIDDEN'},
            )
    filter_glob = StringProperty(
            default="*.py;*.zip",
            options={'HIDDEN'},
            )

    @staticmethod
    def _module_remove(path_addons, module):
        import os
        module = os.path.splitext(module)[0]
        for f in os.listdir(path_addons):
            f_base = os.path.splitext(f)[0]
            if f_base == module:
                f_full = os.path.join(path_addons, f)

                if os.path.isdir(f_full):
                    os.rmdir(f_full)
                else:
                    os.remove(f_full)

    def execute(self, context):
        import addon_utils
        import traceback
        import zipfile
        import shutil
        import os

        pyfile = self.filepath

        if self.target == 'DEFAULT':
            # don't use bpy.utils.script_paths("addons") because we may not be able to write to it.
            path_addons = bpy.utils.user_resource('SCRIPTS', "addons", create=True)
        else:
            path_addons = bpy.context.user_preferences.filepaths.script_directory
            if path_addons:
                path_addons = os.path.join(path_addons, "addons")

        if not path_addons:
            self.report({'ERROR'}, "Failed to get addons path")
            return {'CANCELLED'}

        # create dir is if missing.
        if not os.path.exists(path_addons):
            os.makedirs(path_addons)

        # Check if we are installing from a target path,
        # doing so causes 2+ addons of same name or when the same from/to
        # location is used, removal of the file!
        addon_path = ""
        pyfile_dir = os.path.dirname(pyfile)
        for addon_path in addon_utils.paths():
            if os.path.samefile(pyfile_dir, addon_path):
                self.report({'ERROR'}, "Source file is in the addon search path: %r" % addon_path)
                return {'CANCELLED'}
        del addon_path
        del pyfile_dir
        # done checking for exceptional case

        addons_old = {mod.__name__ for mod in addon_utils.modules(addon_utils.addons_fake_modules)}

        #check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(pyfile):
            try:
                file_to_extract = zipfile.ZipFile(pyfile, 'r')
            except:
                traceback.print_exc()
                return {'CANCELLED'}

            if self.overwrite:
                for f in file_to_extract.namelist():
                    WM_OT_addon_install._module_remove(path_addons, f)
            else:
                for f in file_to_extract.namelist():
                    path_dest = os.path.join(path_addons, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                        return {'CANCELLED'}

            try:  # extract the file to "addons"
                file_to_extract.extractall(path_addons)

                # zip files can create this dir with metadata, don't need it
                macosx_dir = os.path.join(path_addons, '__MACOSX')
                if os.path.isdir(macosx_dir):
                    shutil.rmtree(macosx_dir)

            except:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            path_dest = os.path.join(path_addons, os.path.basename(pyfile))

            if self.overwrite:
                WM_OT_addon_install._module_remove(path_addons, os.path.basename(pyfile))
            elif os.path.exists(path_dest):
                self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                return {'CANCELLED'}

            #if not compressed file just copy into the addon path
            try:
                shutil.copyfile(pyfile, path_dest)

            except:
                traceback.print_exc()
                return {'CANCELLED'}

        addons_new = {mod.__name__ for mod in addon_utils.modules(addon_utils.addons_fake_modules)} - addons_old
        addons_new.discard("modules")

        # disable any addons we may have enabled previously and removed.
        # this is unlikely but do just in case. bug [#23978]
        for new_addon in addons_new:
            addon_utils.disable(new_addon)

        # possible the zip contains multiple addons, we could disallow this
        # but for now just use the first
        for mod in addon_utils.modules(addon_utils.addons_fake_modules):
            if mod.__name__ in addons_new:
                info = addon_utils.module_bl_info(mod)

                # show the newly installed addon.
                context.window_manager.addon_filter = 'All'
                context.window_manager.addon_search = info["name"]
                break

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # TODO, should not be a warning.
        #~ self.report({'WARNING'}, "File installed to '%s'\n" % path_dest)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_remove(Operator):
    "Disable an addon"
    bl_idname = "wm.addon_remove"
    bl_label = "Remove Addon"

    module = StringProperty(
            name="Module",
            description="Module name of the addon to remove",
            )

    @staticmethod
    def path_from_addon(module):
        import os
        import addon_utils

        for mod in addon_utils.modules(addon_utils.addons_fake_modules):
            if mod.__name__ == module:
                filepath = mod.__file__
                if os.path.exists(filepath):
                    if os.path.splitext(os.path.basename(filepath))[0] == "__init__":
                        return os.path.dirname(filepath), True
                    else:
                        return filepath, False
        return None, False

    def execute(self, context):
        import addon_utils
        import os

        path, isdir = WM_OT_addon_remove.path_from_addon(self.module)
        if path is None:
            self.report({'WARNING'}, "Addon path %r could not be found" % path)
            return {'CANCELLED'}

        # in case its enabled
        addon_utils.disable(self.module)

        import shutil
        if isdir:
            shutil.rmtree(path)
        else:
            os.remove(path)

        context.area.tag_redraw()
        return {'FINISHED'}

    # lame confirmation check
    def draw(self, context):
        self.layout.label(text="Remove Addon: %r?" % self.module)
        path, isdir = WM_OT_addon_remove.path_from_addon(self.module)
        self.layout.label(text="Path: %r" % path)

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)


class WM_OT_addon_expand(Operator):
    "Display more information on this addon"
    bl_idname = "wm.addon_expand"
    bl_label = ""

    module = StringProperty(
            name="Module",
            description="Module name of the addon to expand",
            )

    def execute(self, context):
        import addon_utils

        module_name = self.module

        # unlikely to fail, module should have already been imported
        try:
            # mod = __import__(module_name)
            mod = addon_utils.addons_fake_modules.get(module_name)
        except:
            import traceback
            traceback.print_exc()
            return {'CANCELLED'}

        info = addon_utils.module_bl_info(mod)
        info["show_expanded"] = not info["show_expanded"]
        return {'FINISHED'}
