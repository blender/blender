# -*- coding: utf-8 -*-
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

import os
from os.path import dirname
from collections import OrderedDict

import bpy
from nodeitems_utils import NodeCategory, NodeItem, NodeItemCustom
import nodeitems_utils
import bl_operators

import sverchok
from sverchok.utils import get_node_class_reference
from sverchok.utils.sv_help import build_help_remap
from sverchok.ui.sv_icons import node_icon, icon

class SverchNodeCategory(NodeCategory):
    @classmethod
    def poll(cls, context):
        return context.space_data.tree_type == 'SverchCustomTreeType'


def make_node_cats():
    '''
    this loads the index.md file and converts it to an OrderedDict of node categories.

    '''

    index_path = os.path.join(dirname(__file__), 'index.md')

    node_cats = OrderedDict()
    with open(index_path) as md:
        category = None
        temp_list = []
        for line in md:
            if not line.strip():
                continue
            if line.strip().startswith('>'):
                continue
            elif line.startswith('##'):
                if category:
                    node_cats[category] = temp_list
                category = line[2:].strip()
                temp_list = []

            elif line.strip() == '---':
                temp_list.append(['separator'])
            else:
                bl_idname = line.strip()
                temp_list.append([bl_idname])

        # final append
        node_cats[category] = temp_list
    
    return node_cats


def juggle_and_join(node_cats):
    '''
    this step post processes the extended catagorization used
    by ctrl+space dynamic menu, and attempts to merge previously
    joined catagories. Why? Because the default menu gets very
    long if there are too many categories.

    The only real alternative to this approach is to write a
    replacement for nodeitems_utils which respects categories
    and submenus.

    '''
    node_cats = node_cats.copy()

    # join beta and alpha node cats
    alpha = node_cats.pop('Alpha Nodes')
    node_cats['Beta Nodes'].extend(alpha)

    # put masks into list main
    for ltype in ["List Masks", "List Mutators"]:
        node_refs = node_cats.pop(ltype)
        node_cats["List Main"].extend(node_refs)

    objects_cat = node_cats.pop('Objects')
    node_cats['BPY Data'].extend(objects_cat)

    # add extended gens to Gens menu
    gen_ext = node_cats.pop("Generators Extended")
    node_cats["Generator"].extend(gen_ext)

    return node_cats

# We are creating and registering node adding operators dynamically.
# So, we have to remember them in order to unregister them when needed.
node_add_operators = {}

class SverchNodeItem(object):
    """
    A local replacement of nodeitems_utils.NodeItem.
    This calls our custom operator (see make_add_operator) instead of
    standard node.add_node. Having this replacement item class allows us to:
    * Have icons in the T panel
    * Have arbitrary tooltips in the T panel.
    """
    def __init__(self, nodetype, label=None, settings=None, poll=None):
        self.nodetype = nodetype
        self._label = label
        if settings is None:
            self.settings = {}
        else:
            self.settings = settings
        self.poll = poll

        self.make_add_operator()

    @property
    def label(self):
        if self._label:
            return self._label
        else:
            return self.get_node_class().bl_rna.name

    def get_node_class(self):
        return get_node_class_reference(self.nodetype)

    def get_idname(self):
        return get_node_idname_for_operator(self.nodetype)

    def make_add_operator(self):
        """
        Create operator class which adds specific type of node.
        Tooltip (docstring) for that operator is copied from 
        node class docstring.
        """

        global node_add_operators
        
        class SverchNodeAddOperator(bl_operators.node.NodeAddOperator, bpy.types.Operator):
            """Wrapper for node.add_node operator to add specific node"""

            bl_idname = "node.sv_add_" + self.get_idname()
            bl_label = "Add {} node".format(self.label)
            bl_options = {'REGISTER', 'UNDO'}

            def execute(operator, context):
                # please not be confused: "operator" here references to
                # SverchNodeAddOperator instance, and "self" references to
                # SverchNodeItem instance.
                operator.use_transform = True
                operator.type = self.nodetype
                operator.create_node(context)
                return {'FINISHED'}

        node_class = self.get_node_class()
        SverchNodeAddOperator.__name__ = node_class.__name__

        if hasattr(node_class, "get_tooltip"):
            SverchNodeAddOperator.__doc__ = node_class.get_tooltip()
        else:
            SverchNodeAddOperator.__doc__ = node_class.__doc__

        node_add_operators[self.get_idname()] = SverchNodeAddOperator
        bpy.utils.register_class(SverchNodeAddOperator)

    def get_icon(self):
        rna = self.get_node_class().bl_rna
        if hasattr(rna, "bl_icon"):
            return rna.bl_icon
        else:
            return "RNA"

    @staticmethod
    def draw(self, layout, context):
        add = draw_add_node_operator(layout, self.nodetype, label=self._label)

        for setting in self.settings.items():
            ops = add.settings.add()
            ops.name = setting[0]
            ops.value = setting[1]

def get_node_idname_for_operator(nodetype):
    """Select valid bl_idname for node to create node adding operator bl_idname."""
    rna = get_node_class_reference(nodetype)
    if not rna:
        raise Exception("Can't find registered node {}".format(nodetype))
    if hasattr(rna, 'bl_idname'):
        return rna.bl_idname.lower()
    elif nodetype == "NodeReroute":
        return "node_reroute"
    else:
        return rna.name.lower()

def draw_add_node_operator(layout, nodetype, label=None, icon_name=None, params=None):
    """
    Draw node adding operator button.
    This is to be used both in Shift-A menu and in T panel.
    """

    default_context = bpy.app.translations.contexts.default
    node_rna = get_node_class_reference(nodetype).bl_rna

    if label is None:
        if hasattr(node_rna, 'bl_label'):
            label = node_rna.bl_label
        elif nodetype == "NodeReroute":
            label = "Reroute"
        else:
            label = node_rna.name

    if params is None:
        params = dict(text=label)
    params['text_ctxt'] = default_context
    if icon_name is not None:
        params.update(**icon(icon_name))
    else:
        params.update(**node_icon(node_rna))

    add = layout.operator("node.sv_add_" + get_node_idname_for_operator(nodetype), **params)
                            
    add.type = nodetype
    add.use_transform = True

    return add

def sv_group_items(context):
    """
    Based on the built in node_group_items in the blender distrubution
    somewhat edited to fit.
    """
    if context is None:
        return
    space = context.space_data
    if not space:
        return
    ntree = space.edit_tree
    if not ntree:
        return

    yield NodeItemCustom(draw=draw_node_ops)

    def contains_group(nodetree, group):
        if nodetree == group:
            return True
        else:
            for node in nodetree.nodes:
                if node.bl_idname in node_tree_group_type.values() and node.node_tree is not None:
                    if contains_group(node.node_tree, group):
                        return True
        return False

    if ntree.bl_idname == "SverchGroupTreeType":
        yield NodeItem("SvMonadInfoNode", "Monad Info")

    for monad in context.blend_data.node_groups:
        if monad.bl_idname != "SverchGroupTreeType":
            continue
        # make sure class exists
        cls_ref = get_node_class_reference(monad.cls_bl_idname)

        if cls_ref and monad.cls_bl_idname:
            yield NodeItem(monad.cls_bl_idname, monad.name)
        elif monad.cls_bl_idname:
            monad_cls_template_dict = {"cls_bl_idname": "str('{}')".format(monad.cls_bl_idname)}
            yield NodeItem("SvMonadGenericNode", monad.name, monad_cls_template_dict)

def draw_node_ops(self,layout, context):

    make_monad = "node.sv_monad_from_selected"
    ungroup_monad = "node.sv_monad_expand"
    update_import = "node.sv_monad_class_update"
    layout.operator(make_monad, text='make group (+relink)', icon='RNA')
    layout.operator(make_monad, text='make group', icon='RNA').use_relinking = False
    layout.operator(ungroup_monad, text='ungroup', icon='RNA')
    layout.operator(update_import, text='update appended/linked', icon='RNA')
    layout.separator()

def make_categories():
    original_categories = make_node_cats()

    node_cats = juggle_and_join(original_categories)
    node_categories = []
    node_count = 0
    for category, nodes in node_cats.items():
        name_big = "SVERCHOK_" + category.replace(' ', '_')
        node_categories.append(
            SverchNodeCategory(
                name_big,
                category,
                items=[SverchNodeItem(props[0]) for props in nodes if not props[0] == 'separator']))
        node_count += len(nodes)
    node_categories.append(SverchNodeCategory("SVERCHOK_GROUPS", "Groups", items=sv_group_items))

    return node_categories, node_count, original_categories


def reload_menu():
    menu, node_count, original_categories = make_categories()
    if 'SVERCHOK' in nodeitems_utils._node_categories:
        nodeitems_utils.unregister_node_categories("SVERCHOK")
        unregister_node_add_operators()
    nodeitems_utils.register_node_categories("SVERCHOK", menu)
    register_node_add_operators()
    
    build_help_remap(original_categories)
    print("Reload complete, press update")

def register_node_add_operators():
    """Register all our custom node adding operators"""
    for idname in node_add_operators:
        bpy.utils.register_class(node_add_operators[idname])

def unregister_node_add_operators():
    """Unregister all our custom node adding operators"""
    for idname in node_add_operators:
        bpy.utils.unregister_class(node_add_operators[idname])

def register():
    menu, node_count, original_categories = make_categories()
    if 'SVERCHOK' in nodeitems_utils._node_categories:
        nodeitems_utils.unregister_node_categories("SVERCHOK")
    nodeitems_utils.register_node_categories("SVERCHOK", menu)

    build_help_remap(original_categories)

    print("\n** Sverchok loaded with {i} nodes **".format(i=node_count))


def unregister():
    if 'SVERCHOK' in nodeitems_utils._node_categories:
        nodeitems_utils.unregister_node_categories("SVERCHOK")
    unregister_node_add_operators()

