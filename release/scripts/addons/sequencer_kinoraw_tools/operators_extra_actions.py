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

import bpy
import os
from bpy.types import Operator
from bpy.props import (
        IntProperty,
        FloatProperty,
        EnumProperty,
        BoolProperty,
        )
from . import functions


# Skip one second
class Sequencer_Extra_FrameSkip(Operator):
    bl_label = "Skip One Second"
    bl_idname = "screenextra.frame_skip"
    bl_description = "Skip through the Timeline by one-second increments"
    bl_options = {'REGISTER', 'UNDO'}

    back = BoolProperty(
            name="Back",
            default=False
            )

    def execute(self, context):
        one_second = bpy.context.scene.render.fps
        if self.back is True:
            one_second *= -1
        bpy.ops.screen.frame_offset(delta=one_second)

        return {'FINISHED'}


# Trim timeline
class Sequencer_Extra_TrimTimeline(Operator):
    bl_label = "Trim to Timeline Content"
    bl_idname = "timeextra.trimtimeline"
    bl_description = "Automatically set start and end frames"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.sequences
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]

        frame_start = 300000
        frame_end = -300000
        for i in seq.sequences:
            try:
                if i.frame_final_start < frame_start:
                    frame_start = i.frame_final_start
                if i.frame_final_end > frame_end:
                    frame_end = i.frame_final_end - 1
            except AttributeError:
                    pass

        if frame_start != 300000:
            scn.frame_start = frame_start
        if frame_end != -300000:
            scn.frame_end = frame_end

        bpy.ops.sequencer.view_all()

        return {'FINISHED'}


# Trim timeline to selection
class Sequencer_Extra_TrimTimelineToSelection(Operator):
    bl_label = "Trim to Selection"
    bl_idname = "timeextra.trimtimelinetoselection"
    bl_description = "Set start and end frames to selection"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.sequences
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]

        frame_start = 300000
        frame_end = -300000
        for i in seq.sequences:
            try:
                if i.frame_final_start < frame_start and i.select is True:
                    frame_start = i.frame_final_start
                if i.frame_final_end > frame_end and i.select is True:
                    frame_end = i.frame_final_end - 1
            except AttributeError:
                    pass

        if frame_start != 300000:
            scn.frame_start = frame_start
        if frame_end != -300000:
            scn.frame_end = frame_end

        bpy.ops.sequencer.view_selected()
        return {'FINISHED'}


# Open image with editor and create movie clip strip
"""
    When a movie or image strip is selected, this operator creates a movieclip
    or find the correspondent movieclip that already exists for this footage,
    and add a VSE clip strip with same cuts the original strip has.
    It can convert movie strips and image sequences, both with hard cuts or
    soft cuts.
"""


class Sequencer_Extra_CreateMovieclip(Operator):
    bl_label = "Create a Movieclip from selected strip"
    bl_idname = "sequencerextra.createmovieclip"
    bl_description = "Create a Movieclip strip from a MOVIE or IMAGE strip"

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('MOVIE', 'IMAGE')
        else:
            return False

    def execute(self, context):
        strip = functions.act_strip(context)
        scn = context.scene

        if strip.type == 'MOVIE':
            path = strip.filepath
            data_exists = False

            for i in bpy.data.movieclips:
                if i.filepath == path:
                    data_exists = True
                    data = i
            newstrip = None
            if data_exists is False:
                try:
                    data = bpy.data.movieclips.load(filepath=path)
                    newstrip = bpy.ops.sequencer.movieclip_strip_add(
                                        replace_sel=True, overlap=False,
                                        clip=data.name
                                        )
                    newstrip = functions.act_strip(context)
                    newstrip.frame_start = strip.frame_start\
                        - strip.animation_offset_start
                    tin = strip.frame_offset_start + strip.frame_start
                    tout = tin + strip.frame_final_duration
                    # print(newstrip.frame_start, strip.frame_start, tin, tout)
                    functions.triminout(newstrip, tin, tout)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                    return {'CANCELLED'}

            else:
                try:
                    newstrip = bpy.ops.sequencer.movieclip_strip_add(
                                            replace_sel=True, overlap=False,
                                            clip=data.name
                                            )
                    newstrip = functions.act_strip(context)
                    newstrip.frame_start = strip.frame_start\
                        - strip.animation_offset_start
                    # i need to declare the strip this way in order
                    # to get triminout() working
                    clip = bpy.context.scene.sequence_editor.sequences[
                                                                newstrip.name
                                                                ]
                    # i cannot change these movie clip attributes via scripts
                    # but it works in the python console...
                    # clip.animation_offset_start = strip.animation.offset_start
                    # clip.animation_offset_end = strip.animation.offset_end
                    # clip.frame_final_duration = strip.frame_final_duration
                    tin = strip.frame_offset_start + strip.frame_start
                    tout = tin + strip.frame_final_duration
                    # print(newstrip.frame_start, strip.frame_start, tin, tout)
                    functions.triminout(clip, tin, tout)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                    return {'CANCELLED'}

        elif strip.type == 'IMAGE':
            # print("image")
            base_dir = bpy.path.abspath(strip.directory)
            scn.frame_current = strip.frame_start - strip.animation_offset_start

            # searching for the first frame of the sequencer. This is mandatory
            # for hard cutted sequence strips to be correctly converted,
            # avoiding to create a new movie clip if not needed
            filename = sorted(os.listdir(base_dir))[0]
            path = os.path.join(base_dir, filename)
            # print(path)
            data_exists = False
            for i in bpy.data.movieclips:
                # print(i.filepath, path)
                if i.filepath == path:
                    data_exists = True
                    data = i
            # print(data_exists)
            if data_exists is False:
                try:
                    data = bpy.data.movieclips.load(filepath=path)
                    newstrip = bpy.ops.sequencer.movieclip_strip_add(
                                        replace_sel=True, overlap=False,
                                        clip=data.name
                                        )
                    newstrip = functions.act_strip(context)
                    newstrip.frame_start = strip.frame_start\
                        - strip.animation_offset_start
                    clip = bpy.context.scene.sequence_editor.sequences[
                                                                newstrip.name
                                                                ]
                    tin = strip.frame_offset_start + strip.frame_start
                    tout = tin + strip.frame_final_duration
                    # print(newstrip.frame_start, strip.frame_start, tin, tout)
                    functions.triminout(clip, tin, tout)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                    return {'CANCELLED'}
            else:
                try:
                    newstrip = bpy.ops.sequencer.movieclip_strip_add(
                                            replace_sel=True, overlap=False,
                                            clip=data.name
                                            )
                    newstrip = functions.act_strip(context)
                    newstrip.frame_start = strip.frame_start\
                        - strip.animation_offset_start
                    # need to declare the strip this way in order
                    # to get triminout() working
                    clip = bpy.context.scene.sequence_editor.sequences[
                                                                newstrip.name
                                                                ]
                    # cannot change this atributes via scripts...
                    # but it works in the python console...
                    # clip.animation_offset_start = strip.animation.offset_start
                    # clip.animation_offset_end = strip.animation.offset_end
                    # clip.frame_final_duration = strip.frame_final_duration
                    tin = strip.frame_offset_start + strip.frame_start
                    tout = tin + strip.frame_final_duration
                    # print(newstrip.frame_start, strip.frame_start, tin, tout)
                    functions.triminout(clip, tin, tout)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                    return {'CANCELLED'}

        # show the new clip in a movie clip editor, if available.
        if strip.type == 'MOVIE' or 'IMAGE':
            for a in context.window.screen.areas:
                if a.type == 'CLIP_EDITOR':
                    a.spaces[0].clip = data

        return {'FINISHED'}


# Open image with editor
class Sequencer_Extra_Edit(Operator):
    bl_label = "Open with Editor"
    bl_idname = "sequencerextra.edit"
    bl_description = "Open with Movie Clip or Image Editor"

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('MOVIE', 'IMAGE')
        else:
            return False

    def execute(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        data_exists = False

        if strip.type == 'MOVIE':
            path = strip.filepath

            for i in bpy.data.movieclips:
                if i.filepath == path:
                    data_exists = True
                    data = i

            if data_exists is False:
                try:
                    data = bpy.data.movieclips.load(filepath=path)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, "Error loading file")
                    return {'CANCELLED'}

        elif strip.type == 'IMAGE':
            base_dir = bpy.path.abspath(strip.directory)
            strip_elem = strip.strip_elem_from_frame(scn.frame_current)
            elem_name = strip_elem.filename
            path = base_dir + elem_name

            for i in bpy.data.images:
                if i.filepath == path:
                    data_exists = True
                    data = i

            if data_exists is False:
                try:
                    data = bpy.data.images.load(filepath=path)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                    return {'CANCELLED'}

        if strip.type == 'MOVIE':
            for a in context.window.screen.areas:
                if a.type == 'CLIP_EDITOR':
                    a.spaces[0].clip = data
        elif strip.type == 'IMAGE':
            for a in context.window.screen.areas:
                if a.type == 'IMAGE_EDITOR':
                    a.spaces[0].image = data

        return {'FINISHED'}


# Open image with external editor
class Sequencer_Extra_EditExternally(Operator):
    bl_label = "Open with External Editor"
    bl_idname = "sequencerextra.editexternally"
    bl_description = "Open with the default external image editor"

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type == 'IMAGE'
        else:
            return False

    def execute(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        base_dir = bpy.path.abspath(strip.directory)
        strip_elem = strip.strip_elem_from_frame(scn.frame_current)
        path = base_dir + strip_elem.filename

        try:
            bpy.ops.image.external_edit(filepath=path)
        except:
            self.report({'ERROR_INVALID_INPUT'},
                        "Please specify an Image Editor in Preferences > File")
            return {'CANCELLED'}

        return {'FINISHED'}


# File name to strip name
class Sequencer_Extra_FileNameToStripName(Operator):
    bl_label = "File Name to Selected Strips Name"
    bl_idname = "sequencerextra.striprename"
    bl_description = "Set strip name to input file name"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.sequences
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        selection = False
        for i in seq.sequences:
            if i.select is True:
                if i.type == 'IMAGE' and not i.mute:
                    selection = True
                    i.name = i.elements[0].filename
                if (i.type == 'SOUND' or i.type == 'MOVIE') and not i.mute:
                    selection = True
                    i.name = bpy.path.display_name_from_filepath(i.filepath)
        if selection is False:
            self.report({'ERROR_INVALID_INPUT'},
                        "No image or movie strip selected")
            return {'CANCELLED'}
        return {'FINISHED'}


# Navigate up
class Sequencer_Extra_NavigateUp(Operator):
    bl_label = "Navigate Up"
    bl_idname = "sequencerextra.navigateup"
    bl_description = "Move to Parent Timeline"

    @classmethod
    def poll(self, context):
        try:
            if context.scene.sequence_editor.meta_stack:
                return True
            return False
        except:
            return False

    def execute(self, context):
        if (functions.act_strip(context)):
            strip = functions.act_strip(context)
            seq_type = strip.type
            if seq_type == 'META':
                context.scene.sequence_editor.active_strip = None

        bpy.ops.sequencer.meta_toggle()
        return {'FINISHED'}


# Ripple delete
class Sequencer_Extra_RippleDelete(Operator):
    bl_label = "Ripple Delete"
    bl_idname = "sequencerextra.rippledelete"
    bl_description = "Delete a strip and shift back following ones"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        # strip = functions.act_strip(context)
        for strip in context.selected_editable_sequences:
            cut_frame = strip.frame_final_start
            next_edit = 300000
            bpy.ops.sequencer.select_all(action='DESELECT')
            strip.select = True
            bpy.ops.sequencer.delete()
            striplist = []
            for i in seq.sequences:
                try:
                    if (i.frame_final_start > cut_frame and
                            not i.mute):
                        if i.frame_final_start < next_edit:
                            next_edit = i.frame_final_start
                    if not i.mute:
                        striplist.append(i)
                except AttributeError:
                        pass

            if next_edit == 300000:
                return {'FINISHED'}
            ripple_length = next_edit - cut_frame
            for i in range(len(striplist)):
                str = striplist[i]
                try:
                    if str.frame_final_start > cut_frame:
                        str.frame_start = str.frame_start - ripple_length
                except AttributeError:
                        pass
            bpy.ops.sequencer.reload()
        return {'FINISHED'}


# Ripple cut
class Sequencer_Extra_RippleCut(Operator):
    bl_label = "Ripple Cut"
    bl_idname = "sequencerextra.ripplecut"
    bl_description = "Move a strip to buffer and shift back following ones"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        strip = functions.act_strip(context)
        bpy.ops.sequencer.select_all(action='DESELECT')
        strip.select = True
        temp_cf = scn.frame_current
        scn.frame_current = strip.frame_final_start
        bpy.ops.sequencer.copy()
        scn.frame_current = temp_cf

        bpy.ops.sequencerextra.rippledelete()
        return {'FINISHED'}


# Insert
class Sequencer_Extra_Insert(Operator):
    bl_label = "Insert"
    bl_idname = "sequencerextra.insert"
    bl_description = ("Move active strip to current frame and shift "
                      "forward following ones")
    bl_options = {'REGISTER', 'UNDO'}

    singlechannel = BoolProperty(
            name="Single Channel",
            default=False
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        strip = functions.act_strip(context)
        gap = strip.frame_final_duration
        bpy.ops.sequencer.select_all(action='DESELECT')
        current_frame = scn.frame_current

        striplist = []
        for i in seq.sequences:
            try:
                if (i.frame_final_start >= current_frame and
                        not i.mute):
                    if self.singlechannel is True:
                        if i.channel == strip.channel:
                            striplist.append(i)
                    else:
                        striplist.append(i)
            except AttributeError:
                    pass
        try:
            bpy.ops.sequencerextra.selectcurrentframe('EXEC_DEFAULT',
            mode='AFTER')
        except:
            self.report({'ERROR_INVALID_INPUT'}, "Execution Error, "
                        "check your Blender version")
            return {'CANCELLED'}

        for i in range(len(striplist)):
            str = striplist[i]
            try:
                if str.select is True:
                    str.frame_start += gap
            except AttributeError:
                    pass
        try:
            diff = current_frame - strip.frame_final_start
            strip.frame_start += diff
        except AttributeError:
                pass

        strip = functions.act_strip(context)
        scn.frame_current += strip.frame_final_duration
        bpy.ops.sequencer.reload()

        return {'FINISHED'}


# Copy strip properties
class Sequencer_Extra_CopyProperties(Operator):
    bl_label = "Copy Properties"
    bl_idname = "sequencerextra.copyproperties"
    bl_description = "Copy properties of active strip to selected strips"
    bl_options = {'REGISTER', 'UNDO'}

    prop = EnumProperty(
            name="Property",
            items=[
                # common
                ('name', 'Name', ''),
                ('blend_alpha', 'Opacity', ''),
                ('blend_type', 'Blend Mode', ''),
                ('animation_offset', 'Input - Trim Duration', ''),
                # non-sound
                ('use_translation', 'Input - Image Offset', ''),
                ('crop', 'Input - Image Crop', ''),
                ('proxy', 'Proxy / Timecode', ''),
                ('strobe', 'Filter - Strobe', ''),
                ('color_multiply', 'Filter - Multiply', ''),
                ('color_saturation', 'Filter - Saturation', ''),
                ('deinterlace', 'Filter - De-Interlace', ''),
                ('flip', 'Filter - Flip', ''),
                ('float', 'Filter - Convert Float', ''),
                ('alpha_mode', 'Filter - Alpha Mode', ''),
                ('reverse', 'Filter - Backwards', ''),
                # sound
                ('pan', 'Sound - Pan', ''),
                ('pitch', 'Sound - Pitch', ''),
                ('volume', 'Sound - Volume', ''),
                ('cache', 'Sound - Caching', ''),
                # image
                ('directory', 'Image - Directory', ''),
                # movie
                ('mpeg_preseek', 'Movie - MPEG Preseek', ''),
                ('stream_index', 'Movie - Stream Index', ''),
                # wipe
                ('wipe', 'Effect - Wipe', ''),
                # transform
                ('transform', 'Effect - Transform', ''),
                # color
                ('color', 'Effect - Color', ''),
                # speed
                ('speed', 'Effect - Speed', ''),
                # multicam
                ('multicam_source', 'Effect - Multicam Source', ''),
                # effect
                ('effect_fader', 'Effect - Effect Fader', ''),
                ],
            default='blend_alpha'
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        strip = functions.act_strip(context)

        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]

        for i in seq.sequences:
            if (i.select is True and not i.mute):
                try:
                    if self.prop == 'name':
                        i.name = strip.name
                    elif self.prop == 'blend_alpha':
                        i.blend_alpha = strip.blend_alpha
                    elif self.prop == 'blend_type':
                        i.blend_type = strip.blend_type
                    elif self.prop == 'animation_offset':
                        i.animation_offset_start = strip.animation_offset_start
                        i.animation_offset_end = strip.animation_offset_end
                    elif self.prop == 'use_translation':
                        i.use_translation = strip.use_translation
                        i.transform.offset_x = strip.transform.offset_x
                        i.transform.offset_y = strip.transform.offset_y
                    elif self.prop == 'crop':
                        i.use_crop = strip.use_crop
                        i.crop.min_x = strip.crop.min_x
                        i.crop.min_y = strip.crop.min_y
                        i.crop.max_x = strip.crop.max_x
                        i.crop.max_y = strip.crop.max_y
                    elif self.prop == 'proxy':
                        i.use_proxy = strip.use_proxy
                        p = strip.proxy.use_proxy_custom_directory  # pep80
                        i.proxy.use_proxy_custom_directory = p
                        i.proxy.use_proxy_custom_file = strip.proxy.use_proxy_custom_file
                        i.proxy.build_100 = strip.proxy.build_100
                        i.proxy.build_25 = strip.proxy.build_25
                        i.proxy.build_50 = strip.proxy.build_50
                        i.proxy.build_75 = strip.proxy.build_75
                        i.proxy.directory = strip.proxy.directory
                        i.proxy.filepath = strip.proxy.filepath
                        i.proxy.quality = strip.proxy.quality
                        i.proxy.timecode = strip.proxy.timecode
                        i.proxy.use_overwrite = strip.proxy.use_overwrite
                    elif self.prop == 'strobe':
                        i.strobe = strip.strobe
                    elif self.prop == 'color_multiply':
                        i.color_multiply = strip.color_multiply
                    elif self.prop == 'color_saturation':
                        i.color_saturation = strip.color_saturation
                    elif self.prop == 'deinterlace':
                        i.use_deinterlace = strip.use_deinterlace
                    elif self.prop == 'flip':
                        i.use_flip_x = strip.use_flip_x
                        i.use_flip_y = strip.use_flip_y
                    elif self.prop == 'float':
                        i.use_float = strip.use_float
                    elif self.prop == 'alpha_mode':
                        i.alpha_mode = strip.alpha_mode
                    elif self.prop == 'reverse':
                        i.use_reverse_frames = strip.use_reverse_frames
                    elif self.prop == 'pan':
                        i.pan = strip.pan
                    elif self.prop == 'pitch':
                        i.pitch = strip.pitch
                    elif self.prop == 'volume':
                        i.volume = strip.volume
                    elif self.prop == 'cache':
                        i.use_memory_cache = strip.use_memory_cache
                    elif self.prop == 'directory':
                        i.directory = strip.directory
                    elif self.prop == 'mpeg_preseek':
                        i.mpeg_preseek = strip.mpeg_preseek
                    elif self.prop == 'stream_index':
                        i.stream_index = strip.stream_index
                    elif self.prop == 'wipe':
                        i.angle = strip.angle
                        i.blur_width = strip.blur_width
                        i.direction = strip.direction
                        i.transition_type = strip.transition_type
                    elif self.prop == 'transform':
                        i.interpolation = strip.interpolation
                        i.rotation_start = strip.rotation_start
                        i.use_uniform_scale = strip.use_uniform_scale
                        i.scale_start_x = strip.scale_start_x
                        i.scale_start_y = strip.scale_start_y
                        i.translation_unit = strip.translation_unit
                        i.translate_start_x = strip.translate_start_x
                        i.translate_start_y = strip.translate_start_y
                    elif self.prop == 'color':
                        i.color = strip.color
                    elif self.prop == 'speed':
                        i.use_default_fade = strip.use_default_fade
                        i.speed_factor = strip.speed_factor
                        i.use_as_speed = strip.use_as_speed
                        i.scale_to_length = strip.scale_to_length
                        i.multiply_speed = strip.multiply_speed
                        i.use_frame_blend = strip.use_frame_blend
                    elif self.prop == 'multicam_source':
                        i.multicam_source = strip.multicam_source
                    elif self.prop == 'effect_fader':
                        i.use_default_fade = strip.use_default_fade
                        i.effect_fader = strip.effect_fader
                except:
                    pass

        bpy.ops.sequencer.reload()
        return {'FINISHED'}


# Fade in and out
class Sequencer_Extra_FadeInOut(Operator):
    bl_idname = "sequencerextra.fadeinout"
    bl_label = "Fade..."
    bl_description = "Fade volume or opacity of active strip"
    bl_options = {'REGISTER', 'UNDO'}

    mode = EnumProperty(
            name='Direction',
            items=(
                ('IN', "Fade In...", ""),
                ('OUT', "Fade Out...", ""),
                ('INOUT', "Fade In and Out...", "")),
            default='IN',
            )

    fade_duration = IntProperty(
        name='Duration',
        description='Number of frames to fade',
        min=1, max=250,
        default=25)
    fade_amount = FloatProperty(
        name='Amount',
        description='Maximum value of fade',
        min=0.0,
        max=100.0,
        default=1.0)

    @classmethod
    def poll(cls, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        seq = context.scene.sequence_editor
        scn = context.scene
        strip = seq.active_strip
        tmp_current_frame = context.scene.frame_current

        if strip.type == 'SOUND':
            if(self.mode) == 'OUT':
                scn.frame_current = strip.frame_final_end - self.fade_duration
                strip.volume = self.fade_amount
                strip.keyframe_insert('volume')
                scn.frame_current = strip.frame_final_end
                strip.volume = 0
                strip.keyframe_insert('volume')
            elif(self.mode) == 'INOUT':
                scn.frame_current = strip.frame_final_start
                strip.volume = 0
                strip.keyframe_insert('volume')
                scn.frame_current += self.fade_duration
                strip.volume = self.fade_amount
                strip.keyframe_insert('volume')
                scn.frame_current = strip.frame_final_end - self.fade_duration
                strip.volume = self.fade_amount
                strip.keyframe_insert('volume')
                scn.frame_current = strip.frame_final_end
                strip.volume = 0
                strip.keyframe_insert('volume')
            else:
                scn.frame_current = strip.frame_final_start
                strip.volume = 0
                strip.keyframe_insert('volume')
                scn.frame_current += self.fade_duration
                strip.volume = self.fade_amount
                strip.keyframe_insert('volume')

        else:
            if(self.mode) == 'OUT':
                scn.frame_current = strip.frame_final_end - self.fade_duration
                strip.blend_alpha = self.fade_amount
                strip.keyframe_insert('blend_alpha')
                scn.frame_current = strip.frame_final_end
                strip.blend_alpha = 0
                strip.keyframe_insert('blend_alpha')
            elif(self.mode) == 'INOUT':
                scn.frame_current = strip.frame_final_start
                strip.blend_alpha = 0
                strip.keyframe_insert('blend_alpha')
                scn.frame_current += self.fade_duration
                strip.blend_alpha = self.fade_amount
                strip.keyframe_insert('blend_alpha')
                scn.frame_current = strip.frame_final_end - self.fade_duration
                strip.blend_alpha = self.fade_amount
                strip.keyframe_insert('blend_alpha')
                scn.frame_current = strip.frame_final_end
                strip.blend_alpha = 0
                strip.keyframe_insert('blend_alpha')
            else:
                scn.frame_current = strip.frame_final_start
                strip.blend_alpha = 0
                strip.keyframe_insert('blend_alpha')
                scn.frame_current += self.fade_duration
                strip.blend_alpha = self.fade_amount
                strip.keyframe_insert('blend_alpha')

        scn.frame_current = tmp_current_frame

        scn.kr_default_fade_duration = self.fade_duration
        scn.kr_default_fade_amount = self.fade_amount
        return{'FINISHED'}

    def invoke(self, context, event):
        scn = context.scene
        functions.initSceneProperties(context)
        self.fade_duration = scn.kr_default_fade_duration
        self.fade_amount = scn.kr_default_fade_amount
        return context.window_manager.invoke_props_dialog(self)


# Extend to fill
class Sequencer_Extra_ExtendToFill(Operator):
    bl_idname = "sequencerextra.extendtofill"
    bl_label = "Extend to Fill"
    bl_description = "Extend active strip forward to fill adjacent space"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        strip = functions.act_strip(context)
        chn = strip.channel
        stf = strip.frame_final_end
        enf = 300000

        for i in seq.sequences:
            ffs = i.frame_final_start
            if (i.channel == chn and ffs > stf):
                if ffs < enf:
                    enf = ffs
        if enf == 300000 and stf < scn.frame_end:
            enf = scn.frame_end

        if enf == 300000 or enf == stf:
            self.report({'ERROR_INVALID_INPUT'}, 'Unable to extend')
            return {'CANCELLED'}
        else:
            strip.frame_final_end = enf

        bpy.ops.sequencer.reload()
        return {'FINISHED'}


# Place from file browser
class Sequencer_Extra_PlaceFromFileBrowser(Operator):
    bl_label = "Place"
    bl_idname = "sequencerextra.placefromfilebrowser"
    bl_description = "Place or insert active file from File Browser"
    bl_options = {'REGISTER', 'UNDO'}

    insert = BoolProperty(
            name="Insert",
            default=False
            )

    def execute(self, context):
        scn = context.scene
        for a in context.window.screen.areas:
            if a.type == 'FILE_BROWSER':
                params = a.spaces[0].params
                break
        try:
            params
        except UnboundLocalError:
            self.report({'ERROR_INVALID_INPUT'}, 'No visible File Browser')
            return {'CANCELLED'}

        if params.filename == '':
            self.report({'ERROR_INVALID_INPUT'}, 'No file selected')
            return {'CANCELLED'}

        path = os.path.join(params.directory, params.filename)
        frame = context.scene.frame_current
        strip_type = functions.detect_strip_type(params.filename)

        try:
            if strip_type == 'IMAGE':
                image_file = []
                filename = {"name": params.filename}
                image_file.append(filename)
                f_in = scn.frame_current
                f_out = f_in + scn.render.fps - 1
                bpy.ops.sequencer.image_strip_add(files=image_file,
                directory=params.directory, frame_start=f_in,
                frame_end=f_out, relative_path=False)
            elif strip_type == 'MOVIE':
                bpy.ops.sequencer.movie_strip_add(filepath=path,
                frame_start=frame, relative_path=False)
            elif strip_type == 'SOUND':
                bpy.ops.sequencer.sound_strip_add(filepath=path,
                frame_start=frame, relative_path=False)
            else:
                self.report({'ERROR_INVALID_INPUT'}, 'Invalid file format')
                return {'CANCELLED'}
        except:
            self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
            return {'CANCELLED'}

        if self.insert is True:
            try:
                striplist = []
                for i in bpy.context.selected_editable_sequences:
                    if (i.select is True and i.type == "SOUND"):
                        striplist.append(i)
                bpy.ops.sequencerextra.insert()
                if striplist[0]:
                    striplist[0].frame_start = frame
            except:
                self.report({'ERROR_INVALID_INPUT'}, "Execution Error, "
                             "check your Blender version")
                return {'CANCELLED'}
        else:
            strip = functions.act_strip(context)
            scn.frame_current += strip.frame_final_duration
            bpy.ops.sequencer.reload()

        return {'FINISHED'}


# Select strips on same channel
class Sequencer_Extra_SelectSameChannel(Operator):
    bl_label = "Select Strips on the Same Channel"
    bl_idname = "sequencerextra.selectsamechannel"
    bl_description = "Select strips on the same channel as active one"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return True
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        bpy.ops.sequencer.select_active_side(side="LEFT")
        bpy.ops.sequencer.select_active_side(side="RIGHT")

        return {'FINISHED'}


# Current-frame-aware select
class Sequencer_Extra_SelectCurrentFrame(Operator):
    bl_label = "Current-Frame-Aware Select"
    bl_idname = "sequencerextra.selectcurrentframe"
    bl_description = "Select strips according to current frame"
    bl_options = {'REGISTER', 'UNDO'}

    mode = EnumProperty(
            name='Mode',
            items=(
            ('BEFORE', 'Before Current Frame', ''),
            ('AFTER', 'After Current Frame', ''),
            ('ON', 'On Current Frame', '')),
            default='BEFORE',
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.sequences
        else:
            return False

    def execute(self, context):
        mode = self.mode
        scn = context.scene
        seq = scn.sequence_editor
        cf = scn.frame_current
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]

        if mode == 'AFTER':
            for i in seq.sequences:
                try:
                    if (i.frame_final_start >= cf and not i.mute):
                        i.select = True
                except AttributeError:
                        pass
        elif mode == 'ON':
            for i in seq.sequences:
                try:
                    if (i.frame_final_start <= cf and
                            i.frame_final_end > cf and
                            not i.mute):
                        i.select = True
                except AttributeError:
                        pass
        else:
            for i in seq.sequences:
                try:
                    if (i.frame_final_end < cf and not i.mute):
                        i.select = True
                except AttributeError:
                        pass

        return {'FINISHED'}


# Select by type
class Sequencer_Extra_SelectAllByType(Operator):
    bl_label = "All by Type"
    bl_idname = "sequencerextra.select_all_by_type"
    bl_description = "Select all the strips of the same type"
    bl_options = {'REGISTER', 'UNDO'}

    type = EnumProperty(
            name="Strip Type",
            items=(
                ('ACTIVE', 'Same as Active Strip', ''),
                ('IMAGE', 'Image', ''),
                ('META', 'Meta', ''),
                ('SCENE', 'Scene', ''),
                ('MOVIE', 'Movie', ''),
                ('SOUND', 'Sound', ''),
                ('TRANSFORM', 'Transform', ''),
                ('COLOR', 'Color', '')),
            default='ACTIVE',
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.sequences
        else:
            return False

    def execute(self, context):
        strip_type = self.type
        scn = context.scene
        seq = scn.sequence_editor
        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]
        active_strip = functions.act_strip(context)
        if strip_type == 'ACTIVE':
            if active_strip is None:
                self.report({'ERROR_INVALID_INPUT'},
                'No active strip')
                return {'CANCELLED'}
            strip_type = active_strip.type

        striplist = []
        for i in seq.sequences:
            try:
                if (i.type == strip_type and not i.mute):
                    striplist.append(i)
            except AttributeError:
                    pass
        for i in range(len(striplist)):
            str = striplist[i]
            try:
                str.select = True
            except AttributeError:
                    pass

        return {'FINISHED'}


# Open in movie clip editor from file browser
class Clip_Extra_OpenFromFileBrowser(Operator):
    bl_label = "Open from File Browser"
    bl_idname = "clipextra.openfromfilebrowser"
    bl_description = "Load a Movie or Image Sequence from File Browser"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        for a in context.window.screen.areas:
            if a.type == 'FILE_BROWSER':
                params = a.spaces[0].params
                break
        try:
            params
        except:
            self.report({'ERROR_INVALID_INPUT'}, 'No visible File Browser')
            return {'CANCELLED'}

        if params.filename == '':
            self.report({'ERROR_INVALID_INPUT'}, 'No file selected')
            return {'CANCELLED'}

        path = params.directory + params.filename
        strip_type = functions.detect_strip_type(params.filename)
        data_exists = False

        if strip_type in ('MOVIE', 'IMAGE'):
            for i in bpy.data.movieclips:
                if i.filepath == path:
                    data_exists = True
                    data = i

            if data_exists is False:
                try:
                    data = bpy.data.movieclips.load(filepath=path)
                except:
                    self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                    return {'CANCELLED'}
        else:
            self.report({'ERROR_INVALID_INPUT'}, 'Invalid file format')
            return {'CANCELLED'}

        for a in context.window.screen.areas:
            if a.type == 'CLIP_EDITOR':
                a.spaces[0].clip = data

        return {'FINISHED'}


# Open in movie clip editor from sequencer
class Clip_Extra_OpenActiveStrip(Operator):
    bl_label = "Open Active Strip"
    bl_idname = "clipextra.openactivestrip"
    bl_description = "Load a Movie or Image Sequence from Sequence Editor"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        scn = context.scene
        strip = functions.act_strip(context)
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('MOVIE', 'IMAGE')
        else:
            return False

    def execute(self, context):
        strip = functions.act_strip(context)
        data_exists = False

        if strip.type == 'MOVIE':
            path = strip.filepath
        elif strip.type == 'IMAGE':
            base_dir = bpy.path.relpath(strip.directory)
            filename = strip.elements[0].filename
            path = base_dir + '/' + filename
        else:
            self.report({'ERROR_INVALID_INPUT'}, 'Invalid file format')
            return {'CANCELLED'}

        for i in bpy.data.movieclips:
            if i.filepath == path:
                data_exists = True
                data = i
        if data_exists is False:
            try:
                data = bpy.data.movieclips.load(filepath=path)
            except:
                self.report({'ERROR_INVALID_INPUT'}, 'Error loading file')
                return {'CANCELLED'}

        for a in context.window.screen.areas:
            if a.type == 'CLIP_EDITOR':
                a.spaces[0].clip = data

        return {'FINISHED'}


# Jog / Shuttle
class Sequencer_Extra_JogShuttle(Operator):
    bl_label = "Jog/Shuttle"
    bl_idname = "sequencerextra.jogshuttle"
    bl_description = ("Jog through the current sequence\n"
                      "Left Mouse button to confirm, Right mouse\Esc to cancel")

    def execute(self, context):
        scn = context.scene
        start_frame = scn.frame_start
        end_frame = scn.frame_end
        duration = end_frame - start_frame
        diff = self.x - self.init_x
        diff /= 5
        diff = int(diff)
        extended_frame = diff + (self.init_current_frame - start_frame)
        looped_frame = extended_frame % (duration + 1)
        target_frame = start_frame + looped_frame
        context.scene.frame_current = target_frame

    def modal(self, context, event):
        if event.type == 'MOUSEMOVE':
            self.x = event.mouse_x
            self.execute(context)
        elif event.type == 'LEFTMOUSE':
            return {'FINISHED'}
        elif event.type in ('RIGHTMOUSE', 'ESC'):
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}

    def invoke(self, context, event):
        scn = context.scene
        self.x = event.mouse_x
        self.init_x = self.x
        self.init_current_frame = scn.frame_current
        self.execute(context)
        context.window_manager.modal_handler_add(self)

        return {'RUNNING_MODAL'}
