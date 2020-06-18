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
from bpy.types import Operator

from bpy.props import (
    EnumProperty,
    FloatProperty,
    IntProperty,
)


class SequencerCrossfadeSounds(Operator):
    """Do cross-fading volume animation of two selected sound strips"""

    bl_idname = "sequencer.crossfade_sounds"
    bl_label = "Crossfade sounds"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        if context.scene and context.scene.sequence_editor and context.scene.sequence_editor.active_strip:
            return context.scene.sequence_editor.active_strip.type == 'SOUND'
        else:
            return False

    def execute(self, context):
        seq1 = None
        seq2 = None
        for s in context.scene.sequence_editor.sequences:
            if s.select and s.type == 'SOUND':
                if seq1 is None:
                    seq1 = s
                elif seq2 is None:
                    seq2 = s
                else:
                    seq2 = None
                    break
        if seq2 is None:
            self.report({'ERROR'}, "Select 2 sound strips")
            return {'CANCELLED'}
        if seq1.frame_final_start > seq2.frame_final_start:
            s = seq1
            seq1 = seq2
            seq2 = s
        if seq1.frame_final_end > seq2.frame_final_start:
            tempcfra = context.scene.frame_current
            context.scene.frame_current = seq2.frame_final_start
            seq1.keyframe_insert("volume")
            context.scene.frame_current = seq1.frame_final_end
            seq1.volume = 0
            seq1.keyframe_insert("volume")
            seq2.keyframe_insert("volume")
            context.scene.frame_current = seq2.frame_final_start
            seq2.volume = 0
            seq2.keyframe_insert("volume")
            context.scene.frame_current = tempcfra
            return {'FINISHED'}
        else:
            self.report({'ERROR'}, "The selected strips don't overlap")
            return {'CANCELLED'}


class SequencerSplitMulticam(Operator):
    """Split multi-cam strip and select camera"""

    bl_idname = "sequencer.split_multicam"
    bl_label = "Split multicam"
    bl_options = {'REGISTER', 'UNDO'}

    camera: IntProperty(
        name="Camera",
        min=1, max=32,
        soft_min=1, soft_max=32,
        default=1,
    )

    @classmethod
    def poll(cls, context):
        if context.scene and context.scene.sequence_editor and context.scene.sequence_editor.active_strip:
            return context.scene.sequence_editor.active_strip.type == 'MULTICAM'
        else:
            return False

    def execute(self, context):
        camera = self.camera

        s = context.scene.sequence_editor.active_strip

        if s.multicam_source == camera or camera >= s.channel:
            return {'FINISHED'}

        if not s.select:
            s.select = True

        cfra = context.scene.frame_current
        bpy.ops.sequencer.split(frame=cfra, type='SOFT', side='RIGHT')
        for s in context.scene.sequence_editor.sequences_all:
            if s.select and s.type == 'MULTICAM' and s.frame_final_start <= cfra and cfra < s.frame_final_end:
                context.scene.sequence_editor.active_strip = s

        context.scene.sequence_editor.active_strip.multicam_source = camera
        return {'FINISHED'}


class SequencerDeinterlaceSelectedMovies(Operator):
    """Deinterlace all selected movie sources"""

    bl_idname = "sequencer.deinterlace_selected_movies"
    bl_label = "Deinterlace Movies"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.scene and context.scene.sequence_editor)

    def execute(self, context):
        for s in context.scene.sequence_editor.sequences_all:
            if s.select and s.type == 'MOVIE':
                s.use_deinterlace = True

        return {'FINISHED'}


class SequencerFadesClear(Operator):
    """Removes fade animation from selected sequences"""
    bl_idname = "sequencer.fades_clear"
    bl_label = "Clear Fades"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.scene and context.scene.sequence_editor and context.scene.sequence_editor.active_strip

    def execute(self, context):
        fcurves = context.scene.animation_data.action.fcurves
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
            sequence.invalidate('COMPOSITE')

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
        min=0.01)
    type: EnumProperty(
        items=(
            ('IN_OUT', 'Fade In And Out', 'Fade selected strips in and out'),
            ('IN', 'Fade In', 'Fade in selected strips'),
            ('OUT', 'Fade Out', 'Fade out selected strips'),
            ('CURSOR_FROM', 'From Playhead', 'Fade from the time cursor to the end of overlapping sequences'),
            ('CURSOR_TO', 'To Playhead', 'Fade from the start of sequences under the time cursor to the current frame'),
        ),
        name="Fade type",
        description="Fade in, out, both in and out, to, or from the current frame. Default is both in and out",
        default='IN_OUT')

    @classmethod
    def poll(cls, context):
        # Can't use context.selected_sequences as it can have an impact on performances
        return context.scene and context.scene.sequence_editor and context.scene.sequence_editor.active_strip

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
        if self.type in {'CURSOR_TO', 'CURSOR_FROM'}:
            sequences = [
                s for s in sequences
                if s.frame_final_start < context.scene.frame_current < s.frame_final_end
            ]

        max_duration = min(sequences, key=lambda s: s.frame_final_duration).frame_final_duration
        max_duration = floor(max_duration / 2.0) if self.type == 'IN_OUT' else max_duration

        faded_sequences = []
        for sequence in sequences:
            duration = self.calculate_fade_duration(context, sequence)
            duration = min(duration, max_duration)
            if not self.is_long_enough(sequence, duration):
                continue

            animated_property = 'volume' if hasattr(sequence, 'volume') else 'blend_alpha'
            fade_fcurve = self.fade_find_or_create_fcurve(context, sequence, animated_property)
            fades = self.calculate_fades(sequence, fade_fcurve, animated_property, duration)
            self.fade_animation_clear(fade_fcurve, fades)
            self.fade_animation_create(fade_fcurve, fades)
            faded_sequences.append(sequence)
            sequence.invalidate('COMPOSITE')

        sequence_string = "sequence" if len(faded_sequences) == 1 else "sequences"
        self.report({'INFO'}, "Added fade animation to {} {}.".format(len(faded_sequences), sequence_string))
        return {'FINISHED'}

    def calculate_fade_duration(self, context, sequence):
        frame_current = context.scene.frame_current
        duration = 0.0
        if self.type == 'CURSOR_TO':
            duration = abs(frame_current - sequence.frame_final_start)
        elif self.type == 'CURSOR_FROM':
            duration = abs(sequence.frame_final_end - frame_current)
        else:
            duration = calculate_duration_frames(context, self.duration_seconds)
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
        fade_fcurve = None
        fcurves = context.scene.animation_data.action.fcurves
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
        return "Fade {}: {} to {}".format(self.type, self.start, self.end)


def calculate_duration_frames(context, duration_seconds):
    return round(duration_seconds * context.scene.render.fps / context.scene.render.fps_base)


classes = (
    SequencerCrossfadeSounds,
    SequencerSplitMulticam,
    SequencerDeinterlaceSelectedMovies,
    SequencerFadesClear,
    SequencerFadesAdd,
)
