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


def rna_idprop_ui_get(item, create=True):
    try:
        return item['_RNA_UI']
    except:
        if create:
            item['_RNA_UI'] = {}
            return item['_RNA_UI']
        else:
            return None


def rna_idprop_ui_del(item):
    try:
        del item['_RNA_UI']
    except KeyError:
        pass


def rna_idprop_quote_path(prop):
    return "[\"%s\"]" % prop.replace("\"", "\\\"")


def rna_idprop_ui_prop_update(item, prop):
    prop_path = rna_idprop_quote_path(prop)
    prop_rna = item.path_resolve(prop_path, False)
    if isinstance(prop_rna, bpy.types.bpy_prop):
        prop_rna.update()


def rna_idprop_ui_prop_get(item, prop, create=True):

    rna_ui = rna_idprop_ui_get(item, create)

    if rna_ui is None:
        return None

    try:
        return rna_ui[prop]
    except:
        rna_ui[prop] = {}
        return rna_ui[prop]


def rna_idprop_ui_prop_clear(item, prop, remove=True):
    rna_ui = rna_idprop_ui_get(item, False)

    if rna_ui is None:
        return

    try:
        del rna_ui[prop]
    except KeyError:
        pass
    if remove and len(item.keys()) == 1:
        rna_idprop_ui_del(item)


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
    nbr_props = len(keys)
    return (nbr_props > 1) or (nbr_props and '_RNA_UI' not in keys)


def rna_idprop_ui_prop_default_set(item, prop, value):
    defvalue = None
    try:
        prop_type = type(item[prop])

        if prop_type in {int, float}:
            defvalue = prop_type(value)
    except KeyError:
        pass

    if defvalue:
        rna_ui = rna_idprop_ui_prop_get(item, prop, True)
        rna_ui["default"] = defvalue
    else:
        rna_ui = rna_idprop_ui_prop_get(item, prop)
        if rna_ui and "default" in rna_ui:
            del rna_ui["default"]


def rna_idprop_ui_create(
        item, prop, *, default,
        min=0.0, max=1.0,
        soft_min=None, soft_max=None,
        description=None,
        overridable=False,
):
    """Create and initialize a custom property with limits, defaults and other settings."""

    proptype = type(default)

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

    # Clear the UI settings
    rna_ui_group = rna_idprop_ui_get(item, True)
    rna_ui_group[prop] = {}
    rna_ui = rna_ui_group[prop]

    # Assign limits and default
    if proptype in {int, float, bool}:
        # The type must be exactly the same
        rna_ui["min"] = proptype(min)
        rna_ui["soft_min"] = proptype(soft_min)
        rna_ui["max"] = proptype(max)
        rna_ui["soft_max"] = proptype(soft_max)

        if default:
            rna_ui["default"] = default

    # Assign other settings
    if description is not None:
        rna_ui["description"] = description

    prop_path = rna_idprop_quote_path(prop)

    item.property_overridable_library_set(prop_path, overridable)

    return rna_ui


def draw(layout, context, context_member, property_type, use_edit=True):

    def assign_props(prop, val, key):
        prop.data_path = context_member
        prop.property = key

        try:
            prop.value = str(val)
        except:
            pass

    rna_item, context_member = rna_idprop_context_value(context, context_member, property_type)

    # poll should really get this...
    if not rna_item:
        return

    from bpy.utils import escape_identifier

    if rna_item.id_data.library is not None:
        use_edit = False

    assert(isinstance(rna_item, property_type))

    items = rna_item.items()
    items.sort()

    if use_edit:
        row = layout.row()
        props = row.operator("wm.properties_add", text="Add")
        props.data_path = context_member
        del row

    show_developer_ui = context.preferences.view.show_developer_ui
    rna_properties = {prop.identifier for prop in rna_item.bl_rna.properties if prop.is_runtime} if items else None

    layout.use_property_split = True
    layout.use_property_decorate = False  # No animation.

    flow = layout.grid_flow(row_major=False, columns=0, even_columns=True, even_rows=False, align=True)

    for key, val in items:

        if key == '_RNA_UI':
            continue

        is_rna = (key in rna_properties)

        # only show API defined for developers
        if is_rna and not show_developer_ui:
            continue

        to_dict = getattr(val, "to_dict", None)
        to_list = getattr(val, "to_list", None)

        # val_orig = val  # UNUSED
        if to_dict:
            val = to_dict()
            val_draw = str(val)
        elif to_list:
            val = to_list()
            val_draw = str(val)
        else:
            val_draw = val

        row = flow.row(align=True)
        box = row.box()

        if use_edit:
            split = box.split(factor=0.75)
            row = split.row(align=True)
        else:
            row = box.row(align=True)

        row.alignment = 'RIGHT'

        row.label(text=key, translate=False)

        # explicit exception for arrays.
        if to_dict or to_list:
            row.label(text=val_draw, translate=False)
        else:
            if is_rna:
                row.prop(rna_item, key, text="")
            else:
                row.prop(rna_item, '["%s"]' % escape_identifier(key), text="")

        if use_edit:
            row = split.row(align=True)
            if not is_rna:
                props = row.operator("wm.properties_edit", text="Edit")
                assign_props(props, val_draw, key)
                props = row.operator("wm.properties_remove", text="", icon='REMOVE')
                assign_props(props, val_draw, key)
            else:
                row.label(text="API Defined")

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
