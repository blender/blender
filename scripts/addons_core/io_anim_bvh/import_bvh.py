# SPDX-FileCopyrightText: 2011 Campbell Barton
#
# SPDX-License-Identifier: GPL-2.0-or-later

from math import radians, ceil

import bpy
from bpy.app.translations import pgettext_tip as tip_
from mathutils import Vector, Euler, Matrix
from bpy_extras import anim_utils


class BVH_Node:
    __slots__ = (
        # Bvh joint name.
        'name',
        # BVH_Node type or None for no parent.
        'parent',
        # A list of children of this type..
        'children',
        # Worldspace rest location for the head of this node.
        'rest_head_world',
        # Localspace rest location for the head of this node.
        'rest_head_local',
        # Worldspace rest location for the tail of this node.
        'rest_tail_world',
        # Worldspace rest location for the tail of this node.
        'rest_tail_local',
        # List of 6 ints, -1 for an unused channel,
        # otherwise an index for the BVH motion data lines,
        # loc triple then rot triple.
        'channels',
        # A triple of indices as to the order rotation is applied.
        # [0,1,2] is x/y/z - [None, None, None] if no rotation..
        'rot_order',
        # Same as above but a string 'XYZ' format..
        'rot_order_str',
        # A list one tuple's one for each frame: (locx, locy, locz, rotx, roty, rotz),
        # euler rotation ALWAYS stored xyz order, even when native used.
        'anim_data',
        # Convenience function, bool, same as: (channels[0] != -1 or channels[1] != -1 or channels[2] != -1).
        'has_loc',
        # Convenience function, bool, same as: (channels[3] != -1 or channels[4] != -1 or channels[5] != -1).
        'has_rot',
        # Index from the file, not strictly needed but nice to maintain order.
        'index',
        # Use this for whatever you want.
        'temp',
    )

    _eul_order_lookup = {
        (None, None, None): 'XYZ',  # XXX Dummy one, no rotation anyway!
        (0, 1, 2): 'XYZ',
        (0, 2, 1): 'XZY',
        (1, 0, 2): 'YXZ',
        (1, 2, 0): 'YZX',
        (2, 0, 1): 'ZXY',
        (2, 1, 0): 'ZYX',
    }

    def __init__(self, name, rest_head_world, rest_head_local, parent, channels, rot_order, index):
        self.name = name
        self.rest_head_world = rest_head_world
        self.rest_head_local = rest_head_local
        self.rest_tail_world = None
        self.rest_tail_local = None
        self.parent = parent
        self.channels = channels
        self.rot_order = tuple(rot_order)
        self.rot_order_str = BVH_Node._eul_order_lookup[self.rot_order]
        self.index = index

        # convenience functions
        self.has_loc = channels[0] != -1 or channels[1] != -1 or channels[2] != -1
        self.has_rot = channels[3] != -1 or channels[4] != -1 or channels[5] != -1

        self.children = []

        # List of 6 length tuples: (lx, ly, lz, rx, ry, rz)
        # even if the channels aren't used they will just be zero.
        self.anim_data = [(0, 0, 0, 0, 0, 0)]

    def __repr__(self):
        return (
            "BVH name: '%s', rest_loc:(%.3f,%.3f,%.3f), rest_tail:(%.3f,%.3f,%.3f)" % (
                self.name,
                *self.rest_head_world,
                *self.rest_head_world,
            )
        )


def sorted_nodes(bvh_nodes):
    bvh_nodes_list = list(bvh_nodes.values())
    bvh_nodes_list.sort(key=lambda bvh_node: bvh_node.index)
    return bvh_nodes_list


def read_bvh(context, file_path, rotate_mode='XYZ', global_scale=1.0):
    # File loading stuff
    # Open the file for importing
    file = open(file_path, 'r')

    # Separate into a list of lists, each line a list of words.
    file_lines = file.readlines()
    # Non standard carriage returns?
    if len(file_lines) == 1:
        file_lines = file_lines[0].split('\r')

    # Split by whitespace.
    file_lines = [ll for ll in [l.split() for l in file_lines] if ll]

    # Create hierarchy as empties
    if file_lines[0][0].lower() == 'hierarchy':
        # print 'Importing the BVH Hierarchy for:', file_path
        pass
    else:
        raise Exception("This is not a BVH file")

    bvh_nodes = {None: None}
    bvh_nodes_serial = [None]
    bvh_frame_count = None
    bvh_frame_time = None

    channelIndex = -1

    lineIdx = 0  # An index for the file.
    while lineIdx < len(file_lines) - 1:
        if file_lines[lineIdx][0].lower() in {'root', 'joint'}:

            # Join spaces into 1 word with underscores joining it.
            if len(file_lines[lineIdx]) > 2:
                file_lines[lineIdx][1] = '_'.join(file_lines[lineIdx][1:])
                file_lines[lineIdx] = file_lines[lineIdx][:2]

            # MAY NEED TO SUPPORT MULTIPLE ROOTS HERE! Still unsure weather multiple roots are possible?

            # Make sure the names are unique - Object names will match joint names exactly and both will be unique.
            name = file_lines[lineIdx][1]

            # While unlikely, there exists a user report of duplicate joint names, see: #109399.
            if name in bvh_nodes:
                name_orig = name
                name_index = 1
                while (name := "%s.%03d" % (name_orig, name_index)) in bvh_nodes:
                    name_index += 1
                del name_orig, name_index

            # print '%snode: %s, parent: %s' % (len(bvh_nodes_serial) * '  ', name,  bvh_nodes_serial[-1])

            lineIdx += 2  # Increment to the next line (Offset)
            rest_head_local = global_scale * Vector((
                float(file_lines[lineIdx][1]),
                float(file_lines[lineIdx][2]),
                float(file_lines[lineIdx][3]),
            ))
            lineIdx += 1  # Increment to the next line (Channels)

            # `newChannel[Xposition, Yposition, Zposition, Xrotation, Yrotation, Zrotation]`
            # newChannel references indices to the `motiondata`,
            # if not assigned then -1 refers to the last value that will be added
            # on loading at a value of zero, this is appended.
            # We'll add a zero value onto the end of the MotionDATA so this always refers to a value.
            my_channel = [-1, -1, -1, -1, -1, -1]
            my_rot_order = [None, None, None]
            rot_count = 0
            for channel in file_lines[lineIdx][2:]:
                channel = channel.lower()
                channelIndex += 1  # So the index points to the right channel
                if channel == 'xposition':
                    my_channel[0] = channelIndex
                elif channel == 'yposition':
                    my_channel[1] = channelIndex
                elif channel == 'zposition':
                    my_channel[2] = channelIndex

                elif channel == 'xrotation':
                    my_channel[3] = channelIndex
                    my_rot_order[rot_count] = 0
                    rot_count += 1
                elif channel == 'yrotation':
                    my_channel[4] = channelIndex
                    my_rot_order[rot_count] = 1
                    rot_count += 1
                elif channel == 'zrotation':
                    my_channel[5] = channelIndex
                    my_rot_order[rot_count] = 2
                    rot_count += 1

            channels = file_lines[lineIdx][2:]

            my_parent = bvh_nodes_serial[-1]  # account for none

            # Apply the parents offset accumulatively
            if my_parent is None:
                rest_head_world = Vector(rest_head_local)
            else:
                rest_head_world = my_parent.rest_head_world + rest_head_local

            bvh_node = bvh_nodes[name] = BVH_Node(
                name,
                rest_head_world,
                rest_head_local,
                my_parent,
                my_channel,
                my_rot_order,
                len(bvh_nodes) - 1,
            )

            # If we have another child then we can call ourselves a parent, else
            bvh_nodes_serial.append(bvh_node)

        # Account for an end node.
        # There is sometimes a name after 'End Site' but we will ignore it.
        if file_lines[lineIdx][0].lower() == 'end' and file_lines[lineIdx][1].lower() == 'site':
            # Increment to the next line (Offset)
            lineIdx += 2
            rest_tail = global_scale * Vector((
                float(file_lines[lineIdx][1]),
                float(file_lines[lineIdx][2]),
                float(file_lines[lineIdx][3]),
            ))

            bvh_nodes_serial[-1].rest_tail_world = bvh_nodes_serial[-1].rest_head_world + rest_tail
            bvh_nodes_serial[-1].rest_tail_local = bvh_nodes_serial[-1].rest_head_local + rest_tail

            # Just so we can remove the parents in a uniform way,
            # the end has kids so this is a placeholder.
            bvh_nodes_serial.append(None)

        if len(file_lines[lineIdx]) == 1 and file_lines[lineIdx][0] == '}':  # == ['}']
            bvh_nodes_serial.pop()  # Remove the last item

        # End of the hierarchy. Begin the animation section of the file with
        # the following header.
        #  MOTION
        #  Frames: n
        #  Frame Time: dt
        if len(file_lines[lineIdx]) == 1 and file_lines[lineIdx][0].lower() == 'motion':
            lineIdx += 1  # Read frame count.
            if (
                    len(file_lines[lineIdx]) == 2 and
                    file_lines[lineIdx][0].lower() == 'frames:'
            ):
                bvh_frame_count = int(file_lines[lineIdx][1])

            lineIdx += 1  # Read frame rate.
            if (
                    len(file_lines[lineIdx]) == 3 and
                    file_lines[lineIdx][0].lower() == 'frame' and
                    file_lines[lineIdx][1].lower() == 'time:'
            ):
                bvh_frame_time = float(file_lines[lineIdx][2])

            lineIdx += 1  # Set the cursor to the first frame

            break

        lineIdx += 1

    # Remove the None value used for easy parent reference
    del bvh_nodes[None]
    # Don't use anymore
    del bvh_nodes_serial

    # importing world with any order but nicer to maintain order
    # second life expects it, which isn't to spec.
    bvh_nodes_list = sorted_nodes(bvh_nodes)

    while lineIdx < len(file_lines):
        line = file_lines[lineIdx]
        for bvh_node in bvh_nodes_list:
            # for bvh_node in bvh_nodes_serial:
            lx = ly = lz = rx = ry = rz = 0.0
            channels = bvh_node.channels
            anim_data = bvh_node.anim_data
            if channels[0] != -1:
                lx = global_scale * float(line[channels[0]])

            if channels[1] != -1:
                ly = global_scale * float(line[channels[1]])

            if channels[2] != -1:
                lz = global_scale * float(line[channels[2]])

            if channels[3] != -1 or channels[4] != -1 or channels[5] != -1:

                rx = radians(float(line[channels[3]]))
                ry = radians(float(line[channels[4]]))
                rz = radians(float(line[channels[5]]))

            # Done importing motion data #
            anim_data.append((lx, ly, lz, rx, ry, rz))
        lineIdx += 1

    # Assign children
    for bvh_node in bvh_nodes_list:
        bvh_node_parent = bvh_node.parent
        if bvh_node_parent:
            bvh_node_parent.children.append(bvh_node)

    # Now set the tip of each bvh_node
    for bvh_node in bvh_nodes_list:

        if not bvh_node.rest_tail_world:
            if len(bvh_node.children) == 0:
                # could just fail here, but rare BVH files have childless nodes
                bvh_node.rest_tail_world = Vector(bvh_node.rest_head_world)
                bvh_node.rest_tail_local = Vector(bvh_node.rest_head_local)
            elif len(bvh_node.children) == 1:
                bvh_node.rest_tail_world = Vector(bvh_node.children[0].rest_head_world)
                bvh_node.rest_tail_local = bvh_node.rest_head_local + bvh_node.children[0].rest_head_local
            else:
                # allow this, see above
                # if not bvh_node.children:
                #     raise Exception("bvh node has no end and no children. bad file")

                # Removed temp for now
                rest_tail_world = Vector((0.0, 0.0, 0.0))
                rest_tail_local = Vector((0.0, 0.0, 0.0))
                for bvh_node_child in bvh_node.children:
                    rest_tail_world += bvh_node_child.rest_head_world
                    rest_tail_local += bvh_node_child.rest_head_local

                bvh_node.rest_tail_world = rest_tail_world * (1.0 / len(bvh_node.children))
                bvh_node.rest_tail_local = bvh_node.rest_head_local + (rest_tail_local * (1.0 / len(bvh_node.children)))

        # Make sure tail isn't the same location as the head.
        if (bvh_node.rest_tail_local - bvh_node.rest_head_local).length <= 0.001 * global_scale:
            print("\tzero length node found:", bvh_node.name)
            bvh_node.rest_tail_local.y = bvh_node.rest_tail_local.y + global_scale / 10
            bvh_node.rest_tail_world.y = bvh_node.rest_tail_world.y + global_scale / 10

    return bvh_nodes, bvh_frame_time, bvh_frame_count


def bvh_node_dict2objects(context, bvh_name, bvh_nodes, rotate_mode='NATIVE', frame_start=1, IMPORT_LOOP=False):

    if frame_start < 1:
        frame_start = 1

    scene = context.scene
    for obj in scene.objects:
        obj.select_set(False)

    objects = []

    def add_ob(name):
        obj = bpy.data.objects.new(name, None)
        context.collection.objects.link(obj)
        objects.append(obj)
        obj.select_set(True)

        # nicer drawing.
        obj.empty_display_type = 'CUBE'
        obj.empty_display_size = 0.1

        return obj

    # Add objects
    for name, bvh_node in bvh_nodes.items():
        bvh_node.temp = add_ob(name)
        bvh_node.temp.rotation_mode = bvh_node.rot_order_str[::-1]

    # Parent the objects
    for bvh_node in bvh_nodes.values():
        for bvh_node_child in bvh_node.children:
            bvh_node_child.temp.parent = bvh_node.temp

    # Offset
    for bvh_node in bvh_nodes.values():
        # Make relative to parents offset
        bvh_node.temp.location = bvh_node.rest_head_local

    # Add tail objects
    for name, bvh_node in bvh_nodes.items():
        if not bvh_node.children:
            ob_end = add_ob(name + '_end')
            ob_end.parent = bvh_node.temp
            ob_end.location = bvh_node.rest_tail_world - bvh_node.rest_head_world

    for name, bvh_node in bvh_nodes.items():
        obj = bvh_node.temp

        for frame_current in range(len(bvh_node.anim_data)):

            lx, ly, lz, rx, ry, rz = bvh_node.anim_data[frame_current]

            if bvh_node.has_loc:
                obj.delta_location = Vector((lx, ly, lz)) - bvh_node.rest_head_world
                obj.keyframe_insert("delta_location", index=-1, frame=frame_start + frame_current)

            if bvh_node.has_rot:
                obj.delta_rotation_euler = rx, ry, rz
                obj.keyframe_insert("delta_rotation_euler", index=-1, frame=frame_start + frame_current)

    return objects


def bvh_node_dict2armature(
        context,
        bvh_name,
        bvh_nodes,
        bvh_frame_time,
        rotate_mode='XYZ',
        frame_start=1,
        IMPORT_LOOP=False,
        global_matrix=None,
        use_fps_scale=False,
):
    from bpy.utils import escape_identifier

    if frame_start < 1:
        frame_start = 1

    # Add the new armature,
    scene = context.scene
    for obj in scene.objects:
        obj.select_set(False)

    arm_data = bpy.data.armatures.new(bvh_name)
    arm_ob = bpy.data.objects.new(bvh_name, arm_data)

    context.collection.objects.link(arm_ob)

    arm_ob.select_set(True)
    context.view_layer.objects.active = arm_ob

    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    bvh_nodes_list = sorted_nodes(bvh_nodes)

    # Get the average bone length for zero length bones, we may not use this.
    average_bone_length = 0.0
    nonzero_count = 0
    for bvh_node in bvh_nodes_list:
        l = (bvh_node.rest_head_local - bvh_node.rest_tail_local).length
        if l:
            average_bone_length += l
            nonzero_count += 1

    # Very rare cases all bones could be zero length???
    if not average_bone_length:
        average_bone_length = 0.1
    else:
        # Normal operation
        average_bone_length = average_bone_length / nonzero_count

    # XXX, annoying, remove bone.
    while arm_data.edit_bones:
        arm_ob.edit_bones.remove(arm_data.edit_bones[-1])

    ZERO_AREA_BONES = []
    for bvh_node in bvh_nodes_list:

        # New editbone
        bone = bvh_node.temp = arm_data.edit_bones.new(bvh_node.name)

        bone.head = bvh_node.rest_head_world
        bone.tail = bvh_node.rest_tail_world

        # Zero Length Bones! (an exceptional case)
        if (bone.head - bone.tail).length < 0.001:
            print("\tzero length bone found:", bone.name)
            if bvh_node.parent:
                ofs = bvh_node.parent.rest_head_local - bvh_node.parent.rest_tail_local
                if ofs.length:  # is our parent zero length also?? unlikely
                    bone.tail = bone.tail - ofs
                else:
                    bone.tail.y = bone.tail.y + average_bone_length
            else:
                bone.tail.y = bone.tail.y + average_bone_length

            ZERO_AREA_BONES.append(bone.name)

    for bvh_node in bvh_nodes_list:
        if bvh_node.parent:
            # bvh_node.temp is the Editbone

            # Set the bone parent
            bvh_node.temp.parent = bvh_node.parent.temp

            # Set the connection state
            if (
                    (not bvh_node.has_loc) and
                    (bvh_node.parent.temp.name not in ZERO_AREA_BONES) and
                    (bvh_node.parent.rest_tail_local == bvh_node.rest_head_local)
            ):
                bvh_node.temp.use_connect = True

    # Replace the editbone with the editbone name,
    # to avoid memory errors accessing the editbone outside editmode
    for bvh_node in bvh_nodes_list:
        bvh_node.temp = bvh_node.temp.name

    # Now Apply the animation to the armature

    # Get armature animation data
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    pose = arm_ob.pose
    pose_bones = pose.bones

    if rotate_mode == 'NATIVE':
        for bvh_node in bvh_nodes_list:
            bone_name = bvh_node.temp  # may not be the same name as the bvh_node, could have been shortened.
            pose_bone = pose_bones[bone_name]
            pose_bone.rotation_mode = bvh_node.rot_order_str

    elif rotate_mode != 'QUATERNION':
        for pose_bone in pose_bones:
            pose_bone.rotation_mode = rotate_mode
    else:
        # Quats default
        pass

    context.view_layer.update()

    arm_ob_adt = arm_ob.animation_data_create()
    action = bpy.data.actions.new(name=bvh_name)
    # Always use the same name for the slot. The Armature is named after the
    # BVH file, and so when importing multiple files to get multiple Actions
    # for a single character, it is likely that all but one of the Armatures
    # is going to be deleted again. It should be simple to switch between
    # imported Actions while keeping Slot auto-assignment, which means that
    # all Actions should use the same slot name.
    action_slot = action.slots.new(arm_ob.id_type, "Slot")
    channelbag = anim_utils.action_ensure_channelbag_for_slot(action, action_slot)

    arm_ob_adt.action = action
    arm_ob_adt.action_slot = action_slot

    # Replace the bvh_node.temp (currently an editbone)
    # With a tuple  (pose_bone, armature_bone, bone_rest_matrix, bone_rest_matrix_inv)
    num_frame = 0
    for bvh_node in bvh_nodes_list:
        bone_name = bvh_node.temp  # may not be the same name as the bvh_node, could have been shortened.
        pose_bone = pose_bones[bone_name]
        rest_bone = arm_data.bones[bone_name]
        bone_rest_matrix = rest_bone.matrix_local.to_3x3()

        bone_rest_matrix_inv = Matrix(bone_rest_matrix)
        bone_rest_matrix_inv.invert()

        bone_rest_matrix_inv.resize_4x4()
        bone_rest_matrix.resize_4x4()
        bvh_node.temp = (pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv)

        if 0 == num_frame:
            num_frame = len(bvh_node.anim_data)

    # Choose to skip some frames at the beginning. Frame 0 is the rest pose
    # used internally by this importer. Frame 1, by convention, is also often
    # the rest pose of the skeleton exported by the motion capture system.
    skip_frame = 1
    if num_frame > skip_frame:
        num_frame = num_frame - skip_frame

    # Create a shared time axis for all animation curves.
    time = [float(frame_start)] * num_frame
    if use_fps_scale:
        dt = scene.render.fps * bvh_frame_time
        for frame_i in range(1, num_frame):
            time[frame_i] += float(frame_i) * dt
    else:
        for frame_i in range(1, num_frame):
            time[frame_i] += float(frame_i)

    # print("bvh_frame_time = %f, dt = %f, num_frame = %d"
    #      % (bvh_frame_time, dt, num_frame]))

    for i, bvh_node in enumerate(bvh_nodes_list):
        pose_bone, bone, bone_rest_matrix, bone_rest_matrix_inv = bvh_node.temp

        if bvh_node.has_loc:
            # Not sure if there is a way to query this or access it in the
            # PoseBone structure.
            data_path = 'pose.bones["%s"].location' % escape_identifier(pose_bone.name)

            location = [(0.0, 0.0, 0.0)] * num_frame
            for frame_i in range(num_frame):
                bvh_loc = bvh_node.anim_data[frame_i + skip_frame][:3]

                bone_translate_matrix = Matrix.Translation(
                    Vector(bvh_loc) - bvh_node.rest_head_local)
                location[frame_i] = (bone_rest_matrix_inv @
                                     bone_translate_matrix).to_translation()

            # For each location x, y, z.
            for axis_i in range(3):
                curve = channelbag.fcurves.new(data_path=data_path, index=axis_i, group_name=bvh_node.name)
                keyframe_points = curve.keyframe_points
                keyframe_points.add(num_frame)

                for frame_i in range(num_frame):
                    keyframe_points[frame_i].co = (
                        time[frame_i],
                        location[frame_i][axis_i],
                    )

        if bvh_node.has_rot:
            data_path = None
            rotate = None

            if 'QUATERNION' == rotate_mode:
                rotate = [(1.0, 0.0, 0.0, 0.0)] * num_frame
                data_path = ('pose.bones["%s"].rotation_quaternion' % escape_identifier(pose_bone.name))
            else:
                rotate = [(0.0, 0.0, 0.0)] * num_frame
                data_path = ('pose.bones["%s"].rotation_euler' % escape_identifier(pose_bone.name))

            prev_euler = Euler((0.0, 0.0, 0.0))
            for frame_i in range(num_frame):
                bvh_rot = bvh_node.anim_data[frame_i + skip_frame][3:]

                # apply rotation order and convert to XYZ
                # note that the rot_order_str is reversed.
                euler = Euler(bvh_rot, bvh_node.rot_order_str[::-1])
                bone_rotation_matrix = euler.to_matrix().to_4x4()
                bone_rotation_matrix = (
                    bone_rest_matrix_inv @
                    bone_rotation_matrix @
                    bone_rest_matrix
                )

                if len(rotate[frame_i]) == 4:
                    rotate[frame_i] = bone_rotation_matrix.to_quaternion()
                else:
                    rotate[frame_i] = bone_rotation_matrix.to_euler(
                        pose_bone.rotation_mode, prev_euler)
                    prev_euler = rotate[frame_i]

            # For each euler angle x, y, z (or quaternion w, x, y, z).
            for axis_i in range(len(rotate[0])):
                curve = channelbag.fcurves.new(data_path=data_path, index=axis_i, group_name=bvh_node.name)
                keyframe_points = curve.keyframe_points
                keyframe_points.add(num_frame)

                for frame_i in range(num_frame):
                    keyframe_points[frame_i].co = (
                        time[frame_i],
                        rotate[frame_i][axis_i],
                    )

    for cu in channelbag.fcurves:
        if IMPORT_LOOP:
            pass  # 2.5 doenst have cyclic now?

        for bez in cu.keyframe_points:
            bez.interpolation = 'LINEAR'

    # finally apply matrix
    arm_ob.matrix_world = global_matrix
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

    return arm_ob


def load(
        context,
        filepath,
        *,
        target='ARMATURE',
        rotate_mode='NATIVE',
        global_scale=1.0,
        use_cyclic=False,
        frame_start=1,
        global_matrix=None,
        use_fps_scale=False,
        update_scene_fps=False,
        update_scene_duration=False,
        report=print,
):
    import time
    t1 = time.time()
    print("\tparsing bvh %r..." % filepath, end="")

    bvh_nodes, bvh_frame_time, bvh_frame_count = read_bvh(
        context, filepath,
        rotate_mode=rotate_mode,
        global_scale=global_scale,
    )

    print("%.4f" % (time.time() - t1))

    scene = context.scene
    frame_orig = scene.frame_current

    # Broken BVH handling: guess frame rate when it is not contained in the file.
    if bvh_frame_time is None:
        report(
            {'WARNING'},
            "The BVH file does not contain frame duration in its MOTION "
            "section, assuming the BVH and Blender scene have the same "
            "frame rate"
        )
        bvh_frame_time = scene.render.fps_base / scene.render.fps
        # No need to scale the frame rate, as they're equal now anyway.
        use_fps_scale = False

    if update_scene_fps:
        _update_scene_fps(context, report, bvh_frame_time)

        # Now that we have a 1-to-1 mapping of Blender frames and BVH frames, there is no need
        # to scale the FPS any more. It's even better not to, to prevent roundoff errors.
        use_fps_scale = False

    if update_scene_duration:
        _update_scene_duration(context, report, bvh_frame_count, bvh_frame_time, frame_start, use_fps_scale)

    t1 = time.time()
    print("\timporting to blender...", end="")

    bvh_name = bpy.path.display_name_from_filepath(filepath)

    if target == 'ARMATURE':
        bvh_node_dict2armature(
            context, bvh_name, bvh_nodes, bvh_frame_time,
            rotate_mode=rotate_mode,
            frame_start=frame_start,
            IMPORT_LOOP=use_cyclic,
            global_matrix=global_matrix,
            use_fps_scale=use_fps_scale,
        )

    elif target == 'OBJECT':
        bvh_node_dict2objects(
            context, bvh_name, bvh_nodes,
            rotate_mode=rotate_mode,
            frame_start=frame_start,
            IMPORT_LOOP=use_cyclic,
            # global_matrix=global_matrix,  # TODO
        )

    else:
        report({'ERROR'}, tip_("Invalid target %r (must be 'ARMATURE' or 'OBJECT')") % target)
        return {'CANCELLED'}

    print('Done in %.4f\n' % (time.time() - t1))

    context.scene.frame_set(frame_orig)

    return {'FINISHED'}


def _update_scene_fps(context, report, bvh_frame_time):
    """Update the scene's FPS settings from the BVH, but only if the BVH contains enough info."""

    # Broken BVH handling: prevent division by zero.
    if bvh_frame_time == 0.0:
        report(
            {'WARNING'},
            "Unable to update scene frame rate, as the BVH file "
            "contains a zero frame duration in its MOTION section",
        )
        return

    scene = context.scene
    scene_fps = scene.render.fps / scene.render.fps_base
    new_fps = 1.0 / bvh_frame_time

    if scene.render.fps != new_fps or scene.render.fps_base != 1.0:
        print("\tupdating scene FPS (was %f) to BVH FPS (%f)" % (scene_fps, new_fps))
    scene.render.fps = int(round(new_fps))
    scene.render.fps_base = scene.render.fps / new_fps


def _update_scene_duration(
        context, report, bvh_frame_count, bvh_frame_time, frame_start,
        use_fps_scale):
    """Extend the scene's duration so that the BVH file fits in its entirety."""

    if bvh_frame_count is None:
        report(
            {'WARNING'},
            "Unable to extend the scene duration, as the BVH file does not "
            "contain the number of frames in its MOTION section",
        )
        return

    # Not likely, but it can happen when a BVH is just used to store an armature.
    if bvh_frame_count == 0:
        return

    if use_fps_scale:
        scene_fps = context.scene.render.fps / context.scene.render.fps_base
        scaled_frame_count = int(ceil(bvh_frame_count * bvh_frame_time * scene_fps))
        bvh_last_frame = frame_start + scaled_frame_count
    else:
        bvh_last_frame = frame_start + bvh_frame_count

    # Only extend the scene, never shorten it.
    if context.scene.frame_end < bvh_last_frame:
        context.scene.frame_end = bvh_last_frame
