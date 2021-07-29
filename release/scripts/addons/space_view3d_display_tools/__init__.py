# space_view_3d_display_tools.py Copyright (C) 2014, Jordi Vall-llovera
# Multiple display tools for fast navigate/interact with the viewport

# ##### BEGIN GPL LICENSE BLOCK #####
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENCE BLOCK #####
# Contributed to by:
# Jasperge, Pixaal, Meta-androcto, Lapineige, lijenstina,
# Felix Schlitter, Ales Sidenko, Jakub Belcik

bl_info = {
    "name": "Display Tools",
    "author": "Jordi Vall-llovera Medina, Jhon Wallace",
    "version": (1, 6, 4),
    "blender": (2, 7, 0),
    "location": "Toolshelf",
    "description": "Display tools for fast navigation/interaction with the viewport",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/3D_interaction/Display_Tools",
    "category": "3D View"}


# Import From Files
if "bpy" in locals():
    import importlib
    importlib.reload(display)
    importlib.reload(fast_navigate)
    importlib.reload(modifier_tools)

    importlib.reload(shading_menu)
    importlib.reload(select_tools)
    importlib.reload(useless_tools)
    importlib.reload(selection_restrictor)

else:
    from . import display
    from . import fast_navigate
    from . import modifier_tools

    from . import shading_menu
    from . import select_tools
    from . import useless_tools
    from . import selection_restrictor

import bpy
from bpy.types import (
        Panel,
        PropertyGroup,
        AddonPreferences,
        )
from bpy.props import (
        IntProperty,
        BoolProperty,
        BoolVectorProperty,
        EnumProperty,
        StringProperty,
        PointerProperty,
        )


class DisplayToolsPanel(Panel):
    bl_label = "Display Tools"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    draw_type_icons = {
            'BOUNDS': 'BBOX',
            'WIRE': 'WIRE',
            'SOLID': 'SOLID',
            'TEXTURED': 'POTATO'
            }
    bounds_icons = {
            'BOX': 'MESH_CUBE',
            'SPHERE': 'MATSPHERE',
            'CYLINDER': 'MESH_CYLINDER',
            'CONE': 'MESH_CONE'
            }

    def draw(self, context):
        scene = context.scene
        display_tools = scene.display_tools
        render = scene.render
        view = context.space_data
        gs = scene.game_settings
        obj = context.object
        obj_type = obj.type if obj else None
        fx_settings = view.fx_settings

        DISPLAYDROP = display_tools.UiTabDrop[0]
        SHADINGDROP = display_tools.UiTabDrop[1]
        SCENEDROP = display_tools.UiTabDrop[2]
        MODIFIERDROP = display_tools.UiTabDrop[3]
        SELECT2DROP = display_tools.UiTabDrop[4]
        FASTNAVDROP = display_tools.UiTabDrop[5]
        icon_active_0 = "TRIA_RIGHT" if not DISPLAYDROP else "TRIA_DOWN"
        icon_active_1 = "TRIA_RIGHT" if not SHADINGDROP else "TRIA_DOWN"
        icon_active_2 = "TRIA_RIGHT" if not SCENEDROP else "TRIA_DOWN"
        icon_active_3 = "TRIA_RIGHT" if not MODIFIERDROP else "TRIA_DOWN"
        icon_active_4 = "TRIA_RIGHT" if not SELECT2DROP else "TRIA_DOWN"
        icon_active_5 = "TRIA_RIGHT" if not FASTNAVDROP else "TRIA_DOWN"
        icon_wt_handler = "X" if display_tools.WT_handler_enable else "MOD_WIREFRAME"

        layout = self.layout

        # Display Scene options
        box1 = self.layout.box()
        col = box1.column(align=True)
        row = col.row(align=True)
        row.prop(display_tools, "UiTabDrop", index=2, text="Display", icon=icon_active_2)

        if not SCENEDROP:
            if obj:
                row.prop(obj, "show_texture_space", text="", icon="FACESEL_HLT")
                row.prop(obj, "show_name", text="", icon="SORTALPHA")
                row.prop(obj, "show_axis", text="", icon="AXIS_TOP")
        else:
            col = layout.column()
            col.prop(view, "show_manipulator")

            col = layout.column(align=True)
            col.alignment = 'EXPAND'
            col.prop(view, "show_only_render", toggle=True)
            col.prop(view, "show_world", toggle=True)
            col.prop(view, "show_outline_selected", toggle=True)
            col.prop(view, "show_all_objects_origin", toggle=True)
            col.prop(view, "show_backface_culling", toggle=True)
            if obj:
                col.prop(obj, "show_x_ray", text="X-Ray", toggle=True)

            if obj and obj_type == 'MESH':
                col.prop(obj, "show_transparent", text="Transparency", toggle=True)

            col = layout.column()
            col.prop(render, "use_simplify", "Simplify", toggle=True)

            if render.use_simplify is True:
                col = layout.column(align=True)
                col.label("Settings :")
                col.prop(render, "simplify_subdivision", "Subdivision")
                col.prop(render, "simplify_shadow_samples", "Shadow Samples")
                col.prop(render, "simplify_child_particles", "Child Particles")
                col.prop(render, "simplify_ao_sss", "AO and SSS")

        # Draw Type options
        box1 = self.layout.box()
        col = box1.column(align=True)
        row = col.row(align=True)
        row.prop(display_tools, "UiTabDrop", index=0, text="Draw Type", icon=icon_active_0)

        if not DISPLAYDROP:
            hide_wires = row.operator("ut.wire_show_hide", icon="MATSPHERE", text="")
            hide_wires.show = False
            hide_wires.selected = False
            show_wires = row.operator("ut.wire_show_hide", icon="MESH_UVSPHERE", text="")
            show_wires.show = True
            show_wires.selected = False
            row.operator("ut.all_edges", icon="MESH_GRID", text="").on = True
        else:
            if obj:
                col = layout.column(align=True)
                col.alignment = 'EXPAND'
                col.label(text="Maximum:")
                col.prop(obj, "draw_type", text="", icon=self.draw_type_icons[obj.draw_type])

            col = layout.column(align=True)
            col.alignment = 'CENTER'
            col.label(text="Selected Object(s):")
            row = col.row(align=True)
            row.operator("view3d.display_draw_change", text="Wire",
                         icon='WIRE').drawing = 'WIRE'
            row.operator("view3d.display_draw_change", text="Solid",
                        icon='SOLID').drawing = 'SOLID'
            row = col.row()
            row = col.row(align=True)
            row.operator("view3d.display_draw_change", text="Textured",
                         icon="TEXTURE_SHADED").drawing = 'TEXTURED'
            row.operator("view3d.display_draw_change", text="Bounds",
                         icon="BBOX").drawing = 'BOUNDS'

            col = layout.column(align=True)
            col.alignment = 'CENTER'
            col.label(text="Wire Overlay:")

            row = col.row()
            row.operator("object.wt_selection_handler_toggle", icon=icon_wt_handler)

            col = layout.column(align=True)
            col.alignment = 'CENTER'
            row = col.row(align=True)
            row.operator("object.wt_hide_all_wire", icon="SOLID", text="Hide All")
            row.operator("af_ops.wire_all", text="Toggle", icon="WIRE")

            row = col.row()
            row1 = col.row(align=True)
            hide_wire = row1.operator("ut.wire_show_hide", icon="MATSPHERE", text="Hide")
            hide_wire.show = False
            hide_wire.selected = True
            show_wire = row1.operator("ut.wire_show_hide", icon="MESH_UVSPHERE", text="Show")
            show_wire.show = True
            show_wire.selected = True

            col = layout.column(align=True)
            col.alignment = 'CENTER'
            row = col.row()
            row3 = col.row(align=True)
            row3.alignment = 'CENTER'
            row3.label(text="All Edges:")
            row3.operator("ut.all_edges", icon="MESH_PLANE", text="Off").on = False
            row3.operator("ut.all_edges", icon="MESH_GRID", text="On").on = True

            col = layout.column(align=True)
            col.alignment = 'EXPAND'
            col.label("Bounding Box:")
            row = col.row()
            row.prop(display_tools, "BoundingMode", text="Type")
            row = col.row()
            col.separator()
            col.operator("view3d.display_bounds_switch", "Bounds On",
                        icon='BBOX').bounds = True
            col.operator("view3d.display_bounds_switch", "Bounds Off",
                        icon='BBOX').bounds = False

        # Shading options
        box1 = self.layout.box()
        col = box1.column(align=True)
        row = col.row(align=True)
        row.prop(display_tools, "UiTabDrop", index=1, text="Shading", icon=icon_active_1)

        if not SHADINGDROP:
            row.operator("object.shade_smooth", icon="SMOOTH", text="")
            row.operator("object.shade_flat", icon="MESH_ICOSPHERE", text="")
            row.menu("VIEW3D_MT_Shade_menu", icon='SOLID', text="")
        else:
            col = layout.column(align=True)
            col.alignment = 'EXPAND'

            if not scene.render.use_shading_nodes:
                col.prop(gs, "material_mode", text="", toggle=True)

            if view.viewport_shade == 'SOLID':
                col.prop(view, "show_textured_solid", toggle=True)
                col.prop(view, "use_matcap", toggle=True)
                if view.use_matcap:
                    col.template_icon_view(view, "matcap_icon")
            if view.viewport_shade == 'TEXTURED' or context.mode == 'PAINT_TEXTURE':
                if scene.render.use_shading_nodes or gs.material_mode != 'GLSL':
                    col.prop(view, "show_textured_shadeless", toggle=True)

            col.prop(view, "show_backface_culling", toggle=True)

            if view.viewport_shade not in {'BOUNDBOX', 'WIREFRAME'}:
                if obj and obj.mode == 'EDIT':
                    col.prop(view, "show_occlude_wire", toggle=True)
                if obj and obj_type == 'MESH' and obj.mode in {'EDIT'}:
                    col = layout.column(align=True)
                    col.label(text="Faces:")
                    row = col.row(align=True)
                    row.operator("mesh.faces_shade_smooth", text="Smooth")
                    row.operator("mesh.faces_shade_flat", text="Flat")
                    col.label(text="Edges:")
                    row = col.row(align=True)
                    row.operator("mesh.mark_sharp", text="Smooth").clear = True
                    row.operator("mesh.mark_sharp", text="Sharp")
                    col.label(text="Vertices:")
                    row = col.row(align=True)
                    props = row.operator("mesh.mark_sharp", text="Smooth")
                    props.use_verts = True
                    props.clear = True
                    row.operator("mesh.mark_sharp", text="Sharp").use_verts = True

                    col = layout.column(align=True)
                    col.label(text="Normals:")
                    col.operator("mesh.normals_make_consistent", text="Recalculate")
                    col.operator("mesh.flip_normals", text="Flip Direction")
                    col.operator("mesh.set_normals_from_faces", text="Set From Faces")
                    col.separator()

            if view.viewport_shade not in {'BOUNDBOX', 'WIREFRAME'}:
                sub = col.column()
                sub.active = view.region_3d.view_perspective == 'CAMERA'
                sub.prop(fx_settings, "use_dof", toggle=True)
                col.prop(fx_settings, "use_ssao", text="Ambient Occlusion", toggle=True)
                if fx_settings.use_ssao:
                    ssao_settings = fx_settings.ssao
                    subcol = col.column(align=True)
                    subcol.prop(ssao_settings, "factor")
                    subcol.prop(ssao_settings, "distance_max")
                    subcol.prop(ssao_settings, "attenuation")
                    subcol.prop(ssao_settings, "samples")
                    subcol.prop(ssao_settings, "color")

        # Modifier options
        box1 = self.layout.box()
        col = box1.column(align=True)
        row = col.row(align=True)
        row.prop(display_tools, "UiTabDrop", index=3, text="Modifiers", icon=icon_active_3)

        if not MODIFIERDROP:
            mod_all_hide = row.operator("ut.subsurf_show_hide", icon="MOD_SOLIDIFY", text="")
            mod_all_hide.show = False
            mod_all_hide.selected = False
            mod_all_show = row.operator("ut.subsurf_show_hide", icon="MOD_SUBSURF", text="")
            mod_all_show.show = True
            mod_all_show.selected = False
            mod_optimal = row.operator("ut.optimaldisplay", icon="MESH_PLANE", text="")
            mod_optimal.on = True
            mod_optimal.selected = False
        else:
            col = layout.column(align=True)
            col.alignment = 'EXPAND'

            row = col.row(align=True)
            row.label(text="Viewport Visibility:", icon="RESTRICT_VIEW_OFF")
            row = col.row(align=True)
            row.operator("view3d.toggle_apply_modifiers_view", text="Viewport Vis")
            col.separator()

            row = col.row()
            row.label(text="Render Visibility:", icon="RENDER_STILL")
            row = col.row(align=True)
            row.operator("view3d.display_modifiers_render_switch", text="On").mod_render = True
            row.operator("view3d.display_modifiers_render_switch", text="Off").mod_render = False
            col.separator()

            row = col.row()
            row.label("Subsurf Visibility:", icon="ALIASED")

            col = layout.column(align=True)
            row1 = col.row(align=True)
            mod_all2_hide = row1.operator("ut.subsurf_show_hide", icon="MOD_SOLIDIFY", text="Hide")
            mod_all2_hide.show = False
            mod_all2_hide.selected = True
            mod_all2_show = row1.operator("ut.subsurf_show_hide", icon="MOD_SUBSURF", text="Show")
            mod_all2_show.show = True
            mod_all2_show.selected = True

            row2 = col.row(align=True)
            mod_sel_hide = row2.operator("ut.subsurf_show_hide", icon="MOD_SOLIDIFY", text="Hide All")
            mod_sel_hide.show = False
            mod_sel_hide.selected = False
            mod_sel_show = row2.operator("ut.subsurf_show_hide", icon="MOD_SUBSURF", text="Show All")
            mod_sel_show.show = True
            mod_sel_show.selected = False
            col.separator()

            col = layout.column()
            row = col.row(align=True)
            row.label(text="Edit Mode:", icon="EDITMODE_HLT")
            row = col.row(align=True)
            row.operator("view3d.display_modifiers_edit_switch", text="On").mod_edit = True
            row.operator("view3d.display_modifiers_edit_switch", text="Off").mod_edit = False
            col.separator()

            row = col.row()
            row.label(text="Modifier Cage:", icon="MOD_LATTICE")
            row = col.row(align=True)
            row.operator("view3d.display_modifiers_cage_set", text="On").set_cage = True
            row.operator("view3d.display_modifiers_cage_set", text="Off").set_cage = False
            col.separator()

            row = col.row(align=True)
            row.label("Subdivision Level:", icon="MOD_SUBSURF")

            row = col.row(align=True)
            row.operator("view3d.modifiers_subsurf_level_set", text="0").level = 0
            row.operator("view3d.modifiers_subsurf_level_set", text="1").level = 1
            row.operator("view3d.modifiers_subsurf_level_set", text="2").level = 2
            row.operator("view3d.modifiers_subsurf_level_set", text="3").level = 3
            row.operator("view3d.modifiers_subsurf_level_set", text="4").level = 4
            row.operator("view3d.modifiers_subsurf_level_set", text="5").level = 5
            row.operator("view3d.modifiers_subsurf_level_set", text="6").level = 6

        # Selection options
        box1 = self.layout.box()
        col = box1.column(align=True)
        row = col.row(align=True)
        row.prop(display_tools, "UiTabDrop", index=4, text="Selection", icon=icon_active_4)

        if not SELECT2DROP:
            row.operator("view3d.select_border", text="", icon="MESH_PLANE")
            row.operator("view3d.select_circle", text="", icon="MESH_CIRCLE")
            row.label(text="", icon="BLANK1")
        else:
            if obj and obj.mode == 'OBJECT':
                col = layout.column(align=True)
                col.label(text="Render Visibility:")
                col.operator("op.render_show_all_selected", icon="RESTRICT_VIEW_OFF")
                col.operator("op.render_hide_all_selected", icon="RESTRICT_VIEW_ON")
                col.label(text="Show/Hide:")
                col.operator("opr.show_hide_object", text="Show/Hide", icon="GHOST_ENABLED")
                col.operator("opr.show_all_objects", text="Show All", icon="RESTRICT_VIEW_OFF")
                col.operator("opr.hide_all_objects", text="Hide Inactive", icon="RESTRICT_VIEW_ON")

            if obj:
                if obj.mode == 'OBJECT':
                    col = layout.column(align=True)
                    col.operator_menu_enum("object.show_by_type", "type", text="Show By Type")
                    col.operator_menu_enum("object.hide_by_type", "type", text="Hide By Type")
                    layout.label(text="Selection:")
                    col = layout.column(align=True)
                    col.operator_menu_enum("object.select_by_type", "type",
                                           text="Select All by Type...")

                if obj_type == 'MESH' and obj.mode == 'EDIT':
                    col = layout.column(align=True)
                    col.operator("mesh.select_linked", icon="ROTATECOLLECTION")
                    col.operator("opr.loop_multi_select", icon="OUTLINER_DATA_MESH")

            col = layout.column(align=True)
            col.operator("opr.select_all", icon="MOD_MESHDEFORM")
            col.operator("opr.inverse_selection", icon="MOD_REMESH")

        # fast nav options
        box1 = layout.box()
        col = box1.column(align=True)
        row = col.row(align=True)
        row.prop(display_tools, "UiTabDrop", index=5, text="Fast Nav", icon=icon_active_5)

        if not FASTNAVDROP:
            row.operator("view3d.fast_navigate_operator", text="", icon="NEXT_KEYFRAME")
            row.operator("view3d.fast_navigate_stop", text="", icon="PANEL_CLOSE")
            row.label(text="", icon="BLANK1")
        else:
            col = layout.column(align=True)
            col.operator("view3d.fast_navigate_operator", icon="NEXT_KEYFRAME")
            col.operator("view3d.fast_navigate_stop", icon="PANEL_CLOSE")

            layout.label("Settings:")
            layout.prop(display_tools, "OriginalMode")
            layout.prop(display_tools, "FastMode")
            layout.prop(display_tools, "EditActive", "Edit mode")

            layout.prop(display_tools, "Delay")
            col = layout.column(align=True)
            col.active = display_tools.Delay
            col.prop(display_tools, "DelayTimeGlobal", "Delay time")

            layout.prop(display_tools, "ShowParticles")
            col = layout.column(align=True)
            col.active = display_tools.ShowParticles
            col.prop(display_tools, "InitialParticles")
            col.prop(display_tools, "ParticlesPercentageDisplay")

            col = layout.column(align=True)
            col.label("Screen Active Area:")
            col.prop(display_tools, "ScreenStart")
            col.prop(display_tools, "ScreenEnd")


# define scene props
class display_tools_scene_props(PropertyGroup):
    # Init delay variables
    Delay = BoolProperty(
            default=False,
            description="Activate delay return to normal viewport mode"
            )
    DelayTime = IntProperty(
            default=30,
            min=0,
            max=500,
            soft_min=10,
            soft_max=250,
            description="Delay time to return to normal viewport"
                        "mode after move your mouse cursor"
            )
    DelayTimeGlobal = IntProperty(
            default=30,
            min=1,
            max=500,
            soft_min=10,
            soft_max=250,
            description="Delay time to return to normal viewport"
                        "mode after move your mouse cursor"
            )
    # Init variable for fast navigate
    EditActive = BoolProperty(
            default=True,
            description="Activate for fast navigate in edit mode too"
            )
    # Init properties for scene
    FastNavigateStop = BoolProperty(
            name="Fast Navigate Stop",
            description="Stop fast navigate mode",
            default=False
            )
    OriginalMode = EnumProperty(
            items=[('TEXTURED', 'Texture', 'Texture display mode'),
                   ('SOLID', 'Solid', 'Solid display mode')],
            name="Normal",
            default='SOLID'
            )
    BoundingMode = EnumProperty(
            items=[('BOX', 'Box', 'Box shape'),
                   ('SPHERE', 'Sphere', 'Sphere shape'),
                   ('CYLINDER', 'Cylinder', 'Cylinder shape'),
                   ('CONE', 'Cone', 'Cone shape')],
            name="BB Mode"
            )
    FastMode = EnumProperty(
            items=[('WIREFRAME', 'Wireframe', 'Wireframe display'),
                   ('BOUNDBOX', 'Bounding Box', 'Bounding Box display')],
            name="Fast"
            )
    ShowParticles = BoolProperty(
            name="Show Particles",
            description="Show or hide particles on fast navigate mode",
            default=True
            )
    ParticlesPercentageDisplay = IntProperty(
            name="Fast Display",
            description="Display only a percentage of particles when active",
            default=25,
            min=0,
            max=100,
            soft_min=0,
            soft_max=100,
            subtype='FACTOR'
            )
    InitialParticles = IntProperty(
            name="Normal Display",
            description="When idle, how much particles are displayed\n"
                        "Overrides the Particles settings",
            default=100,
            min=0,
            max=100,
            soft_min=0,
            soft_max=100
            )
    Symplify = IntProperty(
            name="Integer",
            description="Enter an integer"
            )
    ScreenStart = IntProperty(
            name="Left Limit",
            default=0,
            min=0,
            max=1024,
            subtype='PIXEL',
            description="Limit the screen active area width from the left side\n"
                        "Changed values will take effect on the next run",
            )
    ScreenEnd = IntProperty(
            name="Right Limit",
            default=0,
            min=0,
            max=1024,
            subtype='PIXEL',
            description="Limit the screen active area width from the right side\n"
                        "Changed values will take effect on the next run",
            )
    # Define the UI drop down prop
    UiTabDrop = BoolVectorProperty(
            name="Tab",
            description="Expand/Collapse UI elements",
            default=(False,) * 6,
            size=6,
            )
    WT_handler_enable = BoolProperty(
            default=False
            )
    WT_handler_previous_object = StringProperty(
            default=""
            )


# Addons Preferences Update Panel
# Define Panels for updating
panels = (
    DisplayToolsPanel,
    )


def update_panel(self, context):
    message = "Display Tools: Updating Panel locations has failed"
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


class DisplayToolsPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    category = StringProperty(
            name="Tab Category",
            description="Choose a name for the category of the panel",
            default="Display",
            update=update_panel
            )

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        col = row.column()
        col.label(text="Tab Category:")
        col.prop(self, "category", text="")


def DRAW_hide_by_type_MENU(self, context):
    self.layout.operator_menu_enum(
        "object.hide_by_type",
        "type", text="Hide By Type"
        )
    self.layout.operator_menu_enum(
        "object.show_by_type",
        "type", text="Show By Type"
        )


# register the classes and props
def register():
    bpy.utils.register_module(__name__)
    bpy.types.VIEW3D_MT_object_showhide.append(DRAW_hide_by_type_MENU)
    # Register Scene Properties
    bpy.types.Scene.display_tools = PointerProperty(
                                        type=display_tools_scene_props
                                        )
    update_panel(None, bpy.context)
    selection_restrictor.register()


def unregister():
    selection_restrictor.unregister()
    bpy.types.VIEW3D_MT_object_showhide.remove(DRAW_hide_by_type_MENU)
    bpy.utils.unregister_module(__name__)
    del bpy.types.Scene.display_tools


if __name__ == "__main__":
    register()
