# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

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
    BoolVectorProperty,
    IntVectorProperty,
    FloatVectorProperty,
)
from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_n as n_,
    pgettext_tip as tip_,
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)


def _rna_path_prop_search_for_context_impl(context, edit_text, unique_attrs):
    # Use the same logic as auto-completing in the Python console to expand the data-path.
    from _bl_console_utils.autocomplete import intellisense
    context_prefix = "context."
    line = context_prefix + edit_text
    cursor = len(line)
    namespace = {"context": context}
    comp_prefix, _, comp_options = intellisense.expand(line=line, cursor=cursor, namespace=namespace, private=False)
    prefix = comp_prefix[len(context_prefix):]  # Strip "context."
    for attr in comp_options.split("\n"):
        if attr.endswith((
                # Exclude function calls because they are generally not part of data-paths.
                "(", ")",
                # RNA properties for introspection, not useful to expand.
                ".bl_rna", ".rna_type",
        )):
            continue
        # If we type/paste in complete attributes, intellisense expands with a ".", remove that again (see #134092)
        attr_full = (prefix + attr.lstrip()).removesuffix(".")
        if attr_full in unique_attrs:
            continue
        unique_attrs.add(attr_full)
        yield attr_full


def rna_path_prop_search_for_context(self, context, edit_text):
    # NOTE(@campbellbarton): Limiting data-path expansion is rather arbitrary.
    # It's possible for example that someone would want to set a shortcut in the preferences or
    # in other region types than those currently expanded. Unless there is a reasonable likelihood
    # users might expand these space-type/region-type combinations - exclude them from this search.
    # After all, this list is mainly intended as a hint, users are not prevented from constructing
    # the data-paths themselves.
    unique_attrs = set()

    for window in context.window_manager.windows:
        for area in window.screen.areas:
            # Users are very unlikely to be setting shortcuts in the preferences, skip this.
            if area.type == 'PREFERENCES':
                continue
            # Ignore the same region type multiple times in an area.
            # Prevents the 3D-viewport quad-view from attempting to expand 3 extra times for example
            region_type_unique = set()
            for region in area.regions:
                if region.type not in {'WINDOW', 'PREVIEW'}:
                    continue
                if region.type in region_type_unique:
                    continue
                region_type_unique.add(region.type)
                with context.temp_override(window=window, area=area, region=region):
                    yield from _rna_path_prop_search_for_context_impl(context, edit_text, unique_attrs)

    if not unique_attrs:
        # Users *might* only have a preferences area shown, in that case just expand the current context.
        yield from _rna_path_prop_search_for_context_impl(context, edit_text, unique_attrs)


rna_path_prop = StringProperty(
    name="Context Attributes",
    description="Context data-path (expanded using visible windows in the current .blend file)",
    maxlen=1024,
    search=rna_path_prop_search_for_context,
)

rna_reverse_prop = BoolProperty(
    name="Reverse",
    description="Cycle backwards",
    default=False,
    options={'SKIP_SAVE'},
)

rna_wrap_prop = BoolProperty(
    name="Wrap",
    description="Wrap back to the first/last values",
    default=False,
    options={'SKIP_SAVE'},
)

rna_relative_prop = BoolProperty(
    name="Relative",
    description="Apply relative to the current value (delta)",
    default=False,
    options={'SKIP_SAVE'},
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
        value = eval("context.{:s}".format(data_path)) if data_path else Ellipsis
    except AttributeError as ex:
        if str(ex).startswith("'NoneType'"):
            # One of the items in the rna path is None, just ignore this
            value = Ellipsis
        else:
            # Print invalid path, but don't show error to the users and fully
            # break the UI if the operator is bound to an event like left click.
            print("context_path_validate error: context.{:s} not found (invalid keymap entry?)".format(data_path))
            value = Ellipsis

    return value


def context_path_to_rna_property(context, data_path):
    from _bl_rna_utils.data_path import property_definition_from_data_path
    rna_prop = property_definition_from_data_path(context, "." + data_path)
    if rna_prop is not None:
        return rna_prop
    return None


def context_path_decompose(data_path):
    # Decompose a data_path into 3 components:
    # base_path, prop_attr, prop_item, where:
    # `"foo.bar["baz"].fiz().bob.buz[10][2]"`, returns...
    # `("foo.bar["baz"].fiz().bob", "buz", "[10][2]")`
    #
    # This is useful as we often want the base and the property, ignoring any item access.
    # Note that item access includes function calls since these aren't properties.
    #
    # Note that the `.` is removed from the start of the first and second values,
    # this is done because `.attr` isn't convenient to use as an argument,
    # also the convention is not to include this within the data paths or the operator logic for `bpy.ops.wm.*`.
    from _bl_rna_utils.data_path import decompose_data_path
    path_split = decompose_data_path("." + data_path)

    # Find the last property that isn't a function call.
    value_prev = ""
    i = len(path_split)
    while (i := i - 1) >= 0:
        value = path_split[i]
        if value.startswith("."):
            if not value_prev.startswith("("):
                break
        value_prev = value

    if i != -1:
        base_path = "".join(path_split[:i])
        prop_attr = path_split[i]
        prop_item = "".join(path_split[i + 1:])

        if base_path:
            assert base_path.startswith(".")
            base_path = base_path[1:]
        if prop_attr:
            assert prop_attr.startswith(".")
            prop_attr = prop_attr[1:]
    else:
        # If there are no properties, everything is an item.
        # Note that should not happen in practice with values which are added onto `context`,
        # include since it's correct to account for this case and not doing so will create a confusing exception.
        base_path = ""
        prop_attr = ""
        prop_item = "".join(path_split)

    return (base_path, prop_attr, prop_item)


def description_from_data_path(base, data_path, *, prefix, value=Ellipsis):
    if context_path_validate(base, data_path) is Ellipsis:
        return None

    if (
            (rna_prop := context_path_to_rna_property(base, data_path)) and
            (description := tip_(rna_prop.description))
    ):
        description = tip_("{:s}: {:s}").format(prefix, description)
        if value != Ellipsis:
            description = "{:s}\n{:s}: {:s}".format(description, tip_("Value"), str(value))
        return description
    return None


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
    return (
        isinstance(id_data, bpy.types.ID) and
        (not isinstance(id_data, (
            bpy.types.WindowManager,
            bpy.types.Screen,
            bpy.types.Brush,
        )))
    )


def operator_path_is_undo(context, data_path):
    data_path_head, _, _ = context_path_decompose(data_path)

    # When we can't find the data owner assume no undo is needed.
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
        exec("context.{:s} += self.value".format(data_path))
    else:
        exec("context.{:s} = self.value".format(data_path))

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Assign"), value=props.value)

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Assign"), value=props.value)

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Scale"), value=props.value)

    def execute(self, context):
        data_path = self.data_path
        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        value = self.value

        if value == 1.0:  # nothing to do
            return {'CANCELLED'}

        exec("context.{:s} *= value".format(data_path))

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
        options={'SKIP_SAVE'},
    )

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Scale"), value=props.value)

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
            exec("context.{:s} = {:s}(round(context.{:s} * value), context.{:s} + {:s})".format(
                data_path, func, data_path, data_path, add,
            ))
        else:
            exec("context.{:s} *= value".format(data_path))

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Assign"), value=props.value)

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Assign"), value=props.value)

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Assign"), value=props.value)

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

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Assign"), value=props.value)

    def execute(self, context):
        data_path = self.data_path
        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}
        exec("context.{:s} = {:s}".format(data_path, self.value))
        return operator_path_undo_return(context, data_path)


class WM_OT_context_toggle(Operator):
    """Toggle a context value"""
    bl_idname = "wm.context_toggle"
    bl_label = "Context Toggle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path_prop
    module: rna_module_prop

    @classmethod
    def description(cls, context, props):
        # Currently unsupported, it might be possible to extract this.
        if props.module:
            return None
        return description_from_data_path(context, props.data_path, prefix=tip_("Toggle"))

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

        exec("base.{:s} = not (base.{:s})".format(data_path, data_path))

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

    @classmethod
    def description(cls, context, props):
        value = "({!r}, {!r})".format(props.value_1, props.value_2)
        return description_from_data_path(context, props.data_path, prefix=tip_("Toggle"), value=value)

    def execute(self, context):
        data_path = self.data_path

        if context_path_validate(context, data_path) is Ellipsis:
            return {'PASS_THROUGH'}

        # failing silently is not ideal, but we don't want errors for shortcut
        # keys that some values that are only available in a particular context
        try:
            exec(
                "context.{:s} = {!r} if (context.{:s} != {!r}) else {!r}".format(
                    data_path,
                    self.value_2,
                    data_path,
                    self.value_2,
                    self.value_1,
                )
            )
        except Exception:
            return {'PASS_THROUGH'}

        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_int(Operator):
    """Set a context value (useful for cycling active material, """ \
        """shape keys, groups, etc.)"""
    bl_idname = "wm.context_cycle_int"
    bl_label = "Context Int Cycle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path_prop
    reverse: rna_reverse_prop
    wrap: rna_wrap_prop

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Cycle"))

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        if self.reverse:
            value -= 1
        else:
            value += 1

        exec("context.{:s} = value".format(data_path))

        if self.wrap:
            if value != eval("context.{:s}".format(data_path)):
                # relies on rna clamping integers out of the range
                if self.reverse:
                    value = (1 << 31) - 1
                else:
                    value = -1 << 31

                exec("context.{:s} = value".format(data_path))

        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_enum(Operator):
    """Toggle a context value"""
    bl_idname = "wm.context_cycle_enum"
    bl_label = "Context Enum Cycle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path_prop
    reverse: rna_reverse_prop
    wrap: rna_wrap_prop

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Cycle"))

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)
        if value is Ellipsis:
            return {'PASS_THROUGH'}

        orig_value = value

        rna_prop = context_path_to_rna_property(context, data_path)
        if type(rna_prop) != bpy.types.EnumProperty:
            raise Exception("expected an enum property")

        enums = rna_prop.enum_items.keys()
        orig_index = enums.index(orig_value)

        # Have the info we need, advance to the next item.
        #
        # When wrap's disabled we may set the value to itself,
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
        exec("context.{:s} = advance_enum".format(data_path))
        return operator_path_undo_return(context, data_path)


class WM_OT_context_cycle_array(Operator):
    """Set a context array value """ \
        """(useful for cycling the active mesh edit mode)"""
    bl_idname = "wm.context_cycle_array"
    bl_label = "Context Array Cycle"
    bl_options = {'UNDO', 'INTERNAL'}

    data_path: rna_path_prop
    reverse: rna_reverse_prop

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Cycle"))

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

        exec("context.{:s} = cycle(context.{:s}[:])".format(data_path, data_path))

        return operator_path_undo_return(context, data_path)


class WM_OT_context_menu_enum(Operator):
    bl_idname = "wm.context_menu_enum"
    bl_label = "Context Enum Menu"
    # The menu items & UI logic handles undo.
    bl_options = {'INTERNAL'}

    data_path: rna_path_prop

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Menu"))

    def execute(self, context):
        data_path = self.data_path
        value = context_path_validate(context, data_path)

        if value is Ellipsis:
            return {'PASS_THROUGH'}

        base_path, prop_attr, _ = context_path_decompose(data_path)
        value_base = context_path_validate(context, base_path)
        rna_prop = context_path_to_rna_property(context, data_path)

        def draw_cb(self, context):
            layout = self.layout
            layout.prop(value_base, prop_attr, expand=True)

        context.window_manager.popup_menu(draw_func=draw_cb, title=rna_prop.name, icon=rna_prop.icon)

        return {'FINISHED'}


class WM_OT_context_pie_enum(Operator):
    bl_idname = "wm.context_pie_enum"
    bl_label = "Context Enum Pie"
    # The menu items & UI logic handles undo.
    bl_options = {'INTERNAL'}

    data_path: rna_path_prop

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Pie Menu"))

    def invoke(self, context, event):
        wm = context.window_manager
        data_path = self.data_path
        value = context_path_validate(context, data_path)

        if value is Ellipsis:
            return {'PASS_THROUGH'}

        base_path, prop_attr, _ = context_path_decompose(data_path)
        value_base = context_path_validate(context, base_path)
        rna_prop = context_path_to_rna_property(context, data_path)

        def draw_cb(self, context):
            layout = self.layout
            layout.prop(value_base, prop_attr, expand=True)

        wm.popup_menu_pie(draw_func=draw_cb, title=rna_prop.name, icon=rna_prop.icon, event=event)

        return {'FINISHED'}


class WM_OT_operator_pie_enum(Operator):
    bl_idname = "wm.operator_pie_enum"
    bl_label = "Operator Enum Pie"
    # The menu items & UI logic handles undo.
    bl_options = {'INTERNAL'}

    data_path: StringProperty(
        name="Operator",
        description="Operator name (in Python as string)",
        maxlen=1024,
    )
    prop_string: StringProperty(
        name="Property",
        description="Property name (as a string)",
        maxlen=1024,
    )

    @classmethod
    def description(cls, context, props):
        return description_from_data_path(context, props.data_path, prefix=tip_("Pie Menu"))

    def invoke(self, context, event):
        wm = context.window_manager

        data_path = self.data_path
        prop_attr = self.prop_string

        # same as eval("bpy.ops." + data_path)
        op_mod_str, ob_id_str = data_path.split(".", 1)
        op = getattr(getattr(bpy.ops, op_mod_str), ob_id_str)
        del op_mod_str, ob_id_str

        try:
            op_rna = op.get_rna_type()
        except KeyError:
            self.report({'ERROR'}, rpt_("Operator not found: bpy.ops.{:s}").format(data_path))
            return {'CANCELLED'}

        def draw_cb(self, context):
            layout = self.layout
            pie = layout.menu_pie()
            pie.operator_enum(data_path, prop_attr)

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

        # Match the pointer type from the target property to `bpy.data.*`
        # so we lookup the correct list.

        rna_prop = context_path_to_rna_property(context, data_path)
        rna_prop_fixed_type = rna_prop.fixed_type

        id_iter = None

        for prop in bpy.data.rna_type.properties:
            if prop.rna_type.identifier == "CollectionProperty":
                if prop.fixed_type == rna_prop_fixed_type:
                    id_iter = prop.identifier
                    break

        if id_iter:
            value_id = getattr(bpy.data, id_iter).get(value)
            exec("context.{:s} = value_id".format(data_path))

        return operator_path_undo_return(context, data_path)


doc_id = StringProperty(
    name="Doc ID",
    maxlen=1024,
    options={'HIDDEN'},
)

data_path_iter = StringProperty(
    description="The data path relative to the context, must point to an iterable",
)

data_path_item = StringProperty(
    description="The data path from each iterable to the value (int or float)",
)


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
            except Exception:
                continue

            if value_orig is True:
                is_set = True
            elif value_orig is False:
                pass
            else:
                self.report(
                    {'WARNING'},
                    rpt_("Non boolean value found: {:s}[ ].{:s}").format(data_path_iter, data_path_item),
                )
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

        exec_str = "item.{:s} = {:s}".format(data_path_item, str(is_set))
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
        options={'SKIP_SAVE'},
    )
    invert: BoolProperty(
        description="Invert the mouse input",
        default=False,
        options={'SKIP_SAVE'},
    )
    initial_x: IntProperty(options={'HIDDEN'})

    def _values_store(self, context):
        data_path_iter = self.data_path_iter
        data_path_item = self.data_path_item

        self._values = values = {}

        for item in getattr(context, data_path_iter):
            try:
                value_orig = eval("item." + data_path_item)
            except Exception:
                continue

            # check this can be set, maybe this is library data.
            try:
                exec("item.{:s} = {:s}".format(data_path_item, str(value_orig)))
            except Exception:
                continue

            values[item] = value_orig

    def _values_delta(self, delta):
        delta *= self.input_scale
        if self.invert:
            delta = - delta

        data_path_item = self.data_path_item
        for item, value_orig in self._values.items():
            if type(value_orig) == int:
                exec("item.{:s} = int({:d})".format(data_path_item, round(value_orig + delta)))
            else:
                exec("item.{:s} = {:f}".format(data_path_item, value_orig + delta))

    def _values_restore(self):
        data_path_item = self.data_path_item
        for item, value_orig in self._values.items():
            exec("item.{:s} = {:s}".format(data_path_item, str(value_orig)))

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
                    header_text = header_text % eval("item.{:s}".format(self.data_path_item))
                else:
                    header_text = (self.header_text % delta) + rpt_(" (delta)")
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
            self.report(
                {'WARNING'},
                rpt_("Nothing to operate on: {:s}[ ].{:s}").format(
                    self.data_path_iter, self.data_path_item,
                ),
            )
            return {'CANCELLED'}
        else:
            self.initial_x = event.mouse_x

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}


class WM_OT_url_open(Operator):
    """Open a website in the web browser"""
    bl_idname = "wm.url_open"
    bl_label = ""
    bl_options = {'INTERNAL'}

    url: StringProperty(
        name="URL",
        description="URL to open",
    )

    @staticmethod
    def _add_utm_param_to_url(url, utm_source):
        import urllib.parse

        # Parse the URL to get its domain and query parameters.
        if not urllib.parse.urlparse(url).scheme:
            url = "https://" + url
        parsed_url = urllib.parse.urlparse(url)

        # Only add a utm source if it points to a blender.org domain.
        domain = parsed_url.netloc
        if not (domain.endswith(".blender.org") or domain == "blender.org"):
            return url

        # Parse the query parameters and add or update the utm_source parameter.
        query_params = urllib.parse.parse_qs(parsed_url.query)
        query_params["utm_source"] = utm_source
        new_query = urllib.parse.urlencode(query_params, doseq=True)

        # Create a new URL with the updated query parameters.
        new_url_parts = list(parsed_url)
        new_url_parts[4] = new_query
        new_url = urllib.parse.urlunparse(new_url_parts)

        return new_url

    @staticmethod
    def _get_utm_source():
        version = bpy.app.version_string
        return "blender-" + version.replace(" ", "-").lower()

    def execute(self, _context):
        import webbrowser
        complete_url = self._add_utm_param_to_url(self.url, self._get_utm_source())
        webbrowser.open(complete_url)
        return {'FINISHED'}


class WM_OT_url_open_preset(Operator):
    """Open a preset website in the web browser"""
    bl_idname = "wm.url_open_preset"
    bl_label = "Open Preset Website"
    bl_options = {'INTERNAL'}
    bl_property = "type"

    @staticmethod
    def _wm_url_open_preset_type_items(_self, _context):
        return [item for (item, _) in WM_OT_url_open_preset.preset_items]

    type: EnumProperty(
        name="Site",
        items=WM_OT_url_open_preset._wm_url_open_preset_type_items,
    )

    def _url_from_bug(self, _context):
        from _bpy_internal.system_info.url_prefill_runtime import url_from_blender
        return url_from_blender()

    def _url_from_release_notes(self, _context):
        return "https://www.blender.org/download/releases/{:d}-{:d}/".format(*bpy.app.version[:2])

    def _url_from_manual(self, _context):
        return "https://docs.blender.org/manual/{:s}/{:d}.{:d}/".format(
            bpy.utils.manual_language_code(), *bpy.app.version[:2],
        )

    def _url_from_api(self, _context):
        return "https://docs.blender.org/api/{:d}.{:d}/".format(*bpy.app.version[:2])

    # This list is: (enum_item, url) pairs.
    # Allow dynamically extending.
    preset_items = [
        # Dynamic URL's.
        (('BUG', iface_("Bug"),
          tip_("Report a bug with pre-filled version information")),
         _url_from_bug),
        (('RELEASE_NOTES', iface_("Release Notes"),
          tip_("Read about what's new in this version of Blender")),
         _url_from_release_notes),
        (('MANUAL', iface_("User Manual"),
          tip_("The reference manual for this version of Blender")),
         _url_from_manual),
        (('API', iface_("Python API Reference"),
          tip_("The API reference manual for this version of Blender")),
         _url_from_api),

        # Static URL's.
        (('FUND', iface_("Development Fund"),
          tip_("The donation program to support maintenance and improvements")),
         "https://fund.blender.org"),
        (('BLENDER', "blender.org",
          tip_("Blender's official web-site")),
         "https://www.blender.org"),
        (('CREDITS', iface_("Credits"),
          tip_("Lists committers to Blender's source code")),
         "https://www.blender.org/about/credits/"),
        (('EXTENSIONS', iface_("Extensions Platform"),
          tip_("Online directory of free and open source extensions")),
         "https://extensions.blender.org/"),
    ]

    def execute(self, context):
        url = None
        type = self.type
        for (item_id, _, _), url in self.preset_items:
            if item_id == type:
                if callable(url):
                    url = url(self, context)
                break

        return bpy.ops.wm.url_open(url=url)


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
            self.report({'ERROR'}, rpt_("File '{:s}' not found").format(filepath))
            return {'CANCELLED'}

        if sys.platform[:3] == "win":
            os.startfile(filepath)
        elif sys.platform == "darwin":
            subprocess.check_call(["open", filepath])
        else:
            try:
                subprocess.check_call(["xdg-open", filepath])
            except Exception:
                # `xdg-open` *should* be supported by recent Gnome, KDE, XFCE.
                import traceback
                traceback.print_exc()

        return {'FINISHED'}


def _wm_doc_get_id(doc_id, *, do_url=True, url_prefix="", report=None):

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
            url = "{:s}/bpy.types.{:s}.html".format(url_prefix, id_split[0])
        else:
            rna = "bpy.types.{:s}".format(id_split[0])

    elif len(id_split) == 2:  # rna, class.prop
        class_name, class_prop = id_split

        # an operator (common case - just button referencing an op)
        if operator_exists_pair(class_name, class_prop):
            if do_url:
                url = "{:s}/bpy.ops.{:s}.html#bpy.ops.{:s}.{:s}".format(url_prefix, class_name, class_name, class_prop)
            else:
                rna = "bpy.ops.{:s}.{:s}".format(class_name, class_prop)
        elif operator_exists_single(class_name):
            # note: ignore the prop name since we don't have a way to link into it
            class_name, class_prop = class_name.split("_OT_", 1)
            class_name = class_name.lower()
            if do_url:
                url = "{:s}/bpy.ops.{:s}.html#bpy.ops.{:s}.{:s}".format(url_prefix, class_name, class_name, class_prop)
            else:
                rna = "bpy.ops.{:s}.{:s}".format(class_name, class_prop)
        else:
            # An RNA setting, common case.

            # Check the built-in RNA types.
            rna_class = getattr(bpy.types, class_name, None)
            if rna_class is None:
                # Check class for dynamically registered types.
                rna_class = bpy.types.PropertyGroup.bl_rna_get_subclass_py(class_name)

            if rna_class is None:
                rna_class = bpy.types.AddonPreferences.bl_rna_get_subclass_py(class_name)

            if rna_class is None:
                if report is not None:
                    report({'ERROR'}, rpt_("Type \"{:s}\" cannot be found").format(class_name))
                return None

            # Detect if this is a inherited member and use that name instead.
            rna_parent = rna_class.bl_rna
            rna_prop = rna_parent.properties.get(class_prop)
            if rna_prop:
                rna_parent = rna_parent.base
                while rna_parent and rna_prop == rna_parent.properties.get(class_prop):
                    class_name = rna_parent.identifier
                    rna_parent = rna_parent.base

                if do_url:
                    url = "{:s}/bpy.types.{:s}.html#bpy.types.{:s}.{:s}".format(
                        url_prefix, class_name, class_name, class_prop,
                    )
                else:
                    rna = "bpy.types.{:s}.{:s}".format(class_name, class_prop)
            else:
                # We assume this is custom property, only try to generate generic url/rna_id...
                if do_url:
                    url = ("{:s}/bpy.types.bpy_struct.html#bpy.types.bpy_struct.items".format(url_prefix))
                else:
                    rna = "bpy.types.bpy_struct"

    return url if do_url else rna


class WM_OT_doc_view_manual(Operator):
    """Load online manual"""
    bl_idname = "wm.doc_view_manual"
    bl_label = "View Manual"

    doc_id: doc_id

    @staticmethod
    def _find_reference(rna_id, url_mapping, *, verbose=True):
        if verbose:
            print("online manual check for: '{:s}'... ".format(rna_id))
        from fnmatch import fnmatchcase
        # XXX, for some reason all RNA ID's are stored lowercase
        # Adding case into all ID's isn't worth the hassle so force lowercase.
        rna_id = rna_id.lower()

        # NOTE: `fnmatch` in Python is slow as it translates the string to a regular-expression
        # which needs to be compiled (as of Python 3.11), this is slow enough to cause a noticeable
        # delay when opening manual links (approaching half a second).
        #
        # Resolve by matching characters that have a special meaning to `fnmatch`.
        # The characters that can occur as the first special character are `*?[`.
        # If any of these are used we must let `fnmatch` run its own matching logic.
        # However, in most cases a literal prefix is used making it considerably faster
        # to do a simple `startswith` check before performing a full match.
        # An alternative solution could be to use `fnmatch` from C which is significantly
        # faster than Python's, see !104581 for details.
        import re
        re_match_non_special = re.compile(r"^[^?\*\[]+").match

        for pattern, url_suffix in url_mapping:

            # Simple optimization, makes a big difference (over 50x speedup).
            # Even when `non_special.end()` is zero (resulting in an empty-string),
            # the `startswith` check succeeds so there is no need to check for an empty match.
            non_special = re_match_non_special(pattern)
            if non_special is None or not rna_id.startswith(pattern[:non_special.end()]):
                continue
            # End simple optimization.

            if fnmatchcase(rna_id, pattern):
                if verbose:
                    print("            match found: '{:s}' --> '{:s}'".format(pattern, url_suffix))
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
        rna_id = _wm_doc_get_id(self.doc_id, do_url=False, report=self.report)
        if rna_id is None:
            return {'CANCELLED'}

        url = self._lookup_rna_url(rna_id)

        if url is None:
            self.report(
                {'WARNING'},
                rpt_("No reference available {!r}, "
                     "update info in '_rna_manual_reference.py' "
                     "or callback to bpy.utils.manual_map()").format(self.doc_id)
            )
            return {'CANCELLED'}
        else:
            return bpy.ops.wm.url_open(url=url)


class WM_OT_doc_view(Operator):
    """Open online reference docs in a web browser"""
    bl_idname = "wm.doc_view"
    bl_label = "View Documentation"

    doc_id: doc_id
    _prefix = "https://docs.blender.org/api/{:d}.{:d}".format(*bpy.app.version[:2])

    def execute(self, _context):
        url = _wm_doc_get_id(self.doc_id, do_url=True, url_prefix=self._prefix, report=self.report)
        if url is None:
            return {'CANCELLED'}

        return bpy.ops.wm.url_open(url=url)


rna_path = StringProperty(
    name="Property Edit",
    description="Property data_path edit",
    maxlen=1024,
    options={'HIDDEN'},
)

rna_custom_property_name = StringProperty(
    name="Property Name",
    description="Property name edit",
    # Match `MAX_IDPROP_NAME - 1` in Blender's source.
    maxlen=63,
)

# Most useful entries of rna_enum_property_subtype_items:
rna_custom_property_type_items = (
    ('FLOAT', "Float", "A single floating-point value"),
    ('FLOAT_ARRAY', "Float Array", "An array of floating-point values"),
    ('INT', "Integer", "A single integer"),
    ('INT_ARRAY', "Integer Array", "An array of integers"),
    ('BOOL', "Boolean", "A true or false value"),
    ('BOOL_ARRAY', "Boolean Array", "An array of true or false values"),
    ('STRING', "String", "A string value"),
    ('DATA_BLOCK', "Data-Block", "A data-block value"),
    ('PYTHON', "Python", "Edit a Python value directly, for unsupported property types"),
)

rna_custom_property_subtype_none_item = (
    'NONE', n_("Plain Data", i18n_contexts.unit), n_("Data values without special behavior")
)

rna_custom_property_subtype_number_items = (
    rna_custom_property_subtype_none_item,
    ('PIXEL', n_("Pixel", i18n_contexts.unit), n_("A distance on screen")),
    ('PERCENTAGE', n_("Percentage", i18n_contexts.unit), n_("A percentage between 0 and 100")),
    ('FACTOR', n_("Factor", i18n_contexts.unit), n_("A factor between 0.0 and 1.0")),
    ('ANGLE', n_("Angle", i18n_contexts.unit), n_("A rotational value specified in radians")),
    ('TIME_ABSOLUTE', n_("Time", i18n_contexts.unit), n_("Time specified in seconds")),
    ('DISTANCE', n_("Distance", i18n_contexts.unit), n_("A distance between two points")),
    ('POWER', n_("Power", i18n_contexts.unit), ""),
    ('TEMPERATURE', n_("Temperature", i18n_contexts.unit), ""),
)

rna_custom_property_subtype_vector_items = (
    rna_custom_property_subtype_none_item,
    ('COLOR', n_("Linear Color", i18n_contexts.unit), n_("Color in the linear space")),
    ('COLOR_GAMMA', n_("Gamma-Corrected Color", i18n_contexts.unit), n_("Color in the gamma corrected space")),
    ('TRANSLATION', n_("Translation", i18n_contexts.unit), ""),
    ('DIRECTION', n_("Direction", i18n_contexts.unit), ""),
    ('VELOCITY', n_("Velocity", i18n_contexts.unit), ""),
    ('ACCELERATION', n_("Acceleration", i18n_contexts.unit), ""),
    ('EULER', n_("Euler Angles", i18n_contexts.unit), n_("Euler rotation angles in radians")),
    ('QUATERNION', n_("Quaternion Rotation", i18n_contexts.unit), n_("Quaternion rotation (affects NLA blending)")),
    ('AXISANGLE', n_("Axis-Angle", i18n_contexts.unit), n_("Angle and axis to rotate around")),
    ('XYZ', n_("XYZ", i18n_contexts.unit), ""),
)

rna_id_type_items = tuple(
    (item.identifier, item.name, item.description, item.icon, item.value)
    for item in bpy.types.ID.bl_rna.properties["id_type"].enum_items
)


class WM_OT_properties_edit(Operator):
    """Change a custom property's type, or adjust how it is displayed in the interface"""
    bl_idname = "wm.properties_edit"
    bl_label = "Edit Property"
    # register only because invoke_props_popup requires.
    bl_options = {'REGISTER', 'INTERNAL'}

    def subtype_items_cb(self, context):
        match self.property_type:
            case 'FLOAT':
                return rna_custom_property_subtype_number_items
            case 'FLOAT_ARRAY':
                return rna_custom_property_subtype_vector_items
            case _:
                # Needed so 'NONE' can always be assigned.
                return (
                    rna_custom_property_subtype_none_item,
                )

    def property_type_update_cb(self, context):
        self.subtype = 'NONE'

    # Common settings used for all property types. Generally, separate properties are used for each
    # type to improve the experience when choosing UI data values.

    data_path: rna_path
    property_name: rna_custom_property_name
    property_type: EnumProperty(
        name="Type",
        items=rna_custom_property_type_items,
        update=property_type_update_cb,
    )
    is_overridable_library: BoolProperty(
        name="Library Overridable",
        description="Allow the property to be overridden when the data-block is linked",
        default=False,
    )
    description: StringProperty(
        name="Description",
    )

    # Shared for integer and string properties.

    use_soft_limits: BoolProperty(
        name="Soft Limits",
        description=(
            "Limits the Property Value slider to a range, "
            "values outside the range must be inputted numerically"
        ),
    )
    array_length: IntProperty(
        name="Array Length",
        default=3,
        min=1,
        # 32 is the maximum size for RNA array properties.
        max=32,
    )

    # Integer properties.

    # This property stores values for both array and non-array properties.
    default_int: IntVectorProperty(
        name="Default Value",
        size=32,
    )
    min_int: IntProperty(
        name="Min",
        default=-10000,
    )
    max_int: IntProperty(
        name="Max",
        default=10000,
    )
    soft_min_int: IntProperty(
        name="Soft Min",
        default=-10000,
    )
    soft_max_int: IntProperty(
        name="Soft Max",
        default=10000,
    )
    step_int: IntProperty(
        name="Step",
        min=1,
        default=1,
    )

    # Boolean properties.

    # This property stores values for both array and non-array properties.
    default_bool: BoolVectorProperty(
        name="Default Value",
        size=32,
    )

    # Float properties.

    # This property stores values for both array and non-array properties.
    default_float: FloatVectorProperty(
        name="Default Value",
        size=32,
    )
    min_float: FloatProperty(
        name="Min",
        default=-10000.0,
    )
    max_float: FloatProperty(
        name="Max",
        default=-10000.0,
    )
    soft_min_float: FloatProperty(
        name="Soft Min",
        default=-10000.0,
    )
    soft_max_float: FloatProperty(
        name="Soft Max",
        default=-10000.0,
    )
    precision: IntProperty(
        name="Precision",
        default=3,
        min=0,
        max=8,
    )
    step_float: FloatProperty(
        name="Step",
        default=0.1,
        min=0.001,
    )
    subtype: EnumProperty(
        name="Subtype",
        items=subtype_items_cb,
        translation_context=i18n_contexts.unit,
    )

    # String properties.

    default_string: StringProperty(
        name="Default Value",
        maxlen=1024,
    )

    # Data-block properties.

    id_type: EnumProperty(
        name="ID Type",
        items=rna_id_type_items,
        translation_context=i18n_contexts.id_id,
        default='OBJECT',
    )

    # Store the value converted to a string as a fallback for otherwise unsupported types.
    eval_string: StringProperty(
        name="Value",
        description="Python value for unsupported custom property types",
    )

    # Helper method to avoid repetitive code to retrieve a single value from sequences and non-sequences.
    @staticmethod
    def _convert_new_value_single(old_value, new_type):
        if hasattr(old_value, "__len__") and len(old_value) > 0:
            return new_type(old_value[0])
        return new_type(old_value)

    # Helper method to create a list of a given value and type, using a sequence or non-sequence old value.
    @staticmethod
    def _convert_new_value_array(old_value, new_type, new_len):
        if hasattr(old_value, "__len__"):
            new_array = [new_type()] * new_len
            for i in range(min(len(old_value), new_len)):
                new_array[i] = new_type(old_value[i])
            return new_array
        return [new_type(old_value)] * new_len

    # Convert an old property for a string, avoiding unhelpful string representations for custom list types.
    @staticmethod
    def convert_custom_property_to_string(item, name):
        # The IDProperty group view API currently doesn't have a "lookup" method.
        for key, value in item.items():
            if key == name:
                old_value = value
                break

        # In order to get a better string conversion, convert the property to a builtin sequence type first.
        to_dict = getattr(old_value, "to_dict", None)
        to_list = getattr(old_value, "to_list", None)
        if to_dict:
            old_value = to_dict()
        elif to_list:
            old_value = to_list()

        return str(old_value)

    # Retrieve the current type of the custom property on the RNA struct. Some properties like group properties
    # can be created in the UI, but editing their meta-data isn't supported. In that case, return 'PYTHON'.
    @staticmethod
    def get_property_type(item, property_name):
        from rna_prop_ui import (
            rna_idprop_value_item_type,
        )

        prop_value = item[property_name]

        prop_type, is_array = rna_idprop_value_item_type(prop_value)
        if prop_type == int:
            if is_array:
                return 'INT_ARRAY'
            return 'INT'
        elif prop_type == float:
            if is_array:
                return 'FLOAT_ARRAY'
            return 'FLOAT'
        elif prop_type == bool:
            if is_array:
                return 'BOOL_ARRAY'
            return 'BOOL'
        elif prop_type == str:
            if is_array:
                return 'PYTHON'
            return 'STRING'
        elif prop_type == type(None) or issubclass(prop_type, bpy.types.ID):
            if is_array:
                return 'PYTHON'
            return 'DATA_BLOCK'

        return 'PYTHON'

    # For `DATA_BLOCK` types, return the `id_type` or an empty string for non data-block types.
    @staticmethod
    def get_property_id_type(item, property_name):
        ui_data = item.id_properties_ui(property_name)
        rna_data = ui_data.as_dict()
        # For non `DATA_BLOCK` types, the `id_type` wont exist.
        return rna_data.get("id_type", "")

    def _init_subtype(self, subtype):
        self.subtype = subtype or 'NONE'

    # Fill the operator's properties with the UI data properties from the existing custom property.
    # Note that if the UI data doesn't exist yet, the access will create it and use those default values.
    def _fill_old_ui_data(self, item, name):
        ui_data = item.id_properties_ui(name)
        rna_data = ui_data.as_dict()

        if self.property_type in {'FLOAT', 'FLOAT_ARRAY'}:
            self.min_float = rna_data["min"]
            self.max_float = rna_data["max"]
            self.soft_min_float = rna_data["soft_min"]
            self.soft_max_float = rna_data["soft_max"]
            self.precision = rna_data["precision"]
            self.step_float = rna_data["step"]
            if rna_data["subtype"] in [item[0] for item in self.subtype_items_cb(None)]:
                self.subtype = rna_data["subtype"]
            self.use_soft_limits = (
                self.min_float != self.soft_min_float or
                self.max_float != self.soft_max_float
            )
            default = self._convert_new_value_array(rna_data["default"], float, 32)
            self.default_float = default if isinstance(default, list) else [default] * 32
        elif self.property_type in {'INT', 'INT_ARRAY'}:
            self.min_int = rna_data["min"]
            self.max_int = rna_data["max"]
            self.soft_min_int = rna_data["soft_min"]
            self.soft_max_int = rna_data["soft_max"]
            self.step_int = rna_data["step"]
            self.use_soft_limits = (
                self.min_int != self.soft_min_int or
                self.max_int != self.soft_max_int
            )
            self.default_int = self._convert_new_value_array(rna_data["default"], int, 32)
        elif self.property_type == 'STRING':
            self.default_string = rna_data["default"]
        elif self.property_type in {'BOOL', 'BOOL_ARRAY'}:
            self.default_bool = self._convert_new_value_array(rna_data["default"], bool, 32)
        elif self.property_type == 'DATA_BLOCK':
            self.id_type = rna_data["id_type"]

        if self.property_type in {'FLOAT_ARRAY', 'INT_ARRAY', 'BOOL_ARRAY'}:
            self.array_length = len(item[name])

        # The dictionary does not contain the description if it was empty.
        self.description = rna_data.get("description", "")

        self._init_subtype(self.subtype)
        escaped_name = bpy.utils.escape_identifier(name)
        self.is_overridable_library = bool(item.is_property_overridable_library('["{:s}"]'.format(escaped_name)))

    # When the operator chooses a different type than the original property,
    # attempt to convert the old value to the new type for continuity and speed.
    def _get_converted_value(self, item, name_old, prop_type_new, id_type_old, id_type_new):
        if prop_type_new == 'INT':
            return self._convert_new_value_single(item[name_old], int)
        elif prop_type_new == 'FLOAT':
            return self._convert_new_value_single(item[name_old], float)
        elif prop_type_new == 'BOOL':
            return self._convert_new_value_single(item[name_old], bool)
        elif prop_type_new == 'INT_ARRAY':
            prop_type_old = self.get_property_type(item, name_old)
            if prop_type_old in {'INT', 'FLOAT', 'INT_ARRAY', 'FLOAT_ARRAY', 'BOOL_ARRAY'}:
                return self._convert_new_value_array(item[name_old], int, self.array_length)
        elif prop_type_new == 'FLOAT_ARRAY':
            prop_type_old = self.get_property_type(item, name_old)
            if prop_type_old in {'INT', 'FLOAT', 'FLOAT_ARRAY', 'INT_ARRAY', 'BOOL_ARRAY'}:
                return self._convert_new_value_array(item[name_old], float, self.array_length)
        elif prop_type_new == 'BOOL_ARRAY':
            prop_type_old = self.get_property_type(item, name_old)
            if prop_type_old in {'INT', 'FLOAT', 'FLOAT_ARRAY', 'INT_ARRAY', 'BOOL_ARRAY'}:
                return self._convert_new_value_array(item[name_old], bool, self.array_length)
            else:
                return [False] * self.array_length
        elif prop_type_new == 'STRING':
            return self.convert_custom_property_to_string(item, name_old)
        elif prop_type_new == 'DATA_BLOCK':
            if id_type_old != id_type_new:
                return None
            old_value = item[name_old]
            if not isinstance(old_value, bpy.types.ID):
                return None
            return old_value

        # If all else fails, create an empty string property. That should avoid errors later on anyway.
        return ""

    # Any time the target type is changed in the dialog, it's helpful to convert the UI data values
    # to the new type as well, when possible, currently this only applies for floats and ints.
    def _convert_old_ui_data_to_new_type(self, prop_type_old, prop_type_new):
        if prop_type_new in {'INT', 'INT_ARRAY'} and prop_type_old in {'FLOAT', 'FLOAT_ARRAY'}:
            self.min_int = int(self.min_float)
            self.max_int = int(self.max_float)
            self.soft_min_int = int(self.soft_min_float)
            self.soft_max_int = int(self.soft_max_float)
            self.default_int = self._convert_new_value_array(self.default_float, int, 32)
        elif prop_type_new in {'FLOAT', 'FLOAT_ARRAY'} and prop_type_old in {'INT', 'INT_ARRAY'}:
            self.min_float = float(self.min_int)
            self.max_float = float(self.max_int)
            self.soft_min_float = float(self.soft_min_int)
            self.soft_max_float = float(self.soft_max_int)
            self.default_float = self._convert_new_value_array(self.default_int, float, 32)
        elif prop_type_new in {'BOOL', 'BOOL_ARRAY'} and prop_type_old in {'INT', 'INT_ARRAY'}:
            self.default_bool = self._convert_new_value_array(self.default_int, bool, 32)

        # Don't convert between string and float/int defaults here, it's not expected like the other conversions.

    # Fill the property's UI data with the values chosen in the operator.
    def _create_ui_data_for_new_prop(self, item, name, prop_type_new):
        if prop_type_new in {'INT', 'INT_ARRAY'}:
            ui_data = item.id_properties_ui(name)
            ui_data.update(
                min=self.min_int,
                max=self.max_int,
                soft_min=self.soft_min_int if self.use_soft_limits else self.min_int,
                soft_max=self.soft_max_int if self.use_soft_limits else self.max_int,
                step=self.step_int,
                default=self.default_int[0] if prop_type_new == 'INT' else self.default_int[:self.array_length],
                description=self.description,
            )
        elif prop_type_new in {'BOOL', 'BOOL_ARRAY'}:
            ui_data = item.id_properties_ui(name)
            ui_data.update(
                default=self.default_bool[0] if prop_type_new == 'BOOL' else self.default_bool[:self.array_length],
                description=self.description,
            )
        elif prop_type_new in {'FLOAT', 'FLOAT_ARRAY'}:
            ui_data = item.id_properties_ui(name)
            ui_data.update(
                min=self.min_float,
                max=self.max_float,
                soft_min=self.soft_min_float if self.use_soft_limits else self.min_float,
                soft_max=self.soft_max_float if self.use_soft_limits else self.max_float,
                step=self.step_float,
                precision=self.precision,
                default=self.default_float[0] if prop_type_new == 'FLOAT' else self.default_float[:self.array_length],
                description=self.description,
                subtype=self.subtype,
            )
        elif prop_type_new == 'STRING':
            ui_data = item.id_properties_ui(name)
            ui_data.update(
                default=self.default_string,
                description=self.description,
            )
        elif prop_type_new == 'DATA_BLOCK':
            ui_data = item.id_properties_ui(name)
            ui_data.update(
                description=self.description,
                id_type=self.id_type,
            )

        escaped_name = bpy.utils.escape_identifier(name)
        item.property_overridable_library_set('["{:s}"]'.format(escaped_name), self.is_overridable_library)

    def _update_blender_for_prop_change(self, context, item, name, prop_type_old, prop_type_new):
        from bpy_extras import anim_utils
        from rna_prop_ui import (
            rna_idprop_ui_prop_update,
        )

        rna_idprop_ui_prop_update(item, name)

        # If we have changed the type of the property, update its potential anim curves!
        if prop_type_old != prop_type_new:
            escaped_name = bpy.utils.escape_identifier(name)
            data_path = '["{:s}"]'.format(escaped_name)
            done = set()

            def _update(fcurves):
                for fcu in fcurves:
                    if fcu not in done and fcu.data_path == data_path:
                        fcu.update_autoflags(item)
                        done.add(fcu)

            def _update_strips(strips):
                for st in strips:
                    if st.type == 'CLIP':
                        channelbag = anim_utils.action_get_channelbag_for_slot(st.action, st.action_slot)
                        if not channelbag:
                            continue
                        _update(channelbag.fcurves)
                    elif st.type == 'META':
                        _update_strips(st.strips)

            adt = getattr(item, "animation_data", None)
            if adt is not None:
                channelbag = anim_utils.action_get_channelbag_for_slot(adt.action, adt.action_slot)
                if channelbag:
                    _update(channelbag.fcurves)
                if adt.drivers:
                    _update(adt.drivers)
                if adt.nla_tracks:
                    for nt in adt.nla_tracks:
                        _update_strips(nt.strips)

        # Otherwise existing buttons which reference freed memory may crash Blender (#26510).
        for win in context.window_manager.windows:
            for area in win.screen.areas:
                area.tag_redraw()

    def execute(self, context):
        name_old = getattr(self, "_old_prop_name", [None])[0]
        if name_old is None:
            self.report({'ERROR'}, "Direct execution not supported")
            return {'CANCELLED'}

        data_path = self.data_path
        name = self.property_name

        item = eval("context.{:s}".format(data_path))
        if (item.id_data and item.id_data.override_library and item.id_data.override_library.reference):
            self.report({'ERROR'}, "Cannot edit properties from override data")
            return {'CANCELLED'}

        prop_type_old = self.get_property_type(item, name_old)
        prop_type_new = self.property_type
        self._old_prop_name[:] = [name]

        id_type_old = self.get_property_id_type(item, name_old)
        id_type_new = self.id_type

        if prop_type_new == 'PYTHON':
            try:
                new_value = eval(self.eval_string)
            except Exception as ex:
                self.report({'WARNING'}, rpt_("Python evaluation failed: {:s}").format(str(ex)))
                return {'CANCELLED'}
            try:
                item[name] = new_value
            except Exception as ex:
                self.report({'ERROR'}, rpt_("Failed to assign value: {:s}").format(str(ex)))
                return {'CANCELLED'}
            if name_old != name:
                del item[name_old]
        else:
            new_value = self._get_converted_value(item, name_old, prop_type_new, id_type_old, id_type_new)
            del item[name_old]
            item[name] = new_value

            self._create_ui_data_for_new_prop(item, name, prop_type_new)

        self._update_blender_for_prop_change(context, item, name, prop_type_old, prop_type_new)

        if name_old != name:
            adt = getattr(item, "animation_data", None)
            if adt is not None:
                adt.fix_paths_rename_all(prefix="", old_name=name_old, new_name=name)

        return {'FINISHED'}

    def invoke(self, context, _event):
        data_path = self.data_path
        if not data_path:
            self.report({'ERROR'}, "Data path not set")
            return {'CANCELLED'}

        name = self.property_name

        self._old_prop_name = [name]

        item = eval("context.{:s}".format(data_path))
        if (item.id_data and item.id_data.override_library and item.id_data.override_library.reference):
            self.report({'ERROR'}, "Properties from override data cannot be edited")
            return {'CANCELLED'}

        # Set operator's property type with the type of the existing property, to display the right settings.
        old_type = self.get_property_type(item, name)
        self.property_type = old_type
        self.last_property_type = old_type

        # So that the operator can do something for unsupported properties, change the property into
        # a string, just for editing in the dialog. When the operator executes, it will be converted back
        # into a python value. Always do this conversion, in case the Python property edit type is selected.
        self.eval_string = self.convert_custom_property_to_string(item, name)

        if old_type != 'PYTHON':
            self._fill_old_ui_data(item, name)

        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def check(self, context):
        changed = False

        # In order to convert UI data between types for type changes before the operator has actually executed,
        # compare against the type the last time the check method was called (the last time a value was edited).
        if self.property_type != self.last_property_type:
            self._convert_old_ui_data_to_new_type(self.last_property_type, self.property_type)
            changed = True

        # Make sure that min is less than max, soft range is inside hard range, etc.
        if self.property_type in {'FLOAT', 'FLOAT_ARRAY'}:
            if self.min_float > self.max_float:
                self.min_float, self.max_float = self.max_float, self.min_float
                changed = True
            if self.use_soft_limits:
                if self.soft_min_float > self.soft_max_float:
                    self.soft_min_float, self.soft_max_float = self.soft_max_float, self.soft_min_float
                    changed = True
                if self.soft_max_float > self.max_float:
                    self.soft_max_float = self.max_float
                    changed = True
                if self.soft_min_float < self.min_float:
                    self.soft_min_float = self.min_float
                    changed = True
        elif self.property_type in {'INT', 'INT_ARRAY'}:
            if self.min_int > self.max_int:
                self.min_int, self.max_int = self.max_int, self.min_int
                changed = True
            if self.use_soft_limits:
                if self.soft_min_int > self.soft_max_int:
                    self.soft_min_int, self.soft_max_int = self.soft_max_int, self.soft_min_int
                    changed = True
                if self.soft_max_int > self.max_int:
                    self.soft_max_int = self.max_int
                    changed = True
                if self.soft_min_int < self.min_int:
                    self.soft_min_int = self.min_int
                    changed = True

        self.last_property_type = self.property_type

        return changed

    def draw(self, _context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        layout.prop(self, "property_type")
        layout.prop(self, "property_name")

        if self.property_type in {'FLOAT', 'FLOAT_ARRAY'}:
            if self.property_type == 'FLOAT_ARRAY':
                layout.prop(self, "array_length")
                col = layout.column(align=True)
                col.prop(self, "default_float", index=0, text="Default")
                for i in range(1, self.array_length):
                    col.prop(self, "default_float", index=i, text=" ")
            else:
                layout.prop(self, "default_float", index=0)

            col = layout.column(align=True)
            col.prop(self, "min_float")
            col.prop(self, "max_float")

            col = layout.column()
            col.prop(self, "use_soft_limits")

            col = layout.column(align=True)
            col.enabled = self.use_soft_limits
            col.prop(self, "soft_min_float", text="Soft Min")
            col.prop(self, "soft_max_float", text="Max")

            layout.prop(self, "step_float")
            layout.prop(self, "precision")

            layout.prop(self, "subtype")
        elif self.property_type in {'INT', 'INT_ARRAY'}:
            if self.property_type == 'INT_ARRAY':
                layout.prop(self, "array_length")
                col = layout.column(align=True)
                col.prop(self, "default_int", index=0, text="Default")
                for i in range(1, self.array_length):
                    col.prop(self, "default_int", index=i, text=" ")
            else:
                layout.prop(self, "default_int", index=0)

            col = layout.column(align=True)
            col.prop(self, "min_int")
            col.prop(self, "max_int")

            col = layout.column()
            col.prop(self, "use_soft_limits")

            col = layout.column(align=True)
            col.enabled = self.use_soft_limits
            col.prop(self, "soft_min_int", text="Soft Min")
            col.prop(self, "soft_max_int", text="Max")

            layout.prop(self, "step_int")
        elif self.property_type in {'BOOL', 'BOOL_ARRAY'}:
            if self.property_type == 'BOOL_ARRAY':
                layout.prop(self, "array_length")
                col = layout.column(align=True)
                col.prop(self, "default_bool", index=0, text="Default")
                for i in range(1, self.array_length):
                    col.prop(self, "default_bool", index=i, text=" ")
            else:
                layout.prop(self, "default_bool", index=0)
        elif self.property_type == 'STRING':
            layout.prop(self, "default_string")
        elif self.property_type == 'DATA_BLOCK':
            layout.prop(self, "id_type")

        if self.property_type == 'PYTHON':
            layout.prop(self, "eval_string")
        else:
            layout.prop(self, "description")

        layout.prop(self, "is_overridable_library")


# Edit the value of a custom property with the given name on the RNA struct at the given data path.
# For supported types, this simply acts as a convenient way to create a popup for a specific property
# and draws the custom property value directly in the popup. For types like groups which can't be edited
# directly with buttons, instead convert the value to a string, evaluate the changed string when executing.
class WM_OT_properties_edit_value(Operator):
    """Edit the value of a custom property"""
    bl_idname = "wm.properties_edit_value"
    bl_label = "Edit Property Value"
    # register only because invoke_props_popup requires.
    bl_options = {'REGISTER', 'INTERNAL'}

    data_path: rna_path
    property_name: rna_custom_property_name

    # Store the value converted to a string as a fallback for otherwise unsupported types.
    eval_string: StringProperty(
        name="Value",
        description="Value for custom property types that can only be edited as a Python expression",
    )

    def execute(self, context):
        if self.eval_string:
            rna_item = eval("context.{:s}".format(self.data_path))
            try:
                new_value = eval(self.eval_string)
            except Exception as ex:
                self.report({'WARNING'}, "Python evaluation failed: " + str(ex))
                return {'CANCELLED'}
            rna_item[self.property_name] = new_value
        return {'FINISHED'}

    def invoke(self, context, _event):
        rna_item = eval("context.{:s}".format(self.data_path))

        if WM_OT_properties_edit.get_property_type(rna_item, self.property_name) == 'PYTHON':
            self.eval_string = WM_OT_properties_edit.convert_custom_property_to_string(rna_item, self.property_name)
        else:
            self.eval_string = ""

        wm = context.window_manager
        return wm.invoke_props_dialog(self)

    def draw(self, context):
        from bpy.utils import escape_identifier

        rna_item = eval("context.{:s}".format(self.data_path))

        layout = self.layout
        if WM_OT_properties_edit.get_property_type(rna_item, self.property_name) == 'PYTHON':
            layout.prop(self, "eval_string")
        else:
            col = layout.column(align=True)
            col.prop(rna_item, '["{:s}"]'.format(escape_identifier(self.property_name)), text="")


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
        item = eval("context.{:s}".format(data_path))

        if (item.id_data and item.id_data.override_library and item.id_data.override_library.reference):
            self.report({'ERROR'}, "Cannot add properties to override data")
            return {'CANCELLED'}

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
    property_name: rna_custom_property_name

    def execute(self, context):
        from rna_prop_ui import (
            rna_idprop_ui_prop_update,
        )
        data_path = self.data_path
        item = eval("context.{:s}".format(data_path))

        if (item.id_data and item.id_data.override_library and item.id_data.override_library.reference):
            self.report({'ERROR'}, "Cannot remove properties from override data")
            return {'CANCELLED'}

        name = self.property_name
        rna_idprop_ui_prop_update(item, name)
        del item[name]

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
        from _bpy_internal.system_info.text_generate_runtime import write
        with open(self.filepath, "w", encoding="utf-8") as output:
            try:
                write(output)
            except Exception as ex:
                # Not expected to occur, simply forward the exception.
                self.report({'ERROR'}, str(ex))

                # Also write into the file (to avoid confusion).
                output.write("ERROR: {:s}\n".format(str(ex)))
                return {'CANCELLED'}

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
    """List all the operators in a text-block, useful for scripting"""
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
        textblock.write("# {:d} Operators\n\n".format(tot))
        textblock.write("\n".join(op_strings))
        self.report({'INFO'}, "See OperatorList.txt text block")
        return {'FINISHED'}


# -----------------------------------------------------------------------------
# Add-on Operators

class WM_OT_owner_enable(Operator):
    """Enable add-on for workspace"""
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
    """Disable add-on for workspace"""
    bl_idname = "wm.owner_disable"
    bl_label = "Disable Add-on"

    owner_id: StringProperty(
        name="UI Tag",
    )

    def execute(self, context):
        workspace = context.workspace
        owner_id = workspace.owner_ids[self.owner_id]
        workspace.owner_ids.remove(owner_id)
        return {'FINISHED'}


class WM_OT_tool_set_by_id(Operator):
    """Set the tool by name (for key-maps)"""
    bl_idname = "wm.tool_set_by_id"
    bl_label = "Set Tool by Name"

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

    @staticmethod
    def space_type_from_operator(op, context):
        if op.properties.is_property_set("space_type"):
            space_type = op.space_type
        else:
            space = context.space_data
            if space is None:
                op.report({'WARNING'}, rpt_("Tool cannot be set with an empty space"))
                return None
            space_type = space.type
        return space_type

    def execute(self, context):
        from bl_ui.space_toolsystem_common import (
            activate_by_id,
            activate_by_id_or_cycle,
        )

        if (space_type := WM_OT_tool_set_by_id.space_type_from_operator(self, context)) is None:
            return {'CANCELLED'}

        fn = activate_by_id_or_cycle if self.cycle else activate_by_id
        if fn(context, space_type, self.name, as_fallback=self.as_fallback):
            if self.as_fallback:
                tool_settings = context.tool_settings
                tool_settings.workspace_tool_type = 'FALLBACK'
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, rpt_("Tool {!r} not found for space {!r}").format(self.name, space_type))
            return {'CANCELLED'}


class WM_OT_tool_set_by_index(Operator):
    """Set the tool by index (for key-maps)"""
    bl_idname = "wm.tool_set_by_index"
    bl_label = "Set Tool by Index"
    index: IntProperty(
        name="Index in Toolbar",
        default=0,
    )
    cycle: BoolProperty(
        name="Cycle",
        description="Cycle through tools in this group",
        default=False,
        options={'SKIP_SAVE'},
    )

    expand: BoolProperty(
        description="Include tool subgroups",
        default=True,
        options={'SKIP_SAVE'},
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

        if (space_type := WM_OT_tool_set_by_id.space_type_from_operator(self, context)) is None:
            return {'CANCELLED'}

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


class WM_OT_tool_set_by_brush_type(Operator):
    """Look up the most appropriate tool for the given brush type and activate that"""
    bl_idname = "wm.tool_set_by_brush_type"
    bl_label = "Set Tool by Brush Type"

    brush_type: StringProperty(
        name="Brush Type",
        description="Brush type identifier for which the most appropriate tool will be looked up",
    )

    space_type: rna_space_type_prop

    def execute(self, context):
        from bl_ui.space_toolsystem_common import (
            ToolSelectPanelHelper,
            activate_by_id
        )

        if (space_type := WM_OT_tool_set_by_id.space_type_from_operator(self, context)) is None:
            return {'CANCELLED'}

        tool_helper_cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
        # Lookup a tool with a matching brush type (ignoring some specific ones).
        tool_id = "builtin.brush"
        for item in ToolSelectPanelHelper._tools_flatten(
                tool_helper_cls.tools_from_context(context, mode=context.mode),
        ):
            if item is None:
                continue

            # Never automatically activate these tools, they use a brush type that we want to use
            # the main brush for (e.g. grease pencil primitive tools use 'DRAW' brush type, which
            # is the most general one).
            if item.idname in {
                    "builtin.arc",
                    "builtin.curve",
                    "builtin.line",
                    "builtin.box",
                    "builtin.circle",
                    "builtin.polyline",
            }:
                continue

            if item.options is not None and ('USE_BRUSHES' in item.options) and item.brush_type is not None:
                if item.brush_type == self.brush_type:
                    tool_id = item.idname
                    break

        if activate_by_id(context, space_type, tool_id):
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, rpt_("Tool {!r} not found for space {!r}").format(tool_id, space_type))
            return {'CANCELLED'}


class WM_OT_toolbar(Operator):
    bl_idname = "wm.toolbar"
    bl_label = "Toolbar"

    @classmethod
    def poll(cls, context):
        return context.space_data is not None

    @staticmethod
    def keymap_from_toolbar(context, space_type, *, use_fallback_keys=True, use_reset=True):
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
        wm.popup_menu_pie(draw_func=draw_cb, title=iface_("Fallback Tool"), event=event)
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
                if not (event.ctrl or event.alt or event.shift or event.oskey or event.hyper):
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
                row.template_event_from_keymap_item(
                    item, text=name, text_ctxt=i18n_contexts.operator_default
                )

        self._keymap = keymap

        context.workspace.status_text_set(status_text_fn)

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


class BatchRenameAction(bpy.types.PropertyGroup):
    __slots__ = ()

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

    # Used when `type == 'SET'`.
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

    # Used when `type == 'STRIP'`.
    strip_chars: EnumProperty(
        name="Strip Characters",
        translation_context=i18n_contexts.id_text,
        options={'ENUM_FLAG'},
        items=(
            ('SPACE', "Spaces", ""),
            ('DIGIT', "Digits", ""),
            ('PUNCT', "Punctuation", ""),
        ),
    )

    # Used when `type == 'STRIP'`.
    strip_part: EnumProperty(
        name="Strip Part",
        options={'ENUM_FLAG'},
        items=(
            ('START', "Start", ""),
            ('END', "End", ""),
        ),
    )

    # Used when `type == 'REPLACE'`.
    replace_src: StringProperty(name="Find")
    replace_dst: StringProperty(name="Replace")
    replace_match_case: BoolProperty(name="Case Sensitive")
    use_replace_regex_src: BoolProperty(
        name="Regular Expression Find",
        description="Use regular expressions to match text in the 'Find' field",
    )
    use_replace_regex_dst: BoolProperty(
        name="Regular Expression Replace",
        description="Use regular expression for the replacement text (supporting groups)",
    )

    # Used when `type == 'CASE'`.
    case_method: EnumProperty(
        name="Case",
        items=(
            ('UPPER', "Upper Case", ""),
            ('LOWER', "Lower Case", ""),
            ('TITLE', "Title Case", ""),
        ),
    )

    # Weak, add/remove as properties.
    op_add: BoolProperty(name="Add", translation_context=i18n_contexts.operator_default)
    op_remove: BoolProperty(name="Remove", translation_context=i18n_contexts.operator_default)


class WM_OT_batch_rename(Operator):
    """Rename multiple items at once"""

    bl_idname = "wm.batch_rename"
    bl_label = "Batch Rename"

    bl_options = {'UNDO'}

    data_type: EnumProperty(
        name="Type",
        items=(
            ('OBJECT', "Objects", "", 'OBJECT_DATA', 0),
            ('COLLECTION', "Collections", "", 'OUTLINER_COLLECTION', 1),
            ('MATERIAL', "Materials", "", 'MATERIAL_DATA', 2),
            None,
            # Enum identifiers are compared with `object.type`.
            # Follow order in "Add" menu.
            ('MESH', "Meshes", "", 'MESH_DATA', 3),
            ('CURVE', "Curves", "", 'CURVE_DATA', 4),
            ('META', "Metaballs", "", 'META_DATA', 5),
            ('VOLUME', "Volumes", "", 'VOLUME_DATA', 6),
            ('GREASEPENCIL', "Grease Pencils", "", 'OUTLINER_DATA_GREASEPENCIL', 7),
            ('ARMATURE', "Armatures", "", 'ARMATURE_DATA', 8),
            ('LATTICE', "Lattices", "", 'LATTICE_DATA', 9),
            ('LIGHT', "Lights", "", 'LIGHT_DATA', 10),
            ('LIGHT_PROBE', "Light Probes", "", 'OUTLINER_DATA_LIGHTPROBE', 11),
            ('CAMERA', "Cameras", "", 'CAMERA_DATA', 12),
            ('SPEAKER', "Speakers", "", 'OUTLINER_DATA_SPEAKER', 13),
            None,
            ('BONE', "Bones", "", 'BONE_DATA', 14),
            ('NODE', "Nodes", "", 'NODETREE', 15),
            ('SEQUENCE_STRIP', "Sequence Strips", "", 'SEQ_SEQUENCER', 16),
            ('ACTION_CLIP', "Action Clips", "", 'ACTION', 17),
            None,
            ('SCENE', "Scenes", "", 'SCENE_DATA', 18),
            ('BRUSH', "Brushes", "", 'BRUSH_DATA', 19),
        ),
        translation_context=i18n_contexts.id_id,
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
    def _selected_ids_from_outliner_by_type(context, ty):
        return [
            id for id in context.selected_ids
            if isinstance(id, ty)
            if id.is_editable
        ]

    @staticmethod
    def _selected_ids_from_outliner_by_type_for_object_data(context, ty):
        # Include selected object-data as well as the selected ID's.
        from bpy.types import Object
        # De-duplicate the result as object-data may cause duplicates.
        return tuple(set([
            id for id_base in context.selected_ids
            if isinstance(id := id_base.data if isinstance(id_base, Object) else id_base, ty)
            if id.is_editable
        ]))

    @staticmethod
    def _selected_actions_from_outliner(context):
        # Actions are a special case because they can be accessed directly or via animation-data.
        from bpy.types import Action

        def action_from_any_id(id_data):
            if isinstance(id_data, Action):
                return id_data
            # Not all ID's have animation data.
            if (animation_data := getattr(id_data, "animation_data", None)) is not None:
                return animation_data.action
            return None

        return tuple(set(
            action for id in context.selected_ids
            if (action := action_from_any_id(id)) is not None
            if action.is_editable
        ))

    @classmethod
    def _data_from_context(cls, context, data_type, only_selected, *, check_context=False):
        def _is_editable(data):
            return data.id_data.is_editable and not data.id_data.override_library

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
                    context.selected_strips
                    if only_selected else
                    scene.sequence_editor.strips_all,
                    "name",
                    iface_("Strip(s)"),
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
                    iface_("Node(s)"),
                )
        elif space_type == 'OUTLINER':
            data_type_test = 'COLLECTION'
            if check_context:
                return data_type_test
            if data_type == data_type_test:
                data = (
                    cls._selected_ids_from_outliner_by_type(context, bpy.types.Collection)
                    if only_selected else
                    scene.collection.children_recursive,
                    "name",
                    iface_("Collection(s)"),
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
                        iface_("Bone(s)"),
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
                        iface_("Edit Bone(s)"),
                    )

        if check_context:
            return 'OBJECT'

        object_data_type_attrs_map = {
            'MESH': ("meshes", iface_("Mesh(es)"), bpy.types.Mesh),
            'CURVE': ("curves", iface_("Curve(s)"), bpy.types.Curve),
            'META': ("metaballs", iface_("Metaball(s)"), bpy.types.MetaBall),
            'VOLUME': ("volumes", iface_("Volume(s)"), bpy.types.Volume),
            'GREASEPENCIL': ("grease_pencils", iface_("Grease Pencil(s)"), bpy.types.GreasePencil),
            'ARMATURE': ("armatures", iface_("Armature(s)"), bpy.types.Armature),
            'LATTICE': ("lattices", iface_("Lattice(s)"), bpy.types.Lattice),
            'LIGHT': ("lights", iface_("Light(s)"), bpy.types.Light),
            'LIGHT_PROBE': ("lightprobes", iface_("Light Probe(s)"), bpy.types.LightProbe),
            'CAMERA': ("cameras", iface_("Camera(s)"), bpy.types.Camera),
            'SPEAKER': ("speakers", iface_("Speaker(s)"), bpy.types.Speaker),
        }

        # Finish with space types.
        if data is None:

            if data_type == 'OBJECT':
                data = (
                    (
                        # Outliner.
                        cls._selected_ids_from_outliner_by_type(context, bpy.types.Object)
                        if space_type == 'OUTLINER' else
                        # 3D View (default).
                        context.selected_editable_objects
                    )
                    if only_selected else
                    [id for id in bpy.data.objects if id.is_editable],
                    "name",
                    iface_("Object(s)"),
                )
            elif data_type == 'COLLECTION':
                data = (
                    # Outliner case is handled already.
                    tuple(set(
                        ob.instance_collection
                        for ob in context.selected_objects
                        if ((ob.instance_type == 'COLLECTION') and
                            (collection := ob.instance_collection) is not None and
                            (collection.is_editable))
                    ))
                    if only_selected else
                    [id for id in bpy.data.collections if id.is_editable],
                    "name",
                    iface_("Collection(s)"),
                )
            elif data_type == 'MATERIAL':
                data = (
                    (
                        # Outliner.
                        cls._selected_ids_from_outliner_by_type(context, bpy.types.Material)
                        if space_type == 'OUTLINER' else
                        # 3D View (default).
                        tuple(set(
                            id
                            for ob in context.selected_objects
                            for slot in ob.material_slots
                            if (id := slot.material) is not None and id.is_editable
                        ))
                    )
                    if only_selected else
                    [id for id in bpy.data.materials if id.is_editable],
                    "name",
                    iface_("Material(s)"),
                )
            elif data_type == 'ACTION_CLIP':
                data = (
                    (
                        # Outliner.
                        cls._selected_actions_from_outliner(context)
                        if space_type == 'OUTLINER' else
                        # 3D View (default).
                        tuple(set(
                            action for ob in context.selected_objects
                            if (((animation_data := ob.animation_data) is not None) and
                                ((action := animation_data.action) is not None) and
                                (action.is_editable))
                        ))
                    )
                    if only_selected else
                    [id for id in bpy.data.actions if id.is_editable],
                    "name",
                    iface_("Action(s)"),
                )
            elif data_type == 'SCENE':
                data = (
                    (
                        # Outliner.
                        cls._selected_ids_from_outliner_by_type(context, bpy.types.Scene)
                        if ((space_type == 'OUTLINER') and only_selected) else
                        [id for id in bpy.data.scenes if id.is_editable]
                    ),
                    "name",
                    iface_("Scene(s)"),
                )
            elif data_type == 'BRUSH':
                data = (
                    (
                        # Outliner.
                        cls._selected_ids_from_outliner_by_type(context, bpy.types.Brush)
                        if ((space_type == 'OUTLINER') and only_selected) else
                        [id for id in bpy.data.brushes if id.is_editable]
                    ),
                    "name",
                    iface_("Brush(es)"),
                )
            elif data_type in object_data_type_attrs_map.keys():
                attr, descr, ty = object_data_type_attrs_map[data_type]
                data = (
                    (
                        # Outliner.
                        cls._selected_ids_from_outliner_by_type_for_object_data(context, ty)
                        if space_type == 'OUTLINER' else
                        # 3D View (default).
                        tuple(set(
                            id
                            for ob in context.selected_objects
                            if ob.type == data_type
                            if (id := ob.data) is not None and id.is_editable
                        ))
                    )
                    if only_selected else
                    [id for id in getattr(bpy.data, attr) if id.is_editable],
                    "name",
                    descr,
                )

        if data is None:
            return None

        data = ([it for it in data[0] if _is_editable(it)], data[1], data[2])

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
                    assert False, "unreachable"

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
                    assert False, "unreachable"
            else:
                assert False, "unreachable"
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

        split = layout.split(align=True)
        split.row(align=True).prop(self, "data_source", expand=True)
        split.prop(self, "data_type", text="")

        for action in self.actions:
            box = layout.box()
            split = box.split(factor=0.87)

            # Column 1: main content.
            col = split.column()

            # Label's width.
            fac = 0.25

            # Row 1: type.
            row = col.split(factor=fac)
            row.alignment = 'RIGHT'
            row.label(text="Type")
            row.prop(action, "type", text="")

            ty = action.type
            if ty == 'SET':
                # Row 2: method.
                row = col.split(factor=fac)
                row.alignment = 'RIGHT'
                row.label(text="Method")
                row.row().prop(action, "set_method", expand=True)

                # Row 3: name.
                row = col.split(factor=fac)
                row.alignment = 'RIGHT'
                row.label(text="Name")
                row.prop(action, "set_name", text="")

            elif ty == 'STRIP':
                # Row 2: chars.
                row = col.split(factor=fac)
                row.alignment = 'RIGHT'
                row.label(text="Characters")
                row.row().prop(action, "strip_chars")

                # Row 3: part.
                row = col.split(factor=fac)
                row.alignment = 'RIGHT'
                row.label(text="Strip From")
                row.row().prop(action, "strip_part")

            elif ty == 'REPLACE':
                # Row 2: find.
                row = col.split(factor=fac)

                re_error_src = None
                if action.use_replace_regex_src:
                    try:
                        re.compile(action.replace_src)
                    except Exception as ex:
                        re_error_src = str(ex)
                        row.alert = True

                row.alignment = 'RIGHT'
                row.label(text="Find")
                sub = row.row(align=True)
                sub.prop(action, "replace_src", text="")
                sub.prop(action, "use_replace_regex_src", text="", icon='SORTBYEXT')

                # Row.
                if re_error_src is not None:
                    row = col.split(factor=fac)
                    row.label(text="")
                    row.alert = True
                    row.label(text=re_error_src)

                # Row 3: replace.
                row = col.split(factor=fac)

                re_error_dst = None
                if action.use_replace_regex_src:
                    if action.use_replace_regex_dst:
                        if re_error_src is None:
                            try:
                                re.sub(action.replace_src, action.replace_dst, "")
                            except Exception as ex:
                                re_error_dst = str(ex)
                                row.alert = True

                row.alignment = 'RIGHT'
                row.label(text="Replace")
                sub = row.row(align=True)
                sub.prop(action, "replace_dst", text="")
                subsub = sub.row(align=True)
                subsub.active = action.use_replace_regex_src
                subsub.prop(action, "use_replace_regex_dst", text="", icon='SORTBYEXT')

                # Row.
                if re_error_dst is not None:
                    row = col.split(factor=fac)
                    row.label(text="")
                    row.alert = True
                    row.label(text=re_error_dst)

                # Row 4: case.
                row = col.split(factor=fac)
                row.label(text="")
                row.prop(action, "replace_match_case")

            elif ty == 'CASE':
                # Row 2: method.
                row = col.split(factor=fac)
                row.alignment = 'RIGHT'
                row.label(text="Convert To")
                row.row().prop(action, "case_method", expand=True)

            # Column 2: add-remove.
            row = split.split(align=True)
            row.prop(action, "op_remove", text="", icon='REMOVE')
            row.prop(action, "op_add", text="", icon='ADD')

        layout.label(text=iface_("Rename {:d} {:s}").format(len(self._data[0]), self._data[2]), translate=False)

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
                    self.report({'ERROR'}, rpt_("Invalid regular expression (find): {:s}").format(str(ex)))
                    return {'CANCELLED'}

                if action.use_replace_regex_dst:
                    try:
                        re.sub(action.replace_src, action.replace_dst, "")
                    except Exception as ex:
                        self.report({'ERROR'}, rpt_("Invalid regular expression (replace): {:s}").format(str(ex)))
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

        self.report({'INFO'}, rpt_("Renamed {:d} of {:d} {:s}").format(change_len, total_len, descr))

        return {'FINISHED'}

    def invoke(self, context, event):

        self._data_update(context)

        if not self.actions:
            self.actions.add()
        wm = context.window_manager
        return wm.invoke_props_dialog(self, width=400)


class WM_MT_splash_quick_setup(Menu):
    bl_label = "Quick Setup"

    def draw(self, context):
        layout = self.layout

        wm = context.window_manager
        prefs = context.preferences

        layout.operator_context = 'EXEC_DEFAULT'

        old_version = bpy.types.PREFERENCES_OT_copy_prev.previous_version()
        can_import = bpy.types.PREFERENCES_OT_copy_prev.poll(context) and old_version

        if can_import:
            layout.label(text="Import Preferences From Previous Version")
            split = layout.split(factor=0.20)  # Left margin.
            split.label()

            split = split.split(factor=0.73)  # Content width.
            col = split.column()
            col.operator(
                "preferences.copy_prev",
                text=iface_("Import Blender {:d}.{:d} Preferences", "Operator").format(*old_version),
                icon='NONE',
                translate=False,
            )
            layout.separator()
            layout.separator(type='LINE')

        if can_import:
            layout.label(text="Create New Preferences")
        else:
            layout.label(text="Quick Setup")

        split = layout.split(factor=0.20)  # Left margin.
        split.label()
        split = split.split(factor=0.73)  # Content width.
        col = split.column()
        col.use_property_split = True
        col.use_property_decorate = False

        # Languages.
        if bpy.app.build_options.international:
            col.prop(prefs.view, "language")

        # Themes.
        sub = col.column(heading="Theme")
        label = bpy.types.USERPREF_MT_interface_theme_presets.bl_label
        if label == "Presets":
            label = "Blender Dark"
        sub.menu("USERPREF_MT_interface_theme_presets", text=label)

        col.separator()

        # Shortcuts.
        kc = wm.keyconfigs.active
        kc_prefs = kc.preferences

        sub = col.column(heading="Keymap")
        text = bpy.path.display_name(kc.name)
        if not text:
            text = "Blender"
        sub.menu("USERPREF_MT_keyconfigs", text=text)

        if hasattr(kc_prefs, "select_mouse"):
            col.row().prop(kc_prefs, "select_mouse", text="Mouse Select", expand=True)

        if hasattr(kc_prefs, "spacebar_action"):
            col.row().prop(kc_prefs, "spacebar_action", text="Spacebar Action")

        # Save Preferences.
        sub = col.column()
        sub.separator(factor=2)

        if can_import:
            sub.operator("wm.save_userpref", text="Save New Preferences", icon='NONE')
        else:
            sub.operator("wm.save_userpref", text="Continue")

        layout.separator(factor=2.0)


class WM_MT_splash(Menu):
    bl_label = "Splash"

    def draw(self, context):
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

        found_recent = col2.template_recent_files(rows=5)

        if found_recent:
            col2_title.label(text="Recent Files")

            col_more = col2.column()
            col_more.operator_context = 'INVOKE_DEFAULT'
            more_props = col_more.operator("wm.search_single_menu", text="More...", icon='VIEWZOOM')
            more_props.menu_idname = "TOPBAR_MT_file_open_recent"
        else:
            # Links if no recent files.
            col2_title.label(text="Getting Started")

            col2.operator("wm.url_open_preset", text="Manual", icon='URL').type = 'MANUAL'
            col2.operator("wm.url_open", text="Support", icon='URL').url = "https://www.blender.org/support/"
            col2.operator("wm.url_open", text="User Communities", icon='URL').url = "https://www.blender.org/community/"
            col2.operator("wm.url_open", text="Get Involved", icon='URL').url = "https://www.blender.org/get-involved/"
            col2.operator("wm.url_open_preset", text="Blender Website", icon='URL').type = 'BLENDER'

        col_sep = layout.column()
        col_sep.separator()
        col_sep.separator(type='LINE')
        col_sep.separator()

        split = layout.split()

        col1 = split.column()
        sub = col1.row()
        sub.operator_context = 'INVOKE_DEFAULT'
        sub.operator("wm.open_mainfile", text="Open...", icon='FILE_FOLDER')
        col1.operator("wm.recover_last_session", icon='RECOVER_LAST')

        col2 = split.column()

        col2.operator("wm.url_open_preset", text="What's New", icon='URL').type = 'RELEASE_NOTES'
        col2.operator("wm.url_open_preset", text="Donate to Blender", icon='FUND').type = 'FUND'

        layout.separator()

        if (not bpy.app.online_access) and bpy.app.online_access_override:
            self.layout.label(text="Running in Offline Mode", icon='INTERNET_OFFLINE')

        layout.separator()


class WM_MT_splash_about(Menu):
    bl_label = "About"

    def draw(self, context):

        layout = self.layout
        layout.operator_context = 'EXEC_DEFAULT'

        split = layout.split(factor=0.65)

        col = split.column(align=True)
        col.scale_y = 0.8
        col.label(text=iface_("Version: {:s}").format(bpy.app.version_string), translate=False)
        col.separator(factor=2.5)
        col.label(text=iface_("Date: {:s} {:s}").format(
            bpy.app.build_commit_date.decode("utf-8", "replace"),
            bpy.app.build_commit_time.decode("utf-8", "replace")),
            translate=False,
        )
        col.label(text=iface_("Hash: {:s}").format(bpy.app.build_hash.decode("ascii")), translate=False)
        col.label(text=iface_("Branch: {:s}").format(bpy.app.build_branch.decode("utf-8", "replace")), translate=False)

        # This isn't useful information on MS-Windows or Apple systems as dynamically switching
        # between windowing systems is only supported between X11/WAYLAND.
        from _bpy import _ghost_backend
        ghost_backend = _ghost_backend()
        if ghost_backend not in {'NONE', 'DEFAULT'}:
            col.label(text=iface_("Windowing Environment: {:s}").format(_ghost_backend()), translate=False)
        del _ghost_backend, ghost_backend

        col.separator(factor=2.0)
        col.label(text="Blender is free software")
        col.label(text="Licensed under the GNU General Public License")

        col = split.column(align=True)
        col.emboss = 'PULLDOWN_MENU'
        col.operator("wm.url_open_preset", text="Donate", icon='FUND').type = 'FUND'
        col.operator("wm.url_open_preset", text="What's New", icon='URL').type = 'RELEASE_NOTES'
        col.separator(factor=2.0)
        col.operator("wm.url_open_preset", text="Credits", icon='URL').type = 'CREDITS'
        col.operator("wm.url_open", text="License", icon='URL').url = "https://www.blender.org/about/license/"
        col.operator("wm.url_open", text="Blender Store", icon='URL').url = "https://store.blender.org"
        col.operator("wm.url_open_preset", text="Blender Website", icon='URL').type = 'BLENDER'


class WM_MT_region_toggle_pie(Menu):
    bl_label = "Region Toggle"

    # Map the `region.type` to the `space_data` attribute & text label.
    # The order of items defines priority, so for example in the sequencer
    # when there is both a toolbar and channels, the toolbar gets the
    # axis-aligned pie, and the channels don't.
    _region_info = {
        'TOOLS': "show_region_toolbar",
        'UI': "show_region_ui",
        # Note that the tool header is enabled/disabled along with the header,
        # no need to include both in this list.
        'HEADER': "show_region_header",
        'FOOTER': "show_region_footer",
        'ASSET_SHELF': "show_region_asset_shelf",
        'CHANNELS': "show_region_channels",
    }
    # Map the `region.alignment` to the axis-aligned pie position.
    _region_align_pie = {
        'LEFT': 0,
        'RIGHT': 1,
        'BOTTOM': 2,
        'TOP': 3,
    }
    # Map the axis-aligned pie to alternative directions, see `ui_radial_dir_order` in C++ source.
    # The value is the preferred direction in order of priority, two diagonals, then the flipped direction.
    _region_dir_pie_alternatives = {
        0: (4, 6, 1),
        1: (5, 7, 0),
        2: (6, 7, 3),
        3: (4, 5, 2),
    }

    @classmethod
    def poll(cls, context):
        return context.space_data is not None

    @classmethod
    def _draw_pie_regions_from_alignment(cls, context, pie):
        space_data = context.space_data
        # Store each region by it's type.
        region_by_type = {}

        for region in context.area.regions:
            region_type = region.type
            # If the attribute doesn't exist, the RNA definition is outdated.
            # See: #134339 and its fix for reference.
            attr = cls._region_info.get(region_type, None)
            if attr is None:
                continue
            # In some cases channels exists but can't be toggled.
            assert hasattr(space_data, attr)

            if space_data.is_property_readonly(attr):
                continue

            # Technically possible these double-up, in practice this should never happen.
            if region_type in region_by_type:
                print("{:s}: Unexpected double-up of region types {!r}".format(cls.__name__, region_type))
            region_by_type[region_type] = region

        # Axis aligned pie menu items to populate.
        items = [[], [], [], [], [], [], [], []]

        # Use predictable ordering.
        for region_type in cls._region_info.keys():
            region = region_by_type.get(region_type)
            if region is None:
                continue
            index = cls._region_align_pie[region.alignment]
            items[index].append(region_type)

        # Handle any overflow (two or more regions with the same alignment).
        # This happens in the sequencer (channels + toolbar),
        # otherwise it should not be that common.
        items_overflow = []
        for index in range(4):
            if len(items[index]) <= 1:
                continue
            for index_other in cls._region_dir_pie_alternatives[index]:
                if not items[index_other]:
                    items[index_other].append(items[index].pop(1))
                    if len(items[index]) <= 1:
                        break
            del index_other

        for index in range(4):
            if len(items[index]) <= 1:
                continue
            for index_other in range(4, 8):
                if not items[index_other]:
                    items[index_other].append(items[index].pop(1))
                    if len(items[index]) <= 1:
                        break
        # Only happens when there are more than 8 regions - practically never!
        for index in range(4):
            while len(items[index]) > 1:
                items_overflow.append([items[index].pop(1)])

        # Use to access the labels.
        enum_items = bpy.types.Region.bl_rna.properties["type"].enum_items_static_ui

        for region_type_list in (items + items_overflow):
            if not region_type_list:
                pie.separator()
                continue
            assert len(region_type_list) == 1
            region_type = region_type_list[0]
            text = enum_items[region_type].name
            attr = cls._region_info[region_type]
            value = getattr(space_data, attr)
            props = pie.operator(
                "wm.context_toggle",
                text=text,
                text_ctxt=i18n_contexts.default,
                icon='CHECKBOX_HLT' if value else 'CHECKBOX_DEHLT',
            )
            props.data_path = "space_data." + attr

    def draw(self, context):
        layout = self.layout
        pie = layout.menu_pie()
        self._draw_pie_regions_from_alignment(context, pie)


class WM_OT_drop_blend_file(Operator):
    bl_idname = "wm.drop_blend_file"
    bl_label = "Handle dropped .blend file"
    bl_options = {'INTERNAL'}

    filepath: StringProperty(
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )

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
    WM_OT_properties_edit_value,
    WM_OT_properties_remove,
    WM_OT_sysinfo,
    WM_OT_owner_disable,
    WM_OT_owner_enable,
    WM_OT_url_open,
    WM_OT_url_open_preset,
    WM_OT_tool_set_by_id,
    WM_OT_tool_set_by_index,
    WM_OT_tool_set_by_brush_type,
    WM_OT_toolbar,
    WM_OT_toolbar_fallback_pie,
    WM_OT_toolbar_prompt,
    BatchRenameAction,
    WM_OT_batch_rename,
    WM_MT_splash_quick_setup,
    WM_MT_splash,
    WM_MT_splash_about,
    WM_MT_region_toggle_pie,
)
