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


def act_strip(context):
    try:
        return context.scene.sequence_editor.active_strip
    except AttributeError:
        return None


class SEQUENCER_HT_header(Header):
    bl_space_type = 'SEQUENCE_EDITOR'

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        row = layout.row(align=True)
        row.template_header()

        if context.area.show_menus:
            row.menu("SEQUENCER_MT_view")

            if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
                row.menu("SEQUENCER_MT_select")
                row.menu("SEQUENCER_MT_marker")
                row.menu("SEQUENCER_MT_add")
                row.menu("SEQUENCER_MT_strip")

        layout.prop(st, "view_type", expand=True, text="")

        if st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}:
            layout.prop(st, "display_mode", expand=True, text="")

        if st.view_type == 'SEQUENCER':
            row = layout.row(align=True)
            row.operator("sequencer.copy", text="", icon='COPYDOWN')
            row.operator("sequencer.paste", text="", icon='PASTEDOWN')

            layout.separator()
            layout.operator("sequencer.refresh_all")
        elif st.view_type == 'SEQUENCER_PREVIEW':
            layout.separator()
            layout.operator("sequencer.refresh_all")
            layout.prop(st, "display_channel", text="Channel")
        else:
            layout.prop(st, "display_channel", text="Channel")

            ed = context.scene.sequence_editor
            if ed:
                row = layout.row(align=True)
                row.prop(ed, "show_overlay", text="", icon='GHOST_ENABLED')
                if ed.show_overlay:
                    row.prop(ed, "overlay_frame", text="")
                    row.prop(ed, "overlay_lock", text="", icon='LOCKED')

                row = layout.row(align=True)
                props = row.operator("render.opengl", text="", icon='RENDER_STILL')
                props.sequencer = True
                props = row.operator("render.opengl", text="", icon='RENDER_ANIMATION')
                props.animation = True
                props.sequencer = True

        layout.template_running_jobs()


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

        layout.operator("sequencer.properties", icon='MENU_PANEL')

        layout.separator()

        if st.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            layout.operator("sequencer.view_all", text="View all Sequences")
        if st.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'}:
            layout.operator_context = 'INVOKE_REGION_PREVIEW'
            layout.operator("sequencer.view_all_preview", text="Fit preview in window")
            layout.operator("sequencer.view_zoom_ratio", text="Show preview 1:1").ratio = 1.0
            layout.operator_context = 'INVOKE_DEFAULT'

            # # XXX, invokes in the header view
            # layout.operator("sequencer.view_ghost_border", text='Overlay Border')

        layout.operator("sequencer.view_selected")

        layout.prop(st, "show_seconds")

        layout.prop(st, "show_frame_indicator")
        if st.display_mode == 'IMAGE':
            layout.prop(st, "show_safe_margin")
        if st.display_mode == 'WAVEFORM':
            layout.prop(st, "show_separate_color")

        layout.separator()
        layout.prop(st, "use_marker_sync")
        layout.separator()

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")


class SEQUENCER_MT_select(Menu):
    bl_label = "Select"

    def draw(self, context):
        layout = self.layout

        layout.operator("sequencer.select_active_side", text="Strips to the Left").side = 'LEFT'
        layout.operator("sequencer.select_active_side", text="Strips to the Right").side = 'RIGHT'
        layout.separator()
        layout.operator("sequencer.select_handles", text="Surrounding Handles").side = 'BOTH'
        layout.operator("sequencer.select_handles", text="Left Handle").side = 'LEFT'
        layout.operator("sequencer.select_handles", text="Right Handle").side = 'RIGHT'
        layout.separator()
        layout.operator_menu_enum("sequencer.select_grouped", "type", text="Grouped")
        layout.operator("sequencer.select_linked")
        layout.operator("sequencer.select_all").action = 'TOGGLE'
        layout.operator("sequencer.select_all", text="Invert Selection").action = 'INVERT'


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
        #  uiItemO(layout, NULL, 0, "sequencer.strip_snap"); // TODO - add this operator
        layout.separator()

        layout.operator("sequencer.cut", text="Cut (hard) at frame").type = 'HARD'
        layout.operator("sequencer.cut", text="Cut (soft) at frame").type = 'SOFT'
        layout.operator("sequencer.images_separate")
        layout.operator("sequencer.offset_clear")
        layout.operator("sequencer.deinterlace_selected_movies")
        layout.operator("sequencer.rebuild_proxy")
        layout.separator()

        layout.operator("sequencer.duplicate")
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
        props = layout.operator("sequencer.reload", text="Reload Strips")
        props.adjust_length = False
        props = layout.operator("sequencer.reload", text="Reload Strips and Adjust Length")
        props.adjust_length = True
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
        return (context.space_data.view_type in {'PREVIEW', 'SEQUENCER_PREVIEW'})

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

        split = layout.split(percentage=0.3)
        split.label(text="Blend:")
        split.prop(strip, "blend_type", text="")

        row = layout.row(align=True)
        sub = row.row()
        sub.active = (not strip.mute)
        sub.prop(strip, "blend_alpha", text="Opacity", slider=True)
        row.prop(strip, "mute", toggle=True, icon='RESTRICT_VIEW_ON' if strip.mute else 'RESTRICT_VIEW_OFF', text="")
        row.prop(strip, "lock", toggle=True, icon='LOCKED' if strip.lock else 'UNLOCKED', text="")

        col = layout.column()
        sub = col.column()
        sub.enabled = not strip.lock
        sub.prop(strip, "channel")
        sub.prop(strip, "frame_start")
        sub.prop(strip, "frame_final_duration")

        col = layout.column(align=True)
        row = col.row()
        row.label(text="Final Length" + ": %s" % bpy.utils.smpte_from_frame(strip.frame_final_duration))
        row = col.row()
        row.active = (frame_current >= strip.frame_start and frame_current <= strip.frame_start + strip.frame_duration)
        row.label(text="Playhead" + ": %d" % (frame_current - strip.frame_start))

        col.label(text="Frame Offset" + " %d:%d" % (strip.frame_offset_start, strip.frame_offset_end))
        col.label(text="Frame Still" + " %d:%d" % (strip.frame_still_start, strip.frame_still_end))

        elem = False

        if strip.type == 'IMAGE':
            elem = strip.getStripElem(frame_current)
        elif strip.type == 'MOVIE':
            elem = strip.elements[0]

        if elem and elem.orig_width > 0 and elem.orig_height > 0:
            col.label(text="Original Dimension" + ": %dx%d" % (elem.orig_width, elem.orig_height))
        else:
            col.label(text="Orig Dim: None")


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
                              'MULTICAM', 'ADJUSTMENT'}

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

            #doesn't work currently
            #layout.prop(strip, "use_frame_blend")

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
            if (strip.use_uniform_scale):
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
            sub = row.row()
            sub.scale_x = 2.0

            sub.operator("screen.animation_play", text="", icon='PAUSE' if context.screen.is_animation_playing else 'PLAY')

            row.label("Cut To")
            for i in range(1, strip.channel):
                row.operator("sequencer.cut_multicam", text=str(i)).camera = i

        col = layout.column(align=True)
        if strip.type == 'SPEED':
            col.prop(strip, "multiply_speed")
        elif strip.type in {'CROSS', 'GAMMA_CROSS', 'WIPE'}:
            col.prop(strip, "use_default_fade", "Default fade")
            if not strip.use_default_fade:
                col.prop(strip, "effect_fader", text="Effect fader")


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

            elem = strip.getStripElem(context.scene.frame_current)
            if elem:
                split = layout.split(percentage=0.2)
                split.label(text="File:")
                split.prop(elem, "filename", text="")  # strip.elements[0] could be a fallback

            layout.operator("sequencer.change_path")

        elif seq_type == 'MOVIE':
            split = layout.split(percentage=0.2)
            split.label(text="Path:")
            split.prop(strip, "filepath", text="")
            
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

        strip = act_strip(context)
        sound = strip.sound

        layout.template_ID(strip, "sound", open="sound.open")

        layout.separator()
        layout.prop(strip, "filepath", text="")

        row = layout.row()
        if sound.packed_file:
            row.operator("sound.unpack", icon='PACKAGE', text="Unpack")
        else:
            row.operator("sound.pack", icon='UGLYPACKAGE', text="Pack")

        row.prop(sound, "use_memory_cache")

        layout.prop(strip, "show_waveform")
        layout.prop(strip, "volume")
        layout.prop(strip, "pitch")
        layout.prop(strip, "pan")

        col = layout.column(align=True)
        col.label(text="Trim Duration:")
        col.prop(strip, "animation_offset_start", text="Start")
        col.prop(strip, "animation_offset_end", text="End")


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
        if scene:
            layout.prop(scene.render, "use_sequencer")

        layout.label(text="Camera Override")
        layout.template_ID(strip, "scene_camera")

        if scene:
            sta = scene.frame_start
            end = scene.frame_end
            layout.label(text="Original frame range" + ": %d-%d (%d)" % (sta, end, end - sta + 1))


class SEQUENCER_PT_filter(SequencerButtonsPanel, Panel):
    bl_label = "Filter"

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
        col.prop(strip, "use_premultiply")
        col.prop(strip, "use_float")

        layout.prop(strip, "use_color_balance")
        if strip.use_color_balance and strip.color_balance:  # TODO - need to add this somehow
            row = layout.row()
            row.active = strip.use_color_balance
            col = row.column()
            col.template_color_wheel(strip.color_balance, "lift", value_slider=False, cubic=True)
            col.row().prop(strip.color_balance, "lift")
            col.prop(strip.color_balance, "invert_lift", text="Inverse")
            col = row.column()
            col.template_color_wheel(strip.color_balance, "gamma", value_slider=False, lock_luminosity=True, cubic=True)
            col.row().prop(strip.color_balance, "gamma")
            col.prop(strip.color_balance, "invert_gamma", text="Inverse")
            col = row.column()
            col.template_color_wheel(strip.color_balance, "gain", value_slider=False, lock_luminosity=True, cubic=True)
            col.row().prop(strip.color_balance, "gain")
            col.prop(strip.color_balance, "invert_gain", text="Inverse")


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

            if strip.type == "MOVIE":
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
        col.active = False  # Currently only opengl preview works!
        col.prop(render, "use_sequencer_gl_preview", text="Open GL Preview")
        col = layout.column()
        #col.active = render.use_sequencer_gl_preview
        col.prop(render, "sequencer_gl_preview", text="")


class SEQUENCER_PT_view(SequencerButtonsPanel_Output, Panel):
    bl_label = "View Settings"

    def draw(self, context):
        layout = self.layout

        st = context.space_data

        col = layout.column()
        if st.display_mode == 'IMAGE':
            col.prop(st, "draw_overexposed")
            col.prop(st, "show_safe_margin")
        elif st.display_mode == 'WAVEFORM':
            col.prop(st, "show_separate_color")
        col.prop(st, "proxy_render_size")

if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
