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
from bpy.types import Header, Menu, Panel
from bl_ui.properties_grease_pencil_common import GreasePencilDataPanel, GreasePencilToolsPanel
from bpy.app.translations import pgettext_iface as iface_

from bl_ui.properties_data_camera import draw_display_safe_settings

def act_strip(context):
    try:
        return context.scene.sequence_editor.active_strip
    except AttributeError:
        return None


def draw_color_balance(layout, color_balance):
    col = layout.column()
    col.label(text="Lift:")
    col.template_color_picker(color_balance, "lift", value_slider=True, cubic=True)
    row = col.row()
    row.prop(color_balance, "lift", text="")
    row.prop(color_balance, "invert_lift", text="Inverse")

    col = layout.column()
    col.label(text="Gamma:")
    col.template_color_picker(color_balance, "gamma", value_slider=True, lock_luminosity=True, cubic=True)
    row = col.row()
    row.prop(color_balance, "gamma", text="")
    row.prop(color_balance, "invert_gamma", text="Inverse")

    col = layout.column()
    col.label(text="Gain:")
    col.template_color_picker(color_balance, "gain", value_slider=True, lock_luminosity=True, cubic=True)
    row = col.row()
    row.prop(color_balance, "gain", text="")
    row.prop(color_balance, "invert_gain", text="Inverse")


class SEQUENCER_HT_header(Header):
    bl_space_type = 'SEQUENCE_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        scene = context.scene

        row = layout.row(align=True)
        row.template_header()

        SEQUENCER_MT_editor_menus.draw_collapsible(context, layout)

        row = layout.row(align=True)
        row.prop(scene, "use_preview_range", text="", toggle=True)
        row.prop(scene, "lock_frame_selection_to_range", text="", toggle=True)

        layout.prop(st, "view_type", expand=True, text="")

        if st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}:
            layout.prop(st, "display_mode", expand=True, text="")

        if st.view_type == 'SEQUENCER':
            row = layout.row(align=True)
            row.operator("sequencer.copy", text="", icon='COPYDOWN')
            row.operator("sequencer.paste", text="", icon='PASTEDOWN')

            layout.separator()
            layout.operator("sequencer.refresh_all")
            layout.prop(st, "show_backdrop")
        else:
            if st.view_type == 'SEQUENCER_PREVIEW':
                layout.separator()
                layout.operator("sequencer.refresh_all")

            layout.prop(st, "preview_channels", expand=True, text="")
            layout.prop(st, "display_channel", text="Channel")

            ed = context.scene.sequence_editor
            if ed:
                row = layout.row(align=True)
                row.prop(ed, "show_overlay", text="", icon='GHOST_ENABLED')
                if ed.show_overlay:
                    row.prop(ed, "overlay_frame", text="")
                    row.prop(ed, "use_overlay_lock", text="", icon='LOCKED')

                    row = layout.row()
                    row.prop(st, "overlay_type", text="")

        if st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}:
            gpd = context.gpencil_data
            toolsettings = context.tool_settings

            # Proportional editing
            if gpd and gpd.use_stroke_edit_mode:
                row = layout.row(align=True)
                row.prop(toolsettings, "proportional_edit", icon_only=True)
                if toolsettings.proportional_edit != 'DISABLED':
                    row.prop(toolsettings, "proportional_edit_falloff", icon_only=True)
					
        row = layout.row(align=True)
        row.operator("render.opengl", text="", icon='RENDER_STILL').sequencer = True
        props = row.operator("render.opengl", text="", icon='RENDER_ANIMATION')
        props.animation = True
        props.sequencer = True

        layout.template_running_jobs()


class SEQUENCER_MT_editor_menus(Menu):
    bl_idname = "SEQUENCER_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        st = context.space_data

        layout.menu("SEQUENCER_MT_view")

        if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            layout.menu("SEQUENCER_MT_select")
            layout.menu("SEQUENCER_MT_marker")
            layout.menu("SEQUENCER_MT_add")
            layout.menu("SEQUENCER_MT_frame")
            layout.menu("SEQUENCER_MT_strip")


class SEQUENCER_MT_view_toggle(Menu):
    bl_label = "View Type"

    def draw(self, context):
        layout = self.layout

        layout.operator("sequencer.view_toggle").type = 'SEQUENCER'
        layout.operator("sequencer.view_toggle").type = 'PREVIEW'
        layout.operator("sequencer.view_toggle").type = 'SEQUENCER_PREVIEW'


class SEQUENCER_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        is_preview = st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}
        is_sequencer_view = st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}

        if st.view_type == 'PREVIEW':
            # Specifying the REGION_PREVIEW context is needed in preview-only
            # mode, else the lookup for the shortcut will fail in
            # wm_keymap_item_find_props() (see #32595).
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
        layout.operator("sequencer.properties", icon='MENU_PANEL')
        layout.operator_context = 'INVOKE_DEFAULT'

        layout.separator()

        if is_sequencer_view:
            layout.operator("sequencer.view_all", text="View all Sequences")
            layout.operator("sequencer.view_selected")
        if is_preview:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
            layout.operator("sequencer.view_all_preview", text="Fit preview in window")

            layout.separator()

            ratios = ((1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1))

            for a, b in ratios:
                layout.operator("sequencer.view_zoom_ratio", text=iface_("Zoom %d:%d") % (a, b), translate=False).ratio = a / b

            layout.separator()

            layout.operator_context = 'INVOKE_DEFAULT'

            # # XXX, invokes in the header view
            # layout.operator("sequencer.view_ghost_border", text="Overlay Border")

        if is_sequencer_view:
            layout.prop(st, "show_seconds")
            layout.prop(st, "show_frame_indicator")
            layout.prop(st, "show_strip_offset")

            layout.prop_menu_enum(st, "waveform_draw_type")

        if is_preview:
            if st.display_mode == 'IMAGE':
                layout.prop(st, "show_safe_areas")
            elif st.display_mode == 'WAVEFORM':
                layout.prop(st, "show_separate_color")

        layout.separator()

        if is_sequencer_view:
            layout.prop(st, "use_marker_sync")
            layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area", text="Toggle Maximize Area")
        layout.operator("screen.screen_full_area").use_hide_panels = True


class SEQUENCER_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("sequencer.select_active_side", text="Strips to the Left").side = 'LEFT'
        layout.operator("sequencer.select_active_side", text="Strips to the Right").side = 'RIGHT'
        op = layout.operator("sequencer.select", text="All strips to the Left")
        op.left_right = 'LEFT'
        op.linked_time = True
        op = layout.operator("sequencer.select", text="All strips to the Right")
        op.left_right = 'RIGHT'
        op.linked_time = True

        layout.separator()
        layout.operator("sequencer.select_handles", text="Surrounding Handles").side = 'BOTH'
        layout.operator("sequencer.select_handles", text="Left Handle").side = 'LEFT'
        layout.operator("sequencer.select_handles", text="Right Handle").side = 'RIGHT'
        layout.separator()
        layout.operator_menu_enum("sequencer.select_grouped", "type", text="Grouped")
        layout.operator("sequencer.select_linked")
        layout.operator("sequencer.select_all").action = 'TOGGLE'
        layout.operator("sequencer.select_all", text="Inverse").action = 'INVERT'


class SEQUENCER_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        from bl_ui.space_time import marker_menu_generic
        marker_menu_generic(layout)


class SEQUENCER_MT_change(Menu):
    bl_label = "Change"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator_menu_enum("sequencer.change_effect_input", "swap")
        layout.operator_menu_enum("sequencer.change_effect_type", "type")
        layout.operator("sequencer.change_path", text="Path/Files")


class SEQUENCER_MT_frame(Menu):
    bl_label = "Frame"

    def draw(self, context):
        layout = self.layout

        layout.operator("anim.previewrange_clear")
        layout.operator("anim.previewrange_set")


class SEQUENCER_MT_add(Menu):
    bl_label = "Add"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        if len(bpy.data.scenes) > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.scene_strip_add", text="Scene...")
        else:
            layout.operator_menu_enum("sequencer.scene_strip_add", "scene", text="Scene...")

        if len(bpy.data.movieclips) > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.movieclip_strip_add", text="Clips...")
        else:
            layout.operator_menu_enum("sequencer.movieclip_strip_add", "clip", text="Clip...")

        if len(bpy.data.masks) > 10:
            layout.operator_context = 'INVOKE_DEFAULT'
            layout.operator("sequencer.mask_strip_add", text="Masks...")
        else:
            layout.operator_menu_enum("sequencer.mask_strip_add", "mask", text="Mask...")

        layout.operator("sequencer.movie_strip_add", text="Movie")
        layout.operator("sequencer.image_strip_add", text="Image")
        layout.operator("sequencer.sound_strip_add", text="Sound")

        layout.menu("SEQUENCER_MT_add_effect")


class SEQUENCER_MT_add_effect(Menu):
    bl_label = "Effect Strip..."

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("sequencer.effect_strip_add", text="Add").type = 'ADD'
        layout.operator("sequencer.effect_strip_add", text="Subtract").type = 'SUBTRACT'
        layout.operator("sequencer.effect_strip_add", text="Alpha Over").type = 'ALPHA_OVER'
        layout.operator("sequencer.effect_strip_add", text="Alpha Under").type = 'ALPHA_UNDER'
        layout.operator("sequencer.effect_strip_add", text="Cross").type = 'CROSS'
        layout.operator("sequencer.effect_strip_add", text="Gamma Cross").type = 'GAMMA_CROSS'
        layout.operator("sequencer.effect_strip_add", text="Gaussian Blur").type = 'GAUSSIAN_BLUR'
        layout.operator("sequencer.effect_strip_add", text="Multiply").type = 'MULTIPLY'
        layout.operator("sequencer.effect_strip_add", text="Over Drop").type = 'OVER_DROP'
        layout.operator("sequencer.effect_strip_add", text="Wipe").type = 'WIPE'
        layout.operator("sequencer.effect_strip_add", text="Glow").type = 'GLOW'
        layout.operator("sequencer.effect_strip_add", text="Transform").type = 'TRANSFORM'
        layout.operator("sequencer.effect_strip_add", text="Color").type = 'COLOR'
        layout.operator("sequencer.effect_strip_add", text="Speed Control").type = 'SPEED'
        layout.operator("sequencer.effect_strip_add", text="Multicam Selector").type = 'MULTICAM'
        layout.operator("sequencer.effect_strip_add", text="Adjustment Layer").type = 'ADJUSTMENT'


class SEQUENCER_MT_strip(Menu):
    bl_label = "Strip"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("transform.transform", text="Grab/Move").mode = 'TRANSLATION'
        layout.operator("transform.transform", text="Grab/Extend from frame").mode = 'TIME_EXTEND'
        layout.operator("sequencer.gap_remove")
        layout.operator("sequencer.gap_insert")

        #  uiItemO(layout, NULL, 0, "sequencer.strip_snap"); // TODO - add this operator
        layout.separator()

        layout.operator("sequencer.cut", text="Cut (hard) at frame").type = 'HARD'
        layout.operator("sequencer.cut", text="Cut (soft) at frame").type = 'SOFT'
        layout.operator("sequencer.slip", text="Slip Strip Contents")
        layout.operator("sequencer.images_separate")
        layout.operator("sequencer.offset_clear")
        layout.operator("sequencer.deinterlace_selected_movies")
        layout.operator("sequencer.rebuild_proxy")
        layout.separator()

        layout.operator("sequencer.duplicate_move")
        layout.operator("sequencer.delete")

        strip = act_strip(context)

        if strip:
            stype = strip.type

            # XXX note strip.type is never equal to 'EFFECT', look at seq_type_items within rna_sequencer.c
            if stype == 'EFFECT':
                pass
                # layout.separator()
                # layout.operator("sequencer.effect_change")
                # layout.operator("sequencer.effect_reassign_inputs")
            elif stype == 'IMAGE':
                layout.separator()
                # layout.operator("sequencer.image_change")
                layout.operator("sequencer.rendersize")
            elif stype == 'SCENE':
                pass
                # layout.separator()
                # layout.operator("sequencer.scene_change", text="Change Scene")
            elif stype == 'MOVIE':
                layout.separator()
                # layout.operator("sequencer.movie_change")
                layout.operator("sequencer.rendersize")
            elif stype == 'SOUND':
                layout.separator()
                layout.operator("sequencer.crossfade_sounds")

        layout.separator()

        layout.operator("sequencer.meta_make")
        layout.operator("sequencer.meta_separate")

        #if (ed && (ed->metastack.first || (ed->act_seq && ed->act_seq->type == SEQ_META))) {
        #	uiItemS(layout);
        #	uiItemO(layout, NULL, 0, "sequencer.meta_toggle");
        #}

        layout.separator()
        layout.operator("sequencer.reload", text="Reload Strips").adjust_length = False
        layout.operator("sequencer.reload", text="Reload Strips and Adjust Length").adjust_length = True
        layout.operator("sequencer.reassign_inputs")
        layout.operator("sequencer.swap_inputs")

        layout.separator()
        layout.operator("sequencer.lock")
        layout.operator("sequencer.unlock")
        layout.operator("sequencer.mute").unselected = False
        layout.operator("sequencer.unmute")

        layout.operator("sequencer.mute", text="Mute Deselected Strips").unselected = True

        layout.operator("sequencer.snap")

        layout.operator_menu_enum("sequencer.swap", "side")

        layout.separator()

        layout.operator("sequencer.swap_data")
        layout.menu("SEQUENCER_MT_change")


class SequencerButtonsPanel():
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(cls, context):
        return cls.has_sequencer(context) and (act_strip(context) is not None)


class SequencerButtonsPanel_Output():
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_preview(context):
        st = context.space_data
        return (st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}) or st.show_backdrop

    @classmethod
    def poll(cls, context):
        return cls.has_preview(context)


class SEQUENCER_PT_edit(SequencerButtonsPanel, Panel):
    bl_label = "Edit Strip"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        frame_current = scene.frame_current
        strip = act_strip(context)

        split = layout.split(percentage=0.3)
        split.label(text="Name:")
        split.prop(strip, "name", text="")

        split = layout.split(percentage=0.3)
        split.label(text="Type:")
        split.prop(strip, "type", text="")

        if strip.type not in {'SOUND'}:
            split = layout.split(percentage=0.3)
            split.label(text="Blend:")
            split.prop(strip, "blend_type", text="")

            row = layout.row(align=True)
            sub = row.row(align=True)
            sub.active = (not strip.mute)
            sub.prop(strip, "blend_alpha", text="Opacity", slider=True)
            row.prop(strip, "mute", toggle=True, icon_only=True)
            row.prop(strip, "lock", toggle=True, icon_only=True)
        else:
            row = layout.row(align=True)
            row.prop(strip, "mute", toggle=True, icon_only=True)
            row.prop(strip, "lock", toggle=True, icon_only=True)

        col = layout.column()
        sub = col.column()
        sub.enabled = not strip.lock
        sub.prop(strip, "channel")
        sub.prop(strip, "frame_start")
        sub.prop(strip, "frame_final_duration")

        col = layout.column(align=True)
        row = col.row(align=True)
        row.label(text=iface_("Final Length: %s") % bpy.utils.smpte_from_frame(strip.frame_final_duration),
                  translate=False)
        row = col.row(align=True)
        row.active = (frame_current >= strip.frame_start and frame_current <= strip.frame_start + strip.frame_duration)
        row.label(text=iface_("Playhead: %d") % (frame_current - strip.frame_start), translate=False)

        col.label(text=iface_("Frame Offset %d:%d") % (strip.frame_offset_start, strip.frame_offset_end),
                  translate=False)
        col.label(text=iface_("Frame Still %d:%d") % (strip.frame_still_start, strip.frame_still_end), translate=False)

        elem = False

        if strip.type == 'IMAGE':
            elem = strip.strip_elem_from_frame(frame_current)
        elif strip.type == 'MOVIE':
            elem = strip.elements[0]

        if elem and elem.orig_width > 0 and elem.orig_height > 0:
            col.label(text=iface_("Original Dimension: %dx%d") % (elem.orig_width, elem.orig_height), translate=False)
        else:
            col.label(text="Original Dimension: None")


class SEQUENCER_PT_effect(SequencerButtonsPanel, Panel):
    bl_label = "Effect Strip"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in {'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
                              'CROSS', 'GAMMA_CROSS', 'MULTIPLY', 'OVER_DROP',
                              'WIPE', 'GLOW', 'TRANSFORM', 'COLOR', 'SPEED',
                              'MULTICAM', 'GAUSSIAN_BLUR'}

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        if strip.input_count > 0:
            col = layout.column()
            col.prop(strip, "input_1")
            if strip.input_count > 1:
                col.prop(strip, "input_2")

        if strip.type == 'COLOR':
            layout.prop(strip, "color")

        elif strip.type == 'WIPE':
            col = layout.column()
            col.prop(strip, "transition_type")
            col.label(text="Direction:")
            col.row().prop(strip, "direction", expand=True)

            col = layout.column()
            col.prop(strip, "blur_width", slider=True)
            if strip.transition_type in {'SINGLE', 'DOUBLE'}:
                col.prop(strip, "angle")

        elif strip.type == 'GLOW':
            flow = layout.column_flow()
            flow.prop(strip, "threshold", slider=True)
            flow.prop(strip, "clamp", slider=True)
            flow.prop(strip, "boost_factor")
            flow.prop(strip, "blur_radius")

            row = layout.row()
            row.prop(strip, "quality", slider=True)
            row.prop(strip, "use_only_boost")

        elif strip.type == 'SPEED':
            layout.prop(strip, "use_default_fade", "Stretch to input strip length")
            if not strip.use_default_fade:
                layout.prop(strip, "use_as_speed")
                if strip.use_as_speed:
                    layout.prop(strip, "speed_factor")
                else:
                    layout.prop(strip, "speed_factor", text="Frame number")
                    layout.prop(strip, "scale_to_length")

        elif strip.type == 'TRANSFORM':
            layout = self.layout
            col = layout.column()

            col.prop(strip, "interpolation")
            col.prop(strip, "translation_unit")
            col = layout.column(align=True)
            col.label(text="Position:")
            col.prop(strip, "translate_start_x", text="X")
            col.prop(strip, "translate_start_y", text="Y")

            layout.separator()

            col = layout.column(align=True)
            col.prop(strip, "use_uniform_scale")
            if strip.use_uniform_scale:
                col = layout.column(align=True)
                col.prop(strip, "scale_start_x", text="Scale")
            else:
                col = layout.column(align=True)
                col.label(text="Scale:")
                col.prop(strip, "scale_start_x", text="X")
                col.prop(strip, "scale_start_y", text="Y")

            layout.separator()

            col = layout.column(align=True)
            col.label(text="Rotation:")
            col.prop(strip, "rotation_start", text="Rotation")

        elif strip.type == 'MULTICAM':
            layout.prop(strip, "multicam_source")

            row = layout.row(align=True)
            sub = row.row(align=True)
            sub.scale_x = 2.0

            sub.operator("screen.animation_play", text="", icon='PAUSE' if context.screen.is_animation_playing else 'PLAY')

            row.label("Cut To")
            for i in range(1, strip.channel):
                row.operator("sequencer.cut_multicam", text="%d" % i).camera = i

        col = layout.column(align=True)
        if strip.type == 'SPEED':
            col.prop(strip, "multiply_speed")
        elif strip.type in {'CROSS', 'GAMMA_CROSS', 'WIPE', 'ALPHA_OVER', 'ALPHA_UNDER', 'OVER_DROP'}:
            col.prop(strip, "use_default_fade", "Default fade")
            if not strip.use_default_fade:
                col.prop(strip, "effect_fader", text="Effect fader")
        elif strip.type == 'GAUSSIAN_BLUR':
            col.prop(strip, "size_x")
            col.prop(strip, "size_y")


class SEQUENCER_PT_input(SequencerButtonsPanel, Panel):
    bl_label = "Strip Input"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE', 'SCENE', 'MOVIECLIP', 'META',
                              'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER',
                              'CROSS', 'GAMMA_CROSS', 'MULTIPLY', 'OVER_DROP',
                              'WIPE', 'GLOW', 'TRANSFORM', 'COLOR',
                              'MULTICAM', 'SPEED', 'ADJUSTMENT'}

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        seq_type = strip.type

        # draw a filename if we have one
        if seq_type == 'IMAGE':
            split = layout.split(percentage=0.2)
            split.label(text="Path:")
            split.prop(strip, "directory", text="")

            # Current element for the filename

            elem = strip.strip_elem_from_frame(context.scene.frame_current)
            if elem:
                split = layout.split(percentage=0.2)
                split.label(text="File:")
                split.prop(elem, "filename", text="")  # strip.elements[0] could be a fallback

            layout.prop(strip.colorspace_settings, "name")
            layout.prop(strip, "alpha_mode")

            layout.operator("sequencer.change_path")

        elif seq_type == 'MOVIE':
            split = layout.split(percentage=0.2)
            split.label(text="Path:")
            split.prop(strip, "filepath", text="")

            layout.prop(strip.colorspace_settings, "name")

            layout.prop(strip, "mpeg_preseek")
            layout.prop(strip, "stream_index")

        layout.prop(strip, "use_translation", text="Image Offset")
        if strip.use_translation:
            col = layout.column(align=True)
            col.prop(strip.transform, "offset_x", text="X")
            col.prop(strip.transform, "offset_y", text="Y")

        layout.prop(strip, "use_crop", text="Image Crop")
        if strip.use_crop:
            col = layout.column(align=True)
            col.prop(strip.crop, "max_y")
            col.prop(strip.crop, "min_x")
            col.prop(strip.crop, "min_y")
            col.prop(strip.crop, "max_x")

        if not isinstance(strip, bpy.types.EffectSequence):
            col = layout.column(align=True)
            col.label(text="Trim Duration (hard):")
            col.prop(strip, "animation_offset_start", text="Start")
            col.prop(strip, "animation_offset_end", text="End")

        col = layout.column(align=True)
        col.label(text="Trim Duration (soft):")
        col.prop(strip, "frame_offset_start", text="Start")
        col.prop(strip, "frame_offset_end", text="End")


class SEQUENCER_PT_sound(SequencerButtonsPanel, Panel):
    bl_label = "Sound"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return (strip.type == 'SOUND')

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        strip = act_strip(context)
        sound = strip.sound

        # TODO: add support to handle SOUND datablock in sequencer soundstrips... For now, hide this useless thing!
        # layout.template_ID(strip, "sound", open="sound.open")

        # layout.separator()
        layout.prop(strip, "filepath", text="")

        if sound is not None:
            row = layout.row()
            if sound.packed_file:
                row.operator("sound.unpack", icon='PACKAGE', text="Unpack")
            else:
                row.operator("sound.pack", icon='UGLYPACKAGE', text="Pack")

            row.prop(sound, "use_memory_cache")

        if st.waveform_draw_type == 'DEFAULT_WAVEFORMS':
            layout.prop(strip, "show_waveform")

        layout.prop(strip, "volume")
        layout.prop(strip, "pitch")
        layout.prop(strip, "pan")

        col = layout.column(align=True)
        col.label(text="Trim Duration (hard):")
        col.prop(strip, "animation_offset_start", text="Start")
        col.prop(strip, "animation_offset_end", text="End")

        col = layout.column(align=True)
        col.label(text="Trim Duration (soft):")
        col.prop(strip, "frame_offset_start", text="Start")
        col.prop(strip, "frame_offset_end", text="End")


class SEQUENCER_PT_scene(SequencerButtonsPanel, Panel):
    bl_label = "Scene"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return (strip.type == 'SCENE')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        layout.template_ID(strip, "scene")

        scene = strip.scene

        layout.label(text="Camera Override")
        layout.template_ID(strip, "scene_camera")

        layout.prop(strip, "use_grease_pencil", text="Show Grease Pencil")

        if scene:
            layout.prop(scene, "audio_volume", text="Audio Volume")

            sta = scene.frame_start
            end = scene.frame_end
            layout.label(text=iface_("Original frame range: %d-%d (%d)") % (sta, end, end - sta + 1), translate=False)


class SEQUENCER_PT_mask(SequencerButtonsPanel, Panel):
    bl_label = "Mask"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return (strip.type == 'MASK')

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        layout.template_ID(strip, "mask")

        mask = strip.mask

        if mask:
            sta = mask.frame_start
            end = mask.frame_end
            layout.label(text=iface_("Original frame range: %d-%d (%d)") % (sta, end, end - sta + 1), translate=False)


class SEQUENCER_PT_filter(SequencerButtonsPanel, Panel):
    bl_label = "Filter"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE', 'SCENE', 'MOVIECLIP', 'MASK',
                              'META', 'ADD', 'SUBTRACT', 'ALPHA_OVER',
                              'ALPHA_UNDER', 'CROSS', 'GAMMA_CROSS', 'MULTIPLY',
                              'OVER_DROP', 'WIPE', 'GLOW', 'TRANSFORM', 'COLOR',
                              'MULTICAM', 'SPEED', 'ADJUSTMENT'}

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        col = layout.column()
        col.label(text="Video:")
        col.prop(strip, "strobe")

        if strip.type == 'MOVIECLIP':
            col = layout.column()
            col.label(text="Tracker:")
            col.prop(strip, "stabilize2d")

            col = layout.column()
            col.label(text="Distortion:")
            col.prop(strip, "undistort")

        split = layout.split(percentage=0.65)

        col = split.column()
        col.prop(strip, "use_reverse_frames", text="Backwards")
        col.prop(strip, "use_deinterlace")

        col = split.column()
        col.label(text="Flip:")
        col.prop(strip, "use_flip_x", text="X")
        col.prop(strip, "use_flip_y", text="Y")

        col = layout.column()
        col.label(text="Colors:")
        col.prop(strip, "color_saturation", text="Saturation")
        col.prop(strip, "color_multiply", text="Multiply")
        col.prop(strip, "use_float")


class SEQUENCER_PT_proxy(SequencerButtonsPanel, Panel):
    bl_label = "Proxy / Timecode"

    @classmethod
    def poll(cls, context):
        if not cls.has_sequencer(context):
            return False

        strip = act_strip(context)
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE', 'SCENE', 'META', 'MULTICAM'}

    def draw_header(self, context):
        strip = act_strip(context)

        self.layout.prop(strip, "use_proxy", text="")

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)

        flow = layout.column_flow()
        flow.prop(strip, "use_proxy_custom_directory")
        flow.prop(strip, "use_proxy_custom_file")
        if strip.proxy:
            if strip.use_proxy_custom_directory and not strip.use_proxy_custom_file:
                flow.prop(strip.proxy, "directory")
            if strip.use_proxy_custom_file:
                flow.prop(strip.proxy, "filepath")

            row = layout.row()
            row.prop(strip.proxy, "build_25")
            row.prop(strip.proxy, "build_50")
            row.prop(strip.proxy, "build_75")
            row.prop(strip.proxy, "build_100")

            col = layout.column()
            col.label(text="Build JPEG quality")
            col.prop(strip.proxy, "quality")

            if strip.type == 'MOVIE':
                col = layout.column()
                col.label(text="Use timecode index:")

                col.prop(strip.proxy, "timecode")


class SEQUENCER_PT_preview(SequencerButtonsPanel_Output, Panel):
    bl_label = "Scene Preview/Render"
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    def draw(self, context):
        layout = self.layout

        render = context.scene.render

        col = layout.column()
        col.prop(render, "use_sequencer_gl_preview", text="Open GL Preview")
        col = layout.column()
        #col.active = render.use_sequencer_gl_preview
        col.prop(render, "sequencer_gl_preview", text="")

        row = col.row()
        row.active = render.sequencer_gl_preview == 'SOLID'
        row.prop(render, "use_sequencer_gl_textured_solid")


class SEQUENCER_PT_view(SequencerButtonsPanel_Output, Panel):
    bl_label = "View Settings"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        col = layout.column()
        if st.display_mode == 'IMAGE':
            col.prop(st, "draw_overexposed")
            col.separator()

        elif st.display_mode == 'WAVEFORM':
            col.prop(st, "show_separate_color")

        col = layout.column()
        col.separator()
        col.prop(st, "proxy_render_size")


class SEQUENCER_PT_view_safe_areas(SequencerButtonsPanel_Output, Panel):
    bl_label = "Safe Areas"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        st = context.space_data
        is_preview = st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}
        return is_preview and (st.display_mode == 'IMAGE')

    def draw_header(self, context):
        st = context.space_data

        self.layout.prop(st, "show_safe_areas", text="")

    def draw(self, context):
        layout = self.layout
        st = context.space_data

        draw_display_safe_settings(layout, st)


class SEQUENCER_PT_modifiers(SequencerButtonsPanel, Panel):
    bl_label = "Modifiers"

    def draw(self, context):
        layout = self.layout

        strip = act_strip(context)
        sequencer = context.scene.sequence_editor

        layout.prop(strip, "use_linear_modifiers")

        layout.operator_menu_enum("sequencer.strip_modifier_add", "type")

        for mod in strip.modifiers:
            box = layout.box()

            row = box.row()
            row.prop(mod, "show_expanded", text="", emboss=False)
            row.prop(mod, "name", text="")

            row.prop(mod, "mute", text="")

            sub = row.row(align=True)
            props = sub.operator("sequencer.strip_modifier_move", text="", icon='TRIA_UP')
            props.name = mod.name
            props.direction = 'UP'
            props = sub.operator("sequencer.strip_modifier_move", text="", icon='TRIA_DOWN')
            props.name = mod.name
            props.direction = 'DOWN'

            row.operator("sequencer.strip_modifier_remove", text="", icon='X', emboss=False).name = mod.name

            if mod.show_expanded:
                row = box.row()
                row.prop(mod, "input_mask_type", expand=True)

                if mod.input_mask_type == 'STRIP':
                    sequences_object = sequencer
                    if sequencer.meta_stack:
                        sequences_object = sequencer.meta_stack[-1]
                    box.prop_search(mod, "input_mask_strip", sequences_object, "sequences", text="Mask")
                else:
                    box.prop(mod, "input_mask_id")

                if mod.type == 'COLOR_BALANCE':
                    box.prop(mod, "color_multiply")
                    draw_color_balance(box, mod.color_balance)
                elif mod.type == 'CURVES':
                    box.template_curve_mapping(mod, "curve_mapping", type='COLOR')
                elif mod.type == 'HUE_CORRECT':
                    box.template_curve_mapping(mod, "curve_mapping", type='HUE')
                elif mod.type == 'BRIGHT_CONTRAST':
                    col = box.column()
                    col.prop(mod, "bright")
                    col.prop(mod, "contrast")


class SEQUENCER_PT_grease_pencil(GreasePencilDataPanel, SequencerButtonsPanel_Output, Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    # NOTE: this is just a wrapper around the generic GP Panel
    # But, it should only show up when there are images in the preview region


class SEQUENCER_PT_grease_pencil_tools(GreasePencilToolsPanel, SequencerButtonsPanel_Output, Panel):
    bl_space_type = 'SEQUENCE_EDITOR'
    bl_region_type = 'UI'

    # NOTE: this is just a wrapper around the generic GP tools panel
	# It contains access to some essential tools usually found only in
	# toolbar, which doesn't exist here...


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
