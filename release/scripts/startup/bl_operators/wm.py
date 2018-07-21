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
from bpy.types import (
    Operator,
    OperatorFileListElement
)
from bpy.props import (
    BoolProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    StringProperty,
    CollectionProperty,
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

rna_wrap_prop = BoolProperty(
    name="Wrap",
    description="Wrap back to the first/last values",
    default=False,
)

rna_relative_prop = BoolProperty(
    name="Relative",
    description="Apply relative to the current value (delta)",
    default=False,
)

rna_space_type_prop = EnumProperty(
    name="Type",
    items=tuple(
        (e.identifier, e.name, "", e. value)
        for e in bpy.types.Space.bl_rna.properties["type"].enum_items
    ),
    default='EMPTY',
)


def context_path_validate(context, data_path):
    try:
        value = eval("context.%s" % data_path) if data_path else Ellipsis
    except AttributeError as ex:
        if str(ex).startswith("'NoneType'"):
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


def module_filesystem_remove(path_base, module_name):
    import os
    module_name = os.path.splitext(module_name)[0]
    for f in os.listdir(path_base):
        f_base = os.path.splitext(f)[0]
        if f_base == module_name:
            f_full = os.path.join(path_base, f)

            if os.path.isdir(f_full):
                os.rmdir(f_full)
            else:
                os.remove(f_full)


class BRUSH_OT_active_index_set(Operator):
    """Set active sculpt/paint brush from it's number"""
    bl_idname = "brush.active_index_set"
    bl_label = "Set Brush Number"

    mode: StringProperty(
        name="Mode",
        description="Paint mode to set brush for",
        maxlen=1024,
    )
    index: IntProperty(
        name="Number",
        description="Brush number",
    )

    _attr_dict = {
        "sculpt": "use_paint_sculpt",
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

    data_path: rna_path_prop
    value: BoolProperty(
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

    data_path: rna_path_prop
    value: IntProperty(
        name="Value",
        description="Assign value",
        default=0,
    )
    relative: rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_scale_float(Operator):
    """Scale a float context value"""
    bl_idname = "wm.context_scale_float"
    bl_label = "Context Scale Float"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path_prop
    value: FloatProperty(
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

    data_path: rna_path_prop
    value: FloatProperty(
        name="Value",
        description="Assign value",
        default=1.0,
    )
    always_step: BoolProperty(
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

    data_path: rna_path_prop
    value: FloatProperty(
        name="Value",
        description="Assignment value",
        default=0.0,
    )
    relative: rna_relative_prop

    execute = execute_context_assign


class WM_OT_context_set_string(Operator):  # same as enum
    """Set a context value"""
    bl_idname = "wm.context_set_string"
    bl_label = "Context Set String"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path_prop
    value: StringProperty(
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

    data_path: rna_path_prop
    value: StringProperty(
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

    data_path: rna_path_prop
    value: StringProperty(
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

    data_path: rna_path_prop

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

    data_path: rna_path_prop
    value_1: StringProperty(
        name="Value",
        description="Toggle enum",
        maxlen=1024,
    )
    value_2: StringProperty(
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

    data_path: rna_path_prop
    reverse: rna_reverse_prop
    wrap: rna_wrap_prop

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

        if self.wrap:
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

    data_path: rna_path_prop
    reverse: rna_reverse_prop
    wrap: rna_wrap_prop

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

        # Have the info we need, advance to the next item.
        #
        # When wrap's disabled we may set the value to its self,
        # this is done to ensure update callbacks run.
        if self.reverse:
            if orig_index == 0:
                advance_enum = enums[-1] if self.wrap else enums[0]
            else:
                advance_enum = enums[orig_index - 1]
        else:
            if orig_index == len(enums) - 1:
                advance_enum = enums[0] if self.wrap else enums[-1]
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

    data_path: rna_path_prop
    reverse: rna_reverse_prop

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

    data_path: rna_path_prop

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

    data_path: rna_path_prop

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

    data_path: StringProperty(
        name="Operator",
        description="Operator name (in python as string)",
        maxlen=1024,
    )
    prop_string: StringProperty(
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

    data_path: rna_path_prop
    value: StringProperty(
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

data_path_iter = StringProperty(
    description="The data path relative to the context, must point to an iterable")

data_path_item = StringProperty(
    description="The data path from each iterable to the value (int or float)")


class WM_OT_context_collection_boolean_set(Operator):
    """Set boolean values for a collection of items"""
    bl_idname = "wm.context_collection_boolean_set"
    bl_label = "Context Collection Boolean Set"
    bl_options = {'UNDO', 'REGISTER', 'INTERNAL'}

    data_path_iter: data_path_iter
    data_path_item: data_path_item

    type: EnumProperty(
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
    bl_options = {'GRAB_CURSOR', 'BLOCKING', 'UNDO', 'INTERNAL'}

    data_path_iter: data_path_iter
    data_path_item: data_path_item
    header_text: StringProperty(
        name="Header Text",
        description="Text to display in header during scale",
    )

    input_scale: FloatProperty(
        description="Scale the mouse movement by this value before applying the delta",
        default=0.01,
    )
    invert: BoolProperty(
        description="Invert the mouse input",
        default=False,
    )
    initial_x: IntProperty(options={'HIDDEN'})

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
    """Open a website in the web-browser"""
    bl_idname = "wm.url_open"
    bl_label = ""
    bl_options = {'INTERNAL'}

    url: StringProperty(
        name="URL",
        description="URL to open",
    )

    def execute(self, context):
        import webbrowser
        webbrowser.open(self.url)
        return {'FINISHED'}


class WM_OT_path_open(Operator):
    """Open a path in a file browser"""
    bl_idname = "wm.path_open"
    bl_label = ""
    bl_options = {'INTERNAL'}

    filepath: StringProperty(
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

    def operator_exists_pair(a, b):
        # Not fast, this is only for docs.
        return b in dir(getattr(bpy.ops, a))

    def operator_exists_single(a):
        a, b = a.partition("_OT_")[::2]
        return operator_exists_pair(a.lower(), b)

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
        if operator_exists_pair(class_name, class_prop):
            if do_url:
                url = (
                    "%s/bpy.ops.%s.html#bpy.ops.%s.%s" %
                    (url_prefix, class_name, class_name, class_prop)
                )
            else:
                rna = "bpy.ops.%s.%s" % (class_name, class_prop)
        elif operator_exists_single(class_name):
            # note: ignore the prop name since we don't have a way to link into it
            class_name, class_prop = class_name.split("_OT_", 1)
            class_name = class_name.lower()
            if do_url:
                url = (
                    "%s/bpy.ops.%s.html#bpy.ops.%s.%s" %
                    (url_prefix, class_name, class_name, class_prop)
                )
            else:
                rna = "bpy.ops.%s.%s" % (class_name, class_prop)
        else:
            # an RNA setting, common case
            rna_class = getattr(bpy.types, class_name)

            # detect if this is a inherited member and use that name instead
            rna_parent = rna_class.bl_rna
            rna_prop = rna_parent.properties.get(class_prop)
            if rna_prop:
                rna_parent = rna_parent.base
                while rna_parent and rna_prop == rna_parent.properties.get(class_prop):
                    class_name = rna_parent.identifier
                    rna_parent = rna_parent.base

                if do_url:
                    url = (
                        "%s/bpy.types.%s.html#bpy.types.%s.%s" %
                        (url_prefix, class_name, class_name, class_prop)
                    )
                else:
                    rna = "bpy.types.%s.%s" % (class_name, class_prop)
            else:
                # We assume this is custom property, only try to generate generic url/rna_id...
                if do_url:
                    url = ("%s/bpy.types.bpy_struct.html#bpy.types.bpy_struct.items" % (url_prefix,))
                else:
                    rna = "bpy.types.bpy_struct"

    return url if do_url else rna


class WM_OT_doc_view_manual(Operator):
    """Load online manual"""
    bl_idname = "wm.doc_view_manual"
    bl_label = "View Manual"

    doc_id: doc_id

    @staticmethod
    def _find_reference(rna_id, url_mapping, verbose=True):
        if verbose:
            print("online manual check for: '%s'... " % rna_id)
        from fnmatch import fnmatchcase
        # XXX, for some reason all RNA ID's are stored lowercase
        # Adding case into all ID's isn't worth the hassle so force lowercase.
        rna_id = rna_id.lower()
        for pattern, url_suffix in url_mapping:
            if fnmatchcase(rna_id, pattern):
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
            self.report(
                {'WARNING'},
                "No reference available %r, "
                "Update info in 'rna_manual_reference.py' "
                "or callback to bpy.utils.manual_map()" %
                self.doc_id
            )
            return {'CANCELLED'}
        else:
            import webbrowser
            webbrowser.open(url)
            return {'FINISHED'}


class WM_OT_doc_view(Operator):
    """Load online reference docs"""
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id: doc_id
    if bpy.app.version_cycle == "release":
        _prefix = ("https://docs.blender.org/api/blender_python_api_current")
    else:
        _prefix = ("https://docs.blender.org/api/blender_python_api_master")

    def execute(self, context):
        url = _wm_doc_get_id(self.doc_id, do_url=True, url_prefix=self._prefix)
        if url is None:
            return {'PASS_THROUGH'}

        import webbrowser
        webbrowser.open(url)

        return {'FINISHED'}


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

rna_use_soft_limits = BoolProperty(
    name="Use Soft Limits",
)

rna_is_overridable_static = BoolProperty(
    name="Is Statically Overridable",
    default=False,
)


class WM_OT_properties_edit(Operator):
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property"
    # register only because invoke_props_popup requires.
    bl_options = {'REGISTER', 'INTERNAL'}

    data_path: rna_path
    property: rna_property
    value: rna_value
    min: rna_min
    max: rna_max
    use_soft_limits: rna_use_soft_limits
    is_overridable_static: rna_is_overridable_static
    soft_min: rna_min
    soft_max: rna_max
    description: StringProperty(
        name="Tooltip",
    )

    def _cmp_props_get(self):
        # Changing these properties will refresh the UI
        return {
            "use_soft_limits": self.use_soft_limits,
            "soft_range": (self.soft_min, self.soft_max),
            "hard_range": (self.min, self.max),
        }

    def execute(self, context):
        from rna_prop_ui import (
            rna_idprop_ui_prop_get,
            rna_idprop_ui_prop_clear,
            rna_idprop_ui_prop_update,
        )

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

        exec_str = "item.property_overridable_static_set('[\"%s\"]', %s)" % (prop, self.is_overridable_static)
        exec(exec_str)

        rna_idprop_ui_prop_update(item, prop)

        self._last_prop[:] = [prop]

        prop_type = type(item[prop])

        prop_ui = rna_idprop_ui_prop_get(item, prop)

        if prop_type in {float, int}:
            prop_ui["min"] = prop_type(self.min)
            prop_ui["max"] = prop_type(self.max)

            if self.use_soft_limits:
                prop_ui["soft_min"] = prop_type(self.soft_min)
                prop_ui["soft_max"] = prop_type(self.soft_max)
            else:
                prop_ui["soft_min"] = prop_type(self.min)
                prop_ui["soft_max"] = prop_type(self.max)

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

            self.soft_min = prop_ui.get("soft_min", self.min)
            self.soft_max = prop_ui.get("soft_max", self.max)
            self.use_soft_limits = (
                self.min != self.soft_min or
                self.max != self.soft_max
            )

        # store for comparison
        self._cmp_props = self._cmp_props_get()

        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def check(self, context):
        cmp_props = self._cmp_props_get()
        changed = False
        if self._cmp_props != cmp_props:
            if cmp_props["use_soft_limits"]:
                if cmp_props["soft_range"] != self._cmp_props["soft_range"]:
                    self.min = min(self.min, self.soft_min)
                    self.max = max(self.max, self.soft_max)
                    changed = True
                if cmp_props["hard_range"] != self._cmp_props["hard_range"]:
                    self.soft_min = max(self.min, self.soft_min)
                    self.soft_max = min(self.max, self.soft_max)
                    changed = True
            else:
                if cmp_props["soft_range"] != cmp_props["hard_range"]:
                    self.soft_min = self.min
                    self.soft_max = self.max
                    changed = True

            changed |= (cmp_props["use_soft_limits"] != self._cmp_props["use_soft_limits"])

            if changed:
                cmp_props = self._cmp_props_get()

            self._cmp_props = cmp_props

        return changed

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "property")
        layout.prop(self, "value")
        row = layout.row(align=True)
        row.prop(self, "min")
        row.prop(self, "max")

        row = layout.row()
        row.prop(self, "use_soft_limits")
        row.prop(self, "is_overridable_static")

        row = layout.row(align=True)
        row.enabled = self.use_soft_limits
        row.prop(self, "soft_min", text="Soft Min")
        row.prop(self, "soft_max", text="Soft Max")
        layout.prop(self, "description")


class WM_OT_properties_add(Operator):
    bl_idname = "wm.properties_add"
    bl_label = "Add Property"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path

    def execute(self, context):
        from rna_prop_ui import (
            rna_idprop_ui_prop_get,
            rna_idprop_ui_prop_update,
        )

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

        prop = unique_name({
            *item.keys(),
            *type(item).bl_rna.properties.keys(),
        })

        item[prop] = 1.0
        rna_idprop_ui_prop_update(item, prop)

        # not essential, but without this we get [#31661]
        prop_ui = rna_idprop_ui_prop_get(item, prop)
        prop_ui["soft_min"] = prop_ui["min"] = 0.0
        prop_ui["soft_max"] = prop_ui["max"] = 1.0

        return {'FINISHED'}


class WM_OT_properties_context_change(Operator):
    """Jump to a different tab inside the properties editor"""
    bl_idname = "wm.properties_context_change"
    bl_label = ""
    bl_options = {'INTERNAL'}

    context: StringProperty(
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

    data_path: rna_path
    property: rna_property

    def execute(self, context):
        from rna_prop_ui import (
            rna_idprop_ui_prop_clear,
            rna_idprop_ui_prop_update,
        )
        data_path = self.data_path
        item = eval("context.%s" % data_path)
        prop = self.property
        rna_idprop_ui_prop_update(item, prop)
        del item[prop]
        rna_idprop_ui_prop_clear(item, prop)

        return {'FINISHED'}


class WM_OT_keyconfig_activate(Operator):
    bl_idname = "wm.keyconfig_activate"
    bl_label = "Activate Keyconfig"

    filepath: StringProperty(
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
            bpy.ops.script.execute_preset(
                filepath=filepath,
                menu_idname="USERPREF_MT_interaction_presets",
            )

        return {'FINISHED'}


class WM_OT_appconfig_activate(Operator):
    bl_idname = "wm.appconfig_activate"
    bl_label = "Activate Application Configuration"

    filepath: StringProperty(
        subtype='FILE_PATH',
    )

    def execute(self, context):
        import os
        bpy.utils.keyconfig_set(self.filepath)

        filepath = self.filepath.replace("keyconfig", "interaction")

        if os.path.exists(filepath):
            bpy.ops.script.execute_preset(
                filepath=filepath,
                menu_idname="USERPREF_MT_interaction_presets",
            )

        return {'FINISHED'}


class WM_OT_sysinfo(Operator):
    """Generate system information, saved into a text file"""

    bl_idname = "wm.sysinfo"
    bl_label = "Save System Info"

    filepath: StringProperty(
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )

    def execute(self, context):
        import sys_info
        sys_info.write_sysinfo(self.filepath)
        return {'FINISHED'}

    def invoke(self, context, event):
        import os

        if not self.filepath:
            self.filepath = os.path.join(
                os.path.expanduser("~"), "system-info.txt")

        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


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
            self.report({'ERROR'}, "Source path %r does not exist" % path_src)
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


class WM_OT_keyconfig_test(Operator):
    """Test key-config for conflicts"""
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
    """Import key configuration from a python script"""
    bl_idname = "wm.keyconfig_import"
    bl_label = "Import Key Configuration..."

    filepath: StringProperty(
        subtype='FILE_PATH',
        default="keymap.py",
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_text: BoolProperty(
        name="Filter text",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter python",
        default=True,
        options={'HIDDEN'},
    )
    keep_original: BoolProperty(
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
        except Exception as ex:
            self.report({'ERROR'}, "Installing keymap failed: %s" % ex)
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
    """Export key configuration to a python script"""
    bl_idname = "wm.keyconfig_export"
    bl_label = "Export Key Configuration..."

    all: BoolProperty(
        name="All Keymaps",
        default=False,
        description="Write all keymaps (not just user modified)",
    )
    filepath: StringProperty(
        subtype='FILE_PATH',
        default="keymap.py",
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_text: BoolProperty(
        name="Filter text",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
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

        keyconfig_utils.keyconfig_export_as_data(
            wm,
            wm.keyconfigs.active,
            self.filepath,
            all_keymaps=self.all,
        )

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_keymap_restore(Operator):
    """Restore key map(s)"""
    bl_idname = "wm.keymap_restore"
    bl_label = "Restore Key Map(s)"

    all: BoolProperty(
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
    """Restore key map item"""
    bl_idname = "wm.keyitem_restore"
    bl_label = "Restore Key Map Item"

    item_id: IntProperty(
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
    """Add key map item"""
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
    """Remove key map item"""
    bl_idname = "wm.keyitem_remove"
    bl_label = "Remove Key Map Item"

    item_id: IntProperty(
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
    """Remove key config"""
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
    """List all the Operators in a text-block, useful for scripting"""
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
# Add-on Operators

class WM_OT_addon_enable(Operator):
    """Enable an add-on"""
    bl_idname = "wm.addon_enable"
    bl_label = "Enable Add-on"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to enable",
    )

    def execute(self, context):
        import addon_utils

        err_str = ""

        def err_cb(ex):
            import traceback
            nonlocal err_str
            err_str = traceback.format_exc()
            print(err_str)

        mod = addon_utils.enable(self.module, default_set=True, handle_error=err_cb)

        if mod:
            info = addon_utils.module_bl_info(mod)

            info_ver = info.get("blender", (0, 0, 0))

            if info_ver > bpy.app.version:
                self.report(
                    {'WARNING'},
                    "This script was written Blender "
                    "version %d.%d.%d and might not "
                    "function (correctly), "
                    "though it is enabled" %
                    info_ver
                )
            return {'FINISHED'}
        else:

            if err_str:
                self.report({'ERROR'}, err_str)

            return {'CANCELLED'}


class WM_OT_addon_disable(Operator):
    """Disable an add-on"""
    bl_idname = "wm.addon_disable"
    bl_label = "Disable Add-on"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to disable",
    )

    def execute(self, context):
        import addon_utils

        err_str = ""

        def err_cb(ex):
            import traceback
            nonlocal err_str
            err_str = traceback.format_exc()
            print(err_str)

        addon_utils.disable(self.module, default_set=True, handle_error=err_cb)

        if err_str:
            self.report({'ERROR'}, err_str)

        return {'FINISHED'}


class WM_OT_owner_enable(Operator):
    """Enable workspace owner ID"""
    bl_idname = "wm.owner_enable"
    bl_label = "Enable Add-on"

    owner_id: StringProperty(
        name="UI Tag",
    )

    def execute(self, context):
        workspace = context.workspace
        workspace.owner_ids.new(self.owner_id)
        return {'FINISHED'}


class WM_OT_owner_disable(Operator):
    """Enable workspace owner ID"""
    bl_idname = "wm.owner_disable"
    bl_label = "Disable UI Tag"

    owner_id: StringProperty(
        name="UI Tag",
    )

    def execute(self, context):
        workspace = context.workspace
        owner_id = workspace.owner_ids[self.owner_id]
        workspace.owner_ids.remove(owner_id)
        return {'FINISHED'}


class WM_OT_theme_install(Operator):
    """Load and apply a Blender XML theme file"""
    bl_idname = "wm.theme_install"
    bl_label = "Install Theme..."

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing theme file if exists",
        default=True,
    )
    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
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
            bpy.ops.script.execute_preset(
                filepath=path_dest,
                menu_idname="USERPREF_MT_interface_theme_presets",
            )

        except:
            traceback.print_exc()
            return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_refresh(Operator):
    """Scan add-on directories for new modules"""
    bl_idname = "wm.addon_refresh"
    bl_label = "Refresh"

    def execute(self, context):
        import addon_utils

        addon_utils.modules_refresh()

        return {'FINISHED'}


# Note: shares some logic with WM_OT_app_template_install
# but not enough to de-duplicate. Fixed here may apply to both.
class WM_OT_addon_install(Operator):
    """Install an add-on"""
    bl_idname = "wm.addon_install"
    bl_label = "Install Add-on from File..."

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing add-ons with the same ID",
        default=True,
    )
    target: EnumProperty(
        name="Target Path",
        items=(('DEFAULT', "Default", ""),
               ('PREFS', "User Prefs", "")),
    )

    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_python: BoolProperty(
        name="Filter python",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.py;*.zip",
        options={'HIDDEN'},
    )

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
            self.report({'ERROR'}, "Failed to get add-ons path")
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
                self.report({'ERROR'}, "Source file is in the add-on search path: %r" % addon_path)
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
                    module_filesystem_remove(path_addons, f)
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
                module_filesystem_remove(path_addons, os.path.basename(pyfile))
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
            addon_utils.disable(new_addon, default_set=True)

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
        msg = (
            tip_("Modules Installed (%s) from %r into %r") %
            (", ".join(sorted(addons_new)), pyfile, path_addons)
        )
        print(msg)
        self.report({'INFO'}, msg)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_addon_remove(Operator):
    """Delete the add-on from the file system"""
    bl_idname = "wm.addon_remove"
    bl_label = "Remove Add-on"

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to remove",
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
            self.report({'WARNING'}, "Add-on path %r could not be found" % path)
            return {'CANCELLED'}

        # in case its enabled
        addon_utils.disable(self.module, default_set=True)

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
        self.layout.label(text="Remove Add-on: %r?" % self.module)
        path, isdir = WM_OT_addon_remove.path_from_addon(self.module)
        self.layout.label(text="Path: %r" % path)

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=600)


class WM_OT_addon_expand(Operator):
    """Display information and preferences for this add-on"""
    bl_idname = "wm.addon_expand"
    bl_label = ""
    bl_options = {'INTERNAL'}

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to expand",
    )

    def execute(self, context):
        import addon_utils

        module_name = self.module

        mod = addon_utils.addons_fake_modules.get(module_name)
        if mod is not None:
            info = addon_utils.module_bl_info(mod)
            info["show_expanded"] = not info["show_expanded"]

        return {'FINISHED'}


class WM_OT_addon_userpref_show(Operator):
    """Show add-on user preferences"""
    bl_idname = "wm.addon_userpref_show"
    bl_label = ""
    bl_options = {'INTERNAL'}

    module: StringProperty(
        name="Module",
        description="Module name of the add-on to expand",
    )

    def execute(self, context):
        import addon_utils

        module_name = self.module

        modules = addon_utils.modules(refresh=False)
        mod = addon_utils.addons_fake_modules.get(module_name)
        if mod is not None:
            info = addon_utils.module_bl_info(mod)
            info["show_expanded"] = True

            context.user_preferences.active_section = 'ADDONS'
            context.window_manager.addon_filter = 'All'
            context.window_manager.addon_search = info["name"]
            bpy.ops.screen.userpref_show('INVOKE_DEFAULT')

        return {'FINISHED'}


# Note: shares some logic with WM_OT_addon_install
# but not enough to de-duplicate. Fixes here may apply to both.
class WM_OT_app_template_install(Operator):
    """Install an application-template"""
    bl_idname = "wm.app_template_install"
    bl_label = "Install Template from File..."

    overwrite: BoolProperty(
        name="Overwrite",
        description="Remove existing template with the same ID",
        default=True,
    )

    filepath: StringProperty(
        subtype='FILE_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.zip",
        options={'HIDDEN'},
    )

    def execute(self, context):
        import traceback
        import zipfile
        import shutil
        import os

        filepath = self.filepath

        path_app_templates = bpy.utils.user_resource(
            'SCRIPTS', os.path.join("startup", "bl_app_templates_user"),
            create=True,
        )

        if not path_app_templates:
            self.report({'ERROR'}, "Failed to get add-ons path")
            return {'CANCELLED'}

        if not os.path.isdir(path_app_templates):
            try:
                os.makedirs(path_app_templates, exist_ok=True)
            except:
                traceback.print_exc()

        app_templates_old = set(os.listdir(path_app_templates))

        # check to see if the file is in compressed format (.zip)
        if zipfile.is_zipfile(filepath):
            try:
                file_to_extract = zipfile.ZipFile(filepath, 'r')
            except:
                traceback.print_exc()
                return {'CANCELLED'}

            if self.overwrite:
                for f in file_to_extract.namelist():
                    module_filesystem_remove(path_app_templates, f)
            else:
                for f in file_to_extract.namelist():
                    path_dest = os.path.join(path_app_templates, os.path.basename(f))
                    if os.path.exists(path_dest):
                        self.report({'WARNING'}, "File already installed to %r\n" % path_dest)
                        return {'CANCELLED'}

            try:  # extract the file to "bl_app_templates_user"
                file_to_extract.extractall(path_app_templates)
            except:
                traceback.print_exc()
                return {'CANCELLED'}

        else:
            # Only support installing zipfiles
            self.report({'WARNING'}, "Expected a zip-file %r\n" % filepath)
            return {'CANCELLED'}

        app_templates_new = set(os.listdir(path_app_templates)) - app_templates_old

        # in case a new module path was created to install this addon.
        bpy.utils.refresh_script_paths()

        # print message
        msg = (
            tip_("Template Installed (%s) from %r into %r") %
            (", ".join(sorted(app_templates_new)), filepath, path_app_templates)
        )
        print(msg)
        self.report({'INFO'}, msg)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_tool_set_by_name(Operator):
    """Set the tool by name (for keymaps)"""
    bl_idname = "wm.tool_set_by_name"
    bl_label = "Set Tool By Name"

    name: StringProperty(
        name="Text",
        description="Display name of the tool",
    )
    cycle: BoolProperty(
        name="Cycle",
        description="Cycle through tools in this group",
        default=False,
        options={'SKIP_SAVE'},
    )

    space_type: rna_space_type_prop

    def execute(self, context):
        from bl_ui.space_toolsystem_common import (
            activate_by_name,
            activate_by_name_or_cycle,
        )

        if self.properties.is_property_set("space_type"):
            space_type = self.space_type
        else:
            space_type = context.space_data.type

        fn = activate_by_name_or_cycle if self.cycle else activate_by_name
        if fn(context, space_type, self.name):
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, f"Tool {self.name!r} not found.")
            return {'CANCELLED'}


class WM_OT_toolbar(Operator):
    bl_idname = "wm.toolbar"
    bl_label = "Toolbar"

    def execute(self, context):
        from bl_ui.space_toolsystem_common import (
            ToolSelectPanelHelper,
            keymap_from_context,
        )
        space_type = context.space_data.type

        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        if cls is None:
            self.report({'WARNING'}, f"Toolbar not found for {space_type!r}")
            return {'CANCELLED'}

        wm = context.window_manager
        keymap = keymap_from_context(context, space_type)

        def draw_menu(popover, context):
            layout = popover.layout

            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("wm.search_menu", text="Search Commands...", icon='VIEWZOOM')

            cls.draw_cls(layout, context, detect_layout=False, scale_y=1.0)

        wm.popover(draw_menu, ui_units_x=8, keymap=keymap)
        return {'FINISHED'}


# Studio Light operations
class WM_OT_studiolight_install(Operator):
    """Install a user defined studio light"""
    bl_idname = "wm.studiolight_install"
    bl_label = "Install Custom Studio Light"

    files: CollectionProperty(
        name="File Path",
        type=OperatorFileListElement,
    )
    directory: StringProperty(
        subtype='DIR_PATH',
    )
    filter_folder: BoolProperty(
        name="Filter folders",
        default=True,
        options={'HIDDEN'},
    )
    filter_glob: StringProperty(
        default="*.png;*.jpg;*.hdr;*.exr",
        options={'HIDDEN'},
    )
    orientation: EnumProperty(
        items=(
            ('MATCAP', "MatCap", ""),
            ('WORLD', "World", ""),
            ('CAMERA', "Camera", ""),
        )
    )

    def execute(self, context):
        import traceback
        import shutil
        import pathlib
        userpref = context.user_preferences

        filepaths = [pathlib.Path(self.directory, e.name) for e in self.files]
        path_studiolights = bpy.utils.user_resource('DATAFILES')

        if not path_studiolights:
            self.report({'ERROR'}, "Failed to get Studio Light path")
            return {'CANCELLED'}

        path_studiolights = pathlib.Path(path_studiolights, "studiolights", self.orientation.lower())
        if not path_studiolights.exists():
            try:
                path_studiolights.mkdir(parents=True, exist_ok=True)
            except:
                traceback.print_exc()

        for filepath in filepaths:
            shutil.copy(str(filepath), str(path_studiolights))
            userpref.studio_lights.new(str(path_studiolights.joinpath(filepath.name)), self.orientation)

        # print message
        msg = (
            tip_("StudioLight Installed %r into %r") %
            (", ".join(str(x.name) for x in self.files), str(path_studiolights))
        )
        print(msg)
        self.report({'INFO'}, msg)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_studiolight_uninstall(Operator):
    bl_idname = 'wm.studiolight_uninstall'
    bl_label = "Uninstall Studio Light"
    index: bpy.props.IntProperty()

    def _remove_path(self, path):
        if path.exists():
            path.unlink()

    def execute(self, context):
        import pathlib
        userpref = context.user_preferences
        for studio_light in userpref.studio_lights:
            if studio_light.index == self.index:
                if len(studio_light.path) > 0:
                    self._remove_path(pathlib.Path(studio_light.path))
                if len(studio_light.path_irr_cache) > 0:
                    self._remove_path(pathlib.Path(studio_light.path_irr_cache))
                if len(studio_light.path_sh_cache) > 0:
                    self._remove_path(pathlib.Path(studio_light.path_sh_cache))
                userpref.studio_lights.remove(studio_light)
                return {'FINISHED'}
        return {'CANCELLED'}


class WM_OT_studiolight_userpref_show(Operator):
    """Show light user preferences"""
    bl_idname = "wm.studiolight_userpref_show"
    bl_label = ""
    bl_options = {'INTERNAL'}

    def execute(self, context):
        context.user_preferences.active_section = 'LIGHTS'
        bpy.ops.screen.userpref_show('INVOKE_DEFAULT')
        return {'FINISHED'}


classes = (
    BRUSH_OT_active_index_set,
    WM_OT_addon_disable,
    WM_OT_addon_enable,
    WM_OT_addon_expand,
    WM_OT_addon_install,
    WM_OT_addon_refresh,
    WM_OT_addon_remove,
    WM_OT_addon_userpref_show,
    WM_OT_app_template_install,
    WM_OT_appconfig_activate,
    WM_OT_appconfig_default,
    WM_OT_context_collection_boolean_set,
    WM_OT_context_cycle_array,
    WM_OT_context_cycle_enum,
    WM_OT_context_cycle_int,
    WM_OT_context_menu_enum,
    WM_OT_context_modal_mouse,
    WM_OT_context_pie_enum,
    WM_OT_context_scale_float,
    WM_OT_context_scale_int,
    WM_OT_context_set_boolean,
    WM_OT_context_set_enum,
    WM_OT_context_set_float,
    WM_OT_context_set_id,
    WM_OT_context_set_int,
    WM_OT_context_set_string,
    WM_OT_context_set_value,
    WM_OT_context_toggle,
    WM_OT_context_toggle_enum,
    WM_OT_copy_prev_settings,
    WM_OT_doc_view,
    WM_OT_doc_view_manual,
    WM_OT_keyconfig_activate,
    WM_OT_keyconfig_export,
    WM_OT_keyconfig_import,
    WM_OT_keyconfig_remove,
    WM_OT_keyconfig_test,
    WM_OT_keyitem_add,
    WM_OT_keyitem_remove,
    WM_OT_keyitem_restore,
    WM_OT_keymap_restore,
    WM_OT_operator_cheat_sheet,
    WM_OT_operator_pie_enum,
    WM_OT_path_open,
    WM_OT_properties_add,
    WM_OT_properties_context_change,
    WM_OT_properties_edit,
    WM_OT_properties_remove,
    WM_OT_sysinfo,
    WM_OT_theme_install,
    WM_OT_owner_disable,
    WM_OT_owner_enable,
    WM_OT_url_open,
    WM_OT_studiolight_install,
    WM_OT_studiolight_uninstall,
    WM_OT_studiolight_userpref_show,
    WM_OT_tool_set_by_name,
    WM_OT_toolbar,
)
