# SPDX-FileCopyrightText: 2010-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator

from bpy.props import (
    EnumProperty,
    FloatProperty,
    IntProperty,
)
from bpy.app.translations import pgettext_tip as tip_


class SequencerCrossfadeSounds(Operator):
    """Do cross-fading volume animation of two selected sound strips"""

    bl_idname = "sequencer.crossfade_sounds"
    bl_label = "Crossfade Sounds"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        strip = context.active_sequence_strip
        return strip and (strip.type == 'SOUND')

    def execute(self, context):
        scene = context.scene
        seq1 = None
        seq2 = None
        for strip in scene.sequence_editor.sequences:
            if strip.select and strip.type == 'SOUND':
                if seq1 is None:
                    seq1 = strip
                elif seq2 is None:
                    seq2 = strip
                else:
                    seq2 = None
                    break
        if seq2 is None:
            self.report({'ERROR'}, "Select 2 sound strips")
            return {'CANCELLED'}
        if seq1.frame_final_start > seq2.frame_final_start:
            seq1, seq2 = seq2, seq1
        if seq1.frame_final_end > seq2.frame_final_start:
            tempcfra = scene.frame_current
            scene.frame_current = seq2.frame_final_start
            seq1.keyframe_insert("volume")
            scene.frame_current = seq1.frame_final_end
            seq1.volume = 0
            seq1.keyframe_insert("volume")
            seq2.keyframe_insert("volume")
            scene.frame_current = seq2.frame_final_start
            seq2.volume = 0
            seq2.keyframe_insert("volume")
            scene.frame_current = tempcfra
            return {'FINISHED'}
        else:
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
        strip = context.active_sequence_strip
        return strip and (strip.type == 'MULTICAM')

    def execute(self, context):
        scene = context.scene
        camera = self.camera

        strip = context.active_sequence_strip

        if strip.multicam_source == camera or camera >= strip.channel:
            return {'FINISHED'}

        cfra = scene.frame_current
        right_strip = strip.split(frame=cfra, split_method='SOFT')

        if right_strip:
            strip.select = False
            right_strip.select = True
            scene.sequence_editor.active_strip = right_strip

        context.active_sequence_strip.multicam_source = camera
        return {'FINISHED'}


class SequencerDeinterlaceSelectedMovies(Operator):
    """Deinterlace all selected movie sources"""

    bl_idname = "sequencer.deinterlace_selected_movies"
    bl_label = "Deinterlace Movies"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return (scene and scene.sequence_editor)

    def execute(self, context):
        for strip in context.scene.sequence_editor.sequences_all:
            if strip.select and strip.type == 'MOVIE':
                strip.use_deinterlace = True

        return {'FINISHED'}


class SequencerFadesClear(Operator):
    """Removes fade animation from selected sequences"""
    bl_idname = "sequencer.fades_clear"
    bl_label = "Clear Fades"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        strip = context.active_sequence_strip
        return strip is not None

    def execute(self, context):
        scene = context.scene
        animation_data = scene.animation_data
        if animation_data is None:
            return {'CANCELLED'}
        action = animation_data.action
        if action is None:
            return {'CANCELLED'}
        fcurves = action.fcurves
        fcurve_map = {
            curve.data_path: curve
            for curve in fcurves
            if curve.data_path.startswith("sequence_editor.sequences_all")
        }
        for sequence in context.selected_sequences:
            animated_property = "volume" if hasattr(sequence, "volume") else "blend_alpha"
            data_path = sequence.path_from_id() + "." + animated_property
            curve = fcurve_map.get(data_path)
            if curve:
                fcurves.remove(curve)
            setattr(sequence, animated_property, 1.0)
            sequence.invalidate_cache('COMPOSITE')

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
             "Fade from the time cursor to the end of overlapping sequences"),
            ('CURSOR_TO', "To Current Frame",
             "Fade from the start of sequences under the time cursor to the current frame"),
        ),
        name="Fade Type",
        description="Fade in, out, both in and out, to, or from the current frame. Default is both in and out",
        default='IN_OUT',
    )

    @classmethod
    def poll(cls, context):
        # Can't use context.selected_sequences as it can have an impact on performances
        strip = context.active_sequence_strip
        return strip is not None

    def execute(self, context):
        from math import floor

        # We must create a scene action first if there's none
        scene = context.scene
        if not scene.animation_data:
            scene.animation_data_create()
        if not scene.animation_data.action:
            action = bpy.data.actions.new(scene.name + "Action")
            scene.animation_data.action = action

        sequences = context.selected_sequences

        if not sequences:
            self.report({'ERROR'}, "No sequences selected")
            return {'CANCELLED'}

        if self.type in {'CURSOR_TO', 'CURSOR_FROM'}:
            sequences = [
                strip for strip in sequences
                if strip.frame_final_start < scene.frame_current < strip.frame_final_end
            ]
            if not sequences:
                self.report({'ERROR'}, "Current frame not within strip framerange")
                return {'CANCELLED'}

        max_duration = min(sequences, key=lambda strip: strip.frame_final_duration).frame_final_duration
        max_duration = floor(max_duration / 2.0) if self.type == 'IN_OUT' else max_duration

        faded_sequences = []
        for sequence in sequences:
            duration = self.calculate_fade_duration(context, sequence)
            duration = min(duration, max_duration)
            if not self.is_long_enough(sequence, duration):
                continue

            animated_property = "volume" if hasattr(sequence, "volume") else "blend_alpha"
            fade_fcurve = self.fade_find_or_create_fcurve(context, sequence, animated_property)
            fades = self.calculate_fades(sequence, fade_fcurve, animated_property, duration)
            self.fade_animation_clear(fade_fcurve, fades)
            self.fade_animation_create(fade_fcurve, fades)
            faded_sequences.append(sequence)
            sequence.invalidate_cache('COMPOSITE')

        sequence_string = "sequence" if len(faded_sequences) == 1 else "sequences"
        self.report({'INFO'}, tip_("Added fade animation to %d %s") % (len(faded_sequences), sequence_string))
        return {'FINISHED'}

    def calculate_fade_duration(self, context, sequence):
        scene = context.scene
        frame_current = scene.frame_current
        duration = 0.0
        if self.type == 'CURSOR_TO':
            duration = abs(frame_current - sequence.frame_final_start)
        elif self.type == 'CURSOR_FROM':
            duration = abs(sequence.frame_final_end - frame_current)
        else:
            duration = calculate_duration_frames(scene, self.duration_seconds)
        return max(1, duration)

    def is_long_enough(self, sequence, duration=0.0):
        minimum_duration = duration * 2 if self.type == 'IN_OUT' else duration
        return sequence.frame_final_duration >= minimum_duration

    def calculate_fades(self, sequence, fade_fcurve, animated_property, duration):
        """
        Returns a list of Fade objects
        """
        fades = []
        if self.type in {'IN', 'IN_OUT', 'CURSOR_TO'}:
            fade = Fade(sequence, fade_fcurve, 'IN', animated_property, duration)
            fades.append(fade)
        if self.type in {'OUT', 'IN_OUT', 'CURSOR_FROM'}:
            fade = Fade(sequence, fade_fcurve, 'OUT', animated_property, duration)
            fades.append(fade)
        return fades

    def fade_find_or_create_fcurve(self, context, sequence, animated_property):
        """
        Iterates over all the fcurves until it finds an fcurve with a data path
        that corresponds to the sequence.
        Returns the matching FCurve or creates a new one if the function can't find a match.
        """
        scene = context.scene
        fade_fcurve = None
        fcurves = scene.animation_data.action.fcurves
        searched_data_path = sequence.path_from_id(animated_property)
        for fcurve in fcurves:
            if fcurve.data_path == searched_data_path:
                fade_fcurve = fcurve
                break
        if not fade_fcurve:
            fade_fcurve = fcurves.new(data_path=searched_data_path)
        return fade_fcurve

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
                except BaseException:
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
        # The graph editor and the audio waveforms only redraw upon "moving" a keyframe
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

    def __init__(self, sequence, fade_fcurve, type, animated_property, duration):
        from mathutils import Vector
        self.type = type
        self.animated_property = animated_property
        self.duration = duration
        self.max_value = self.calculate_max_value(sequence, fade_fcurve)

        if type == 'IN':
            self.start = Vector((sequence.frame_final_start, 0.0))
            self.end = Vector((sequence.frame_final_start + self.duration, self.max_value))
        elif type == 'OUT':
            self.start = Vector((sequence.frame_final_end - self.duration, self.max_value))
            self.end = Vector((sequence.frame_final_end, 0.0))

    def calculate_max_value(self, sequence, fade_fcurve):
        """
        Returns the maximum Y coordinate the fade animation should use for a given sequence
        Uses either the sequence's value for the animated property, or the next keyframe after the fade
        """
        max_value = 0.0

        if not fade_fcurve.keyframe_points:
            max_value = getattr(sequence, self.animated_property, 1.0)
        else:
            if self.type == 'IN':
                fade_end = sequence.frame_final_start + self.duration
                keyframes = (k for k in fade_fcurve.keyframe_points if k.co[0] >= fade_end)
            if self.type == 'OUT':
                fade_start = sequence.frame_final_end - self.duration
                keyframes = (k for k in reversed(fade_fcurve.keyframe_points) if k.co[0] <= fade_start)
            try:
                max_value = next(keyframes).co[1]
            except StopIteration:
                pass

        return max_value if max_value > 0.0 else 1.0

    def __repr__(self):
        return "Fade %r: %r to %r" % (self.type, self.start, self.end)


def calculate_duration_frames(scene, duration_seconds):
    return round(duration_seconds * scene.render.fps / scene.render.fps_base)


classes = (
    SequencerCrossfadeSounds,
    SequencerSplitMulticam,
    SequencerDeinterlaceSelectedMovies,
    SequencerFadesClear,
    SequencerFadesAdd,
)
