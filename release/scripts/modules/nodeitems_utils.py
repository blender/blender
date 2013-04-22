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
from bpy.types import Menu, Panel


class NodeCategory():
    @classmethod
    def poll(cls, context):
        return True

    @property
    def items(self):
        if hasattr(self, '_items'):
            return self._items
        elif hasattr(self, '_itemfunc'):
            return self._itemfunc(self)

    def __init__(self, identifier, name, description="", items=[]):
        self.identifier = identifier
        self.name = name
        self.description = description
        if callable(items):
            self._itemfunc = items
        else:
            self._items = items

class NodeItem():
    def __init__(self, nodetype, label=None, settings={}):
        self.nodetype = nodetype
        self._label = label
        self.settings = settings

    @property
    def label(self):
        if self._label:
            return self._label
        else:
            # if no custom label is defined, fall back to the node type UI name
            return getattr(bpy.types, self.nodetype).bl_rna.name


_node_categories = {}

def register_node_categories(identifier, cat_list):
    if identifier in _node_categories:
        raise KeyError("Node categories list '%s' already registered" % identifier)
        return

    # works as draw function for both menus and panels
    def draw_node_item(self, context):
        layout = self.layout
        col = layout.column()
        for item in self.category.items:
            op = col.operator("node.add_node", text=item.label)
            op.type = item.nodetype
            op.use_transform = True

            for setting in item.settings.items():
                ops = op.settings.add()
                ops.name = setting[0]
                ops.value = setting[1]

    menu_types = []
    panel_types = []
    for cat in cat_list:
        menu_type = type("NODE_MT_category_"+cat.identifier, (bpy.types.Menu,), {
            "bl_space_type" : 'NODE_EDITOR',
            "bl_label" : cat.name,
            "category" : cat,
            "poll" : cat.poll,
            "draw" : draw_node_item,
            })
        panel_type = type("NODE_PT_category_"+cat.identifier, (bpy.types.Panel,), {
            "bl_space_type" : 'NODE_EDITOR',
            "bl_region_type" : 'TOOLS',
            "bl_label" : cat.name,
            "bl_options" : {'DEFAULT_CLOSED'},
            "category" : cat,
            "poll" : cat.poll,
            "draw" : draw_node_item,
            })

        menu_types.append(menu_type)
        panel_types.append(panel_type)

        bpy.utils.register_class(menu_type)
        bpy.utils.register_class(panel_type)

    def draw_add_menu(self, context):
        layout = self.layout

        for cat in cat_list:
            if cat.poll(context):
                layout.menu("NODE_MT_category_%s" % cat.identifier)

    bpy.types.NODE_MT_add.append(draw_add_menu)

    # stores: (categories list, menu draw function, submenu types, panel types)
    _node_categories[identifier] = (cat_list, draw_add_menu, menu_types, panel_types)


def unregister_node_cat_types(cats):
    bpy.types.NODE_MT_add.remove(cats[1])
    for mt in cats[2]:
        bpy.utils.unregister_class(mt)
    for pt in cats[3]:
        bpy.utils.unregister_class(pt)


def unregister_node_categories(identifier=None):
    # unregister existing UI classes
    if identifier:
        cat_types = _node_categories.get(identifier, None)
        if cat_types:
            unregister_node_cat_types(cat_types)
        del _node_categories[identifier]

    else:
        for cat_types in _node_categories.values():
            unregister_node_cat_types(cat_types)
        _node_categories.clear()

