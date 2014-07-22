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


def rna_idprop_ui_prop_get(item, prop, create=True):

    rna_ui = rna_idprop_ui_get(item, create)

    if rna_ui is None:
        return None

    try:
        return rna_ui[prop]
    except:
        rna_ui[prop] = {}
        return rna_ui[prop]


def rna_idprop_ui_prop_clear(item, prop):
    rna_ui = rna_idprop_ui_get(item, False)

    if rna_ui is None:
        return

    try:
        del rna_ui[prop]
    except:
        pass


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

    rna_properties = {prop.identifier for prop in rna_item.bl_rna.properties if prop.is_runtime} if items else None

    for key, val in items:

        if key == '_RNA_UI':
            continue

        row = layout.row()
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

        box = row.box()

        if use_edit:
            split = box.split(percentage=0.75)
            row = split.row()
        else:
            row = box.row()

        row.label(text=key, translate=False)

        # explicit exception for arrays
        is_rna = (key in rna_properties)

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
            else:
                row.label(text="API Defined")

            props = row.operator("wm.properties_remove", text="", icon='ZOOMOUT')
            assign_props(props, val_draw, key)


class PropertyPanel():
    """
    The subclass should have its own poll function
    and the variable '_context_path' MUST be set.
    """
    bl_label = "Custom Properties"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        rna_item, context_member = rna_idprop_context_value(context, cls._context_path, cls._property_type)
        return bool(rna_item)

    """
    def draw_header(self, context):
        rna_item, context_member = rna_idprop_context_value(context, self._context_path, self._property_type)
        tot = len(rna_item.keys())
        if tot:
            self.layout().label("%d:" % tot)
    """

    def draw(self, context):
        draw(self.layout, context, self._context_path, self._property_type)
