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
from bpy.types import Operator
from bpy.props import (StringProperty,
                       BoolProperty,
                       IntProperty,
                       FloatProperty,
                       EnumProperty,
                       )

from bpy.app.translations import pgettext_tip as tip_


rna_path_prop = StringProperty(
        name="Context Attributes",
        description="RNA context string",
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
    """Set active sculpt/paint brush from it's number"""
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
    """Set a context value"""
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
    """Set a context value"""
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


class WM_OT_context_scale_float(Operator):
    """Scale a float context value"""
    bl_idname = "wm.context_scale_float"
    bl_label = "Context Scale Float"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path_prop
    value = FloatProperty(
            name="Value",
            description="Assign value",
            default=1.0,
            )

    def execute(self, context):
        data_path = self.data_path
        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        value = self.value

        if value == 1.0:  # nothing to do
            return {'CANCELLED'}

        exec("context.%s *= value" % data_path)

        return operator_path_undo_return(context, data_path)


class WM_OT_context_scale_int(Operator):
    """Scale an int context value"""
    bl_idname = "wm.context_scale_int"
    bl_label = "Context Scale Int"
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
    """Set a context value"""
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
    """Set a context value"""
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
    """Set a context value"""
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
    """Set a context value"""
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
    """Toggle a context value"""
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
    """Toggle a context value"""
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

        # failing silently is not ideal, but we don't want errors for shortcut
        # keys that some values that are only available in a particular context
        try:
            exec("context.%s = ('%s', '%s')[context.%s != '%s']" %
                 (data_path, self.value_1,
                  self.value_2, data_path,
                  self.value_2,
                  ))
        except:
            return {'PASS_THROUGH'}

        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_int(Operator):
    """Set a context value (useful for cycling active material, """ \
    """vertex keys, groups, etc.)"""
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
    """Toggle a context value"""
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
    """Set a context array value """ \
    """(useful for cycling the active mesh edit mode)"""
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


class WM_OT_context_menu_enum(Operator):
    bl_idname = "wm.context_menu_enum"
    bl_label = "Context Enum Menu"
    bl_options = {'UNDO', 'INTERNAL'}
    data_path = rna_path_prop

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)

        if value is Ellipsis:
            return {'PASS_THROUGH'}

        base_path, prop_string = data_path.rsplit(".", 1)
        value_base = context_path_validate(context, base_path)
        prop = value_base.bl_rna.properties[prop_string]

        def draw_cb(self, context):
            layout = self.layout
            layout.prop(value_base, prop_string, expand=True)

        context.window_manager.popup_menu(draw_func=draw_cb, title=prop.name, icon=prop.icon)

        return {'FINISHED'}


class WM_OT_context_pie_enum(Operator):
    bl_idname = "wm.context_pie_enum"
    bl_label = "Context Enum Pie"
    bl_options = {'UNDO', 'INTERNAL'}
    data_path = rna_path_prop

    def invoke(self, context, event):
        wm = context.window_manager
        data_path = self.data_path
        value = context_path_validate(context, data_path)

        if value is Ellipsis:
            return {'PASS_THROUGH'}

        base_path, prop_string = data_path.rsplit(".", 1)
        value_base = context_path_validate(context, base_path)
        prop = value_base.bl_rna.properties[prop_string]

        def draw_cb(self, context):
            layout = self.layout
            layout.prop(value_base, prop_string, expand=True)

        wm.popup_menu_pie(draw_func=draw_cb, title=prop.name, icon=prop.icon, event=event)

        return {'FINISHED'}


class WM_OT_operator_pie_enum(Operator):
    bl_idname = "wm.operator_pie_enum"
    bl_label = "Operator Enum Pie"
    bl_options = {'UNDO', 'INTERNAL'}
    data_path = StringProperty(
            name="Operator",
            description="Operator name (in python as string)",
            maxlen=1024,
            )
    prop_string = StringProperty(
            name="Property",
            description="Property name (as a string)",
            maxlen=1024,
            )

    def invoke(self, context, event):
        wm = context.window_manager

        data_path = self.data_path
        prop_string = self.prop_string

        # same as eval("bpy.ops." + data_path)
        op_mod_str, ob_id_str = data_path.split(".", 1)
        op = getattr(getattr(bpy.ops, op_mod_str), ob_id_str)
        del op_mod_str, ob_id_str

        try:
            op_rna = op.get_rna()
        except KeyError:
            self.report({'ERROR'}, "Operator not found: bpy.ops.%s" % data_path)
            return {'CANCELLED'}

        def draw_cb(self, context):
            layout = self.layout
            pie = layout.menu_pie()
            pie.operator_enum(data_path, prop_string)

        wm.popup_menu_pie(draw_func=draw_cb, title=op_rna.bl_rna.name, event=event)

        return {'FINISHED'}


class WM_OT_context_set_id(Operator):
    """Set a context value to an ID data-block"""
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
    """Set boolean values for a collection of items"""
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

            if value_orig is True:
                is_set = True
            elif value_orig is False:
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
    """Adjust arbitrary values with mouse input"""
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
    bl_options = {'INTERNAL'}

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
    bl_options = {'INTERNAL'}

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
            os.startfile(filepath)
        elif sys.platform == "darwin":
            subprocess.check_call(["open", filepath])
        else:
            try:
                subprocess.check_call(["xdg-open", filepath])
            except:
                # xdg-open *should* be supported by recent Gnome, KDE, Xfce
                import traceback
                traceback.print_exc()

        return {'FINISHED'}


def _wm_doc_get_id(doc_id, do_url=True, url_prefix=""):
    id_split = doc_id.split(".")
    url = rna = None

    if len(id_split) == 1:  # rna, class
        if do_url:
            url = "%s/bpy.types.%s.html" % (url_prefix, id_split[0])
        else:
            rna = "bpy.types.%s" % id_split[0]

    elif len(id_split) == 2:  # rna, class.prop
        class_name, class_prop = id_split

        # an operator (common case - just button referencing an op)
        if hasattr(bpy.types, class_name.upper() + "_OT_" + class_prop):
            if do_url:
                url = ("%s/bpy.ops.%s.html#bpy.ops.%s.%s" % (url_prefix, class_name, class_name, class_prop))
            else:
                rna = "bpy.ops.%s.%s" % (class_name, class_prop)
        else:
            rna_class = getattr(bpy.types, class_name)

            # an operator setting (selected from a running operator), rare case
            # note: Py defined operators are subclass of Operator,
            #       C defined operators are subclass of OperatorProperties.
            #       we may need to check on this at some point.
            if issubclass(rna_class, (bpy.types.Operator, bpy.types.OperatorProperties)):
                # note: ignore the prop name since we don't have a way to link into it
                class_name, class_prop = class_name.split("_OT_", 1)
                class_name = class_name.lower()
                if do_url:
                    url = ("%s/bpy.ops.%s.html#bpy.ops.%s.%s" % (url_prefix, class_name, class_name, class_prop))
                else:
                    rna = "bpy.ops.%s.%s" % (class_name, class_prop)
            else:
                # an RNA setting, common case

                # detect if this is a inherited member and use that name instead
                rna_parent = rna_class.bl_rna
                rna_prop = rna_parent.properties[class_prop]
                rna_parent = rna_parent.base
                while rna_parent and rna_prop == rna_parent.properties.get(class_prop):
                    class_name = rna_parent.identifier
                    rna_parent = rna_parent.base

                if do_url:
                    url = ("%s/bpy.types.%s.html#bpy.types.%s.%s" % (url_prefix, class_name, class_name, class_prop))
                else:
                    rna = ("bpy.types.%s.%s" % (class_name, class_prop))

    return url if do_url else rna


class WM_OT_doc_view_manual(Operator):
    """Load online manual"""
    bl_idname = "wm.doc_view_manual"
    bl_label = "View Manual"

    doc_id = doc_id

    @staticmethod
    def _find_reference(rna_id, url_mapping, verbose=True):
        if verbose:
            print("online manual check for: '%s'... " % rna_id)
        from fnmatch import fnmatch
        for pattern, url_suffix in url_mapping:
            if fnmatch(rna_id, pattern):
                if verbose:
                    print("            match found: '%s' --> '%s'" % (pattern, url_suffix))
                return url_suffix
        if verbose:
            print("match not found")
        return None

    @staticmethod
    def _lookup_rna_url(rna_id, verbose=True):
        for prefix, url_manual_mapping in bpy.utils.manual_map():
            rna_ref = WM_OT_doc_view_manual._find_reference(rna_id, url_manual_mapping, verbose=verbose)
            if rna_ref is not None:
                url = prefix + rna_ref
                return url

    def execute(self, context):
        rna_id = _wm_doc_get_id(self.doc_id, do_url=False)
        if rna_id is None:
            return {'PASS_THROUGH'}

        url = self._lookup_rna_url(rna_id)

        if url is None:
            self.report({'WARNING'}, "No reference available %r, "
                                     "Update info in 'rna_wiki_reference.py' "
                                     " or callback to bpy.utils.manual_map()" %
                                     self.doc_id)
            return {'CANCELLED'}
        else:
            import webbrowser
            webbrowser.open(url)
            return {'FINISHED'}


class WM_OT_doc_view(Operator):
    """Load online reference docs"""
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id = doc_id
    if bpy.app.version_cycle == "release":
        _prefix = ("http://www.blender.org/documentation/blender_python_api_%s%s_release" %
                   ("_".join(str(v) for v in bpy.app.version[:2]), bpy.app.version_char))
    else:
        _prefix = ("http://www.blender.org/documentation/blender_python_api_%s" %
                   "_".join(str(v) for v in bpy.app.version))

    def execute(self, context):
        url = _wm_doc_get_id(self.doc_id, do_url=True, url_prefix=self._prefix)
        if url is None:
            return {'PASS_THROUGH'}

        import webbrowser
        webbrowser.open(url)

        return {'FINISHED'}


'''
class WM_OT_doc_edit(Operator):
    """Edit online reference docs"""
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
'''


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
        default=-10000.0,
        precision=3,
        )

rna_max = FloatProperty(
        name="Max",
        default=10000.0,
        precision=3,
        )


class WM_OT_properties_edit(Operator):
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property"
    # register only because invoke_props_popup requires.
    bl_options = {'REGISTER', 'INTERNAL'}

    data_path = rna_path
    property = rna_property
    value = rna_value
    min = rna_min
    max = rna_max
    description = StringProperty(
            name="Tooltip",
            )

    def execute(self, context):
        from rna_prop_ui import rna_idprop_ui_prop_get, rna_idprop_ui_prop_clear

        data_path = self.data_path
        value = self.value
        prop = self.property

        prop_old = getattr(self, "_last_prop", [None])[0]

        if prop_old is None:
            self.report({'ERROR'}, "Direct execution not supported")
            return {'CANCELLED'}

        try:
            value_eval = eval(value)
            # assert else None -> None, not "None", see [#33431]
            assert(type(value_eval) in {str, float, int, bool, tuple, list})
        except:
            value_eval = value

        # First remove
        item = eval("context.%s" % data_path)
        prop_type_old = type(item[prop_old])

        rna_idprop_ui_prop_clear(item, prop_old)
        exec_str = "del item[%r]" % prop_old
        # print(exec_str)
        exec(exec_str)

        # Reassign
        exec_str = "item[%r] = %s" % (prop, repr(value_eval))
        # print(exec_str)
        exec(exec_str)
        self._last_prop[:] = [prop]

        prop_type = type(item[prop])

        prop_ui = rna_idprop_ui_prop_get(item, prop)

        if prop_type in {float, int}:
            prop_ui["soft_min"] = prop_ui["min"] = prop_type(self.min)
            prop_ui["soft_max"] = prop_ui["max"] = prop_type(self.max)

        prop_ui["description"] = self.description

        # If we have changed the type of the property, update its potential anim curves!
        if prop_type_old != prop_type:
            data_path = '["%s"]' % bpy.utils.escape_identifier(prop)
            done = set()

            def _update(fcurves):
                for fcu in fcurves:
                    if fcu not in done and fcu.data_path == data_path:
                        fcu.update_autoflags(item)
                        done.add(fcu)

            def _update_strips(strips):
                for st in strips:
                    if st.type == 'CLIP' and st.action:
                        _update(st.action.fcurves)
                    elif st.type == 'META':
                        _update_strips(st.strips)

            adt = getattr(item, "animation_data", None)
            if adt is not None:
                if adt.action:
                    _update(adt.action.fcurves)
                if adt.drivers:
                    _update(adt.drivers)
                if adt.nla_tracks:
                    for nt in adt.nla_tracks:
                        _update_strips(nt.strips)

        # otherwise existing buttons which reference freed
        # memory may crash blender [#26510]
        # context.area.tag_redraw()
        for win in context.window_manager.windows:
            for area in win.screen.areas:
                area.tag_redraw()

        return {'FINISHED'}

    def invoke(self, context, event):
        from rna_prop_ui import rna_idprop_ui_prop_get

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
    bl_idname = "wm.properties_add"
    bl_label = "Add Property"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path = rna_path

    def execute(self, context):
        from rna_prop_ui import rna_idprop_ui_prop_get

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

        prop = unique_name(item.keys())

        item[prop] = 1.0

        # not essential, but without this we get [#31661]
        prop_ui = rna_idprop_ui_prop_get(item, prop)
        prop_ui["soft_min"] = prop_ui["min"] = 0.0
        prop_ui["soft_max"] = prop_ui["max"] = 1.0

        return {'FINISHED'}


class WM_OT_properties_context_change(Operator):
    "Jump to a different tab inside the properties editor"
    bl_idname = "wm.properties_context_change"
    bl_label = ""
    bl_options = {'INTERNAL'}

    context = StringProperty(
            name="Context",
            maxlen=64,
            )

    def execute(self, context):
        context.space_data.context = self.context
        return {'FINISHED'}


class WM_OT_properties_remove(Operator):
    """Internal use (edit a property data_path)"""
    bl_idname = "wm.properties_remove"
    bl_label = "Remove Property"
    bl_options = {'UNDO', 'INTERNAL'}

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
        if bpy.utils.keyconfig_set(self.filepath, report=self.report):
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


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
    """Generate System Info"""
    bl_idname = "wm.sysinfo"
    bl_label = "System Info"

    def execute(self, context):
        import sys_info
        sys_info.write_sysinfo(self)
        return {'FINISHED'}


class WM_OT_copy_prev_settings(Operator):
    """Copy settings from previous version"""
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

            # reload recent-files.txt
            bpy.ops.wm.read_history()

            # don't loose users work if they open the splash later.
            if bpy.data.is_saved is bpy.data.is_dirty is False:
                bpy.ops.wm.read_homefile()
            else:
                self.report({'INFO'}, "Reload Start-Up file to restore settings")

            return {'FINISHED'}

        return {'CANCELLED'}


class WM_OT_blenderplayer_start(Operator):
    """Launch the blender-player with the current blend-file"""
    bl_idname = "wm.blenderplayer_start"
    bl_label = "Start Game In Player"

    def execute(self, context):
        import os
        import sys
        import subprocess

        gs = context.scene.game_settings

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

        filepath = bpy.data.filepath + '~' if bpy.data.is_saved else os.path.join(bpy.app.tempdir, "game.blend")
        bpy.ops.wm.save_as_mainfile('EXEC_DEFAULT', filepath=filepath, copy=True)

        # start the command line call with the player path
        args = [player_path]

        # handle some UI options as command line arguments
        args.extend([
            "-g", "show_framerate", "=", "%d" % gs.show_framerate_profile,
            "-g", "show_profile", "=", "%d" % gs.show_framerate_profile,
            "-g", "show_properties", "=", "%d" % gs.show_debug_properties,
            "-g", "ignore_deprecation_warnings", "=", "%d" % (not gs.use_deprecation_warnings),
            ])

        # finish the call with the path to the blend file
        args.append(filepath)

        subprocess.call(args)
        os.remove(filepath)
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
        if bpy.utils.keyconfig_set(path, report=self.report):
            return {'FINISHED'}
        else:
            return {'CANCELLED'}

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

        if not self.filepath.endswith(".py"):
            self.filepath += ".py"

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

        err_str = ""

        def err_cb():
            import traceback
            nonlocal err_str
            err_str = traceback.format_exc()
            print(err_str)

        mod = addon_utils.enable(self.module, default_set=True, handle_error=err_cb)

        if mod:
            info = addon_utils.module_bl_info(mod)

            info_ver = info.get("blender", (0, 0, 0))

            if info_ver > bpy.app.version:
                self.report({'WARNING'},
                            ("This script was written Blender "
                             "version %d.%d.%d and might not "
                             "function (correctly), "
                             "though it is enabled" %
                             info_ver))
            return {'FINISHED'}
        else:

            if err_str:
                self.report({'ERROR'}, err_str)

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

        err_str = ""

        def err_cb():
            import traceback
            nonlocal err_str
            err_str = traceback.format_exc()
            print(err_str)

        addon_utils.disable(self.module, handle_error=err_cb)

        if err_str:
            self.report({'ERROR'}, err_str)

        return {'FINISHED'}


class WM_OT_theme_install(Operator):
    "Load and apply a Blender XML theme file"
    bl_idname = "wm.theme_install"
    bl_label = "Install Theme..."

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

        path_themes = bpy.utils.user_resource('SCRIPTS', "presets/interface_theme", create=True)

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
            bpy.ops.script.execute_preset(filepath=path_dest, menu_idname="USERPREF_MT_interface_theme_presets")

        except:
            traceback.print_exc()
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_refresh(Operator):
    "Scan addon directories for new modules"
    bl_idname = "wm.addon_refresh"
    bl_label = "Refresh"

    def execute(self, context):
        import addon_utils

        addon_utils.modules_refresh()

        return {'FINISHED'}


class WM_OT_addon_install(Operator):
    "Install an addon"
    bl_idname = "wm.addon_install"
    bl_label = "Install from File..."

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
            path_addons = context.user_preferences.filepaths.script_directory
            if path_addons:
                path_addons = os.path.join(path_addons, "addons")

        if not path_addons:
            self.report({'ERROR'}, "Failed to get addons path")
            return {'CANCELLED'}

        if not os.path.isdir(path_addons):
            try:
                os.makedirs(path_addons, exist_ok=True)
            except:
                traceback.print_exc()

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

        addons_old = {mod.__name__ for mod in addon_utils.modules()}

        # check to see if the file is in compressed format (.zip)
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

            # if not compressed file just copy into the addon path
            try:
                shutil.copyfile(pyfile, path_dest)

            except:
                traceback.print_exc()
                return {'CANCELLED'}

        addons_new = {mod.__name__ for mod in addon_utils.modules()} - addons_old
        addons_new.discard("modules")

        # disable any addons we may have enabled previously and removed.
        # this is unlikely but do just in case. bug [#23978]
        for new_addon in addons_new:
            addon_utils.disable(new_addon)

        # possible the zip contains multiple addons, we could disallow this
        # but for now just use the first
        for mod in addon_utils.modules(refresh=False):
            if mod.__name__ in addons_new:
                info = addon_utils.module_bl_info(mod)

                # show the newly installed addon.
                context.window_manager.addon_filter = 'All'
                context.window_manager.addon_search = info["name"]
                break

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # print message
        msg = tip_("Modules Installed from %r into %r (%s)") % (pyfile, path_addons, ", ".join(sorted(addons_new)))
        print(msg)
        self.report({'INFO'}, msg)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_remove(Operator):
    "Delete the addon from the file system"
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

        for mod in addon_utils.modules():
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

        addon_utils.modules_refresh()

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
    bl_options = {'INTERNAL'}

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
