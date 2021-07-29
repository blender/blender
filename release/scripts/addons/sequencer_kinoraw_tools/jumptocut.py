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
from . import functions
from bpy.types import (
        Operator,
        )
from bpy.props import (
        IntProperty,
        )
from bpy.app.handlers import persistent


class OBJECT_OT_Setinout(Operator):
    bl_label = "Set IN and OUT to selected"
    bl_idname = "sequencerextra.setinout"
    bl_description = "Set IN and OUT markers to the selected strips limits"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.active_strip
        else:
            return False

    def execute(self, context):
        functions.initSceneProperties(context)

        scn = context.scene
        markers = scn.timeline_markers
        seq = scn.sequence_editor

        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]

        # search for timeline limits
        tl_start = 300000
        tl_end = -300000
        for i in context.selected_editable_sequences:
            if i.select is True:
                start = i.frame_start + i.frame_offset_start - i.frame_still_start
                end = start + i.frame_final_duration
                if start < tl_start:
                    tl_start = start
                if end > tl_end:
                    tl_end = end
                # print(tl_start,tl_end)

        if scn.kr_auto_markers:
            scn.kr_in_marker = tl_start
            scn.kr_out_marker = tl_end
        else:
            scn.kr_in_marker = tl_start
            scn.kr_out_marker = tl_end

            if "IN" in markers:
                mark = markers["IN"]
                mark.frame = scn.kr_in_marker
            else:
                mark = markers.new(name="IN")
                mark.frame = scn.kr_in_marker

            if "OUT" in markers:
                mark = markers["OUT"]
                mark.frame = scn.kr_out_marker
            else:
                mark = markers.new(name="OUT")
                mark.frame = scn.kr_in_marker

        return {'FINISHED'}


class OBJECT_OT_Triminout(Operator):
    bl_label = "Trim to in & out"
    bl_idname = "sequencerextra.triminout"
    bl_description = "Trim the selected strip to IN and OUT markers (if exists)"

    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            if scn.sequence_editor.active_strip:
                markers = scn.timeline_markers
                if "IN" and "OUT" in markers:
                    return True
        else:
            return False

    def execute(self, context):

        scene = context.scene
        seq = scene.sequence_editor

        meta_level = len(seq.meta_stack)
        if meta_level > 0:
            seq = seq.meta_stack[meta_level - 1]

        markers = scene.timeline_markers
        sin = markers["IN"].frame
        sout = markers["OUT"].frame
        strips = context.selected_editable_sequences

        # (triminout function only works fine
        # with one strip selected at a time)
        for strip in strips:
            # deselect all other strips
            for i in strips:
                i.select = False
            # select current strip
            strip.select = True
            remove = functions.triminout(strip, sin, sout)
            if remove is True:
                bpy.ops.sequencer.delete()

        # select all strips again
        for strip in strips:
            try:
                strip.select = True
            except ReferenceError:
                pass

        bpy.ops.sequencer.reload()

        return {'FINISHED'}


# SOURCE IN OUT
class OBJECT_OT_Sourcein(Operator):  # Operator source in
    bl_label = "Source IN"
    bl_idname = "sequencerextra.sourcein"
    bl_description = "Add or move a marker named IN"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn:
            return scn.sequence_editor
        else:
            return False

    def execute(self, context):
        functions.initSceneProperties(context)
        scn = context.scene
        markers = scn.timeline_markers

        if scn.kr_auto_markers:
            scn.kr_in_marker = scn.frame_current

        else:
            scn.kr_in_marker = scn.frame_current
            if "IN" in markers:
                mark = markers["IN"]
                mark.frame = scn.kr_in_marker
            else:
                mark = markers.new(name="IN")
                mark.frame = scn.kr_in_marker

            # limit OUT marker position with IN marker
            if scn.kr_in_marker > scn.kr_out_marker:
                scn.kr_out_marker = scn.kr_in_marker

            if "OUT" in markers:
                mark = markers["OUT"]
                mark.frame = scn.kr_out_marker

        for m in markers:
            m.select = False
            if m.name in {"IN", "OUT"}:
                m.select = True
        bpy.ops.sequencer.reload()

        return {'FINISHED'}


class OBJECT_OT_Sourceout(Operator):  # Operator source out
    bl_label = "Source OUT"
    bl_idname = "sequencerextra.sourceout"
    bl_description = "Add or move a marker named OUT"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn:
            return scn.sequence_editor
        else:
            return False

    def execute(self, context):
        scn = context.scene
        functions.initSceneProperties(context)
        markers = scn.timeline_markers

        if scn.kr_auto_markers:
            scn.kr_out_marker = scn.frame_current

        else:
            scn.kr_out_marker = scn.frame_current

            # limit OUT marker position with IN marker
            if scn.kr_out_marker < scn.kr_in_marker:
                scn.kr_out_marker = scn.kr_in_marker

            if "OUT" in markers:
                mark = markers["OUT"]
                mark.frame = scn.kr_out_marker
            else:
                mark = markers.new(name="OUT")
                mark.frame = scn.kr_out_marker

        for m in markers:
            m.select = False
            if m.name in {"IN", "OUT"}:
                m.select = True
        bpy.ops.sequencer.reload()
        return {'FINISHED'}


class OBJECT_OT_Setstartend(Operator):  # Operator set start & end
    bl_label = "Set Start and End"
    bl_idname = "sequencerextra.setstartend"
    bl_description = "Set Start and End to IN and OUT marker values"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        markers = scn.timeline_markers
        if "IN" and "OUT" in markers:
            return True
        else:
            return False

    def execute(self, context):
        functions.initSceneProperties(context)
        scn = context.scene
        markers = scn.timeline_markers
        sin = markers["IN"]
        sout = markers["OUT"]
        scn.frame_start = sin.frame
        scn.frame_end = sout.frame - 1
        bpy.ops.sequencer.reload()

        return {'FINISHED'}


# Copy paste

class OBJECT_OT_Metacopy(Operator):  # Operator copy source in/out
    bl_label = "Trim and Meta-Copy"
    bl_idname = "sequencerextra.metacopy"
    bl_description = ("Make meta from selected strips, trim it to in / out\n"
                     "(if available) and copy it to clipboard")

    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            # redo
            scene = bpy.context.scene
            seq = scene.sequence_editor
            markers = scene.timeline_markers
            strip1 = seq.active_strip

            if strip1 is None:
                self.report({'ERROR'}, "No strip selected")
                return {"CANCELLED"}

            if "IN" and "OUT" in markers:
                sin = markers["IN"].frame
                sout = markers["OUT"].frame
                bpy.ops.sequencer.meta_make()
                strip2 = seq.active_strip
                functions.triminout(strip2, sin, sout)
                bpy.ops.sequencer.copy()
                bpy.ops.sequencer.meta_separate()
                self.report({'INFO'}, "META2 has been trimed and copied")
            else:
                bpy.ops.sequencer.meta_make()
                bpy.ops.sequencer.copy()
                bpy.ops.sequencer.meta_separate()
                self.report({'WARNING'}, "No In and Out!! META has been copied")

        except Exception as e:
            functions.error_handlers(self,
                                    "sequencerextra.metacopy", e, "Trim and Meta-Copy")

            return {"CANCELLED"}

        return {'FINISHED'}


class OBJECT_OT_Metapaste(Operator):  # Operator paste source in/out
    bl_label = "Paste in current Frame"
    bl_idname = "sequencerextra.metapaste"
    bl_description = "Paste source from clipboard to current frame"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # redo
        scene = bpy.context.scene
        bpy.ops.sequencer.paste()
        bpy.ops.sequencer.snap(frame=scene.frame_current)
        strips = context.selected_editable_sequences
        context.scene.sequence_editor.active_strip = strips[0]
        context.scene.update()

        return {'FINISHED'}


# Operator paste source in/out
class OBJECT_OT_Unmetatrim(Operator):
    bl_label = "Paste in current Frame"
    bl_idname = "sequencerextra.meta_separate_trim"
    bl_description = "Unmeta and trim the content to meta duration"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            if scn.sequence_editor.active_strip:
                return scn.sequence_editor.active_strip.type == "META"
        else:
            return False

    def execute(self, context):
        scn = context.scene
        seq = scn.sequence_editor
        markers = scn.timeline_markers

        # setting in and out around meta
        # while keeping data to restore in and out positions
        strip = seq.active_strip
        sin = strip.frame_start + strip.frame_offset_start
        sout = sin + strip.frame_final_duration

        borrarin = False
        borrarout = False
        original_in = 0
        original_out = 0

        if "IN" in markers:
            original_in = markers["IN"].frame
            markers["IN"].frame = sin
        else:
            mark = markers.new(name="IN")
            mark.frame = sin
            borrarin = True

        if "OUT" in markers:
            original_out = markers["OUT"].frame
            markers["OUT"].frame = sout
        else:
            mark = markers.new(name="OUT")
            mark.frame = sout
            borrarout = True

        # here starts the operator...

        # get all META from selected strips
        metastrips = []
        for i in context.selected_editable_sequences:
            if i.type == "META":
                metastrips.append(i)

        for meta in metastrips:
            bpy.ops.sequencer.reload()

            # deselect all strips
            for i in context.selected_editable_sequences:
                i.select = False

            # make active current meta
            meta.select = True
            seq.active_strip = meta
            bpy.ops.sequencer.reload()

            # set in and out to meta
            sin = meta.frame_start + meta.frame_offset_start
            sout = sin + meta.frame_final_duration
            # print("meta: ", sin, sout)

            # grab meta content
            newstrips = []
            for i in meta.sequences:
                newstrips.append(i)

            # store meta channel
            basechan = meta.channel
            # look for upper and lower channels used by strips inside the meta
            lowerchan = 32
            upperchan = 0
            for i in newstrips:
                if i.channel < lowerchan:
                    lowerchan = i.channel
                if i.channel > upperchan:
                    upperchan = i.channel

            # calculate channel increment needed
            deltachan = basechan - lowerchan
            # reorder strips inside the meta
            # before separate we need to store channel data
            delta = upperchan - lowerchan + 1
            for i in newstrips:
                i.channel = i.channel + delta
            chandict = {}
            for i in newstrips:
                i.channel = i.channel + deltachan - delta
                chandict[i.name] = i.channel

            """
            for i in chandict:
                print(i, chandict[i])
            """
            # go inside meta to trim strips
            bpy.ops.sequencer.meta_toggle()

            # update seq definition according to meta
            meta_level = len(seq.meta_stack)
            if meta_level > 0:
                seq = seq.meta_stack[meta_level - 1]

            # create a list to store clips outside selection
            # that will be removed
            rmlist = []

            # deselect all separated strips
            for j in newstrips:
                j.select = False
                # print("newstrips: ",j.name, j.type)

            # trim each strip separately
            # first check special strips:
            # (those who can move when any other does)
            for i in newstrips:
                if i.type in {"CROSS", "SPEED", "WIPE"}:
                    i.select = True
                    remove = functions.triminout(i, sin, sout)
                    if remove is True:
                        # print("checked: ",i.name, i.type)
                        rmlist.append(i)
                    i.select = False

            # now for the rest of strips
            for i in newstrips:
                i.select = True
                remove = functions.triminout(i, sin, sout)
                if remove is True:
                    # print("checked: ",i.name, i.type)
                    rmlist.append(i)
                i.select = False

            # back outside the meta and separate it
            bpy.ops.sequencer.meta_toggle()
            bpy.ops.sequencer.meta_separate()

            # reset seq definition
            seq = scn.sequence_editor

            # remove strips from outside the meta duration
            for i in rmlist:
                # print("removing: ",i.name, i.type)
                for j in scn.sequence_editor.sequences_all:
                    j.select = False

                i.select = True
                scn.sequence_editor.active_strip = i
                bpy.ops.sequencer.delete()

            # select all strips and set one of the strips as active
            for i in newstrips:
                if i not in rmlist:
                    i.select = True
                    scn.sequence_editor.active_strip = i

            bpy.ops.sequencer.reload()

            # restore original IN and OUT values
            if borrarin:
                markers.remove(markers['IN'])
            else:
                markers["IN"].frame = original_in
            if borrarout:
                markers.remove(markers['OUT'])
            else:
                markers["OUT"].frame = original_out
            scn.update()

        return {'FINISHED'}


class OBJECT_OT_Extrasnap(Operator):  # Operator paste source in/out
    bl_label = "Extra Snap"
    bl_idname = "sequencerextra.extrasnap"
    bl_description = "Snap the right, center or left of the strip to current frame"
    bl_options = {'REGISTER', 'UNDO'}

    # align: 0 = left snap, 1 = center snap, 2= right snap
    align = IntProperty(
            name="Align",
            min=0, max=2,
            default=1
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.active_strip
        else:
            return False

    def execute(self, context):
        scene = bpy.context.scene
        bpy.ops.sequencer.snap(frame=scene.frame_current)

        if self.align != 0:
            strips = context.selected_editable_sequences
            for strip in strips:
                if self.align == 1:  # center snap
                    strip.frame_start -= strip.frame_final_duration / 2
                else:                # right snap
                    strip.frame_start -= strip.frame_final_duration

        return {'FINISHED'}


class OBJECT_OT_Extrahandles(Operator):  # Operator paste source in/out
    bl_label = "Extra Handles"
    bl_idname = "sequencerextra.extrahandles"
    bl_description = "Snap the right, center or left of the strip to current frame"
    bl_options = {'REGISTER', 'UNDO'}

    # side: 0 = left , 1 = both, 2= right
    side = IntProperty(
            name="Side",
            min=0, max=2,
            default=1
            )

    @classmethod
    def poll(self, context):
        scn = context.scene
        if scn and scn.sequence_editor:
            return scn.sequence_editor.active_strip
        else:
            return False

    def execute(self, context):
        strips = context.selected_editable_sequences

        resetLeft = False
        resetRight = False
        changelistLeft = []
        changelistRight = []

        for strip in strips:
            if self.side == 0 or self.side == 1:
                if strip.select_left_handle:
                    resetLeft = True
                    changelistLeft.append(strip)
            if self.side == 1 or self.side == 2:
                if strip.select_right_handle:
                    resetRight = True
                    changelistRight.append(strip)

        if len(changelistLeft) == len(strips):
            resetLeft = False

        if len(changelistRight) == len(strips):
            resetRight = False

        if ((len(changelistRight) != len(strips)) or
                (len(changelistRight) != len(strips))) and \
                self.side == 1:
            resetLeft = True
            resetRight = True

        for strip in strips:
            if resetLeft:
                strip.select_left_handle = False

            if self.side == 0 or self.side == 1:
                if strip.select_left_handle:
                    strip.select_left_handle = False
                else:
                    strip.select_left_handle = True

            if resetRight:
                strip.select_right_handle = False

            if self.side == 1 or self.side == 2:
                if strip.select_right_handle:
                    strip.select_right_handle = False
                else:
                    strip.select_right_handle = True

        return {'FINISHED'}


@persistent
def marker_handler(scn):
    context = bpy.context
    functions.initSceneProperties(context)

    if scn.kr_auto_markers:
        markers = scn.timeline_markers

        if "IN" in markers:
            mark = markers["IN"]
            mark.frame = scn.kr_in_marker
        else:
            mark = markers.new(name="IN")
            mark.frame = scn.kr_in_marker

        if "OUT" in markers:
            mark = markers["OUT"]
            mark.frame = scn.kr_out_marker
        else:
            mark = markers.new(name="OUT")
            mark.frame = scn.kr_out_marker

        # limit OUT marker position with IN marker
        if scn.kr_in_marker > scn.kr_out_marker:
            scn.kr_out_marker = scn.kr_in_marker

        return {'FINISHED'}
    else:
        return {'CANCELLED'}


bpy.app.handlers.scene_update_post.append(marker_handler)
