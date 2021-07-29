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
# Contributed to by:
# Pontiac, Fourmadmen, varkenvarken, tuga3d, meta-androcto, metalliandy     #
# dreampainter, cotejrp1, liero, Kayo Phoenix, sugiany, dommetysk, Jambay   #
# Phymec, Anthony D'Agostino, Pablo Vazquez, Richard Wilks, lijenstina,     #
# Sjaak-de-Draak, Phil Cote, cotejrp1, xyz presets by elfnor, revolt_randy, #


bl_info = {
    "name": "Extra Objects",
    "author": "Multiple Authors",
    "version": (0, 3, 2),
    "blender": (2, 74, 5),
    "location": "View3D > Add > Mesh",
    "description": "Add extra mesh object types",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Add_Mesh/Add_Extra",
    "category": "Add Mesh",
}

# Note: Blocks has to be loaded before the WallFactory or the script
#       will not work properly after (F8) reload

if "bpy" in locals():
    import importlib
    importlib.reload(add_mesh_star)
    importlib.reload(add_mesh_twisted_torus)
    importlib.reload(add_mesh_gemstones)
    importlib.reload(add_mesh_gears)
    importlib.reload(add_mesh_3d_function_surface)
    importlib.reload(add_mesh_round_cube)
    importlib.reload(add_mesh_supertoroid)
    importlib.reload(add_mesh_pyramid)
    importlib.reload(add_mesh_torusknot)
    importlib.reload(add_mesh_honeycomb)
    importlib.reload(add_mesh_teapot)
    importlib.reload(add_mesh_pipe_joint)
    importlib.reload(add_mesh_solid)
    importlib.reload(add_mesh_round_brilliant)
    importlib.reload(add_mesh_menger_sponge)
    importlib.reload(add_mesh_vertex)
    importlib.reload(add_empty_as_parent)
    importlib.reload(mesh_discombobulator)
    importlib.reload(add_mesh_beam_builder)
    importlib.reload(Blocks)
    importlib.reload(Wallfactory)
    importlib.reload(add_shape_geodesic)
    importlib.reload(forms_271)
    importlib.reload(geodesic_classes_271)
    importlib.reload(third_domes_panel_271)
    importlib.reload(vefm_271)
    importlib.reload(add_mesh_triangles)
else:
    from . import add_mesh_star
    from . import add_mesh_twisted_torus
    from . import add_mesh_gemstones
    from . import add_mesh_gears
    from . import add_mesh_3d_function_surface
    from . import add_mesh_round_cube
    from . import add_mesh_supertoroid
    from . import add_mesh_pyramid
    from . import add_mesh_torusknot
    from . import add_mesh_honeycomb
    from . import add_mesh_teapot
    from . import add_mesh_pipe_joint
    from . import add_mesh_solid
    from . import add_mesh_round_brilliant
    from . import add_mesh_menger_sponge
    from . import add_mesh_vertex
    from . import add_empty_as_parent
    from . import mesh_discombobulator
    from . import add_mesh_beam_builder
    from . import Blocks
    from . import Wallfactory
    from . import add_mesh_triangles

    from .geodesic_domes import add_shape_geodesic
    from .geodesic_domes import forms_271
    from .geodesic_domes import geodesic_classes_271
    from .geodesic_domes import third_domes_panel_271
    from .geodesic_domes import vefm_271

import bpy
from bpy.types import Menu
from bpy.props import (
                BoolProperty,
                IntProperty,
                FloatProperty,
                StringProperty,
                )


class INFO_MT_mesh_vert_add(Menu):
    # Define the "Single Vert" menu
    bl_idname = "INFO_MT_mesh_vert_add"
    bl_label = "Single Vert"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_vert_add",
                        text="Add Single Vert")
        layout.separator()
        layout.operator("mesh.primitive_emptyvert_add",
                        text="Object Origin Only")
        layout.operator("mesh.primitive_symmetrical_vert_add",
                        text="Origin & Vert Mirrored")
        layout.operator("mesh.primitive_symmetrical_empty_add",
                        text="Object Origin Mirrored")


class INFO_MT_mesh_gears_add(Menu):
    # Define the "Gears" menu
    bl_idname = "INFO_MT_mesh_gears_add"
    bl_label = "Gears"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_gear",
                        text="Gear")
        layout.operator("mesh.primitive_worm_gear",
                        text="Worm")


class INFO_MT_mesh_diamonds_add(Menu):
    # Define the "Diamonds" menu
    bl_idname = "INFO_MT_mesh_diamonds_add"
    bl_label = "Diamonds"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_brilliant_add",
                        text="Brilliant Diamond")
        layout.operator("mesh.primitive_diamond_add",
                        text="Diamond")
        layout.operator("mesh.primitive_gem_add",
                        text="Gem")


class INFO_MT_mesh_math_add(Menu):
    # Define the "Math Function" menu
    bl_idname = "INFO_MT_mesh_math_add"
    bl_label = "Math Functions"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_z_function_surface",
                        text="Z Math Surface")
        layout.operator("mesh.primitive_xyz_function_surface",
                        text="XYZ Math Surface")
        self.layout.operator("mesh.primitive_solid_add", text="Regular Solid")
        self.layout.operator("mesh.make_triangle", icon="MESH_DATA")


class INFO_MT_mesh_mech(Menu):
    # Define the "Math Function" menu
    bl_idname = "INFO_MT_mesh_mech_add"
    bl_label = "Mechanical"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.menu("INFO_MT_mesh_pipe_joints_add",
                text="Pipe Joints", icon="SNAP_PEEL_OBJECT")
        layout.menu("INFO_MT_mesh_gears_add",
                text="Gears", icon="SCRIPTWIN")


class INFO_MT_mesh_extras_add(Menu):
    # Define the "Extra Objects" menu
    bl_idname = "INFO_MT_mesh_extras_add"
    bl_label = "Extras"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.menu("INFO_MT_mesh_diamonds_add", text="Diamonds",
                    icon="PMARKER_SEL")
        layout.separator()
        layout.operator("mesh.add_beam",
                        text="Beam Builder")
        layout.operator("mesh.wall_add",
                        text="Wall Factory")
        layout.separator()
        layout.operator("mesh.primitive_star_add",
                        text="Simple Star")
        layout.operator("mesh.primitive_steppyramid_add",
                        text="Step Pyramid")
        layout.operator("mesh.honeycomb_add",
                        text="Honeycomb")
        layout.operator("mesh.primitive_teapot_add",
                        text="Teapot+")
        layout.operator("mesh.menger_sponge_add",
                        text="Menger Sponge")


class INFO_MT_mesh_torus_add(Menu):
    # Define the "Torus Objects" menu
    bl_idname = "INFO_MT_mesh_torus_add"
    bl_label = "Torus Objects"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_twisted_torus_add",
                        text="Twisted Torus")
        layout.operator("mesh.primitive_supertoroid_add",
                        text="Supertoroid")
        layout.operator("mesh.primitive_torusknot_add",
                        text="Torus Knot")


class INFO_MT_mesh_pipe_joints_add(Menu):
    # Define the "Pipe Joints" menu
    bl_idname = "INFO_MT_mesh_pipe_joints_add"
    bl_label = "Pipe Joints"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("mesh.primitive_elbow_joint_add",
                        text="Pipe Elbow")
        layout.operator("mesh.primitive_tee_joint_add",
                        text="Pipe T-Joint")
        layout.operator("mesh.primitive_wye_joint_add",
                        text="Pipe Y-Joint")
        layout.operator("mesh.primitive_cross_joint_add",
                        text="Pipe Cross-Joint")
        layout.operator("mesh.primitive_n_joint_add",
                        text="Pipe N-Joint")


class discombobulator_scene_props(bpy.types.PropertyGroup):
    DISC_doodads = []
    # Protusions Buttons:
    repeatprot = IntProperty(
            name="Repeat protusions",
            description=("Make several layers of protusion \n"
                         "Use carefully, runs recursively the discombulator"),
            default=1, min=1, max=4  # set to 4 because it's 2**n reqursive
            )
    doprots = BoolProperty(
            name="Make protusions",
            description="Check if we want to add protusions to the mesh",
            default=True
            )
    subpolygon1 = BoolProperty(
            name="1",
            default=True
            )
    subpolygon2 = BoolProperty(
            name="2",
            default=True
            )
    subpolygon3 = BoolProperty(
            name="3",
            default=True
            )
    subpolygon4 = BoolProperty(
            name="4",
            default=True
            )
    polygonschangedpercent = FloatProperty(
            name="Polygon %",
            description="Percentage of changed polygons",
            default=1.0
            )
    minHeight = FloatProperty(
            name="Min height",
            description="Minimal height of the protusions",
            default=0.2
            )
    maxHeight = FloatProperty(
            name="Max height",
            description="Maximal height of the protusions",
            default=0.4
            )
    minTaper = FloatProperty(
            name="Min taper",
            description="Minimal height of the protusions",
            default=0.15, min=0.0, max=1.0,
            subtype='PERCENTAGE'
            )
    maxTaper = FloatProperty(
            name="Max taper",
            description="Maximal height of the protusions",
            default=0.35, min=0.0, max=1.0,
            subtype='PERCENTAGE'
            )
    # Doodads buttons:
    dodoodads = BoolProperty(
            name="Make doodads",
            description="Check if we want to generate doodads",
            default=False
            )
    mindoodads = IntProperty(
            name="Minimum doodads number",
            description="Ask for the minimum number of doodads to generate per polygon",
            default=1, min=0, max=50
            )
    maxdoodads = IntProperty(
            name="Maximum doodads number",
            description="Ask for the maximum number of doodads to generate per polygon",
            default=6, min=1, max=50
            )
    doodMinScale = FloatProperty(
            name="Scale min", description="Minimum scaling of doodad",
            default=0.5, min=0.0, max=1.0,
            subtype='PERCENTAGE'
            )
    doodMaxScale = FloatProperty(
            name="Scale max",
            description="Maximum scaling of doodad",
            default=1.0, min=0.0, max=1.0,
            subtype='PERCENTAGE'
            )
    # Materials buttons:
    sideProtMat = IntProperty(
            name="Side's prot mat",
            description="Material of protusion's sides",
            default=0, min=0
            )
    topProtMat = IntProperty(
            name="Prot's top mat",
            description="Material of protusion's top",
            default=0, min=0
            )


# Register all operators and panels

# Define "Extras" menu
def menu_func(self, context):
    lay_out = self.layout
    lay_out.operator_context = 'INVOKE_REGION_WIN'

    lay_out.separator()
    lay_out.menu("INFO_MT_mesh_vert_add",
                text="Single Vert", icon="LAYER_ACTIVE")
    lay_out.operator("mesh.primitive_round_cube_add",
                    text="Round Cube", icon="MOD_SUBSURF")
    lay_out.menu("INFO_MT_mesh_math_add",
                text="Math Function", icon="PACKAGE")
    lay_out.menu("INFO_MT_mesh_mech_add",
                text="Mechanical", icon="SCRIPTWIN")
    lay_out.menu("INFO_MT_mesh_torus_add",
                text="Torus Objects", icon="MESH_TORUS")
    lay_out.separator()
    lay_out.operator("mesh.generate_geodesic_dome",
                    text="Geodesic Dome", icon="MESH_ICOSPHERE")
    lay_out.operator("discombobulate.ops",
                    text="Discombobulator", icon="RETOPO")
    lay_out.separator()
    lay_out.menu("INFO_MT_mesh_extras_add",
                text="Extras", icon="MESH_DATA")
    lay_out.separator()
    lay_out.operator("object.parent_to_empty",
                    text="Parent To Empty", icon="LINK_AREA")


def register():
    bpy.utils.register_module(__name__)

    # Register Discombobulator properties
    bpy.types.Scene.discomb = bpy.props.PointerProperty(
                                        type=discombobulator_scene_props
                                        )
    # Error messages for Geodesic Domes
    bpy.types.Scene.error_message = StringProperty(
                                        name="actual error",
                                        default=""
                                        )
    bpy.types.Scene.geodesic_not_yet_called = BoolProperty(
                                        name="geodesic_not_called",
                                        default=True
                                        )
    bpy.types.Scene.gd_help_text_width = IntProperty(
                                        name="Text Width",
                                        description="The width above which the text wraps",
                                        default=60,
                                        max=180, min=20
                                        )

    # Add "Extras" menu to the "Add Mesh" menu
    bpy.types.INFO_MT_mesh_add.append(menu_func)


def unregister():
    # Remove "Extras" menu from the "Add Mesh" menu.
    bpy.types.INFO_MT_mesh_add.remove(menu_func)

    del bpy.types.Scene.discomb
    del bpy.types.Scene.error_message
    del bpy.types.Scene.geodesic_not_yet_called
    del bpy.types.Scene.gd_help_text_width

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
