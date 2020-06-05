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
    Menu,
    Operator,
)
from bpy.props import (
    BoolProperty,
    CollectionProperty,
    EnumProperty,
    FloatProperty,
    IntProperty,
    StringProperty,
)

# FIXME, we need a way to detect key repeat events.
# unfortunately checking event previous values isn't reliable.
use_toolbar_release_hack = True


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

# Note, this can be used for more operators,
# currently not used for all "WM_OT_context_" operators.
rna_module_prop = StringProperty(
    name="Module",
    description="Optionally override the context with a module",
    maxlen=1024,
)


def context_path_validate(context, data_path):
    try:
        value = eval("context.%s" % data_path) if data_path else Ellipsis
    except AttributeError as ex:
        if str(ex).startswith("'NoneType'"):
            # One of the items in the rna path is None, just ignore this
            value = Ellipsis
        else:
            # Print invalid path, but don't show error to the users and fully
            # break the UI if the operator is bound to an event like left click.
            print("context_path_validate error: context.%s not found (invalid keymap entry?)" % data_path)
            value = Ellipsis

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
    # When we can't find the data owner assume no undo is needed.
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
    module: rna_module_prop

    def execute(self, context):
        data_path = self.data_path

        module = self.module
        if not module:
            base = context
        else:
            from importlib import import_module
            base = import_module(self.module)

        if context_path_validate(base, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        exec("base.%s = not (base.%s)" % (data_path, data_path))

        return operator_path_undo_return(base, data_path)


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
            op_rna = op.get_rna_type()
        except KeyError:
            self.report({'ERROR'}, "Operator not found: bpy.ops.%s" % data_path)
            return {'CANCELLED'}

        def draw_cb(self, context):
            layout = self.layout
            pie = layout.menu_pie()
            pie.operator_enum(data_path, prop_string)

        wm.popup_menu_pie(draw_func=draw_cb, title=op_rna.name, event=event)

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
        items=(
            ('TOGGLE', "Toggle", ""),
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
            context.area.header_text_set(None)
            return operator_value_undo_return(item)

        elif event_type in {'RIGHTMOUSE', 'ESC'}:
            self._values_restore()
            context.area.header_text_set(None)
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

    def execute(self, _context):
        import webbrowser
        webbrowser.open(self.url)
        return {'FINISHED'}


class WM_OT_url_open_preset(Operator):
    """Open a preset website in the web-browser"""
    bl_idname = "wm.url_open_preset"
    bl_label = "Open Preset Website"
    bl_options = {'INTERNAL'}

    type: EnumProperty(
        name="Site",
        items=lambda self, _context: (
            item for (item, _) in WM_OT_url_open_preset.preset_items
        ),
    )

    id: StringProperty(
        name="Identifier",
        description="Optional identifier",
    )

    def _url_from_bug(self, _context):
        from bl_ui_utils.bug_report_url import url_prefill_from_blender
        return url_prefill_from_blender()

    def _url_from_bug_addon(self, _context):
        from bl_ui_utils.bug_report_url import url_prefill_from_blender
        return url_prefill_from_blender(addon_info=self.id)

    def _url_from_release_notes(self, _context):
        return "https://www.blender.org/download/releases/%d-%d/" % bpy.app.version[:2]

    def _url_from_manual(self, _context):
        if bpy.app.version_cycle in {"rc", "release"}:
            manual_version = "%d.%d" % bpy.app.version[:2]
        else:
            manual_version = "dev"
        return "https://docs.blender.org/manual/en/" + manual_version + "/"

    # This list is: (enum_item, url) pairs.
    # Allow dynamically extending.
    preset_items = [
        # Dynamic URL's.
        (('BUG', "Bug",
          "Report a bug with pre-filled version information"),
         _url_from_bug),
        (('BUG_ADDON', "Add-On Bug",
          "Report a bug in an add-on"),
         _url_from_bug_addon),
        (('RELEASE_NOTES', "Release Notes",
          "Read about whats new in this version of Blender"),
         _url_from_release_notes),
        (('MANUAL', "Manual",
          "The reference manual for this version of Blender"),
         _url_from_manual),

        # Static URL's.
        (('FUND', "Development Fund",
          "The donation program to support maintenance and improvements"),
         "https://fund.blender.org"),
        (('BLENDER', "blender.org",
          "Blender's official web-site"),
         "https://www.blender.org"),
        (('CREDITS', "Credits",
          "Lists committers to Blender's source code"),
         "https://www.blender.org/about/credits/"),
    ]

    def execute(self, context):
        url = None
        type = self.type
        for (item_id, _, _), url in self.preset_items:
            if item_id == type:
                if callable(url):
                    url = url(self, context)
                break

        import webbrowser
        webbrowser.open(url)

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

    def execute(self, _context):
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
            # An RNA setting, common case.

            # Check the built-in RNA types.
            rna_class = getattr(bpy.types, class_name, None)
            if rna_class is None:
                # Check class for dynamically registered types.
                rna_class = bpy.types.PropertyGroup.bl_rna_get_subclass_py(class_name)

            # Detect if this is a inherited member and use that name instead.
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

    def execute(self, _context):
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
    """Open online reference docs in a web browser"""
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id: doc_id
    if bpy.app.version_cycle in {"release", "rc", "beta"}:
        _prefix = ("https://docs.blender.org/api/%d.%d" %
                   (bpy.app.version[0], bpy.app.version[1]))
    else:
        _prefix = ("https://docs.blender.org/api/master")

    def execute(self, _context):
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

rna_default = StringProperty(
    name="Default Value",
    description="Default value of the property. Important for NLA mixing",
    maxlen=1024,
)

rna_property = StringProperty(
    name="Property Name",
    description="Property name edit",
    maxlen=1024,
)

rna_min = FloatProperty(
    name="Min",
    description="Minimum value of the property",
    default=-10000.0,
    precision=3,
)

rna_max = FloatProperty(
    name="Max",
    description="Maximum value of the property",
    default=10000.0,
    precision=3,
)

rna_use_soft_limits = BoolProperty(
    name="Use Soft Limits",
    description="Limits the Property Value slider to a range, values outside the range must be inputted numerically",
)

rna_is_overridable_library = BoolProperty(
    name="Is Library Overridable",
    description="Allow the property to be overridden when the Data-Block is linked",
    default=False,
)

# Most useful entries of rna_enum_property_subtype_items for number arrays:
rna_vector_subtype_items = (
    ('NONE', "Plain Data", "Data values without special behavior"),
    ('COLOR', "Linear Color", "Color in the linear space"),
    ('COLOR_GAMMA', "Gamma-Corrected Color", "Color in the gamma corrected space"),
    ('EULER', "Euler Angles", "Euler rotation angles in radians"),
    ('QUATERNION', "Quaternion Rotation", "Quaternion rotation (affects NLA blending)"),
)


class WM_OT_properties_edit(Operator):
    """Edit the attributes of the property"""
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property"
    # register only because invoke_props_popup requires.
    bl_options = {'REGISTER', 'INTERNAL'}

    data_path: rna_path
    property: rna_property
    value: rna_value
    default: rna_default
    min: rna_min
    max: rna_max
    use_soft_limits: rna_use_soft_limits
    is_overridable_library: rna_is_overridable_library
    soft_min: rna_min
    soft_max: rna_max
    description: StringProperty(
        name="Tooltip",
    )
    subtype: EnumProperty(
        name="Subtype",
        items=lambda self, _context: WM_OT_properties_edit.subtype_items,
    )

    subtype_items = rna_vector_subtype_items

    def _init_subtype(self, prop_type, is_array, subtype):
        subtype = subtype or 'NONE'
        subtype_items = rna_vector_subtype_items

        # Add a temporary enum entry to preserve unknown subtypes
        if not any(subtype == item[0] for item in subtype_items):
            subtype_items += ((subtype, subtype, ""),)

        WM_OT_properties_edit.subtype_items = subtype_items
        self.subtype = subtype

    def _cmp_props_get(self):
        # Changing these properties will refresh the UI
        return {
            "use_soft_limits": self.use_soft_limits,
            "soft_range": (self.soft_min, self.soft_max),
            "hard_range": (self.min, self.max),
        }

    def get_value_eval(self):
        try:
            value_eval = eval(self.value)
            # assert else None -> None, not "None", see [#33431]
            assert(type(value_eval) in {str, float, int, bool, tuple, list})
        except:
            value_eval = self.value

        return value_eval

    def get_default_eval(self):
        try:
            default_eval = eval(self.default)
            # assert else None -> None, not "None", see [#33431]
            assert(type(default_eval) in {str, float, int, bool, tuple, list})
        except:
            default_eval = self.default

        return default_eval

    def execute(self, context):
        from rna_prop_ui import (
            rna_idprop_ui_prop_get,
            rna_idprop_ui_prop_clear,
            rna_idprop_ui_prop_update,
            rna_idprop_ui_prop_default_set,
            rna_idprop_value_item_type,
        )

        data_path = self.data_path
        prop = self.property

        prop_old = getattr(self, "_last_prop", [None])[0]

        if prop_old is None:
            self.report({'ERROR'}, "Direct execution not supported")
            return {'CANCELLED'}

        value_eval = self.get_value_eval()
        default_eval = self.get_default_eval()

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

        exec_str = "item.property_overridable_library_set('[\"%s\"]', %s)" % (prop, self.is_overridable_library)
        exec(exec_str)

        rna_idprop_ui_prop_update(item, prop)

        self._last_prop[:] = [prop]

        prop_value = item[prop]
        prop_type_new = type(prop_value)
        prop_type, is_array = rna_idprop_value_item_type(prop_value)

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

        if prop_type == float and is_array and self.subtype != 'NONE':
            prop_ui["subtype"] = self.subtype
        else:
            prop_ui.pop("subtype", None)

        prop_ui["description"] = self.description

        rna_idprop_ui_prop_default_set(item, prop, default_eval)

        # If we have changed the type of the property, update its potential anim curves!
        if prop_type_old != prop_type_new:
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

    def invoke(self, context, _event):
        from rna_prop_ui import (
            rna_idprop_ui_prop_get,
            rna_idprop_value_to_python,
            rna_idprop_value_item_type
        )

        data_path = self.data_path

        if not data_path:
            self.report({'ERROR'}, "Data path not set")
            return {'CANCELLED'}

        self._last_prop = [self.property]

        item = eval("context.%s" % data_path)

        # retrieve overridable static
        exec_str = "item.is_property_overridable_library('[\"%s\"]')" % (self.property)
        self.is_overridable_library = bool(eval(exec_str))

        # default default value
        prop_type, is_array = rna_idprop_value_item_type(self.get_value_eval())
        if prop_type in {int, float}:
            self.default = str(prop_type(0))
        else:
            self.default = ""

        # setup defaults
        prop_ui = rna_idprop_ui_prop_get(item, self.property, False)  # don't create
        if prop_ui:
            self.min = prop_ui.get("min", -1000000000)
            self.max = prop_ui.get("max", 1000000000)
            self.description = prop_ui.get("description", "")

            defval = prop_ui.get("default", None)
            if defval is not None:
                self.default = str(rna_idprop_value_to_python(defval))

            self.soft_min = prop_ui.get("soft_min", self.min)
            self.soft_max = prop_ui.get("soft_max", self.max)
            self.use_soft_limits = (
                self.min != self.soft_min or
                self.max != self.soft_max
            )

            subtype = prop_ui.get("subtype", None)
        else:
            subtype = None

        self._init_subtype(prop_type, is_array, subtype)

        # store for comparison
        self._cmp_props = self._cmp_props_get()

        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def check(self, _context):
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

    def draw(self, _context):
        from rna_prop_ui import (
            rna_idprop_value_item_type,
        )

        layout = self.layout
        layout.prop(self, "property")
        layout.prop(self, "value")

        value = self.get_value_eval()
        proptype, is_array = rna_idprop_value_item_type(value)

        row = layout.row()
        row.enabled = proptype in {int, float}
        row.prop(self, "default")

        row = layout.row(align=True)
        row.prop(self, "min")
        row.prop(self, "max")

        row = layout.row()
        row.prop(self, "use_soft_limits")
        if bpy.app.use_override_library:
            row.prop(self, "is_overridable_library")

        row = layout.row(align=True)
        row.enabled = self.use_soft_limits
        row.prop(self, "soft_min", text="Soft Min")
        row.prop(self, "soft_max", text="Soft Max")
        layout.prop(self, "description")

        if is_array and proptype == float:
            layout.prop(self, "subtype")


class WM_OT_properties_add(Operator):
    """Add your own property to the data-block"""
    bl_idname = "wm.properties_add"
    bl_label = "Add Property"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path

    def execute(self, context):
        from rna_prop_ui import (
            rna_idprop_ui_create,
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

        rna_idprop_ui_create(item, prop, default=1.0)

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


class WM_OT_sysinfo(Operator):
    """Generate system information, saved into a text file"""

    bl_idname = "wm.sysinfo"
    bl_label = "Save System Info"

    filepath: StringProperty(
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )

    def execute(self, _context):
        import sys_info
        sys_info.write_sysinfo(self.filepath)
        return {'FINISHED'}

    def invoke(self, context, _event):
        import os

        if not self.filepath:
            self.filepath = os.path.join(
                os.path.expanduser("~"), "system-info.txt")

        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class WM_OT_operator_cheat_sheet(Operator):
    """List all the Operators in a text-block, useful for scripting"""
    bl_idname = "wm.operator_cheat_sheet"
    bl_label = "Operator Cheat Sheet"

    def execute(self, _context):
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


class WM_OT_tool_set_by_id(Operator):
    """Set the tool by name (for keymaps)"""
    bl_idname = "wm.tool_set_by_id"
    bl_label = "Set Tool By Name"

    name: StringProperty(
        name="Identifier",
        description="Identifier of the tool",
    )
    cycle: BoolProperty(
        name="Cycle",
        description="Cycle through tools in this group",
        default=False,
        options={'SKIP_SAVE'},
    )
    as_fallback: BoolProperty(
        name="Set Fallback",
        description="Set the fallback tool instead of the primary tool",
        default=False,
        options={'SKIP_SAVE', 'HIDDEN'},
    )

    space_type: rna_space_type_prop

    if use_toolbar_release_hack:
        def invoke(self, context, event):
            # Hack :S
            if not self.properties.is_property_set("name"):
                WM_OT_toolbar._key_held = False
                return {'PASS_THROUGH'}
            elif (WM_OT_toolbar._key_held == event.type) and (event.value != 'RELEASE'):
                return {'PASS_THROUGH'}
            WM_OT_toolbar._key_held = None

            return self.execute(context)

    def execute(self, context):
        from bl_ui.space_toolsystem_common import (
            activate_by_id,
            activate_by_id_or_cycle,
        )

        if self.properties.is_property_set("space_type"):
            space_type = self.space_type
        else:
            space_type = context.space_data.type

        fn = activate_by_id_or_cycle if self.cycle else activate_by_id
        if fn(context, space_type, self.name, as_fallback=self.as_fallback):
            if self.as_fallback:
                tool_settings = context.tool_settings
                tool_settings.workspace_tool_type = 'FALLBACK'
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, f"Tool {self.name!r:s} not found for space {space_type!r:s}.")
            return {'CANCELLED'}


class WM_OT_tool_set_by_index(Operator):
    """Set the tool by index (for keymaps)"""
    bl_idname = "wm.tool_set_by_index"
    bl_label = "Set Tool By Index"
    index: IntProperty(
        name="Index in toolbar",
        default=0,
    )
    cycle: BoolProperty(
        name="Cycle",
        description="Cycle through tools in this group",
        default=False,
        options={'SKIP_SAVE'},
    )

    expand: BoolProperty(
        description="Include tool sub-groups",
        default=True,
    )

    as_fallback: BoolProperty(
        name="Set Fallback",
        description="Set the fallback tool instead of the primary",
        default=False,
        options={'SKIP_SAVE', 'HIDDEN'},
    )

    space_type: rna_space_type_prop

    def execute(self, context):
        from bl_ui.space_toolsystem_common import (
            activate_by_id,
            activate_by_id_or_cycle,
            item_from_index_active,
            item_from_flat_index,
        )

        if self.properties.is_property_set("space_type"):
            space_type = self.space_type
        else:
            space_type = context.space_data.type

        fn = item_from_flat_index if self.expand else item_from_index_active
        item = fn(context, space_type, self.index)
        if item is None:
            # Don't report, since the number of tools may change.
            return {'CANCELLED'}

        # Same as: WM_OT_tool_set_by_id
        fn = activate_by_id_or_cycle if self.cycle else activate_by_id
        if fn(context, space_type, item.idname, as_fallback=self.as_fallback):
            if self.as_fallback:
                tool_settings = context.tool_settings
                tool_settings.workspace_tool_type = 'FALLBACK'
            return {'FINISHED'}
        else:
            # Since we already have the tool, this can't happen.
            raise Exception("Internal error setting tool")


class WM_OT_toolbar(Operator):
    bl_idname = "wm.toolbar"
    bl_label = "Toolbar"

    @classmethod
    def poll(cls, context):
        return context.space_data is not None

    if use_toolbar_release_hack:
        _key_held = None

        def invoke(self, context, event):
            WM_OT_toolbar._key_held = event.type
            return self.execute(context)

    @staticmethod
    def keymap_from_toolbar(context, space_type, use_fallback_keys=True, use_reset=True):
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        from bl_keymap_utils import keymap_from_toolbar

        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        if cls is None:
            return None, None

        return cls, keymap_from_toolbar.generate(
            context,
            space_type,
            use_fallback_keys=use_fallback_keys,
            use_reset=use_reset,
        )

    def execute(self, context):
        space_type = context.space_data.type
        cls, keymap = self.keymap_from_toolbar(context, space_type)
        if keymap is None:
            return {'CANCELLED'}

        def draw_menu(popover, context):
            layout = popover.layout
            layout.operator_context = 'INVOKE_REGION_WIN'
            cls.draw_cls(layout, context, detect_layout=False, scale_y=1.0)

        wm = context.window_manager
        wm.popover(draw_menu, ui_units_x=8, keymap=keymap)
        return {'FINISHED'}


class WM_OT_toolbar_fallback_pie(Operator):
    bl_idname = "wm.toolbar_fallback_pie"
    bl_label = "Fallback Tool Pie Menu"

    @classmethod
    def poll(cls, context):
        return context.space_data is not None

    def invoke(self, context, event):
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        space_type = context.space_data.type
        cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        if cls is None:
            return {'PASS_THROUGH'}

        # It's possible we don't have the fallback tool available.
        # This can happen in the image editor for example when there is no selection
        # in painting modes.
        item, _ = cls._tool_get_by_id(context, cls.tool_fallback_id)
        if item is None:
            print("Tool", cls.tool_fallback_id, "not active in", cls)
            return {'PASS_THROUGH'}

        def draw_cb(self, context):
            from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
            ToolSelectPanelHelper.draw_fallback_tool_items_for_pie_menu(self.layout, context)

        wm = context.window_manager
        wm.popup_menu_pie(draw_func=draw_cb, title="Fallback Tool", event=event)
        return {'FINISHED'}


class WM_OT_toolbar_prompt(Operator):
    """Leader key like functionality for accessing tools"""
    bl_idname = "wm.toolbar_prompt"
    bl_label = "Toolbar Prompt"

    @staticmethod
    def _status_items_generate(cls, keymap, context):
        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper

        # The keymap doesn't have the same order the tools are declared in,
        # while we could support this, it's simpler to apply order here.
        tool_map_id_to_order = {}
        # Map the
        tool_map_id_to_label = {}
        for item in ToolSelectPanelHelper._tools_flatten(cls.tools_from_context(context)):
            if item is not None:
                tool_map_id_to_label[item.idname] = item.label
                tool_map_id_to_order[item.idname] = len(tool_map_id_to_order)

        status_items = []

        for item in keymap.keymap_items:
            name = item.name
            key_str = item.to_string()
            # These are duplicated from regular numbers.
            if key_str.startswith("Numpad "):
                continue
            properties = item.properties
            idname = item.idname
            if idname == "wm.tool_set_by_id":
                tool_idname = properties["name"]
                name = tool_map_id_to_label[tool_idname]
                name = name.replace("Annotate ", "")
            else:
                continue

            status_items.append((tool_idname, name, item))

        status_items.sort(
            key=lambda a: tool_map_id_to_order[a[0]]
        )
        return status_items

    def modal(self, context, event):
        event_type = event.type
        event_value = event.value

        if event_type in {
                'LEFTMOUSE', 'RIGHTMOUSE', 'MIDDLEMOUSE',
                'WHEELDOWNMOUSE', 'WHEELUPMOUSE', 'WHEELINMOUSE', 'WHEELOUTMOUSE',
                'ESC',
        }:
            context.workspace.status_text_set(None)
            return {'CANCELLED', 'PASS_THROUGH'}

        keymap = self._keymap
        item = keymap.keymap_items.match_event(event)
        if item is not None:
            idname = item.idname
            properties = item.properties
            if idname == "wm.tool_set_by_id":
                tool_idname = properties["name"]
                bpy.ops.wm.tool_set_by_id(name=tool_idname)

            context.workspace.status_text_set(None)
            return {'FINISHED'}

        # Pressing entry even again exists, as long as it's not mapped to a key (for convenience).
        if event_type == self._init_event_type:
            if event_value == 'RELEASE':
                if not (event.ctrl or event.alt or event.shift or event.oskey):
                    context.workspace.status_text_set(None)
                    return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        space_data = context.space_data
        if space_data is None:
            return {'CANCELLED'}

        space_type = space_data.type
        cls, keymap = WM_OT_toolbar.keymap_from_toolbar(
            context,
            space_type,
            use_fallback_keys=False,
            use_reset=False,
        )
        if (keymap is None) or (not keymap.keymap_items):
            return {'CANCELLED'}

        self._init_event_type = event.type

        # Strip Left/Right, since "Left Alt" isn't especially useful.
        init_event_type_as_text = self._init_event_type.title().split("_")
        if init_event_type_as_text[0] in {"Left", "Right"}:
            del init_event_type_as_text[0]
        init_event_type_as_text = " ".join(init_event_type_as_text)

        status_items = self._status_items_generate(cls, keymap, context)

        def status_text_fn(self, context):

            layout = self.layout
            if True:
                box = layout.row(align=True).box()
                box.scale_x = 0.8
                box.label(text=init_event_type_as_text)

            flow = layout.grid_flow(columns=len(status_items), align=True, row_major=True)
            for _, name, item in status_items:
                row = flow.row(align=True)
                row.template_event_from_keymap_item(item, text=name)

        self._keymap = keymap

        context.workspace.status_text_set(status_text_fn)

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


class BatchRenameAction(bpy.types.PropertyGroup):
    # category: StringProperty()
    type: EnumProperty(
        name="Operation",
        items=(
            ('REPLACE', "Find/Replace", "Replace text in the name"),
            ('SET', "Set Name", "Set a new name or prefix/suffix the existing one"),
            ('STRIP', "Strip Characters", "Strip leading/trailing text from the name"),
            ('CASE', "Change Case", "Change case of each name"),
        ),
    )

    # We could split these into sub-properties, however it's not so important.

    # type: 'SET'.
    set_name: StringProperty(name="Name")
    set_method: EnumProperty(
        name="Method",
        items=(
            ('NEW', "New", ""),
            ('PREFIX', "Prefix", ""),
            ('SUFFIX', "Suffix", ""),
        ),
        default='SUFFIX',
    )

    # type: 'STRIP'.
    strip_chars: EnumProperty(
        name="Strip Characters",
        options={'ENUM_FLAG'},
        items=(
            ('SPACE', "Spaces", ""),
            ('DIGIT', "Digits", ""),
            ('PUNCT', "Punctuation", ""),
        ),
    )

    # type: 'STRIP'.
    strip_part: EnumProperty(
        name="Strip Part",
        options={'ENUM_FLAG'},
        items=(
            ('START', "Start", ""),
            ('END', "End", ""),
        ),
    )

    # type: 'REPLACE'.
    replace_src: StringProperty(name="Find")
    replace_dst: StringProperty(name="Replace")
    replace_match_case: BoolProperty(name="Case Sensitive")
    use_replace_regex_src: BoolProperty(
        name="Regular Expression Find",
        description="Use regular expressions to match text in the 'Find' field"
    )
    use_replace_regex_dst: BoolProperty(
        name="Regular Expression Replace",
        description="Use regular expression for the replacement text (supporting groups)"
    )

    # type: 'CASE'.
    case_method: EnumProperty(
        name="Case",
        items=(
            ('UPPER', "Upper Case", ""),
            ('LOWER', "Lower Case", ""),
            ('TITLE', "Title Case", ""),
        ),
    )

    # Weak, add/remove as properties.
    op_add: BoolProperty()
    op_remove: BoolProperty()


class WM_OT_batch_rename(Operator):
    bl_idname = "wm.batch_rename"
    bl_label = "Batch Rename"

    bl_options = {'UNDO'}

    data_type: EnumProperty(
        name="Type",
        items=(
            ('OBJECT', "Objects", ""),
            ('MATERIAL', "Materials", ""),
            None,
            # Enum identifiers are compared with 'object.type'.
            ('MESH', "Meshes", ""),
            ('CURVE', "Curves", ""),
            ('META', "Meta Balls", ""),
            ('ARMATURE', "Armatures", ""),
            ('LATTICE', "Lattices", ""),
            ('GPENCIL', "Grease Pencils", ""),
            ('CAMERA', "Cameras", ""),
            ('SPEAKER', "Speakers", ""),
            ('LIGHT_PROBE', "Light Probes", ""),
            None,
            ('BONE', "Bones", ""),
            ('NODE', "Nodes", ""),
            ('SEQUENCE_STRIP', "Sequence Strips", ""),
        ),
        description="Type of data to rename",
    )

    data_source: EnumProperty(
        name="Source",
        items=(
            ('SELECT', "Selected", ""),
            ('ALL', "All", ""),
        ),
    )

    actions: CollectionProperty(type=BatchRenameAction)

    @staticmethod
    def _data_from_context(context, data_type, only_selected, check_context=False):

        mode = context.mode
        scene = context.scene
        space = context.space_data
        space_type = None if (space is None) else space.type

        data = None
        if space_type == 'SEQUENCE_EDITOR':
            data_type_test = 'SEQUENCE_STRIP'
            if check_context:
                return data_type_test
            if data_type == data_type_test:
                data = (
                    # TODO, we don't have access to seqbasep, this won't work when inside metas.
                    [seq for seq in context.scene.sequence_editor.sequences_all if seq.select]
                    if only_selected else
                    context.scene.sequence_editor.sequences_all,
                    "name",
                    "Strip(s)",
                )
        elif space_type == 'NODE_EDITOR':
            data_type_test = 'NODE'
            if check_context:
                return data_type_test
            if data_type == data_type_test:
                data = (
                    context.selected_nodes
                    if only_selected else
                    list(space.node_tree.nodes),
                    "name",
                    "Node(s)",
                )
        else:
            if mode == 'POSE' or (mode == 'WEIGHT_PAINT' and context.pose_object):
                data_type_test = 'BONE'
                if check_context:
                    return data_type_test
                if data_type == data_type_test:
                    data = (
                        [pchan.bone for pchan in context.selected_pose_bones]
                        if only_selected else
                        [pbone.bone for ob in context.objects_in_mode_unique_data for pbone in ob.pose.bones],
                        "name",
                        "Bone(s)",
                    )
            elif mode == 'EDIT_ARMATURE':
                data_type_test = 'BONE'
                if check_context:
                    return data_type_test
                if data_type == data_type_test:
                    data = (
                        context.selected_editable_bones
                        if only_selected else
                        [ebone for ob in context.objects_in_mode_unique_data for ebone in ob.data.edit_bones],
                        "name",
                        "Edit Bone(s)",
                    )

        if check_context:
            return 'OBJECT'

        object_data_type_attrs_map = {
            'MESH': ("meshes", "Mesh(es)"),
            'CURVE': ("curves", "Curve(s)"),
            'META': ("metaballs", "MetaBall(s)"),
            'ARMATURE': ("armatures", "Armature(s)"),
            'LATTICE': ("lattices", "Lattice(s)"),
            'GPENCIL': ("grease_pencils", "Grease Pencil(s)"),
            'CAMERA': ("cameras", "Camera(s)"),
            'SPEAKER': ("speakers", "Speaker(s)"),
            'LIGHT_PROBE': ("light_probes", "LightProbe(s)"),
        }

        # Finish with space types.
        if data is None:

            if data_type == 'OBJECT':
                data = (
                    context.selected_editable_objects
                    if only_selected else
                    [id for id in bpy.data.objects if id.library is None],
                    "name",
                    "Object(s)",
                )
            elif data_type == 'MATERIAL':
                data = (
                    tuple(set(
                        slot.material
                        for ob in context.selected_objects
                        for slot in ob.material_slots
                        if slot.material is not None
                    ))
                    if only_selected else
                    [id for id in bpy.data.materials if id.library is None],
                    "name",
                    "Material(s)",
                )
            elif data_type in object_data_type_attrs_map.keys():
                attr, descr = object_data_type_attrs_map[data_type]
                data = (
                    tuple(set(
                        id
                        for ob in context.selected_objects
                        if ob.type == data_type
                        for id in (ob.data,)
                        if id is not None and id.library is None
                    ))
                    if only_selected else
                    [id for id in getattr(bpy.data, attr) if id.library is None],
                    "name",
                    descr,
                )

        return data

    @staticmethod
    def _apply_actions(actions, name):
        import string
        import re

        for action in actions:
            ty = action.type
            if ty == 'SET':
                text = action.set_name
                method = action.set_method
                if method == 'NEW':
                    name = text
                elif method == 'PREFIX':
                    name = text + name
                elif method == 'SUFFIX':
                    name = name + text
                else:
                    assert(0)

            elif ty == 'STRIP':
                chars = action.strip_chars
                chars_strip = (
                    "{:s}{:s}{:s}"
                ).format(
                    string.punctuation if 'PUNCT' in chars else "",
                    string.digits if 'DIGIT' in chars else "",
                    " " if 'SPACE' in chars else "",
                )
                part = action.strip_part
                if 'START' in part:
                    name = name.lstrip(chars_strip)
                if 'END' in part:
                    name = name.rstrip(chars_strip)

            elif ty == 'REPLACE':
                if action.use_replace_regex_src:
                    replace_src = action.replace_src
                    if action.use_replace_regex_dst:
                        replace_dst = action.replace_dst
                    else:
                        replace_dst = action.replace_dst.replace("\\", "\\\\")
                else:
                    replace_src = re.escape(action.replace_src)
                    replace_dst = action.replace_dst.replace("\\", "\\\\")
                name = re.sub(
                    replace_src,
                    replace_dst,
                    name,
                    flags=(
                        0 if action.replace_match_case else
                        re.IGNORECASE
                    ),
                )
            elif ty == 'CASE':
                method = action.case_method
                if method == 'UPPER':
                    name = name.upper()
                elif method == 'LOWER':
                    name = name.lower()
                elif method == 'TITLE':
                    name = name.title()
                else:
                    assert(0)
            else:
                assert(0)
        return name

    def _data_update(self, context):
        only_selected = self.data_source == 'SELECT'

        self._data = self._data_from_context(context, self.data_type, only_selected)
        if self._data is None:
            self.data_type = self._data_from_context(context, None, False, check_context=True)
            self._data = self._data_from_context(context, self.data_type, only_selected)

        self._data_source_prev = self.data_source
        self._data_type_prev = self.data_type

    def draw(self, context):
        import re

        layout = self.layout

        split = layout.split(factor=0.5)
        split.label(text="Data Type:")
        split.prop(self, "data_type", text="")

        split = layout.split(factor=0.5)
        split.label(text="Rename {:d} {:s}:".format(len(self._data[0]), self._data[2]))
        split.row().prop(self, "data_source", expand=True)

        for action in self.actions:
            box = layout.box()

            row = box.row(align=True)
            row.prop(action, "type", text="")
            row.prop(action, "op_add", text="", icon='ADD')
            row.prop(action, "op_remove", text="", icon='REMOVE')

            ty = action.type
            if ty == 'SET':
                box.prop(action, "set_method")
                box.prop(action, "set_name")
            elif ty == 'STRIP':
                box.row().prop(action, "strip_chars")
                box.row().prop(action, "strip_part")
            elif ty == 'REPLACE':

                row = box.row(align=True)
                re_error_src = None
                if action.use_replace_regex_src:
                    try:
                        re.compile(action.replace_src)
                    except Exception as ex:
                        re_error_src = str(ex)
                        row.alert = True
                row.prop(action, "replace_src")
                row.prop(action, "use_replace_regex_src", text="", icon='SORTBYEXT')
                if re_error_src is not None:
                    box.label(text=re_error_src)

                re_error_dst = None
                row = box.row(align=True)
                if action.use_replace_regex_src:
                    if action.use_replace_regex_dst:
                        if re_error_src is None:
                            try:
                                re.sub(action.replace_src, action.replace_dst, "")
                            except Exception as ex:
                                re_error_dst = str(ex)
                                row.alert = True

                row.prop(action, "replace_dst")
                rowsub = row.row(align=True)
                rowsub.active = action.use_replace_regex_src
                rowsub.prop(action, "use_replace_regex_dst", text="", icon='SORTBYEXT')
                if re_error_dst is not None:
                    box.label(text=re_error_dst)

                row = box.row()
                row.prop(action, "replace_match_case")
            elif ty == 'CASE':
                box.row().prop(action, "case_method", expand=True)

    def check(self, context):
        changed = False
        for i, action in enumerate(self.actions):
            if action.op_add:
                action.op_add = False
                self.actions.add()
                if i + 2 != len(self.actions):
                    self.actions.move(len(self.actions) - 1, i + 1)
                changed = True
                break
            if action.op_remove:
                action.op_remove = False
                if len(self.actions) > 1:
                    self.actions.remove(i)
                changed = True
                break

        if (
                (self._data_source_prev != self.data_source) or
                (self._data_type_prev != self.data_type)
        ):
            self._data_update(context)
            changed = True

        return changed

    def execute(self, context):
        import re

        seq, attr, descr = self._data

        actions = self.actions

        # Sanitize actions.
        for action in actions:
            if action.use_replace_regex_src:
                try:
                    re.compile(action.replace_src)
                except Exception as ex:
                    self.report({'ERROR'}, "Invalid regular expression (find): " + str(ex))
                    return {'CANCELLED'}

                if action.use_replace_regex_dst:
                    try:
                        re.sub(action.replace_src, action.replace_dst, "")
                    except Exception as ex:
                        self.report({'ERROR'}, "Invalid regular expression (replace): " + str(ex))
                        return {'CANCELLED'}

        total_len = 0
        change_len = 0
        for item in seq:
            name_src = getattr(item, attr)
            name_dst = self._apply_actions(actions, name_src)
            if name_src != name_dst:
                setattr(item, attr, name_dst)
                change_len += 1
            total_len += 1

        self.report({'INFO'}, "Renamed {:d} of {:d} {:s}".format(change_len, total_len, descr))

        return {'FINISHED'}

    def invoke(self, context, event):

        self._data_update(context)

        if not self.actions:
            self.actions.add()
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)


class WM_MT_splash(Menu):
    bl_label = "Splash"

    def draw_setup(self, context):
        wm = context.window_manager
        # prefs = context.preferences

        layout = self.layout

        layout.operator_context = 'EXEC_DEFAULT'

        layout.label(text="Quick Setup")

        split = layout.split(factor=0.25)
        split.label()
        split = split.split(factor=2.0 / 3.0)

        col = split.column()

        sub = col.split(factor=0.35)
        row = sub.row()
        row.alignment = 'RIGHT'
        row.label(text="Language")
        prefs = context.preferences
        sub.prop(prefs.view, "language", text="")

        col.separator()

        sub = col.split(factor=0.35)
        row = sub.row()
        row.alignment = 'RIGHT'
        row.label(text="Shortcuts")
        text = bpy.path.display_name(wm.keyconfigs.active.name)
        if not text:
            text = "Blender"
        sub.menu("USERPREF_MT_keyconfigs", text=text)

        kc = wm.keyconfigs.active
        kc_prefs = kc.preferences
        has_select_mouse = hasattr(kc_prefs, "select_mouse")
        if has_select_mouse:
            sub = col.split(factor=0.35)
            row = sub.row()
            row.alignment = 'RIGHT'
            row.label(text="Select With")
            sub.row().prop(kc_prefs, "select_mouse", expand=True)
            has_select_mouse = True

        has_spacebar_action = hasattr(kc_prefs, "spacebar_action")
        if has_spacebar_action:
            sub = col.split(factor=0.35)
            row = sub.row()
            row.alignment = 'RIGHT'
            row.label(text="Spacebar")
            sub.row().prop(kc_prefs, "spacebar_action", expand=True)
            has_select_mouse = True

        col.separator()

        sub = col.split(factor=0.35)
        row = sub.row()
        row.alignment = 'RIGHT'
        row.label(text="Theme")
        label = bpy.types.USERPREF_MT_interface_theme_presets.bl_label
        if label == "Presets":
            label = "Blender Dark"
        sub.menu("USERPREF_MT_interface_theme_presets", text=label)

        # Keep height constant
        if not has_select_mouse:
            col.label()
        if not has_spacebar_action:
            col.label()

        layout.label()

        row = layout.row()

        sub = row.row()
        old_version = bpy.types.PREFERENCES_OT_copy_prev.previous_version()
        if bpy.types.PREFERENCES_OT_copy_prev.poll(context) and old_version:
            sub.operator("preferences.copy_prev", text="Load %d.%d Settings" % old_version)
            sub.operator("wm.save_userpref", text="Save New Settings")
        else:
            sub.label()
            sub.label()
            sub.operator("wm.save_userpref", text="Next")

        layout.separator()
        layout.separator()

    def draw(self, context):
        # Draw setup screen if no preferences have been saved yet.
        import os

        userconfig_path = bpy.utils.user_resource('CONFIG')
        userdef_path = os.path.join(userconfig_path, "userpref.blend")

        if not os.path.isfile(userdef_path):
            self.draw_setup(context)
            return

        # Pass
        layout = self.layout
        layout.operator_context = 'EXEC_DEFAULT'
        layout.emboss = 'PULLDOWN_MENU'

        split = layout.split()

        # Templates
        col1 = split.column()
        col1.label(text="New File")

        bpy.types.TOPBAR_MT_file_new.draw_ex(col1, context, use_splash=True)

        # Recent
        col2 = split.column()
        col2_title = col2.row()

        found_recent = col2.template_recent_files()

        if found_recent:
            col2_title.label(text="Recent Files")
        else:

            # Links if no recent files
            col2_title.label(text="Getting Started")

            col2.operator("wm.url_open_preset", text="Manual", icon='URL').type = 'MANUAL'
            col2.operator("wm.url_open_preset", text="Blender Website", icon='URL').type = 'BLENDER'
            col2.operator("wm.url_open_preset", text="Credits", icon='URL').type = 'CREDITS'

        layout.separator()

        split = layout.split()

        col1 = split.column()
        sub = col1.row()
        sub.operator_context = 'INVOKE_DEFAULT'
        sub.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        col1.operator("wm.recover_last_session", icon='RECOVER_LAST')

        col2 = split.column()

        col2.operator("wm.url_open_preset", text="Release Notes", icon='URL').type = 'RELEASE_NOTES'
        col2.operator("wm.url_open_preset", text="Development Fund", icon='FUND').type = 'FUND'

        layout.separator()
        layout.separator()


class WM_MT_splash_about(Menu):
    bl_label = "About"

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'EXEC_DEFAULT'

        layout.label(text="Blender is free software")
        layout.label(text="Licensed under the GNU General Public License")
        layout.separator()
        layout.separator()

        split = layout.split()
        split.emboss = 'PULLDOWN_MENU'
        split.scale_y = 1.3

        col1 = split.column()

        col1.operator("wm.url_open_preset", text="Release Notes", icon='URL').type = 'RELEASE_NOTES'
        col1.operator("wm.url_open_preset", text="Credits", icon='URL').type = 'CREDITS'
        col1.operator("wm.url_open", text="License", icon='URL').url = "https://www.blender.org/about/license/"

        col2 = split.column()

        col2.operator("wm.url_open_preset", text="Blender Website", icon='URL').type = 'BLENDER'
        col2.operator("wm.url_open", text="Blender Store", icon='URL').url = "https://store.blender.org"
        col2.operator("wm.url_open_preset", text="Development Fund", icon='FUND').type = 'FUND'


class WM_OT_drop_blend_file(Operator):
    bl_idname = "wm.drop_blend_file"
    bl_label = "Handle dropped .blend file"
    bl_options = {'INTERNAL'}

    filepath: StringProperty()

    def invoke(self, context, _event):
        context.window_manager.popup_menu(self.draw_menu, title=bpy.path.basename(self.filepath), icon='QUESTION')
        return {'FINISHED'}

    def draw_menu(self, menu, _context):
        layout = menu.layout

        col = layout.column()
        col.operator_context = 'INVOKE_DEFAULT'
        props = col.operator("wm.open_mainfile", text="Open", icon='FILE_FOLDER')
        props.filepath = self.filepath
        props.display_file_selector = False

        layout.separator()
        col = layout.column()
        col.operator_context = 'INVOKE_DEFAULT'
        col.operator("wm.link", text="Link...", icon='LINK_BLEND').filepath = self.filepath
        col.operator("wm.append", text="Append...", icon='APPEND_BLEND').filepath = self.filepath


classes = (
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
    WM_OT_doc_view,
    WM_OT_doc_view_manual,
    WM_OT_drop_blend_file,
    WM_OT_operator_cheat_sheet,
    WM_OT_operator_pie_enum,
    WM_OT_path_open,
    WM_OT_properties_add,
    WM_OT_properties_context_change,
    WM_OT_properties_edit,
    WM_OT_properties_remove,
    WM_OT_sysinfo,
    WM_OT_owner_disable,
    WM_OT_owner_enable,
    WM_OT_url_open,
    WM_OT_url_open_preset,
    WM_OT_tool_set_by_id,
    WM_OT_tool_set_by_index,
    WM_OT_toolbar,
    WM_OT_toolbar_fallback_pie,
    WM_OT_toolbar_prompt,
    BatchRenameAction,
    WM_OT_batch_rename,
    WM_MT_splash,
    WM_MT_splash_about,
)
