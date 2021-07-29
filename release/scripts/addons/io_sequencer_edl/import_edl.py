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
from . import parse_edl


def id_animdata_action_ensure(id_data):
    id_data.animation_data_create()
    animation_data = id_data.animation_data
    if animation_data.action is None:
        animation_data.action = bpy.data.actions.new(name="Scene Action")


def scale_meta_speed(sequence_editor, strip_list, strip_movie, scale):
    # Add an effect
    dummy_frame = 0
    strip_speed = sequence_editor.sequences.new_effect(
            name="Speed",
            type='SPEED',
            seq1=strip_movie,
            frame_start=dummy_frame,
            channel=strip_movie.channel + 1)
    strip_list.append(strip_speed)

    # not working in 2.6x :|
    strip_speed.use_frame_blend = True
    # meta = sequence_editor.new([strip_movie, strip_speed], 199, strip_movie.channel)

    # XXX-Meta Operator Mess
    scene = sequence_editor.id_data
    # we _know_ there are no others selected
    for strip in strip_list:
        strip.select = False
    strip_movie.select = True
    strip_speed.select = True
    bpy.ops.sequencer.meta_make()
    strip_meta = scene.sequence_editor.sequences[-1]  # XXX, weak assumption
    assert(strip_meta.type == 'META')
    strip_list.append(strip_meta)
    strip_movie.select = strip_speed.select = strip_meta.select = False
    # XXX-Meta Operator Mess (END)

    if scale >= 1.0:
        strip_movie.frame_still_end = int(strip_movie.frame_duration * (scale - 1.0))
    else:
        strip_speed.multiply_speed = 1.0 / scale
        strip_meta.frame_offset_end = strip_movie.frame_duration - int(strip_movie.frame_duration * scale)

    strip_speed.update()
    strip_meta.update()
    return strip_meta


def apply_dissolve_fcurve(strip_movie, blendin):
    scene = strip_movie.id_data
    id_animdata_action_ensure(scene)
    action = scene.animation_data.action

    data_path = strip_movie.path_from_id("blend_alpha")
    blend_alpha_fcurve = action.fcurves.new(data_path, index=0)
    blend_alpha_fcurve.keyframe_points.insert(strip_movie.frame_final_start, 0.0)
    blend_alpha_fcurve.keyframe_points.insert(strip_movie.frame_final_end, 1.0)

    blend_alpha_fcurve.keyframe_points[0].interpolation = 'LINEAR'
    blend_alpha_fcurve.keyframe_points[1].interpolation = 'LINEAR'

    if strip_movie.type != 'SOUND':
        strip_movie.blend_type = 'ALPHA_OVER'


def replace_ext(path, ext):
    return path[:path.rfind(".") + 1] + ext


def load_edl(scene, filename, reel_files, reel_offsets, global_offset):
    """
    reel_files - key:reel <--> reel:filename
    """

    strip_list = []

    import os
    # For test file
    # frame_offset = -769

    fps = scene.render.fps
    dummy_frame = 1

    elist = parse_edl.EditList()
    if not elist.parse(filename, fps):
        return "Unable to parse %r" % filename

    scene.sequence_editor_create()
    sequence_editor = scene.sequence_editor

    for strip in sequence_editor.sequences_all:
        strip.select = False

    # elist.clean()

    track = 0

    edits = elist.edits[:]
    # edits.sort(key = lambda edit: int(edit.recIn))

    prev_edit = None
    for edit in edits:
        print(edit)
        if edit.reel.lower() in parse_edl.BLACK_ID:
            frame_offset = 0
        else:
            frame_offset = reel_offsets[edit.reel]

        src_start = int(edit.srcIn) + frame_offset
        # UNUSED
        # src_end = int(edit.srcOut) + frame_offset
        # src_length = src_end - src_start

        rec_start = int(edit.recIn) + 1
        rec_end = int(edit.recOut) + 1
        rec_length = rec_end - rec_start

        # apply global offset
        rec_start += global_offset
        rec_end += global_offset

        # print(src_length, rec_length, src_start)

        if edit.m2 is not None:
            scale = fps / edit.m2.fps
        else:
            scale = 1.0

        unedited_start = rec_start - src_start
        offset_start = src_start - int(src_start * scale)  # works for scaling up AND down

        if edit.transition_type == parse_edl.TRANSITION_CUT and (not elist.overlap_test(edit)):
            track = 1

        strip = None
        final_strips = []
        if edit.reel.lower() in parse_edl.BLACK_ID:
            strip = sequence_editor.sequences.new_effect(
                    name="Color",
                    type='COLOR',
                    frame_start=rec_start,
                    frame_end=rec_start + max(1, rec_length),
                    channel=track + 1)
            strip_list.append(strip)
            final_strips.append(strip)
            strip.color = 0.0, 0.0, 0.0

        else:
            path_full = reel_files[edit.reel]
            path_dironly, path_fileonly = os.path.split(path_full)

            if edit.edit_type & (parse_edl.EDIT_VIDEO | parse_edl.EDIT_VIDEO_AUDIO):
                # and edit.transition_type == parse_edl.TRANSITION_CUT:

                # try:
                strip = sequence_editor.sequences.new_movie(
                        name=edit.reel,
                        filepath=path_full,
                        channel=track + 1,
                        frame_start=unedited_start + offset_start)
                strip_list.append(strip)
                # except:
                #     return "Invalid input for movie"

                # Apply scaled rec in bounds
                if scale != 1.0:
                    meta = scale_meta_speed(sequence_editor, strip_list, strip, scale)
                    final_strip = meta
                else:
                    final_strip = strip

                final_strip.update()
                final_strip.frame_offset_start = rec_start - final_strip.frame_final_start
                final_strip.frame_offset_end = rec_end - final_strip.frame_final_end
                final_strip.update()
                final_strip.frame_offset_end += (final_strip.frame_final_end - rec_end)
                final_strip.update()

                if edit.transition_duration:
                    if not prev_edit:
                        print("Error no previous strip")
                    else:
                        new_end = rec_start + int(edit.transition_duration)
                        for other in prev_edit.custom_data:
                            if other.type != 'SOUND':
                                other.frame_offset_end += (other.frame_final_end - new_end)
                                other.update()

                # Apply disolve
                if edit.transition_type == parse_edl.TRANSITION_DISSOLVE:
                    apply_dissolve_fcurve(final_strip, edit.transition_duration)

                if edit.transition_type == parse_edl.TRANSITION_WIPE:
                    other_track = track + 2
                    for other in prev_edit.custom_data:
                        if other.type != 'SOUND':
                            strip_wipe = sequence_editor.sequences.new_effect(
                                    name="Wipe",
                                    type='WIPE',
                                    seq1=final_strip,
                                    frame_start=dummy_frame,
                                    channel=other_track)
                            strip_list.append(strip_wipe)

                            from math import radians
                            if edit.wipe_type == parse_edl.WIPE_0:
                                strip_wipe.angle = radians(+90)
                            else:
                                strip_wipe.angle = radians(-90)

                            other_track += 1

                # strip.frame_offset_end = strip.frame_duration - int(edit.srcOut)
                # end_offset = (unedited_start + strip.frame_duration) - end
                # print start, end, end_offset
                # strip.frame_offset_end = end_offset
                #
                # break
                # print(strip)

                final_strips.append(final_strip)

            if edit.edit_type & (parse_edl.EDIT_AUDIO | parse_edl.EDIT_AUDIO_STEREO | parse_edl.EDIT_VIDEO_AUDIO):

                if scale == 1.0:  # TODO - scaled audio

                    try:
                        strip = sequence_editor.sequences.new_sound(
                                name=edit.reel,
                                filepath=path_full,
                                channel=track + 6,
                                frame_start=unedited_start + offset_start)
                        strip_list.append(strip)
                    except:

                        # See if there is a wave file there
                        path_full_wav = replace_ext(path_full, "wav")

                        # try:
                        strip = sequence_editor.sequences.new_sound(
                                name=edit.reel,
                                filepath=path_full_wav,
                                channel=track + 6,
                                frame_start=unedited_start + offset_start)
                        strip_list.append(strip)
                        # except:
                        #    return "Invalid input for audio"

                    final_strip = strip

                    # Copied from above
                    final_strip.update()
                    final_strip.frame_offset_start = rec_start - final_strip.frame_final_start
                    final_strip.frame_offset_end = rec_end - final_strip.frame_final_end
                    final_strip.update()
                    final_strip.frame_offset_end += (final_strip.frame_final_end - rec_end)
                    final_strip.update()

                    if edit.transition_type == parse_edl.TRANSITION_DISSOLVE:
                        apply_dissolve_fcurve(final_strip, edit.transition_duration)

                    final_strips.append(final_strip)

        if final_strips:
            for strip in final_strips:
                # strip.frame_duration = length
                strip.name = edit.as_name()
                edit.custom_data[:] = final_strips
                # track = not track
                prev_edit = edit
            track += 1

        # break

    for strip in strip_list:
        strip.update(True)
        strip.select = True

    return ""


def _test():
    elist = parse_edl.EditList()
    _filename = "/fe/edl/cinesoft/rush/blender_edl.edl"
    _fps = 25
    if not elist.parse(_filename, _fps):
        assert(0)
    reels = elist.reels_as_dict()

    print(list(reels.keys()))

    # import pdb; pdb.set_trace()
    msg = load_edl(bpy.context.scene,
                   _filename,
                   {'tapec': "/fe/edl/cinesoft/rush/rushes3.avi"},
                   {'tapec': 0})  # /tmp/test.edl
    print(msg)
# _test()
