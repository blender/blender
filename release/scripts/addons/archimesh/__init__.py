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

# ----------------------------------------------------------
# Author: Antonio Vazquez (antonioya)
# ----------------------------------------------------------

# ----------------------------------------------
# Define Addon info
# ----------------------------------------------
bl_info = {
    "name": "Archimesh",
    "author": "Antonio Vazquez (antonioya)",
    "location": "View3D > Add > Mesh > Archimesh",
    "version": (1, 1, 4),
    "blender": (2, 6, 8),
    "description": "Generate rooms, doors, windows, and other architecture objects",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/Scripts/Add_Mesh/Archimesh",
    "category": "Add Mesh"
    }

import sys
import os

# ----------------------------------------------
# Import modules
# ----------------------------------------------
if "bpy" in locals():
    import importlib
    importlib.reload(achm_room_maker)
    importlib.reload(achm_door_maker)
    importlib.reload(achm_window_maker)
    importlib.reload(achm_roof_maker)
    importlib.reload(achm_column_maker)
    importlib.reload(achm_stairs_maker)
    importlib.reload(achm_kitchen_maker)
    importlib.reload(achm_shelves_maker)
    importlib.reload(achm_books_maker)
    importlib.reload(achm_lamp_maker)
    importlib.reload(achm_curtain_maker)
    importlib.reload(achm_venetian_maker)
    importlib.reload(achm_main_panel)
    importlib.reload(achm_window_panel)
    print("archimesh: Reloaded multifiles")
else:
    from . import achm_books_maker
    from . import achm_column_maker
    from . import achm_curtain_maker
    from . import achm_venetian_maker
    from . import achm_door_maker
    from . import achm_kitchen_maker
    from . import achm_lamp_maker
    from . import achm_main_panel
    from . import achm_roof_maker
    from . import achm_room_maker
    from . import achm_shelves_maker
    from . import achm_stairs_maker
    from . import achm_window_maker
    from . import achm_window_panel

    print("archimesh: Imported multifiles")

# noinspection PyUnresolvedReferences
import bpy
# noinspection PyUnresolvedReferences
from bpy.props import (
        BoolProperty,
        FloatVectorProperty,
        IntProperty,
        FloatProperty,
        StringProperty,
        )
# noinspection PyUnresolvedReferences
from bpy.types import (
        AddonPreferences,
        Menu,
        Scene,
        INFO_MT_mesh_add,
        WindowManager,
        )

# ----------------------------------------------------------
# Decoration assets
# ----------------------------------------------------------


class AchmInfoMtMeshDecorationAdd(Menu):
    bl_idname = "INFO_MT_mesh_decoration_add"
    bl_label = "Decoration assets"

    # noinspection PyUnusedLocal
    def draw(self, context):
        self.layout.operator("mesh.archimesh_books", text="Add Books")
        self.layout.operator("mesh.archimesh_lamp", text="Add Lamp")
        self.layout.operator("mesh.archimesh_roller", text="Add Roller curtains")
        self.layout.operator("mesh.archimesh_venetian", text="Add Venetian blind")
        self.layout.operator("mesh.archimesh_japan", text="Add Japanese curtains")

# ----------------------------------------------------------
# Registration
# ----------------------------------------------------------


class AchmInfoMtMeshCustomMenuAdd(Menu):
    bl_idname = "INFO_MT_mesh_custom_menu_add"
    bl_label = "Archimesh"

    # noinspection PyUnusedLocal
    def draw(self, context):
        self.layout.operator_context = 'INVOKE_REGION_WIN'
        self.layout.operator("mesh.archimesh_room", text="Add Room")
        self.layout.operator("mesh.archimesh_door", text="Add Door")
        self.layout.operator("mesh.archimesh_window", text="Add Rail Window")
        self.layout.operator("mesh.archimesh_winpanel", text="Add Panel Window")
        self.layout.operator("mesh.archimesh_kitchen", text="Add Cabinet")
        self.layout.operator("mesh.archimesh_shelves", text="Add Shelves")
        self.layout.operator("mesh.archimesh_column", text="Add Column")
        self.layout.operator("mesh.archimesh_stairs", text="Add Stairs")
        self.layout.operator("mesh.archimesh_roof", text="Add Roof")
        self.layout.menu("INFO_MT_mesh_decoration_add", text="Decoration props", icon="GROUP")

# --------------------------------------------------------------
# Register all operators and panels
# --------------------------------------------------------------


# Add-ons Preferences Update Panel

# Define Panel classes for updating
panels = (
        achm_main_panel.ArchimeshMainPanel,
        )


def update_panel(self, context):
    message = "Archimesh: Updating Panel locations has failed"
    try:
        for panel in panels:
            if "bl_rna" in panel.__dict__:
                bpy.utils.unregister_class(panel)

        for panel in panels:
            panel.bl_category = context.user_preferences.addons[__name__].preferences.category
            bpy.utils.register_class(panel)

    except Exception as e:
        print("\n[{}]\n{}\n\nError:\n{}".format(__name__, message, e))
        pass


class Archi_Pref(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    category = StringProperty(
            name="Tab Category",
            description="Choose a name for the category of the panel",
            default="Create",
            update=update_panel
            )

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        col = row.column()
        col.label(text="Tab Category:")
        col.prop(self, "category", text="")


# Define menu
# noinspection PyUnusedLocal
def AchmMenu_func(self, context):
    self.layout.menu("INFO_MT_mesh_custom_menu_add", icon="GROUP")


def register():
    bpy.utils.register_class(AchmInfoMtMeshCustomMenuAdd)
    bpy.utils.register_class(AchmInfoMtMeshDecorationAdd)
    bpy.utils.register_class(achm_room_maker.AchmRoom)
    bpy.utils.register_class(achm_room_maker.AchmRoomGeneratorPanel)
    bpy.utils.register_class(achm_room_maker.AchmExportRoom)
    bpy.utils.register_class(achm_room_maker.AchmImportRoom)
    bpy.utils.register_class(achm_door_maker.AchmDoor)
    bpy.utils.register_class(achm_door_maker.AchmDoorObjectgeneratorpanel)
    bpy.utils.register_class(achm_window_maker.AchmWindows)
    bpy.utils.register_class(achm_window_maker.AchmWindowObjectgeneratorpanel)
    bpy.utils.register_class(achm_roof_maker.AchmRoof)
    bpy.utils.register_class(achm_column_maker.AchmColumn)
    bpy.utils.register_class(achm_stairs_maker.AchmStairs)
    bpy.utils.register_class(achm_kitchen_maker.AchmKitchen)
    bpy.utils.register_class(achm_kitchen_maker.AchmExportInventory)
    bpy.utils.register_class(achm_shelves_maker.AchmShelves)
    bpy.utils.register_class(achm_books_maker.AchmBooks)
    bpy.utils.register_class(achm_lamp_maker.AchmLamp)
    bpy.utils.register_class(achm_curtain_maker.AchmRoller)
    bpy.utils.register_class(achm_curtain_maker.AchmJapan)
    bpy.utils.register_class(achm_venetian_maker.AchmVenetian)
    bpy.utils.register_class(achm_venetian_maker.AchmVenetianObjectgeneratorpanel)
    bpy.utils.register_class(achm_main_panel.ArchimeshMainPanel)
    bpy.utils.register_class(achm_main_panel.AchmHoleAction)
    bpy.utils.register_class(achm_main_panel.AchmPencilAction)
    bpy.utils.register_class(achm_main_panel.AchmRunHintDisplayButton)
    bpy.utils.register_class(achm_window_panel.AchmWinPanel)
    bpy.utils.register_class(achm_window_panel.AchmWindowEditPanel)
    bpy.utils.register_class(Archi_Pref)
    INFO_MT_mesh_add.append(AchmMenu_func)
    update_panel(None, bpy.context)
    # Define properties
    Scene.archimesh_select_only = BoolProperty(
            name="Only selected",
            description="Apply auto holes only to selected objects",
            default=False,
            )
    Scene.archimesh_ceiling = BoolProperty(
            name="Ceiling",
            description="Create a ceiling",
            default=False,
            )
    Scene.archimesh_floor = BoolProperty(
            name="Floor",
            description="Create a floor automatically",
            default=False,
            )

    Scene.archimesh_merge = BoolProperty(
            name="Close walls",
            description="Close walls to create a full closed room",
            default=False,
            )

    Scene.archimesh_text_color = FloatVectorProperty(
            name="Hint color",
            description="Color for the text and lines",
            default=(0.173, 0.545, 1.0, 1.0),
            min=0.1,
            max=1,
            subtype='COLOR',
            size=4,
            )
    Scene.archimesh_walltext_color = FloatVectorProperty(
            name="Hint color",
            description="Color for the wall label",
            default=(1, 0.8, 0.1, 1.0),
            min=0.1,
            max=1,
            subtype='COLOR',
            size=4,
            )
    Scene.archimesh_font_size = IntProperty(
            name="Text Size",
            description="Text size for hints",
            default=14, min=10, max=150,
            )
    Scene.archimesh_wfont_size = IntProperty(
            name="Text Size",
            description="Text size for wall labels",
            default=16, min=10, max=150,
            )
    Scene.archimesh_hint_space = FloatProperty(
            name='Separation', min=0, max=5, default=0.1,
            precision=2,
            description='Distance from object to display hint',
            )
    Scene.archimesh_gl_measure = BoolProperty(
            name="Measures",
            description="Display measures",
            default=True,
            )
    Scene.archimesh_gl_name = BoolProperty(
            name="Names",
            description="Display names",
            default=True,
            )
    Scene.archimesh_gl_ghost = BoolProperty(
            name="All",
            description="Display measures for all objects,"
            " not only selected",
            default=True,
            )

    # OpenGL flag
    wm = WindowManager
    # register internal property
    wm.archimesh_run_opengl = BoolProperty(default=False)


def unregister():
    bpy.utils.unregister_class(AchmInfoMtMeshCustomMenuAdd)
    bpy.utils.unregister_class(AchmInfoMtMeshDecorationAdd)
    bpy.utils.unregister_class(achm_room_maker.AchmRoom)
    bpy.utils.unregister_class(achm_room_maker.AchmRoomGeneratorPanel)
    bpy.utils.unregister_class(achm_room_maker.AchmExportRoom)
    bpy.utils.unregister_class(achm_room_maker.AchmImportRoom)
    bpy.utils.unregister_class(achm_door_maker.AchmDoor)
    bpy.utils.unregister_class(achm_door_maker.AchmDoorObjectgeneratorpanel)
    bpy.utils.unregister_class(achm_window_maker.AchmWindows)
    bpy.utils.unregister_class(achm_window_maker.AchmWindowObjectgeneratorpanel)
    bpy.utils.unregister_class(achm_roof_maker.AchmRoof)
    bpy.utils.unregister_class(achm_column_maker.AchmColumn)
    bpy.utils.unregister_class(achm_stairs_maker.AchmStairs)
    bpy.utils.unregister_class(achm_kitchen_maker.AchmKitchen)
    bpy.utils.unregister_class(achm_kitchen_maker.AchmExportInventory)
    bpy.utils.unregister_class(achm_shelves_maker.AchmShelves)
    bpy.utils.unregister_class(achm_books_maker.AchmBooks)
    bpy.utils.unregister_class(achm_lamp_maker.AchmLamp)
    bpy.utils.unregister_class(achm_curtain_maker.AchmRoller)
    bpy.utils.unregister_class(achm_curtain_maker.AchmJapan)
    bpy.utils.unregister_class(achm_venetian_maker.AchmVenetian)
    bpy.utils.unregister_class(achm_venetian_maker.AchmVenetianObjectgeneratorpanel)
    bpy.utils.unregister_class(achm_main_panel.ArchimeshMainPanel)
    bpy.utils.unregister_class(achm_main_panel.AchmHoleAction)
    bpy.utils.unregister_class(achm_main_panel.AchmPencilAction)
    bpy.utils.unregister_class(achm_main_panel.AchmRunHintDisplayButton)
    bpy.utils.unregister_class(achm_window_panel.AchmWinPanel)
    bpy.utils.unregister_class(achm_window_panel.AchmWindowEditPanel)
    bpy.utils.unregister_class(Archi_Pref)
    INFO_MT_mesh_add.remove(AchmMenu_func)

    # Remove properties
    del Scene.archimesh_select_only
    del Scene.archimesh_ceiling
    del Scene.archimesh_floor
    del Scene.archimesh_merge
    del Scene.archimesh_text_color
    del Scene.archimesh_walltext_color
    del Scene.archimesh_font_size
    del Scene.archimesh_wfont_size
    del Scene.archimesh_hint_space
    del Scene.archimesh_gl_measure
    del Scene.archimesh_gl_name
    del Scene.archimesh_gl_ghost
    # remove OpenGL data
    achm_main_panel.AchmRunHintDisplayButton.handle_remove(achm_main_panel.AchmRunHintDisplayButton, bpy.context)
    wm = bpy.context.window_manager
    p = 'archimesh_run_opengl'
    if p in wm:
        del wm[p]


if __name__ == '__main__':
    register()
