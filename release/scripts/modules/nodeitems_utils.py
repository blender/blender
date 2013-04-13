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


node_categories = []


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


# Empty base class to detect subclasses in bpy.types
class NodeCategoryUI():
    pass


def register_node_ui():
    # works as draw function for both menus and panels
    def draw_node_item(self, context):
        layout = self.layout
        for item in self.category.items:
            op = layout.operator("node.add_node", text=item.label)
            op.type = item.nodetype
            op.use_transform = True

            for setting in item.settings.items():
                ops = op.settings.add()
                ops.name = setting[0]
                ops.value = setting[1]

    for cat in node_categories:
        menu = type("NODE_MT_category_"+cat.identifier, (bpy.types.Menu, NodeCategoryUI), {
            "bl_space_type" : 'NODE_EDITOR',
            "bl_label" : cat.name,
            "category" : cat,
            "poll" : cat.poll,
            "draw" : draw_node_item,
            })
        panel = type("NODE_PT_category_"+cat.identifier, (bpy.types.Panel, NodeCategoryUI), {
            "bl_space_type" : 'NODE_EDITOR',
            "bl_region_type" : 'TOOLS',
            "bl_label" : cat.name,
            "category" : cat,
            "poll" : cat.poll,
            "draw" : draw_node_item,
            })
        bpy.utils.register_class(menu)
        bpy.utils.register_class(panel)


    def draw_add_menu(self, context):
        layout = self.layout

        for cat in node_categories:
            if cat.poll(context):
                layout.menu("NODE_MT_category_%s" % cat.identifier)

    add_menu = type("NODE_MT_add", (bpy.types.Menu, NodeCategoryUI), {
        "bl_space_type" : 'NODE_EDITOR',
        "bl_label" : "Add",
        "draw" : draw_add_menu,
        })
    bpy.utils.register_class(add_menu)


def unregister_node_ui():
    # unregister existing UI classes
    for c in NodeCategoryUI.__subclasses__():
        if hasattr(c, "bl_rna"):
            bpy.utils.unregister_class(c)
            del c

