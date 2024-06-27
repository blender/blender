# SPDX-FileCopyrightText: 2010-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import traceback

from string import capwords
from collections import defaultdict
from types import ModuleType
from typing import Iterable

import bpy

from .utils.rig import METARIG_DIR, get_resource

from . import feature_set_list


class ArmatureSubMenu(bpy.types.Menu):
    # bl_idname = 'ARMATURE_MT_armature_class'

    operators: list[tuple[str, str]]

    def draw(self, context):
        layout = self.layout
        layout.label(text=self.bl_label)
        for op, name in self.operators:
            text = capwords(name.replace("_", " ")) + " (Meta-Rig)"
            layout.operator(op, icon='OUTLINER_OB_ARMATURE', text=text)


def get_metarigs(metarig_table: dict[str, ModuleType | dict],
                 base_dir: str, base_path: list[str], *,
                 path: Iterable[str] = (), nested=False):
    """ Searches for metarig modules, and returns a list of the
        imported modules.
    """

    dir_path = os.path.join(base_dir, *path)

    try:
        files: list[str] = os.listdir(dir_path)
    except FileNotFoundError:
        files = []

    files.sort()

    for f in files:
        is_dir = os.path.isdir(os.path.join(dir_path, f))  # Whether the file is a directory

        # Stop cases
        if f[0] in [".", "_"]:
            continue
        if f.count(".") >= 2 or (is_dir and "." in f):
            print("Warning: %r, filename contains a '.', skipping" % os.path.join(*path, f))
            continue

        if is_dir:  # Check directories
            get_metarigs(metarig_table[f], base_dir, base_path, path=[*path, f], nested=True)
        elif f.endswith(".py"):
            # Check straight-up python files
            f = f[:-3]
            module = get_resource('.'.join([*base_path, *path, f]))
            if nested:
                metarig_table[f] = module
            else:
                metarig_table[METARIG_DIR][f] = module


def make_metarig_add_execute(module):
    """ Create an execute method for a metarig creation operator.
    """
    def execute(_self, context):
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
        module.create(obj)

        bpy.ops.object.mode_set(mode='OBJECT')
        return {'FINISHED'}
    return execute


def make_metarig_menu_func(bl_idname: str, text: str):
    """ For some reason lambdas don't work for adding multiple menu
        items, so we use this instead to generate the functions.
    """
    def metarig_menu(self, _context):
        self.layout.operator(bl_idname, icon='OUTLINER_OB_ARMATURE', text=text)
    return metarig_menu


def make_submenu_func(bl_idname: str, text: str):
    def metarig_menu(self, _context):
        self.layout.menu(bl_idname, icon='OUTLINER_OB_ARMATURE', text=text)
    return metarig_menu


# Get the metarig modules
def get_internal_metarigs():
    base_rigify_dir = os.path.dirname(__file__)
    base_rigify_path = __name__.split('.')[:-1]

    get_metarigs(metarigs,
                 os.path.join(base_rigify_dir, METARIG_DIR),
                 [*base_rigify_path, METARIG_DIR])


def infinite_default_dict():
    return defaultdict(infinite_default_dict)


metarigs = infinite_default_dict()
metarig_ops = {}
armature_submenus = []
menu_funcs = []


def create_metarig_ops(dic: dict | None = None):
    if dic is None:
        dic = metarigs

    """Create metarig add Operators"""
    for metarig_category in dic:
        if metarig_category == "external":
            create_metarig_ops(dic[metarig_category])
            continue
        if metarig_category not in metarig_ops:
            metarig_ops[metarig_category] = []
        for m in dic[metarig_category].values():
            name = m.__name__.rsplit('.', 1)[1]

            # Dynamically construct an Operator
            op_type = type("Add_" + name + "_Metarig", (bpy.types.Operator,), {})
            op_type.bl_idname = "object.armature_" + name + "_metarig_add"
            op_type.bl_label = "Add " + name.replace("_", " ").capitalize() + " (metarig)"
            op_type.bl_options = {'REGISTER', 'UNDO'}
            op_type.execute = make_metarig_add_execute(m)

            metarig_ops[metarig_category].append((op_type, name))


def create_menu_funcs():
    global menu_funcs
    for mop, name in metarig_ops[METARIG_DIR]:
        text = capwords(name.replace("_", " ")) + " (Meta-Rig)"
        menu_funcs += [make_metarig_menu_func(mop.bl_idname, text)]


def create_armature_submenus(dic: dict | None = None):
    if dic is None:
        dic = metarigs
    global menu_funcs
    metarig_categories = list(dic.keys())
    metarig_categories.sort()
    for metarig_category in metarig_categories:
        # Create menu functions
        if metarig_category == "external":
            create_armature_submenus(dic=metarigs["external"])
            continue
        if metarig_category == METARIG_DIR:
            continue

        armature_submenus.append(type('Class_' + metarig_category + '_submenu',
                                      (ArmatureSubMenu,), {}))
        armature_submenus[-1].bl_label = metarig_category + ' (submenu)'
        armature_submenus[-1].bl_idname = 'ARMATURE_MT_%s_class' % metarig_category
        armature_submenus[-1].operators = []
        menu_funcs += [make_submenu_func(armature_submenus[-1].bl_idname, metarig_category)]

        for mop, name in metarig_ops[metarig_category]:
            arm_sub = next((e for e in armature_submenus
                            if e.bl_label == metarig_category + ' (submenu)'),
                           '')
            arm_sub.operators.append((mop.bl_idname, name,))


def init_metarig_menu():
    get_internal_metarigs()
    create_metarig_ops()
    create_menu_funcs()
    create_armature_submenus()


#################
# Registering

def register():
    from bpy.utils import register_class

    for cl in metarig_ops:
        for mop, name in metarig_ops[cl]:
            register_class(mop)

    for arm_sub in armature_submenus:
        register_class(arm_sub)

    for mf in menu_funcs:
        bpy.types.VIEW3D_MT_armature_add.append(mf)


def unregister():
    from bpy.utils import unregister_class

    for cl in metarig_ops:
        for mop, name in metarig_ops[cl]:
            unregister_class(mop)

    for arm_sub in armature_submenus:
        unregister_class(arm_sub)

    for mf in menu_funcs:
        bpy.types.VIEW3D_MT_armature_add.remove(mf)


def get_external_metarigs(feature_module_names: list[str]):
    unregister()

    # Clear and fill metarigs public variables
    metarigs.clear()
    get_internal_metarigs()

    for module_name in feature_module_names:
        # noinspection PyBroadException
        try:
            base_dir, base_path = feature_set_list.get_dir_path(module_name, METARIG_DIR)

            get_metarigs(metarigs['external'], base_dir, base_path)
        except Exception:
            print(f"Rigify Error: Could not load feature set '{module_name}' metarigs: "
                  f"exception occurred.\n")
            traceback.print_exc()
            print("")
            feature_set_list.mark_feature_set_exception(module_name)
            continue

    metarig_ops.clear()
    armature_submenus.clear()
    menu_funcs.clear()

    create_metarig_ops()
    create_menu_funcs()
    create_armature_submenus()
    register()
