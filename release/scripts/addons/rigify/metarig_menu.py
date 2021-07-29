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

import os
from string import capwords

import bpy

from . import utils


class ArmatureSubMenu(bpy.types.Menu):
    # bl_idname = 'ARMATURE_MT_armature_class'

    def draw(self, context):
        layout = self.layout
        layout.label(self.bl_label)
        for op, name in self.operators:
            text = capwords(name.replace("_", " ")) + " (Meta-Rig)"
            layout.operator(op, icon='OUTLINER_OB_ARMATURE', text=text)


def get_metarig_list(path, depth=0):
    """ Searches for metarig modules, and returns a list of the
        imported modules.
    """
    metarigs = []
    metarigs_dict = dict()
    MODULE_DIR = os.path.dirname(__file__)
    METARIG_DIR_ABS = os.path.join(MODULE_DIR, utils.METARIG_DIR)
    SEARCH_DIR_ABS = os.path.join(METARIG_DIR_ABS, path)
    files = os.listdir(SEARCH_DIR_ABS)
    files.sort()

    for f in files:
        # Is it a directory?
        complete_path = os.path.join(SEARCH_DIR_ABS, f)
        if os.path.isdir(complete_path) and depth == 0:
            if f[0] != '_':
                metarigs_dict[f] = get_metarig_list(f, depth=1)
            else:
                continue
        elif not f.endswith(".py"):
            continue
        elif f == "__init__.py":
            continue
        else:
            module_name = f[:-3]
            try:
                if depth == 1:
                    metarigs += [utils.get_metarig_module(module_name, utils.METARIG_DIR + '.' + path)]
                else:
                    metarigs += [utils.get_metarig_module(module_name, utils.METARIG_DIR)]
            except (ImportError):
                pass

    if depth == 1:
        return metarigs

    metarigs_dict[utils.METARIG_DIR] = metarigs
    return metarigs_dict


def make_metarig_add_execute(m):
    """ Create an execute method for a metarig creation operator.
    """
    def execute(self, context):
        # Add armature object
        bpy.ops.object.armature_add()
        obj = context.active_object
        obj.name = "metarig"
        obj.data.name = "metarig"

        # Remove default bone
        bpy.ops.object.mode_set(mode='EDIT')
        bones = context.active_object.data.edit_bones
        bones.remove(bones[0])

        # Create metarig
        m.create(obj)

        bpy.ops.object.mode_set(mode='OBJECT')
        return {'FINISHED'}
    return execute


def make_metarig_menu_func(bl_idname, text):
    """ For some reason lambda's don't work for adding multiple menu
        items, so we use this instead to generate the functions.
    """
    def metarig_menu(self, context):
        self.layout.operator(bl_idname, icon='OUTLINER_OB_ARMATURE', text=text)
    return metarig_menu


def make_submenu_func(bl_idname, text):
    def metarig_menu(self, context):
        self.layout.menu(bl_idname, icon='OUTLINER_OB_ARMATURE', text=text)
    return metarig_menu


# Get the metarig modules
metarigs_dict = get_metarig_list("")
armature_submenus = []

# Create metarig add Operators
metarig_ops = {}
for metarig_class in metarigs_dict:
    metarig_ops[metarig_class] = []
    for m in metarigs_dict[metarig_class]:
        name = m.__name__.rsplit('.', 1)[1]

        # Dynamically construct an Operator
        T = type("Add_" + name + "_Metarig", (bpy.types.Operator,), {})
        T.bl_idname = "object.armature_" + name + "_metarig_add"
        T.bl_label = "Add " + name.replace("_", " ").capitalize() + " (metarig)"
        T.bl_options = {'REGISTER', 'UNDO'}
        T.execute = make_metarig_add_execute(m)

        metarig_ops[metarig_class].append((T, name))

menu_funcs = []

for mop, name in metarig_ops[utils.METARIG_DIR]:
    text = capwords(name.replace("_", " ")) + " (Meta-Rig)"
    menu_funcs += [make_metarig_menu_func(mop.bl_idname, text)]

metarigs_dict.pop(utils.METARIG_DIR)

metarig_classes = list(metarigs_dict.keys())
metarig_classes.sort()
for metarig_class in metarig_classes:
    # Create menu functions

    armature_submenus.append(type('Class_' + metarig_class + '_submenu', (ArmatureSubMenu,), {}))
    armature_submenus[-1].bl_label = metarig_class + ' (submenu)'
    armature_submenus[-1].bl_idname = 'ARMATURE_MT_%s_class' % metarig_class
    armature_submenus[-1].operators = []
    menu_funcs += [make_submenu_func(armature_submenus[-1].bl_idname, metarig_class)]

    for mop, name in metarig_ops[metarig_class]:
        arm_sub = next((e for e in armature_submenus if e.bl_label == metarig_class + ' (submenu)'), '')
        arm_sub.operators.append((mop.bl_idname, name,))

def register():
    for cl in metarig_ops:
        for mop, name in metarig_ops[cl]:
            bpy.utils.register_class(mop)

    for arm_sub in armature_submenus:
        bpy.utils.register_class(arm_sub)

    for mf in menu_funcs:
        bpy.types.INFO_MT_armature_add.append(mf)


def unregister():
    for cl in metarig_ops:
        for mop, name in metarig_ops[cl]:
            bpy.utils.unregister_class(mop)

    for arm_sub in armature_submenus:
        bpy.utils.unregister_class(arm_sub)

    for mf in menu_funcs:
        bpy.types.INFO_MT_armature_add.remove(mf)
