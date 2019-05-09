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


class NodeCategory:
    @classmethod
    def poll(cls, _context):
        return True

    def __init__(self, identifier, name, description="", items=None):
        self.identifier = identifier
        self.name = name
        self.description = description

        if items is None:
            self.items = lambda context: []
        elif callable(items):
            self.items = items
        else:
            def items_gen(context):
                for item in items:
                    if item.poll is None or context is None or item.poll(context):
                        yield item
            self.items = items_gen


class NodeItem:
    def __init__(self, nodetype, label=None, settings=None, poll=None):

        if settings is None:
            settings = {}

        self.nodetype = nodetype
        self._label = label
        self.settings = settings
        self.poll = poll

    @property
    def label(self):
        if self._label:
            return self._label
        else:
            # if no custom label is defined, fall back to the node type UI name
            bl_rna = bpy.types.Node.bl_rna_get_subclass(self.nodetype)
            if bl_rna is not None:
                return bl_rna.name
            else:
                return "Unknown"

    @property
    def translation_context(self):
        if self._label:
            return bpy.app.translations.contexts.default
        else:
            # if no custom label is defined, fall back to the node type UI name
            bl_rna = bpy.types.Node.bl_rna_get_subclass(self.nodetype)
            if bl_rna is not None:
                return bl_rna.translation_context
            else:
                return bpy.app.translations.contexts.default

    # NB: is a staticmethod because called with an explicit self argument
    # NodeItemCustom sets this as a variable attribute in __init__
    @staticmethod
    def draw(self, layout, _context):
        props = layout.operator("node.add_node", text=self.label, text_ctxt=self.translation_context)
        props.type = self.nodetype
        props.use_transform = True

        for setting in self.settings.items():
            ops = props.settings.add()
            ops.name = setting[0]
            ops.value = setting[1]


class NodeItemCustom:
    def __init__(self, poll=None, draw=None):
        self.poll = poll
        self.draw = draw


_node_categories = {}


def register_node_categories(identifier, cat_list):
    if identifier in _node_categories:
        raise KeyError("Node categories list '%s' already registered" % identifier)
        return

    # works as draw function for menus
    def draw_node_item(self, context):
        layout = self.layout
        col = layout.column()
        for item in self.category.items(context):
            item.draw(item, col, context)

    menu_types = []
    for cat in cat_list:
        menu_type = type("NODE_MT_category_" + cat.identifier, (bpy.types.Menu,), {
            "bl_space_type": 'NODE_EDITOR',
            "bl_label": cat.name,
            "category": cat,
            "poll": cat.poll,
            "draw": draw_node_item,
        })

        menu_types.append(menu_type)

        bpy.utils.register_class(menu_type)

    def draw_add_menu(self, context):
        layout = self.layout

        for cat in cat_list:
            if cat.poll(context):
                layout.menu("NODE_MT_category_%s" % cat.identifier)

    # stores: (categories list, menu draw function, submenu types)
    _node_categories[identifier] = (cat_list, draw_add_menu, menu_types)


def node_categories_iter(context):
    for cat_type in _node_categories.values():
        for cat in cat_type[0]:
            if cat.poll and ((context is None) or cat.poll(context)):
                yield cat


def node_items_iter(context):
    for cat in node_categories_iter(context):
        for item in cat.items(context):
            yield item


def unregister_node_cat_types(cats):
    for mt in cats[2]:
        bpy.utils.unregister_class(mt)


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


def draw_node_categories_menu(self, context):
    for cats in _node_categories.values():
        cats[1](self, context)
