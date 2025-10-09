# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    FileHandler,
    Operator,
)

from bpy.props import (
    EnumProperty,
    FloatProperty,
    IntProperty,
)
from bpy.app.translations import pgettext_rpt as rpt_


def _animated_properties_get(strip):
    animated_properties = []
    if hasattr(strip, "volume"):
        animated_properties.append("volume")
    if hasattr(strip, "blend_alpha"):
        animated_properties.append("blend_alpha")
    return animated_properties


class SequencerCrossfadeSounds(Operator):
    """Do cross-fading volume animation of two selected sound strips"""

    bl_idname = "sequencer.crossfade_sounds"
    bl_label = "Crossfade Sounds"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        sequencer_scene = context.sequencer_scene
        if not sequencer_scene:
            return False
        strip = context.active_strip
        return strip and (strip.type == 'SOUND')

    def execute(self, context):
        scene = context.sequencer_scene
        strip1 = None
        strip2 = None
        for strip in scene.sequence_editor.strips_all:
            if strip.select and strip.type == 'SOUND':
                if strip1 is None:
                    strip1 = strip
                elif strip2 is None:
                    strip2 = strip
                else:
                    strip2 = None
                    break
        if strip2 is None:
            self.report({'ERROR'}, "Select 2 sound strips")
            return {'CANCELLED'}
        if strip1.frame_final_start > strip2.frame_final_start:
            strip1, strip2 = strip2, strip1
        if strip1.frame_final_end > strip2.frame_final_start:
            tempcfra = scene.frame_current
            scene.frame_current = strip2.frame_final_start
            strip1.keyframe_insert("volume")
            scene.frame_current = strip1.frame_final_end
            strip1.volume = 0
            strip1.keyframe_insert("volume")
            strip2.keyframe_insert("volume")
            scene.frame_current = strip2.frame_final_start
            strip2.volume = 0
            strip2.keyframe_insert("volume")
            scene.frame_current = tempcfra
            return {'FINISHED'}

        self.report({'ERROR'}, "The selected strips don't overlap")
        return {'CANCELLED'}


class SequencerSplitMulticam(Operator):
    """Split multicam strip and select camera"""

    bl_idname = "sequencer.split_multicam"
    bl_label = "Split Multicam"
    bl_options = {'REGISTER', 'UNDO'}

    camera: IntProperty(
        name="Camera",
        min=1, max=32,
        soft_min=1, soft_max=32,
        default=1,
    )

    @classmethod
    def poll(cls, context):
        sequencer_scene = context.sequencer_scene
        if not sequencer_scene:
            return False
        strip = context.active_strip
        return strip and (strip.type == 'MULTICAM')

    def execute(self, context):
        scene = context.sequencer_scene
        camera = self.camera

        strip = context.active_strip

        if strip.multicam_source == camera or camera >= strip.channel:
            return {'FINISHED'}

        cfra = scene.frame_current
        right_strip = strip.split(frame=cfra, split_method='SOFT')

        if right_strip:
            strip.select = False
            right_strip.select = True
            scene.sequence_editor.active_strip = right_strip

        context.active_strip.multicam_source = camera
        return {'FINISHED'}


class SequencerDeinterlaceSelectedMovies(Operator):
    """Deinterlace all selected movie sources"""

    bl_idname = "sequencer.deinterlace_selected_movies"
    bl_label = "Deinterlace Movies"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        scene = context.sequencer_scene
        return (scene and scene.sequence_editor)

    def execute(self, context):
        scene = context.sequencer_scene
        for strip in scene.sequence_editor.strips_all:
            if strip.select and strip.type == 'MOVIE':
                strip.use_deinterlace = True

        return {'FINISHED'}


class SequencerFadesClear(Operator):
    """Removes fade animation from selected strips"""
    bl_idname = "sequencer.fades_clear"
    bl_label = "Clear Fades"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        sequencer_scene = context.sequencer_scene
        if not sequencer_scene:
            return False
        strip = context.active_strip
        return strip is not None

    def execute(self, context):
        from bpy_extras import anim_utils

        scene = context.sequencer_scene
        animation_data = scene.animation_data
        if animation_data is None:
            return {'CANCELLED'}
        channelbag = anim_utils.action_get_channelbag_for_slot(animation_data.action, animation_data.action_slot)
        if channelbag is None:
            return {'CANCELLED'}
        fcurves = channelbag.fcurves
        fcurve_map = {
            curve.data_path: curve
            for curve in fcurves
            if curve.data_path.startswith("sequence_editor.strips_all")
        }
        for strip in context.selected_strips:
            for animated_property in _animated_properties_get(strip):
                data_path = strip.path_from_id() + "." + animated_property
                curve = fcurve_map.get(data_path)
                if curve:
                    fcurves.remove(curve)
                setattr(strip, animated_property, 1.0)
            strip.invalidate_cache('COMPOSITE')

        return {'FINISHED'}


class SequencerFadesAdd(Operator):
    """Adds or updates a fade animation for either visual or audio strips"""
    bl_idname = "sequencer.fades_add"
    bl_label = "Add Fades"
    bl_options = {'REGISTER', 'UNDO'}

    duration_seconds: FloatProperty(
        name="Fade Duration",
        description="Duration of the fade in seconds",
        default=1.0,
        min=0.01,
    )
    type: EnumProperty(
        items=(
            ('IN_OUT', "Fade In and Out", "Fade selected strips in and out"),
            ('IN', "Fade In", "Fade in selected strips"),
            ('OUT', "Fade Out", "Fade out selected strips"),
            ('CURSOR_FROM', "From Current Frame",
             "Fade from the time cursor to the end of overlapping strips"),
            ('CURSOR_TO', "To Current Frame",
             "Fade from the start of strips under the time cursor to the current frame"),
        ),
        name="Fade Type",
        description="Fade in, out, both in and out, to, or from the current frame. Default is both in and out",
        default='IN_OUT',
    )

    @classmethod
    def poll(cls, context):
        sequencer_scene = context.sequencer_scene
        if not sequencer_scene:
            return False
        # Can't use context.selected_strips as it can have an impact on performances
        strip = context.active_strip
        return strip is not None

    def execute(self, context):
        from math import floor

        # We must create a scene action first if there's none
        scene = context.sequencer_scene
        if not scene.animation_data:
            scene.animation_data_create()
        if not scene.animation_data.action:
            action = bpy.data.actions.new(scene.name + "Action")
            scene.animation_data.action = action

        strips = context.selected_strips

        if not strips:
            self.report({'ERROR'}, "No strips selected")
            return {'CANCELLED'}

        if self.type in {'CURSOR_TO', 'CURSOR_FROM'}:
            strips = [
                strip for strip in strips
                if strip.frame_final_start < scene.frame_current < strip.frame_final_end
            ]
            if not strips:
                self.report({'ERROR'}, "Current frame not within strip framerange")
                return {'CANCELLED'}

        max_duration = min(strips, key=lambda strip: strip.frame_final_duration).frame_final_duration
        max_duration = floor(max_duration / 2.0) if self.type == 'IN_OUT' else max_duration

        faded_strips = []
        for strip in strips:
            duration = self.calculate_fade_duration(context, strip)
            duration = min(duration, max_duration)
            if not self.is_long_enough(strip, duration):
                continue

            for animated_property in _animated_properties_get(strip):
                fade_fcurve = self.fade_find_or_create_fcurve(context, strip, animated_property)
                fades = self.calculate_fades(strip, fade_fcurve, animated_property, duration)
                self.fade_animation_clear(fade_fcurve, fades)
                self.fade_animation_create(fade_fcurve, fades)
            faded_strips.append(strip)
            strip.invalidate_cache('COMPOSITE')

        strip_string = "strip" if len(faded_strips) == 1 else "strips"
        self.report({'INFO'}, rpt_("Added fade animation to {:d} {:s}").format(len(faded_strips), strip_string))
        return {'FINISHED'}

    def calculate_fade_duration(self, context, strip):
        scene = context.sequencer_scene
        frame_current = scene.frame_current
        duration = 0.0
        if self.type == 'CURSOR_TO':
            duration = abs(frame_current - strip.frame_final_start)
        elif self.type == 'CURSOR_FROM':
            duration = abs(strip.frame_final_end - frame_current)
        else:
            duration = calculate_duration_frames(scene, self.duration_seconds)
        return max(1, duration)

    def is_long_enough(self, strip, duration=0.0):
        minimum_duration = duration * 2 if self.type == 'IN_OUT' else duration
        return strip.frame_final_duration >= minimum_duration

    def calculate_fades(self, strip, fade_fcurve, animated_property, duration):
        """
        Returns a list of Fade objects
        """
        fades = []
        if self.type in {'IN', 'IN_OUT', 'CURSOR_TO'}:
            fade = Fade(strip, fade_fcurve, 'IN', animated_property, duration)
            fades.append(fade)
        if self.type in {'OUT', 'IN_OUT', 'CURSOR_FROM'}:
            fade = Fade(strip, fade_fcurve, 'OUT', animated_property, duration)
            fades.append(fade)
        return fades

    def fade_find_or_create_fcurve(self, context, strip, animated_property):
        """
        Iterates over all the fcurves until it finds an fcurve with a data path
        that corresponds to the strip.
        Returns the matching FCurve or creates a new one if the function can't find a match.
        """
        scene = context.sequencer_scene
        action = scene.animation_data.action
        searched_data_path = strip.path_from_id(animated_property)
        return action.fcurve_ensure_for_datablock(scene, searched_data_path)

    def fade_animation_clear(self, fade_fcurve, fades):
        """
        Removes existing keyframes in the fades' time range, in fast mode, without
        updating the fcurve
        """
        keyframe_points = fade_fcurve.keyframe_points
        for fade in fades:
            for keyframe in keyframe_points:
                # The keyframe points list doesn't seem to always update as the
                # operator re-runs Leading to trying to remove nonexistent keyframes
                try:
                    if fade.start.x < keyframe.co[0] <= fade.end.x:
                        keyframe_points.remove(keyframe, fast=True)
                except Exception:
                    pass
            fade_fcurve.update()

    def fade_animation_create(self, fade_fcurve, fades):
        """
        Inserts keyframes in the fade_fcurve in fast mode using the Fade objects.
        Updates the fcurve after having inserted all keyframes to finish the animation.
        """
        keyframe_points = fade_fcurve.keyframe_points
        for fade in fades:
            for point in (fade.start, fade.end):
                keyframe_points.insert(frame=point.x, value=point.y, options={'FAST'})
        fade_fcurve.update()
        # The graph editor and the audio wave-forms only redraw upon "moving" a keyframe.
        keyframe_points[-1].co = keyframe_points[-1].co


class Fade:
    # Data structure to represent fades.
    __slots__ = (
        "type",
        "animated_property",
        "duration",
        "max_value",
        "start",
        "end",
    )

    def __init__(self, strip, fade_fcurve, ty, animated_property, duration):
        from mathutils import Vector
        self.type = ty
        self.animated_property = animated_property
        self.duration = duration
        self.max_value = self.calculate_max_value(strip, fade_fcurve)

        if ty == 'IN':
            self.start = Vector((strip.frame_final_start, 0.0))
            self.end = Vector((strip.frame_final_start + self.duration, self.max_value))
        elif ty == 'OUT':
            self.start = Vector((strip.frame_final_end - self.duration, self.max_value))
            self.end = Vector((strip.frame_final_end, 0.0))

    def calculate_max_value(self, strip, fade_fcurve):
        """
        Returns the maximum Y coordinate the fade animation should use for a given strip
        Uses either the strip's value for the animated property, or the next keyframe after the fade
        """
        max_value = 0.0

        if not fade_fcurve.keyframe_points:
            max_value = getattr(strip, self.animated_property, 1.0)
        else:
            if self.type == 'IN':
                fade_end = strip.frame_final_start + self.duration
                keyframes = (k for k in fade_fcurve.keyframe_points if k.co[0] >= fade_end)
            if self.type == 'OUT':
                fade_start = strip.frame_final_end - self.duration
                keyframes = (k for k in reversed(fade_fcurve.keyframe_points) if k.co[0] <= fade_start)
            try:
                max_value = next(keyframes).co[1]
            except StopIteration:
                pass

        return max_value if max_value > 0.0 else 1.0

    def __repr__(self):
        return "Fade {!r}: {!r} to {!r}".format(self.type, self.start, self.end)


def calculate_duration_frames(scene, duration_seconds):
    return round(duration_seconds * scene.render.fps / scene.render.fps_base)


class SequencerFileHandlerBase:
    @classmethod
    def poll_drop(cls, context):
        return (
            (context.region is not None) and
            (context.region.type == 'WINDOW') and
            (context.area is not None) and
            (context.area.ui_type == 'SEQUENCE_EDITOR')
        )


class SEQUENCER_FH_image_strip(FileHandler, SequencerFileHandlerBase):
    bl_idname = "SEQUENCER_FH_image_strip"
    bl_label = "Image strip"
    bl_import_operator = "SEQUENCER_OT_image_strip_add"
    bl_file_extensions = ";".join(bpy.path.extensions_image)


class SEQUENCER_FH_movie_strip(FileHandler, SequencerFileHandlerBase):
    bl_idname = "SEQUENCER_FH_movie_strip"
    bl_label = "Movie strip"
    bl_import_operator = "SEQUENCER_OT_movie_strip_add"
    bl_file_extensions = ";".join(bpy.path.extensions_movie)


class SEQUENCER_FH_sound_strip(FileHandler, SequencerFileHandlerBase):
    bl_idname = "SEQUENCER_FH_sound_strip"
    bl_label = "Sound strip"
    bl_import_operator = "SEQUENCER_OT_sound_strip_add"
    bl_file_extensions = ";".join(bpy.path.extensions_audio)


classes = (
    SequencerCrossfadeSounds,
    SequencerSplitMulticam,
    SequencerDeinterlaceSelectedMovies,
    SequencerFadesClear,
    SequencerFadesAdd,

    SEQUENCER_FH_image_strip,
    SEQUENCER_FH_movie_strip,
    SEQUENCER_FH_sound_strip,
)
