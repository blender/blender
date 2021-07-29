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


def get_metarig_list(path):
    """ Searches for metarig modules, and returns a list of the
        imported modules.
    """
    metarigs = []
    MODULE_DIR = os.path.dirname(__file__)
    METARIG_DIR_ABS = os.path.join(MODULE_DIR, utils.METARIG_DIR)
    SEARCH_DIR_ABS = os.path.join(METARIG_DIR_ABS, path)
    files = os.listdir(SEARCH_DIR_ABS)
    files.sort()

    for f in files:
        # Is it a directory?
        if os.path.isdir(os.path.join(SEARCH_DIR_ABS, f)):
            continue
        elif not f.endswith(".py"):
            continue
        elif f == "__init__.py":
            continue
        else:
            module_name = f[:-3]
            try:
                metarigs += [utils.get_metarig_module(module_name)]
            except (ImportError):
                pass
    return metarigs


def make_metarig_add_execute(m):
    """ Create an execute method for a metarig creation operator.
    """
    def execute(self, context):
        # Add armature object
        bpy.ops.object.armature_add()
        obj = context.active_object
        obj.name = "metarig"

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


# Get the metarig modules
metarigs = get_metarig_list("")

# Create metarig add Operators
metarig_ops = []
for m in metarigs:
    name = m.__name__.rsplit('.', 1)[1]

    # Dynamically construct an Operator
    T = type("Add_" + name + "_Metarig", (bpy.types.Operator,), {})
    T.bl_idname = "object.armature_" + name + "_metarig_add"
    T.bl_label = "Add " + name.replace("_", " ").capitalize() + " (metarig)"
    T.bl_options = {'REGISTER', 'UNDO'}
    T.execute = make_metarig_add_execute(m)

    metarig_ops.append((T, name))

# Create menu functions
menu_funcs = []
for mop, name in metarig_ops:
    text = capwords(name.replace("_", " ")) + " (Meta-Rig)"
    menu_funcs += [make_metarig_menu_func(mop.bl_idname, text)]


def register():
    for mop, name in metarig_ops:
        bpy.utils.register_class(mop)

    for mf in menu_funcs:
        bpy.types.INFO_MT_armature_add.append(mf)


def unregister():
    for mop, name in metarig_ops:
        bpy.utils.unregister_class(mop)

    for mf in menu_funcs:
        bpy.types.INFO_MT_armature_add.remove(mf)
