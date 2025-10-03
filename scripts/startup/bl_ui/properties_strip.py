# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Panel,
)
from bpy.app.translations import (
    contexts as i18n_contexts,
    pgettext_rpt as rpt_,
)
from rna_prop_ui import PropertyPanel


class StripButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "strip"

    @classmethod
    def poll(cls, context):
        return context.active_strip is not None


class StripColorTagPicker:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "none"  # Used as popover

    @classmethod
    def poll(cls, context):
        return context.active_strip is not None


class STRIP_PT_color_tag_picker(StripColorTagPicker, Panel):
    bl_label = "Color Tag"
    bl_options = {'HIDE_HEADER'}

    def draw(self, _context):
        layout = self.layout

        row = layout.row(align=True)
        row.operator("sequencer.strip_color_tag_set", icon='X').color = 'NONE'
        for i in range(1, 10):
            icon = 'STRIP_COLOR_{:02d}'.format(i)
            row.operator("sequencer.strip_color_tag_set", icon=icon).color = 'COLOR_{:02d}'.format(i)


class STRIP_PT_strip(StripButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout
        strip = context.active_strip
        strip_type = strip.type

        if strip_type in {
                'ADD', 'SUBTRACT', 'ALPHA_OVER', 'ALPHA_UNDER', 'MULTIPLY',
                'GLOW', 'SPEED', 'MULTICAM', 'GAUSSIAN_BLUR', 'COLORMIX',
        }:
            icon_header = 'SHADERFX'
        elif strip_type in {
                'CROSS', 'GAMMA_CROSS', 'WIPE',
        }:
            icon_header = 'ARROW_LEFTRIGHT'
        elif strip_type == 'SCENE':
            icon_header = 'SCENE_DATA'
        elif strip_type == 'MOVIECLIP':
            icon_header = 'TRACKER'
        elif strip_type == 'MASK':
            icon_header = 'MOD_MASK'
        elif strip_type == 'MOVIE':
            icon_header = 'FILE_MOVIE'
        elif strip_type == 'SOUND':
            icon_header = 'FILE_SOUND'
        elif strip_type == 'IMAGE':
            icon_header = 'FILE_IMAGE'
        elif strip_type == 'COLOR':
            icon_header = 'COLOR'
        elif strip_type == 'TEXT':
            icon_header = 'FONT_DATA'
        elif strip_type == 'ADJUSTMENT':
            icon_header = 'COLOR'
        elif strip_type == 'META':
            icon_header = 'SEQ_STRIP_META'
        else:
            icon_header = 'SEQ_SEQUENCER'

        row = layout.row(align=True)
        row.use_property_decorate = False
        row.label(text="", icon=icon_header)
        row.separator()
        row.prop(strip, "name", text="")

        sub = row.row(align=True)
        if strip.color_tag == 'NONE':
            sub.popover(panel="STRIP_PT_color_tag_picker", text="", icon='COLOR')
        else:
            icon = 'STRIP_' + strip.color_tag
            sub.popover(panel="STRIP_PT_color_tag_picker", text="", icon=icon)

        row.separator()
        row.prop(strip, "mute", toggle=True, icon_only=True, emboss=False)


class STRIP_PT_adjust_crop(StripButtonsPanel, Panel):
    bl_label = "Crop"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type != 'SOUND'

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True
        layout.active = not strip.mute

        col = layout.column(align=True)
        col.prop(strip.crop, "min_x")
        col.prop(strip.crop, "max_x")
        col.prop(strip.crop, "max_y")
        col.prop(strip.crop, "min_y")


class STRIP_PT_effect(StripButtonsPanel, Panel):
    bl_label = "Effect Strip"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type in {
            'ADD',
            'SUBTRACT',
            'ALPHA_OVER',
            'ALPHA_UNDER',
            'CROSS',
            'GAMMA_CROSS',
            'MULTIPLY',
            'WIPE',
            'GLOW',
            'COLOR',
            'SPEED',
            'MULTICAM',
            'GAUSSIAN_BLUR',
            'TEXT',
            'COLORMIX',
        }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_strip

        layout.active = not strip.mute

        if strip.input_count > 0:
            col = layout.column()
            row = col.row()
            row.prop(strip, "input_1")

            if strip.input_count > 1:
                row.operator("sequencer.swap_inputs", text="", icon='SORT_ASC')
                row = col.row()
                row.prop(strip, "input_2")
                row.operator("sequencer.swap_inputs", text="", icon='SORT_DESC')

        strip_type = strip.type

        if strip_type == 'COLOR':
            layout.template_color_picker(strip, "color", value_slider=True, cubic=True)
            layout.prop(strip, "color", text="")

        elif strip_type == 'WIPE':
            col = layout.column()
            col.prop(strip, "transition_type")
            col.alignment = 'RIGHT'
            col.row().prop(strip, "direction", expand=True)

            col = layout.column()
            col.prop(strip, "blur_width", slider=True)
            if strip.transition_type in {'SINGLE', 'DOUBLE'}:
                col.prop(strip, "angle")

        elif strip_type == 'GLOW':
            flow = layout.column_flow()
            flow.prop(strip, "threshold", slider=True)
            flow.prop(strip, "clamp", slider=True)
            flow.prop(strip, "boost_factor")
            flow.prop(strip, "blur_radius")
            flow.prop(strip, "quality", slider=True)
            flow.prop(strip, "use_only_boost")

        elif strip_type == 'SPEED':
            col = layout.column(align=True)
            col.prop(strip, "speed_control", text="Speed Control")
            if strip.speed_control == 'MULTIPLY':
                col.prop(strip, "speed_factor", text=" ")
            elif strip.speed_control == 'LENGTH':
                col.prop(strip, "speed_length", text=" ")
            elif strip.speed_control == 'FRAME_NUMBER':
                col.prop(strip, "speed_frame_number", text=" ")

            row = layout.row(align=True)
            row.enabled = strip.speed_control != 'STRETCH'
            row = layout.row(align=True, heading="Interpolation")
            row.prop(strip, "use_frame_interpolate", text="")

        elif strip_type == 'MULTICAM':
            col = layout.column(align=True)
            strip_channel = strip.channel

            col.prop(strip, "multicam_source", text="Source Channel")

            # The multicam strip needs at least 2 strips to be useful
            if strip_channel > 2:
                BT_ROW = 4
                col.label(text="Cut To")
                row = col.row()

                for i in range(1, strip_channel):
                    if (i % BT_ROW) == 1:
                        row = col.row(align=True)

                    # Workaround - .enabled has to have a separate UI block to work
                    if i == strip.multicam_source:
                        sub = row.row(align=True)
                        sub.enabled = False
                        sub.operator("sequencer.split_multicam", text="{:d}".format(i), translate=False).camera = i
                    else:
                        sub_1 = row.row(align=True)
                        sub_1.enabled = True
                        sub_1.operator("sequencer.split_multicam", text="{:d}".format(i), translate=False).camera = i

                if strip.channel > BT_ROW and (strip_channel - 1) % BT_ROW:
                    for i in range(strip.channel, strip_channel + ((BT_ROW + 1 - strip_channel) % BT_ROW)):
                        row.label(text="")
            else:
                col.separator()
                col.label(text="Two or more channels are needed below this strip", icon='INFO')

        elif strip_type == 'TEXT':
            layout = self.layout
            col = layout.column()
            col.scale_x = 1.3
            col.scale_y = 1.3
            col.use_property_split = False
            col.prop(strip, "text", text="")
            col.use_property_split = True
            layout.prop(strip, "wrap_width", text="Wrap Width")

        col = layout.column(align=True)
        if strip_type in {'CROSS', 'GAMMA_CROSS', 'WIPE', 'ALPHA_OVER', 'ALPHA_UNDER'}:
            col.prop(strip, "use_default_fade", text="Default Fade")
            if not strip.use_default_fade:
                col.prop(strip, "effect_fader", text="Effect Fader")
        elif strip_type == 'GAUSSIAN_BLUR':
            col = layout.column(align=True)
            col.prop(strip, "size_x", text="Size X")
            col.prop(strip, "size_y", text="Y")
        elif strip_type == 'COLORMIX':
            layout.prop(strip, "blend_effect", text="Blend Mode")
            row = layout.row(align=True)
            row.prop(strip, "factor", slider=True)


class STRIP_PT_effect_text_layout(StripButtonsPanel, Panel):
    bl_label = "Layout"
    bl_parent_id = "STRIP_PT_effect"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        return strip.type == 'TEXT'

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True
        col = layout.column()
        col.prop(strip, "location", text="Location")
        col.prop(strip, "alignment_x", text="Alignment X")
        col.prop(strip, "anchor_x", text="Anchor X")
        col.prop(strip, "anchor_y", text="Y")


class STRIP_PT_effect_text_style(StripButtonsPanel, Panel):
    bl_label = "Style"
    bl_parent_id = "STRIP_PT_effect"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        return strip.type == 'TEXT'

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True
        col = layout.column()

        row = col.row(align=True)
        row.use_property_decorate = False
        row.template_ID(strip, "font", open="font.open", unlink="font.unlink")
        row.prop(strip, "use_bold", text="", icon='BOLD')
        row.prop(strip, "use_italic", text="", icon='ITALIC')

        col.prop(strip, "font_size")
        col.prop(strip, "color")


class STRIP_PT_effect_text_outline(StripButtonsPanel, Panel):
    bl_label = "Outline"
    bl_options = {'DEFAULT_CLOSED'}

    bl_parent_id = "STRIP_PT_effect_text_style"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        return strip.type == 'TEXT'

    def draw_header(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.prop(strip, "use_outline", text="")

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True

        col = layout.column()
        col.prop(strip, "outline_color", text="Color")
        col.prop(strip, "outline_width", text="Width")
        col.active = strip.use_outline and (not strip.mute)


class STRIP_PT_effect_text_shadow(StripButtonsPanel, Panel):
    bl_label = "Shadow"
    bl_options = {'DEFAULT_CLOSED'}

    bl_parent_id = "STRIP_PT_effect_text_style"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        return strip.type == 'TEXT'

    def draw_header(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.prop(strip, "use_shadow", text="")

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True

        col = layout.column()
        col.prop(strip, "shadow_color", text="Color")
        col.prop(strip, "shadow_angle", text="Angle")
        col.prop(strip, "shadow_offset", text="Offset")
        col.prop(strip, "shadow_blur", text="Blur")
        col.active = strip.use_shadow and (not strip.mute)


class STRIP_PT_effect_text_box(StripButtonsPanel, Panel):
    bl_label = "Box"
    bl_translation_context = i18n_contexts.id_sequence
    bl_options = {'DEFAULT_CLOSED'}

    bl_parent_id = "STRIP_PT_effect_text_style"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        return strip.type == 'TEXT'

    def draw_header(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.prop(strip, "use_box", text="")

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True

        col = layout.column()
        col.prop(strip, "box_color", text="Color")
        col.prop(strip, "box_margin", text="Margin")
        col.prop(strip, "box_roundness", text="Roundness")
        col.active = strip.use_box and (not strip.mute)


class STRIP_PT_source(StripButtonsPanel, Panel):
    bl_label = "Source"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type in {'MOVIE', 'IMAGE', 'SOUND'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.sequencer_scene
        strip = context.active_strip
        strip_type = strip.type

        layout.active = not strip.mute

        # Draw a filename if we have one.
        if strip_type == 'SOUND':
            sound = strip.sound
            layout.template_ID(strip, "sound", open="sound.open")
            if sound is not None:

                col = layout.column()
                col.prop(sound, "filepath", text="")

                col.alignment = 'RIGHT'
                sub = col.column(align=True)
                split = sub.split(factor=0.5, align=True)
                split.alignment = 'RIGHT'
                if sound.packed_file:
                    split.label(text="Unpack")
                    split.operator("sound.unpack", icon='PACKAGE', text="")
                else:
                    split.label(text="Pack")
                    split.operator("sound.pack", icon='UGLYPACKAGE', text="")

                layout.prop(sound, "use_memory_cache")

                col = layout.box()
                col = col.column(align=True)
                split = col.split(factor=0.5, align=False)
                split.alignment = 'RIGHT'
                split.label(text="Sample Rate")
                split.alignment = 'LEFT'
                if sound.samplerate <= 0:
                    split.label(text="Unknown")
                else:
                    split.label(text="{:d} Hz".format(sound.samplerate), translate=False)

                split = col.split(factor=0.5, align=False)
                split.alignment = 'RIGHT'
                split.label(text="Channels")
                split.alignment = 'LEFT'

                # FIXME(@campbellbarton): this is ugly, we may want to support a way of showing a label from an enum.
                channel_enum_items = sound.bl_rna.properties["channels"].enum_items
                split.label(text=channel_enum_items[channel_enum_items.find(sound.channels)].name)
                del channel_enum_items
        else:
            if strip_type == 'IMAGE':
                col = layout.column()
                col.prop(strip, "directory", text="")

                # Current element for the filename.
                elem = strip.strip_elem_from_frame(scene.frame_current)
                if elem:
                    col.prop(elem, "filename", text="")  # strip.elements[0] could be a fallback

                col.prop(strip.colorspace_settings, "name", text="Color Space")

                col.prop(strip, "alpha_mode", text="Alpha")
                sub = col.column(align=True)
                sub.operator("sequencer.change_path", text="Change Data/Files", icon='FILEBROWSER').filter_image = True
            else:  # elif strip_type == 'MOVIE':
                elem = strip.elements[0]

                col = layout.column()
                col.prop(strip, "filepath", text="")
                col.prop(strip.colorspace_settings, "name", text="Color Space")
                col.prop(strip, "stream_index")
                col.prop(strip, "use_deinterlace")

            if scene.render.use_multiview:
                layout.prop(strip, "use_multiview")

                col = layout.column()
                col.active = strip.use_multiview

                col.row().prop(strip, "views_format", expand=True)

                box = col.box()
                box.active = strip.views_format == 'STEREO_3D'
                box.template_image_stereo_3d(strip.stereo_3d_format)

            # Resolution.
            col = layout.box()
            col = col.column(align=True)
            split = col.split(factor=0.5, align=False)
            split.alignment = 'RIGHT'
            split.label(text="Resolution")
            size = (elem.orig_width, elem.orig_height) if elem else (0, 0)
            if size[0] and size[1]:
                split.alignment = 'LEFT'
                split.label(text="{:d}x{:d}".format(*size), translate=False)
            else:
                split.label(text="None")
            # FPS
            if elem.orig_fps:
                split = col.split(factor=0.5, align=False)
                split.alignment = 'RIGHT'
                split.label(text="FPS")
                split.alignment = 'LEFT'
                split.label(text="{:.2f}".format(elem.orig_fps), translate=False)


class STRIP_PT_movie_clip(StripButtonsPanel, Panel):
    bl_label = "Movie Clip"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type == 'MOVIECLIP'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        strip = context.active_strip

        layout.active = not strip.mute
        layout.template_ID(strip, "clip")

        if strip.type == 'MOVIECLIP':
            col = layout.column(heading="Use")
            col.prop(strip, "stabilize2d", text="2D Stabilized Clip")
            col.prop(strip, "undistort", text="Undistorted Clip")

        clip = strip.clip
        if clip:
            sta = clip.frame_start
            end = clip.frame_start + clip.frame_duration
            layout.label(
                text=rpt_("Original frame range: {:d}-{:d} ({:d})").format(sta, end, end - sta + 1),
                translate=False,
            )


class STRIP_PT_scene(StripButtonsPanel, Panel):
    bl_label = "Scene"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return (strip.type == 'SCENE')

    def draw(self, context):
        strip = context.active_strip
        scene = strip.scene

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.active = not strip.mute

        layout.template_ID(strip, "scene", text="Scene", new="scene.new_sequencer")
        layout.prop(strip, "scene_input", text="Input")

        if strip.scene_input == 'CAMERA':
            layout.template_ID(strip, "scene_camera", text="Camera")

        if strip.scene_input == 'CAMERA':
            layout = layout.column(heading="Show")
            layout.prop(strip, "use_annotations", text="Annotations")
            if scene:
                # Warning, this is not a good convention to follow.
                # Expose here because setting the alpha from the "Render" menu is very inconvenient.
                layout.prop(scene.render, "film_transparent")


class STRIP_PT_scene_sound(StripButtonsPanel, Panel):
    bl_label = "Sound"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return (strip.type == 'SCENE')

    def draw(self, context):
        strip = context.active_strip

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        layout.active = not strip.mute

        col = layout.column()

        col.use_property_decorate = True
        split = col.split(factor=0.4)
        split.alignment = 'RIGHT'
        split.label(text="Strip Volume", text_ctxt=i18n_contexts.id_sound)
        split.prop(strip, "volume", text="")
        col.use_property_decorate = False


class STRIP_PT_mask(StripButtonsPanel, Panel):
    bl_label = "Mask"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return (strip.type == 'MASK')

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_strip

        layout.active = not strip.mute

        layout.template_ID(strip, "mask")

        mask = strip.mask

        if mask:
            sta = mask.frame_start
            end = mask.frame_end
            layout.label(
                text=rpt_("Original frame range: {:d}-{:d} ({:d})").format(sta, end, end - sta + 1),
                translate=False,
            )


class STRIP_PT_time(StripButtonsPanel, Panel):
    bl_label = "Time"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type

    def draw_header_preset(self, context):
        layout = self.layout
        layout.alignment = 'RIGHT'
        strip = context.active_strip

        layout.prop(strip, "lock", text="", icon_only=True, emboss=False)

    def draw(self, context):
        from bpy.utils import smpte_from_frame

        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False

        scene = context.sequencer_scene
        frame_current = scene.frame_current
        strip = context.active_strip

        is_effect = isinstance(strip, bpy.types.EffectStrip)

        # Get once.
        frame_start = strip.frame_start
        frame_final_start = strip.frame_final_start
        frame_final_end = strip.frame_final_end
        frame_final_duration = strip.frame_final_duration
        frame_offset_start = strip.frame_offset_start
        frame_offset_end = strip.frame_offset_end

        length_list = (
            str(round(frame_start, 0)),
            str(round(frame_final_end, 0)),
            str(round(frame_final_duration, 0)),
            str(round(frame_offset_start, 0)),
            str(round(frame_offset_end, 0)),
        )

        if not is_effect:
            length_list = length_list + (
                str(round(strip.animation_offset_start, 0)),
                str(round(strip.animation_offset_end, 0)),
            )

        max_length = max(len(x) for x in length_list)
        max_factor = (1.9 - max_length) / 30
        factor = 0.45

        layout.enabled = not strip.lock
        layout.active = not strip.mute

        sub = layout.row(align=True)
        split = sub.split(factor=factor + max_factor)
        split.alignment = 'RIGHT'
        split.label(text="")
        split.prop(strip, "show_retiming_keys")

        sub = layout.row(align=True)
        split = sub.split(factor=factor + max_factor)
        split.alignment = 'RIGHT'
        split.label(text="Channel")
        split.prop(strip, "channel", text="")

        sub = layout.column(align=True)
        split = sub.split(factor=factor + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="Start")
        split.prop(strip, "frame_start", text=smpte_from_frame(frame_start))

        split = sub.split(factor=factor + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="Duration")
        split.prop(strip, "frame_final_duration", text=smpte_from_frame(frame_final_duration))

        # Use label, editing this value from the UI allows negative values,
        # users can adjust duration.
        split = sub.split(factor=factor + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="End")
        split = split.split(factor=factor + 0.3 + max_factor, align=True)
        split.label(text="{:>14s}".format(smpte_from_frame(frame_final_end)), translate=False)
        split.alignment = 'RIGHT'
        split.label(text=str(frame_final_end) + " ")

        if not is_effect:

            layout.alignment = 'RIGHT'
            sub = layout.column(align=True)

            split = sub.split(factor=factor + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Strip Offset Start")
            split.prop(strip, "frame_offset_start", text=smpte_from_frame(frame_offset_start))

            split = sub.split(factor=factor + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="End")
            split.prop(strip, "frame_offset_end", text=smpte_from_frame(frame_offset_end))

            layout.alignment = 'RIGHT'
            sub = layout.column(align=True)

            split = sub.split(factor=factor + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="Hold Offset Start")
            split.prop(strip, "animation_offset_start", text=smpte_from_frame(strip.animation_offset_start))

            split = sub.split(factor=factor + max_factor, align=True)
            split.alignment = 'RIGHT'
            split.label(text="End")
            split.prop(strip, "animation_offset_end", text=smpte_from_frame(strip.animation_offset_end))

            if strip.type == 'SOUND':
                sub2 = layout.column(align=True)
                split = sub2.split(factor=factor + max_factor, align=True)
                split.alignment = 'RIGHT'
                split.label(text="Sound Offset", text_ctxt=i18n_contexts.id_sound)
                split.prop(strip, "sound_offset", text="")

        col = layout.column(align=True)
        col = col.box()
        col.active = (
            (frame_current >= frame_final_start) and
            (frame_current <= frame_final_start + frame_final_duration)
        )

        split = col.split(factor=factor + max_factor, align=True)
        split.alignment = 'RIGHT'
        split.label(text="Current Frame")
        split = split.split(factor=factor + 0.3 + max_factor, align=True)
        frame_display = frame_current - frame_final_start
        split.label(text="{:>14s}".format(smpte_from_frame(frame_display)), translate=False)
        split.alignment = 'RIGHT'
        split.label(text=str(frame_display) + " ")

        if strip.type == 'SCENE':
            scene = strip.scene

            if scene:
                sta = scene.frame_start
                end = scene.frame_end
                split = col.split(factor=factor + max_factor)
                split.alignment = 'RIGHT'
                split.label(text="Original Frame Range")
                split.alignment = 'LEFT'
                split.label(text="{:d}-{:d} ({:d})".format(sta, end, end - sta + 1), translate=False)


class STRIP_PT_adjust_sound(StripButtonsPanel, Panel):
    bl_label = "Sound"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type == 'SOUND'

    def draw(self, context):
        layout = self.layout

        strip = context.active_strip
        sound = strip.sound

        layout.active = not strip.mute

        if sound is not None:
            layout.use_property_split = True
            col = layout.column()

            split = col.split(factor=0.4)
            split.alignment = 'RIGHT'
            split.label(text="Volume", text_ctxt=i18n_contexts.id_sound)
            split.prop(strip, "volume", text="")

            layout.use_property_split = False
            col = layout.column()

            split = col.split(factor=0.4)
            split.label(text="")
            split.prop(sound, "use_mono")

            layout.use_property_split = True
            col = layout.column()

            audio_channels = context.sequencer_scene.render.ffmpeg.audio_channels
            pan_enabled = sound.use_mono and audio_channels != 'MONO'
            pan_text = "{:.2f}°".format(strip.pan * 90.0)

            split = col.split(factor=0.4)
            split.alignment = 'RIGHT'
            split.label(text="Pan", text_ctxt=i18n_contexts.id_sound)
            split.prop(strip, "pan", text="")
            split.enabled = pan_enabled

            if audio_channels not in {'MONO', 'STEREO'}:
                split = col.split(factor=0.4)
                split.alignment = 'RIGHT'
                split.label(text="Pan Angle")
                split.enabled = pan_enabled
                subsplit = split.row()
                subsplit.alignment = 'CENTER'
                subsplit.label(text=pan_text)
                subsplit.label(text=" ")  # Compensate for no decorate.
                subsplit.enabled = pan_enabled

            layout.use_property_split = False
            col = layout.column()

            split = col.split(factor=0.4)
            split.label(text="")
            split.prop(strip, "pitch_correction")

            split = col.split(factor=0.4)
            split.label(text="")
            split.prop(strip, "show_waveform")


class STRIP_PT_adjust_comp(StripButtonsPanel, Panel):
    bl_label = "Compositing"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type != 'SOUND'

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_strip

        layout.active = not strip.mute

        col = layout.column()
        col.prop(strip, "blend_type", text="Blend")
        col.prop(strip, "blend_alpha", text="Opacity", slider=True)


class STRIP_PT_adjust_transform(StripButtonsPanel, Panel):
    bl_label = "Transform"

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type != 'SOUND'

    def draw(self, context):
        strip = context.active_strip
        layout = self.layout
        layout.use_property_split = True
        layout.active = not strip.mute

        col = layout.column(align=True)
        col.prop(strip.transform, "filter", text="Filter")

        col = layout.column(align=True)
        col.prop(strip.transform, "offset_x", text="Position X")
        col.prop(strip.transform, "offset_y", text="Y")

        col = layout.column(align=True)
        col.prop(strip.transform, "scale_x", text="Scale X")
        col.prop(strip.transform, "scale_y", text="Y")

        col = layout.column(align=True)
        col.prop(strip.transform, "rotation", text="Rotation")

        col = layout.column(align=True)
        col.prop(strip.transform, "origin")

        col = layout.column(heading="Mirror", align=True, heading_ctxt=i18n_contexts.id_image)
        col.prop(strip, "use_flip_x", text="X", toggle=True)
        col.prop(strip, "use_flip_y", text="Y", toggle=True)


class STRIP_PT_adjust_video(StripButtonsPanel, Panel):
    bl_label = "Video"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type in {
            'MOVIE', 'IMAGE', 'SCENE', 'MOVIECLIP', 'MASK',
            'META', 'ADD', 'SUBTRACT', 'ALPHA_OVER',
            'ALPHA_UNDER', 'CROSS', 'GAMMA_CROSS', 'MULTIPLY',
            'WIPE', 'GLOW', 'COLOR', 'MULTICAM', 'SPEED', 'ADJUSTMENT', 'COLORMIX',
        }

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        col = layout.column()

        strip = context.active_strip

        layout.active = not strip.mute

        col.prop(strip, "strobe")
        col.prop(strip, "use_reverse_frames")


class STRIP_PT_adjust_color(StripButtonsPanel, Panel):
    bl_label = "Color"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        strip = context.active_strip
        if not strip:
            return False

        return strip.type in {
            'MOVIE', 'IMAGE', 'SCENE', 'MOVIECLIP', 'MASK',
            'META', 'ADD', 'SUBTRACT', 'ALPHA_OVER',
            'ALPHA_UNDER', 'CROSS', 'GAMMA_CROSS', 'MULTIPLY',
            'WIPE', 'GLOW', 'COLOR', 'MULTICAM', 'SPEED', 'ADJUSTMENT', 'COLORMIX',
        }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        strip = context.active_strip

        layout.active = not strip.mute

        col = layout.column()
        col.prop(strip, "color_saturation", text="Saturation")
        col.prop(strip, "color_multiply", text="Multiply")
        col.prop(strip, "multiply_alpha")
        col.prop(strip, "use_float", text="Convert to Float")


class STRIP_PT_custom_props(StripButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_WORKBENCH',
    }
    _context_path = "active_strip"
    _property_type = (bpy.types.Strip,)


classes = (
    STRIP_PT_color_tag_picker,

    STRIP_PT_strip,
    STRIP_PT_effect,
    STRIP_PT_scene,
    STRIP_PT_scene_sound,
    STRIP_PT_mask,
    STRIP_PT_effect_text_style,
    STRIP_PT_effect_text_outline,
    STRIP_PT_effect_text_shadow,
    STRIP_PT_effect_text_box,
    STRIP_PT_effect_text_layout,
    STRIP_PT_movie_clip,

    STRIP_PT_adjust_comp,
    STRIP_PT_adjust_transform,
    STRIP_PT_adjust_crop,
    STRIP_PT_adjust_video,
    STRIP_PT_adjust_color,
    STRIP_PT_adjust_sound,

    STRIP_PT_time,
    STRIP_PT_source,

    STRIP_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
