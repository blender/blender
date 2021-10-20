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

from mathutils import Vector
from bpy.types import bpy_prop_array
from idprop.types import IDPropertyArray, IDPropertyGroup

ARRAY_TYPES = (list, tuple, IDPropertyArray, Vector, bpy_prop_array)

# Maximum length of an array property for which a multi-line
# edit field will be displayed in the Custom Properties panel.
MAX_DISPLAY_ROWS = 4


def rna_idprop_quote_path(prop):
    return "[\"%s\"]" % bpy.utils.escape_identifier(prop)


def rna_idprop_ui_prop_update(item, prop):
    prop_path = rna_idprop_quote_path(prop)
    prop_rna = item.path_resolve(prop_path, False)
    if isinstance(prop_rna, bpy.types.bpy_prop):
        prop_rna.update()


def rna_idprop_ui_prop_clear(item, prop):
    ui_data = item.id_properties_ui(prop)
    ui_data.clear()


def rna_idprop_context_value(context, context_member, property_type):
    space = context.space_data

    if space is None or isinstance(space, bpy.types.SpaceProperties):
        pin_id = space.pin_id
    else:
        pin_id = None

    if pin_id and isinstance(pin_id, property_type):
        rna_item = pin_id
        context_member = "space_data.pin_id"
    else:
        rna_item = eval("context." + context_member)

    return rna_item, context_member


def rna_idprop_has_properties(rna_item):
    keys = rna_item.keys()
    return bool(keys)


def rna_idprop_value_to_python(value):
    if isinstance(value, IDPropertyArray):
        return value.to_list()
    elif isinstance(value, IDPropertyGroup):
        return value.to_dict()
    else:
        return value


def rna_idprop_value_item_type(value):
    is_array = isinstance(value, ARRAY_TYPES) and len(value) > 0
    item_value = value[0] if is_array else value
    return type(item_value), is_array


def rna_idprop_ui_prop_default_set(item, prop, value):
    ui_data = item.id_properties_ui(prop)
    ui_data.update(default=value)


def rna_idprop_ui_create(
        item, prop, *, default,
        min=0.0, max=1.0,
        soft_min=None, soft_max=None,
        description=None,
        overridable=False,
        subtype=None,
):
    """Create and initialize a custom property with limits, defaults and other settings."""

    proptype, _ = rna_idprop_value_item_type(default)

    # Sanitize limits
    if proptype is bool:
        min = soft_min = False
        max = soft_max = True

    if soft_min is None:
        soft_min = min
    if soft_max is None:
        soft_max = max

    # Assign the value
    item[prop] = default

    rna_idprop_ui_prop_update(item, prop)

    # Update the UI settings.
    ui_data = item.id_properties_ui(prop)
    ui_data.update(
        subtype=subtype,
        min=min,
        max=max,
        soft_min=soft_min,
        soft_max=soft_max,
        description=description,
        default=default,
    )

    prop_path = rna_idprop_quote_path(prop)

    item.property_overridable_library_set(prop_path, overridable)


def draw(layout, context, context_member, property_type, *, use_edit=True):

    def assign_props(prop, value, key):
        prop.data_path = context_member
        prop.property_name = key

        try:
            prop.value = str(value)
        except:
            pass

    rna_item, context_member = rna_idprop_context_value(context, context_member, property_type)

    # poll should really get this...
    if not rna_item:
        return

    from bpy.utils import escape_identifier

    if rna_item.id_data.library is not None:
        use_edit = False
    is_lib_override = rna_item.id_data.override_library and rna_item.id_data.override_library.reference

    assert(isinstance(rna_item, property_type))

    items = list(rna_item.items())
    items.sort()

    # TODO: Allow/support adding new custom props to overrides.
    if use_edit and not is_lib_override:
        row = layout.row()
        props = row.operator("wm.properties_add", text="Add")
        props.data_path = context_member
        del row

    show_developer_ui = context.preferences.view.show_developer_ui
    rna_properties = {prop.identifier for prop in rna_item.bl_rna.properties if prop.is_runtime} if items else None

    layout.use_property_split = True
    layout.use_property_decorate = False  # No animation.

    flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

    for key, value in items:
        is_rna = (key in rna_properties)

        # Only show API defined properties to developers.
        if is_rna and not show_developer_ui:
            continue

        to_dict = getattr(value, "to_dict", None)
        to_list = getattr(value, "to_list", None)

        if to_dict:
            value = to_dict()
            val_draw = str(value)
        elif to_list:
            value = to_list()
            val_draw = str(value)
        else:
            val_draw = value

        row = layout.row(align=True)
        box = row.box()

        if use_edit:
            split = box.split(factor=0.75)
            row = split.row()
        else:
            split = box.split(factor=1.00)
            row = split.row()

        row.alignment = 'RIGHT'

        row.label(text=key, translate=False)

        # Explicit exception for arrays.
        show_array_ui = to_list and not is_rna and 0 < len(value) <= MAX_DISPLAY_ROWS

        if show_array_ui and isinstance(value[0], (int, float)):
            row.prop(rna_item, '["%s"]' % escape_identifier(key), text="")
        elif to_dict or to_list:
            row.label(text=val_draw, translate=False)
        else:
            if is_rna:
                row.prop(rna_item, key, text="")
            else:
                row.prop(rna_item, '["%s"]' % escape_identifier(key), text="")

        if use_edit:
            row = split.row(align=True)
            # Do not allow editing of overridden properties (we cannot use a poll function
            # of the operators here since they's have no access to the specific property).
            row.enabled = not(is_lib_override and key in rna_item.id_data.override_library.reference)
            if is_rna:
                row.label(text="API Defined")
            elif is_lib_override:
                row.label(text="Library Override")
            else:
                props = row.operator("wm.properties_edit", text="Edit")
                assign_props(props, val_draw, key)
                props = row.operator("wm.properties_remove", text="", icon='REMOVE')
                assign_props(props, val_draw, key)

    del flow


class PropertyPanel:
    """
    The subclass should have its own poll function
    and the variable '_context_path' MUST be set.
    """
    bl_label = "Custom Properties"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = 1000  # Order panel after all others

    @classmethod
    def poll(cls, context):
        rna_item, _context_member = rna_idprop_context_value(context, cls._context_path, cls._property_type)
        return bool(rna_item)

    """
    def draw_header(self, context):
        rna_item, context_member = rna_idprop_context_value(context, self._context_path, self._property_type)
        tot = len(rna_item.keys())
        if tot:
            self.layout().label(text="%d:" % tot)
    """

    def draw(self, context):
        draw(self.layout, context, self._context_path, self._property_type)
