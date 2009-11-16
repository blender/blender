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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy


class USERPREF_HT_header(bpy.types.Header):
    bl_space_type = 'USER_PREFERENCES'

    def draw(self, context):
        layout = self.layout
        layout.template_header(menus=False)

        userpref = context.user_preferences

        layout.operator_context = 'EXEC_AREA'
        layout.itemO("wm.save_homefile", text="Save As Default")

        if userpref.active_section == 'INPUT':
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.itemO("wm.keyconfig_export", "Export Key Configuration...")


class USERPREF_MT_view(bpy.types.Menu):
    bl_label = "View"

    def draw(self, context):
        pass # layout = self.layout


class USERPREF_PT_tabs(bpy.types.Panel):
    bl_label = ""
    bl_space_type = 'USER_PREFERENCES'
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences

        layout.itemR(userpref, "active_section", expand=True)


class USERPREF_PT_interface(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Interface"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INTERFACE')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        view = userpref.view

        split = layout.split()

        col = split.column()
        sub = col.split(percentage=0.85)

        sub1 = sub.column()
        sub1.itemL(text="Display:")
        sub1.itemR(view, "tooltips")
        sub1.itemR(view, "display_object_info", text="Object Info")
        sub1.itemR(view, "use_large_cursors")
        sub1.itemR(view, "show_view_name", text="View Name")
        sub1.itemR(view, "show_playback_fps", text="Playback FPS")
        sub1.itemR(view, "global_scene")
        sub1.itemR(view, "pin_floating_panels")
        sub1.itemR(view, "object_center_size")
        sub1.itemS()
        sub1.itemS()
        sub1.itemS()
        sub1.itemR(view, "show_mini_axis", text="Display Mini Axis")
        sub2 = sub1.column()
        sub2.enabled = view.show_mini_axis
        sub2.itemR(view, "mini_axis_size", text="Size")
        sub2.itemR(view, "mini_axis_brightness", text="Brightness")

        col = split.column()
        sub = col.split(percentage=0.85)

        sub1 = sub.column()
        sub1.itemL(text="View Manipulation:")
        sub1.itemR(view, "auto_depth")
        sub1.itemR(view, "global_pivot")
        sub1.itemR(view, "zoom_to_mouse")
        sub1.itemR(view, "rotate_around_selection")
        sub1.itemS()


        sub1.itemR(view, "auto_perspective")
        sub1.itemR(view, "smooth_view")
        sub1.itemR(view, "rotation_angle")

        col = split.column()
        sub = col.split(percentage=0.85)
        sub1 = sub.column()

#Toolbox doesn't exist yet
#		sub1.itemL(text="Toolbox:")
#		sub1.itemR(view, "use_column_layout")
#		sub1.itemL(text="Open Toolbox Delay:")
#		sub1.itemR(view, "open_left_mouse_delay", text="Hold LMB")
#		sub1.itemR(view, "open_right_mouse_delay", text="Hold RMB")

        #manipulator
        sub1.itemR(view, "use_manipulator")
        sub2 = sub1.column()
        sub2.enabled = view.use_manipulator
        sub2.itemR(view, "manipulator_size", text="Size")
        sub2.itemR(view, "manipulator_handle_size", text="Handle Size")
        sub2.itemR(view, "manipulator_hotspot", text="Hotspot")

        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        sub1.itemL(text="Menus:")
        sub1.itemR(view, "open_mouse_over")
        sub1.itemL(text="Menu Open Delay:")
        sub1.itemR(view, "open_toplevel_delay", text="Top Level")
        sub1.itemR(view, "open_sublevel_delay", text="Sub Level")


class USERPREF_PT_edit(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Edit"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'EDITING')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        edit = userpref.edit

        split = layout.split()

        col = split.column()
        sub = col.split(percentage=0.85)

        sub1 = sub.column()
        sub1.itemL(text="Link Materials To:")
        sub1.row().itemR(edit, "material_link", expand=True)
        sub1.itemS()
        sub1.itemS()
        sub1.itemS()
        sub1.itemL(text="New Objects:")
        sub1.itemR(edit, "enter_edit_mode")
        sub1.itemL(text="Align To:")
        sub1.row().itemR(edit, "object_align", expand=True)
        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        sub1.itemL(text="Undo:")
        sub1.itemR(edit, "global_undo")
        sub1.itemR(edit, "undo_steps", text="Steps")
        sub1.itemR(edit, "undo_memory_limit", text="Memory Limit")

        col = split.column()
        sub = col.split(percentage=0.85)

        sub1 = sub.column()
        sub1.itemL(text="Snap:")
        sub1.itemR(edit, "snap_translate", text="Translate")
        sub1.itemR(edit, "snap_rotate", text="Rotate")
        sub1.itemR(edit, "snap_scale", text="Scale")
        sub1.itemS()
        sub1.itemS()
        sub1.itemS()
        sub1.itemL(text="Grease Pencil:")
        sub1.itemR(edit, "grease_pencil_manhattan_distance", text="Manhattan Distance")
        sub1.itemR(edit, "grease_pencil_euclidean_distance", text="Euclidean Distance")
        # sub1.itemR(edit, "grease_pencil_simplify_stroke", text="Simplify Stroke")
        sub1.itemR(edit, "grease_pencil_eraser_radius", text="Eraser Radius")
        sub1.itemR(edit, "grease_pencil_smooth_stroke", text="Smooth Stroke")

        col = split.column()
        sub = col.split(percentage=0.85)

        sub1 = sub.column()
        sub1.itemL(text="Keyframing:")
        sub1.itemR(edit, "use_visual_keying")
        sub1.itemR(edit, "keyframe_insert_needed", text="Only Insert Needed")
        sub1.itemS()
        sub1.itemL(text="New F-Curve Defaults:")
        sub1.itemR(edit, "new_interpolation_type", text="Interpolation")
        sub1.itemS()
        sub1.itemR(edit, "auto_keying_enable", text="Auto Keyframing:")
        sub2 = sub1.column()
        sub2.active = edit.auto_keying_enable
        sub2.itemR(edit, "auto_keyframe_insert_keyingset", text="Only Insert for Keying Set")
        sub2.itemR(edit, "auto_keyframe_insert_available", text="Only Insert Available")

        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        sub1.itemL(text="Transform:")
        sub1.itemR(edit, "drag_immediately")

        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        col = split.column()
        sub = col.split(percentage=0.85)

        sub1 = sub.column()
        sub1.itemL(text="Duplicate Data:")
        sub1.itemR(edit, "duplicate_mesh", text="Mesh")
        sub1.itemR(edit, "duplicate_surface", text="Surface")
        sub1.itemR(edit, "duplicate_curve", text="Curve")
        sub1.itemR(edit, "duplicate_text", text="Text")
        sub1.itemR(edit, "duplicate_metaball", text="Metaball")
        sub1.itemR(edit, "duplicate_armature", text="Armature")
        sub1.itemR(edit, "duplicate_lamp", text="Lamp")
        sub1.itemR(edit, "duplicate_material", text="Material")
        sub1.itemR(edit, "duplicate_texture", text="Texture")
        sub1.itemR(edit, "duplicate_ipo", text="F-Curve")
        sub1.itemR(edit, "duplicate_action", text="Action")
        sub1.itemR(edit, "duplicate_particle", text="Particle")


class USERPREF_PT_system(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "System"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'SYSTEM')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        system = userpref.system

        split = layout.split()

        col = split.column()
        sub = col.split(percentage=0.9)

        sub1 = sub.column()
        sub1.itemL(text="General:")
        sub1.itemR(system, "dpi")
        sub1.itemR(system, "frame_server_port")
        sub1.itemR(system, "scrollback", text="Console Scrollback")
        sub1.itemR(system, "emulate_numpad")
        sub1.itemR(system, "auto_run_python_scripts")

        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        sub1.itemL(text="Sound:")
        sub1.row().itemR(system, "audio_device", expand=True)
        sub2 = sub1.column()
        sub2.active = system.audio_device != 'NONE'
        sub2.itemR(system, "enable_all_codecs")
        sub2.itemR(system, "game_sound")
        sub2.itemR(system, "audio_channels", text="Channels")
        sub2.itemR(system, "audio_mixing_buffer", text="Mixing Buffer")
        sub2.itemR(system, "audio_sample_rate", text="Sample Rate")
        sub2.itemR(system, "audio_sample_format", text="Sample Format")

        col = split.column()
        sub = col.split(percentage=0.9)

        sub1 = sub .column()
        sub1.itemL(text="Weight Colors:")
        sub1.itemR(system, "use_weight_color_range", text="Use Custom Range")
        sub2 = sub1.column()
        sub2.active = system.use_weight_color_range
        sub2.template_color_ramp(system, "weight_color_range", expand=True)

        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        sub1.itemR(system, "language")
        sub1.itemL(text="Translate:")
        sub1.itemR(system, "translate_tooltips", text="Tooltips")
        sub1.itemR(system, "translate_buttons", text="Labels")
        sub1.itemR(system, "translate_toolbox", text="Toolbox")

        sub1.itemS()

        sub1.itemR(system, "use_textured_fonts")

        col = split.column()
        sub = col.split(percentage=0.9)

        sub1 = sub.column()

        sub1.itemL(text="OpenGL:")
        sub1.itemR(system, "clip_alpha", slider=True)
        sub1.itemR(system, "use_mipmaps")
        sub1.itemR(system, "use_vbos")
        sub1.itemL(text="Window Draw Method:")
        sub1.row().itemR(system, "window_draw_method", expand=True)
        sub1.itemL(text="Textures:")
        sub1.itemR(system, "gl_texture_limit", text="Limit Size")
        sub1.itemR(system, "texture_time_out", text="Time Out")
        sub1.itemR(system, "texture_collection_rate", text="Collection Rate")

        sub1.itemS()
        sub1.itemS()
        sub1.itemS()

        sub1.itemL(text="Sequencer:")
        sub1.itemR(system, "prefetch_frames")
        sub1.itemR(system, "memory_cache_limit")


class USERPREF_PT_theme(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Themes"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences

        return (userpref.active_section == 'THEMES')

    def draw(self, context):
        layout = self.layout

        theme = context.user_preferences.themes[0]


        split = layout.split(percentage=0.33)
        split.itemR(theme, "active_theme", text="")

        layout.itemS()

        split = layout.split()

        if theme.active_theme == 'VIEW_3D':
            v3d = theme.view_3d

            col = split.column()
            col.itemR(v3d, "back")
            col.itemR(v3d, "button")
            col.itemR(v3d, "button_title")
            col.itemR(v3d, "button_text")
            col.itemR(v3d, "header")

            col = split.column()
            col.itemR(v3d, "grid")
            col.itemR(v3d, "wire")
            col.itemR(v3d, "lamp", slider=True)
            col.itemR(v3d, "editmesh_active", slider=True)

            col = split.column()

            col.itemR(v3d, "object_selected")
            col.itemR(v3d, "object_active")
            col.itemR(v3d, "object_grouped")
            col.itemR(v3d, "object_grouped_active")
            col.itemR(v3d, "transform")

            col = split.column()
            col.itemR(v3d, "vertex")
            col.itemR(v3d, "face", slider=True)
            col.itemR(v3d, "normal")
            col.itemR(v3d, "bone_solid")
            col.itemR(v3d, "bone_pose")

#           col.itemR(v3d, "edge") Doesn't seem to work
        elif theme.active_theme == 'USER_INTERFACE':

            ui = theme.user_interface.wcol_regular

            layout.itemL(text="Regular:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")
            layout.itemS()

            ui = theme.user_interface.wcol_tool
            layout.itemL(text="Tool:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_radio
            layout.itemL(text="Radio Buttons:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_text
            layout.itemL(text="Text:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_option
            layout.itemL(text="Option:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_toggle
            layout.itemL(text="Toggle:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_num
            layout.itemL(text="Number Field:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_numslider
            layout.itemL(text="Value Slider:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_box
            layout.itemL(text="Box:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_menu
            layout.itemL(text="Menu:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_pulldown
            layout.itemL(text="Pulldown:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_menu_back
            layout.itemL(text="Menu Back:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_menu_item
            layout.itemL(text="Menu Item:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_scroll
            layout.itemL(text="Scroll Bar:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_list_item
            layout.itemL(text="List Item:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "outline")
            sub1.itemR(ui, "item", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "inner", slider=True)
            sub1.itemR(ui, "inner_sel", slider=True)
            sub1 = sub.column()
            sub1.itemR(ui, "text")
            sub1.itemR(ui, "text_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "shaded")
            sub2 = sub1.column(align=True)
            sub2.active = ui.shaded
            sub2.itemR(ui, "shadetop")
            sub2.itemR(ui, "shadedown")

            ui = theme.user_interface.wcol_state
            layout.itemL(text="State:")

            sub = layout.row()
            sub1 = sub.column()
            sub1.itemR(ui, "inner_anim")
            sub1.itemR(ui, "inner_anim_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "inner_driven")
            sub1.itemR(ui, "inner_driven_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "inner_key")
            sub1.itemR(ui, "inner_key_sel")
            sub1 = sub.column()
            sub1.itemR(ui, "blend")

            ui = theme.user_interface
            layout.itemS()

            sub = layout.row()
            sub.itemR(ui, "icon_file")

            layout.itemS()
            layout.itemS()


        elif theme.active_theme == 'GRAPH_EDITOR':
            graph = theme.graph_editor

            col = split.column()
            col.itemR(graph, "back")
            col.itemR(graph, "button")
            col.itemR(graph, "button_title")
            col.itemR(graph, "button_text")

            col = split.column()
            col.itemR(graph, "header")
            col.itemR(graph, "grid")
            col.itemR(graph, "list")
            col.itemR(graph, "channel_group")

            col = split.column()
            col.itemR(graph, "active_channels_group")
            col.itemR(graph, "dopesheet_channel")
            col.itemR(graph, "dopesheet_subchannel")
            col.itemR(graph, "vertex")

            col = split.column()
            col.itemR(graph, "current_frame")
            col.itemR(graph, "handle_vertex")
            col.itemR(graph, "handle_vertex_select")
            col.itemS()
            col.itemR(graph, "handle_vertex_size")

        elif theme.active_theme == 'FILE_BROWSER':
            file_browse = theme.file_browser

            col = split.column()
            col.itemR(file_browse, "back")
            col.itemR(file_browse, "text")
            col.itemR(file_browse, "text_hi")

            col = split.column()
            col.itemR(file_browse, "header")
            col.itemR(file_browse, "list")

            col = split.column()
            col.itemR(file_browse, "selected_file")
            col.itemR(file_browse, "tiles")

            col = split.column()
            col.itemR(file_browse, "active_file")
            col.itemR(file_browse, "active_file_text")

        elif theme.active_theme == 'NLA_EDITOR':
            nla = theme.nla_editor

            col = split.column()
            col.itemR(nla, "back")
            col.itemR(nla, "button")
            col.itemR(nla, "button_title")

            col = split.column()
            col.itemR(nla, "button_text")
            col.itemR(nla, "text")
            col.itemR(nla, "header")

            col = split.column()
            col.itemR(nla, "grid")
            col.itemR(nla, "bars")
            col.itemR(nla, "bars_selected")

            col = split.column()
            col.itemR(nla, "strips")
            col.itemR(nla, "strips_selected")
            col.itemR(nla, "current_frame")

        elif theme.active_theme == 'DOPESHEET_EDITOR':
            dope = theme.dopesheet_editor

            col = split.column()
            col.itemR(dope, "back")
            col.itemR(dope, "list")
            col.itemR(dope, "text")
            col.itemR(dope, "header")

            col = split.column()
            col.itemR(dope, "grid")
            col.itemR(dope, "channels")
            col.itemR(dope, "channels_selected")
            col.itemR(dope, "channel_group")

            col = split.column()
            col.itemR(dope, "active_channels_group")
            col.itemR(dope, "long_key")
            col.itemR(dope, "long_key_selected")

            col = split.column()
            col.itemR(dope, "current_frame")
            col.itemR(dope, "dopesheet_channel")
            col.itemR(dope, "dopesheet_subchannel")

        elif theme.active_theme == 'IMAGE_EDITOR':
            image = theme.image_editor

            col = split.column()
            col.itemR(image, "back")
            col.itemR(image, "button")

            col = split.column()
            col.itemR(image, "button_title")
            col.itemR(image, "button_text")

            col = split.column()
            col.itemR(image, "header")

            col = split.column()
            col.itemR(image, "editmesh_active", slider=True)

        elif theme.active_theme == 'SEQUENCE_EDITOR':
            seq = theme.sequence_editor

            col = split.column()
            col.itemR(seq, "back")
            col.itemR(seq, "button")
            col.itemR(seq, "button_title")
            col.itemR(seq, "button_text")
            col.itemR(seq, "text")

            col = split.column()
            col.itemR(seq, "header")
            col.itemR(seq, "grid")
            col.itemR(seq, "movie_strip")
            col.itemR(seq, "image_strip")
            col.itemR(seq, "scene_strip")

            col = split.column()
            col.itemR(seq, "audio_strip")
            col.itemR(seq, "effect_strip")
            col.itemR(seq, "plugin_strip")
            col.itemR(seq, "transition_strip")

            col = split.column()
            col.itemR(seq, "meta_strip")
            col.itemR(seq, "current_frame")
            col.itemR(seq, "keyframe")
            col.itemR(seq, "draw_action")

        elif theme.active_theme == 'PROPERTIES':
            prop = theme.properties

            col = split.column()
            col.itemR(prop, "back")

            col = split.column()
            col.itemR(prop, "title")

            col = split.column()
            col.itemR(prop, "text")

            col = split.column()
            col.itemR(prop, "header")

        elif theme.active_theme == 'TEXT_EDITOR':
            text = theme.text_editor

            col = split.column()
            col.itemR(text, "back")
            col.itemR(text, "button")
            col.itemR(text, "button_title")
            col.itemR(text, "button_text")

            col = split.column()
            col.itemR(text, "text")
            col.itemR(text, "text_hi")
            col.itemR(text, "header")
            col.itemR(text, "line_numbers_background")

            col = split.column()
            col.itemR(text, "selected_text")
            col.itemR(text, "cursor")
            col.itemR(text, "syntax_builtin")
            col.itemR(text, "syntax_special")

            col = split.column()
            col.itemR(text, "syntax_comment")
            col.itemR(text, "syntax_string")
            col.itemR(text, "syntax_numbers")

        elif theme.active_theme == 'TIMELINE':
            time = theme.timeline

            col = split.column()
            col.itemR(time, "back")
            col.itemR(time, "text")

            col = split.column()
            col.itemR(time, "header")

            col = split.column()
            col.itemR(time, "grid")

            col = split.column()
            col.itemR(time, "current_frame")

        elif theme.active_theme == 'NODE_EDITOR':
            node = theme.node_editor

            col = split.column()
            col.itemR(node, "back")
            col.itemR(node, "button")
            col.itemR(node, "button_title")
            col.itemR(node, "button_text")


            col = split.column()
            col.itemR(node, "text")
            col.itemR(node, "text_hi")
            col.itemR(node, "header")
            col.itemR(node, "wires")

            col = split.column()
            col.itemR(node, "wire_select")
            col.itemR(node, "selected_text")
            col.itemR(node, "node_backdrop", slider=True)
            col.itemR(node, "in_out_node")

            col = split.column()
            col.itemR(node, "converter_node")
            col.itemR(node, "operator_node")
            col.itemR(node, "group_node")

        elif theme.active_theme == 'LOGIC_EDITOR':
            logic = theme.logic_editor

            col = split.column()
            col.itemR(logic, "back")
            col.itemR(logic, "button")

            col = split.column()
            col.itemR(logic, "button_title")
            col.itemR(logic, "button_text")

            col = split.column()
            col.itemR(logic, "text")
            col.itemR(logic, "header")

            col = split.column()
            col.itemR(logic, "panel")

        elif theme.active_theme == 'OUTLINER':
            out = theme.outliner

            col = split.column()
            col.itemR(out, "back")

            col = split.column()
            col.itemR(out, "text")

            col = split.column()
            col.itemR(out, "text_hi")

            col = split.column()
            col.itemR(out, "header")

        elif theme.active_theme == 'INFO':
            info = theme.info

            col = split.column()
            col.itemR(info, "back")

            col = split.column()
            col.itemR(info, "header")

            col = split.column()
            col.itemR(info, "header_text")

            col = split.column()

        elif theme.active_theme == 'USER_PREFERENCES':
            prefs = theme.user_preferences

            col = split.column()
            col.itemR(prefs, "back")

            col = split.column()
            col.itemR(prefs, "text")

            col = split.column()
            col.itemR(prefs, "header")

            col = split.column()
            col.itemR(prefs, "header_text")


class USERPREF_PT_file(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Files"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'FILES')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        paths = userpref.filepaths

        split = layout.split(percentage=0.6)

        col = split.column()
        col.itemL(text="File Paths:")
        sub = col.split(percentage=0.3)

        sub.itemL(text="Fonts:")
        sub.itemR(paths, "fonts_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Textures:")
        sub.itemR(paths, "textures_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Texture Plugins:")
        sub.itemR(paths, "texture_plugin_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Sequence Plugins:")
        sub.itemR(paths, "sequence_plugin_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Render Output:")
        sub.itemR(paths, "render_output_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Scripts:")
        sub.itemR(paths, "python_scripts_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Sounds:")
        sub.itemR(paths, "sounds_directory", text="")
        sub = col.split(percentage=0.3)
        sub.itemL(text="Temp:")
        sub.itemR(paths, "temporary_directory", text="")

        col = split.column()
        sub = col.split(percentage=0.2)
        sub.column() # sub1, unused
        sub2 = sub.column()
        sub2.itemL(text="Save & Load:")
        sub2.itemR(paths, "use_relative_paths")
        sub2.itemR(paths, "compress_file")
        sub2.itemR(paths, "load_ui")
        sub2.itemR(paths, "filter_file_extensions")
        sub2.itemR(paths, "hide_dot_files_datablocks")
        sub2.itemS()
        sub2.itemS()
        sub2.itemL(text="Auto Save:")
        sub2.itemR(paths, "save_version")
        sub2.itemR(paths, "recent_files")
        sub2.itemR(paths, "save_preview_images")
        sub2.itemR(paths, "auto_save_temporary_files")
        sub3 = sub2.column()
        sub3.enabled = paths.auto_save_temporary_files
        sub3.itemR(paths, "auto_save_time", text="Timer (mins)")


class USERPREF_PT_input(bpy.types.Panel):
    bl_space_type = 'USER_PREFERENCES'
    bl_label = "Input"
    bl_region_type = 'WINDOW'
    bl_show_header = False

    def poll(self, context):
        userpref = context.user_preferences
        return (userpref.active_section == 'INPUT')

    def draw(self, context):
        layout = self.layout

        userpref = context.user_preferences
        wm = context.manager
        #input = userpref.input
        #input = userpref
        inputs = userpref.inputs

        split = layout.split(percentage=0.25)

        # General settings
        row = split.row()
        col = row.column()

        sub = col.column()
        sub.itemL(text="Configuration:")
        sub.item_pointerR(wm, "active_keyconfig", wm, "keyconfigs", text="")

        col.itemS()

        sub = col.column()
        sub.itemL(text="Mouse:")
        sub1 = sub.column()
        sub1.enabled = (inputs.select_mouse == 'RIGHT')
        sub1.itemR(inputs, "emulate_3_button_mouse")
        sub.itemR(inputs, "continuous_mouse")

        sub.itemL(text="Select With:")
        sub.row().itemR(inputs, "select_mouse", expand=True)
        sub.itemL(text="Middle Mouse:")
        sub.row().itemR(inputs, "middle_mouse", expand=True)

        sub.itemS()
        sub.itemS()
        sub.itemS()

        sub.itemL(text="Orbit Style:")
        sub.row().itemR(inputs, "view_rotation", expand=True)

        sub.itemL(text="Zoom Style:")
        sub.row().itemR(inputs, "viewport_zoom_style", expand=True)

        #sub.itemR(inputs, "use_middle_mouse_paste")

        #col.itemS()

        #sub = col.column()
        #sub.itemL(text="Mouse Wheel:")
        #sub.itemR(view, "wheel_invert_zoom", text="Invert Zoom")
        #sub.itemR(view, "wheel_scroll_lines", text="Scroll Lines")

        col.itemS()

        sub = col.column()
        sub.itemL(text="NDOF Device:")
        sub.itemR(inputs, "ndof_pan_speed", text="Pan Speed")
        sub.itemR(inputs, "ndof_rotate_speed", text="Orbit Speed")

        row.itemS()

        # Keymap Settings
        col = split.column()

        # kc = wm.active_keyconfig
        defkc = wm.default_keyconfig
        km = wm.active_keymap

        subsplit = col.split()
        subsplit.item_pointerR(wm, "active_keymap", defkc, "keymaps", text="Map:")
        if km.user_defined:
            row = subsplit.row()
            row.itemO("WM_OT_keymap_restore", text="Restore")
            row.item_booleanO("WM_OT_keymap_restore", "all", True, text="Restore All")
        else:
            row = subsplit.row()
            row.itemO("WM_OT_keymap_edit", text="Edit")
            row.itemL()

        col.itemS()

        for kmi in km.items:
            subcol = col.column()
            subcol.set_context_pointer("keyitem", kmi)

            row = subcol.row()

            if kmi.expanded:
                row.itemR(kmi, "expanded", text="", icon='ICON_TRIA_DOWN')
            else:
                row.itemR(kmi, "expanded", text="", icon='ICON_TRIA_RIGHT')

            itemrow = row.row()
            itemrow.enabled = km.user_defined
            if kmi.active:
                itemrow.itemR(kmi, "active", text="", icon='ICON_CHECKBOX_HLT')
            else:
                itemrow.itemR(kmi, "active", text="", icon='ICON_CHECKBOX_DEHLT')

            itemcol = itemrow.column()
            itemcol.active = kmi.active
            row = itemcol.row()
            
            if km.modal:
                row.itemR(kmi, "propvalue", text="")
            else:
                row.itemR(kmi, "idname", text="")

            sub = row.row()
            sub.scale_x = 0.6
            sub.itemR(kmi, "map_type", text="")

            sub = row.row(align=True)
            if kmi.map_type == 'KEYBOARD':
                sub.itemR(kmi, "type", text="", full_event=True)
            elif kmi.map_type == 'MOUSE':
                sub.itemR(kmi, "type", text="", full_event=True)
            elif kmi.map_type == 'TWEAK':
                sub.scale_x = 0.5
                sub.itemR(kmi, "type", text="")
                sub.itemR(kmi, "value", text="")
            elif kmi.map_type == 'TIMER':
                sub.itemR(kmi, "type", text="")
            else:
                sub.itemL()

            if kmi.expanded:
                if kmi.map_type not in ('TEXTINPUT', 'TIMER'):
                    sub = itemcol.row(align=True)

                    if kmi.map_type == 'KEYBOARD':
                        sub.itemR(kmi, "type", text="", event=True)
                        sub.itemR(kmi, "value", text="")
                    elif kmi.map_type == 'MOUSE':
                        sub.itemR(kmi, "type", text="")
                        sub.itemR(kmi, "value", text="")
                    else:
                        sub.itemL()
                        sub.itemL()

                    subrow = sub.row()
                    subrow.scale_x = 0.75
                    subrow.itemR(kmi, "any")
                    subrow.itemR(kmi, "shift")
                    subrow.itemR(kmi, "ctrl")
                    subrow.itemR(kmi, "alt")
                    subrow.itemR(kmi, "oskey", text="Cmd")
                    sub.itemR(kmi, "key_modifier", text="", event=True)

                flow = itemcol.column_flow(columns=2)
                props = kmi.properties

                if props != None:
                    for pname in dir(props):
                        if not props.is_property_hidden(pname):
                            flow.itemR(props, pname)

                itemcol.itemS()

            itemrow.itemO("wm.keyitem_remove", text="", icon='ICON_ZOOMOUT')

        itemrow = col.row()
        itemrow.itemL()
        itemrow.itemO("wm.keyitem_add", text="", icon='ICON_ZOOMIN')
        itemrow.enabled = km.user_defined

bpy.types.register(USERPREF_HT_header)
bpy.types.register(USERPREF_MT_view)
bpy.types.register(USERPREF_PT_tabs)
bpy.types.register(USERPREF_PT_interface)
bpy.types.register(USERPREF_PT_theme)
bpy.types.register(USERPREF_PT_edit)
bpy.types.register(USERPREF_PT_system)
bpy.types.register(USERPREF_PT_file)
bpy.types.register(USERPREF_PT_input)

from bpy.props import *


class WM_OT_keyconfig_export(bpy.types.Operator):
    "Export key configuration to a python script."
    bl_idname = "wm.keyconfig_export"
    bl_label = "Export Key Configuration..."

    path = bpy.props.StringProperty(name="File Path", description="File path to write file to.")

    def _string_value(self, value):
        result = ""
        if isinstance(value, str):
            if value != "":
                result = "\'%s\'" % value
        elif isinstance(value, bool):
            if value:
                result = "True"
            else:
                result = "False"
        elif isinstance(value, float):
            result = "%.10f" % value
        elif isinstance(value, int):
            result = "%d" % value
        elif getattr(value, '__len__', False):
            if len(value):
                result = "["
                for i in range(0, len(value)):
                    result += self._string_value(value[i])
                    if i != len(value)-1:
                        result += ", "
                result += "]"
        else:
            print("Export key configuration: can't write ", value)

        return result

    def execute(self, context):
        if not self.path:
            raise Exception("File path not set.")

        f = open(self.path, "w")
        if not f:
            raise Exception("Could not open file.")

        wm = context.manager
        kc = wm.active_keyconfig

        f.write('# Configuration %s\n' % kc.name)

        f.write("wm = bpy.data.windowmanagers[0]\n")
        f.write("kc = wm.add_keyconfig(\'%s\')\n\n" % kc.name)

        for km in kc.keymaps:
            f.write("# Map %s\n" % km.name)
            f.write("km = kc.add_keymap(\'%s\', space_type=\'%s\', region_type=\'%s\')\n\n" % (km.name, km.space_type, km.region_type))
            for kmi in km.items:
                f.write("kmi = km.add_item(\'%s\', \'%s\', \'%s\'" % (kmi.idname, kmi.type, kmi.value))
                if kmi.shift:
                    f.write(", shift=True")
                if kmi.ctrl:
                    f.write(", ctrl=True")
                if kmi.alt:
                    f.write(", alt=True")
                if kmi.oskey:
                    f.write(", oskey=True")
                if kmi.key_modifier and kmi.key_modifier != 'NONE':
                    f.write(", key_modifier=\'%s\'" % kmi.key_modifier)
                f.write(")\n")

                props = kmi.properties

                if props != None:
                    for pname in dir(props):
                        if props.is_property_set(pname) and not props.is_property_hidden(pname):
                            value = eval("props.%s" % pname)
                            value = self._string_value(value)
                            if value != "":
                                f.write("kmi.properties.%s = %s\n" % (pname, value))

            f.write("\n")

        f.close()

        return ('FINISHED',)

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return ('RUNNING_MODAL',)


class WM_OT_keymap_edit(bpy.types.Operator):
    "Edit key map."
    bl_idname = "wm.keymap_edit"
    bl_label = "Edit Key Map"

    def execute(self, context):
        wm = context.manager
        km = wm.active_keymap
        km.copy_to_user()
        return ('FINISHED',)


class WM_OT_keymap_restore(bpy.types.Operator):
    "Restore key map"
    bl_idname = "wm.keymap_restore"
    bl_label = "Restore Key Map"

    all = BoolProperty(attr="all", name="All Keymaps", description="Restore all keymaps to default.")

    def execute(self, context):
        wm = context.manager

        if self.all:
            for km in wm.default_keyconfig.keymaps:
                km.restore_to_default()
        else:
            km = wm.active_keymap
            km.restore_to_default()

        return ('FINISHED',)


class WM_OT_keyitem_add(bpy.types.Operator):
    "Add key map item."
    bl_idname = "wm.keyitem_add"
    bl_label = "Add Key Map Item"

    def execute(self, context):
        wm = context.manager
        km = wm.active_keymap
        km.add_item("", 'A', 'PRESS') # kmi
        return ('FINISHED',)


class WM_OT_keyitem_remove(bpy.types.Operator):
    "Remove key map item."
    bl_idname = "wm.keyitem_remove"
    bl_label = "Remove Key Map Item"

    def execute(self, context):
        wm = context.manager
        kmi = context.keyitem
        km = wm.active_keymap
        km.remove_item(kmi)
        return ('FINISHED',)

bpy.ops.add(WM_OT_keyconfig_export)
bpy.ops.add(WM_OT_keymap_edit)
bpy.ops.add(WM_OT_keymap_restore)
bpy.ops.add(WM_OT_keyitem_add)
bpy.ops.add(WM_OT_keyitem_remove)
