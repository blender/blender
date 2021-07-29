# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 3
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

# <pep8-80 compliant>

# This script was developed with financial support from the Foundation for
# Science and Technology of Portugal, under the grant SFRH/BD/66452/2009.


bl_info = {
    "name": "Acclaim Motion Capture Files (.asf, .amc)",
    "author": "Daniel Monteiro Basso <daniel@basso.inf.br>",
    "version": (2013, 1, 26, 1),
    "blender": (2, 65, 9),
    "location": "File > Import-Export",
    "description": "Imports Acclaim Skeleton and Motion Capture Files",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/Acclaim_Importer",
    "category": "Import-Export",
}


import re
import bpy
from mathutils import Vector, Matrix
from math import radians, degrees
from bpy.props import (
        StringProperty,
        BoolProperty,
        FloatProperty,
        IntProperty,
        )


class DataStructure:
    """
        Parse the Skeleton and Motion Files to an internal data structure.
    """
    doc = re.compile(r"(?ms):(\w+)\s+([^:]+)")
    block = re.compile(r"(?ms)begin\s+(.*?)\s+end")
    bonedata = re.compile(r"(?ms)(name|direction|length|axis|dof)\s+(.*?)\s*$"
                          "|limits(\s.*)")

    def __init__(self, file_path, scale=1.):
        self.scale = scale
        source = open(file_path, encoding="utf-8", errors="replace").read()
        sections = dict(DataStructure.doc.findall(source))
        if not sections:
            raise ValueError("Wrong file structure.")

        if 'units' in sections:
            units = dict(u.strip().split()
                for u in sections['units'].splitlines()
                if u.strip())
            if 'length' in units:
                self.scale /= float(units['length'])

        if 'bonedata' not in sections:
            raise ValueError("Bone data section not found.")
        bm = DataStructure.block.findall(sections['bonedata'])
        if not bm:
            raise ValueError("Bone data section malformed.")
        self.bones = {'root': {
            'dof': ['X', 'Y', 'Z'],
            'direction': Vector(),  # should be orientation of root sector
            'length': 1,
            'axis': Matrix(),
            'axis_inv': Matrix(),
            }}
        for b in bm:
            bd = dict((i[0] or 'limits', i[0] and i[1] or i[2])
                        for i in DataStructure.bonedata.findall(b))
            for k in bd:
                s = [t for t in re.split(r"[^a-zA-Z0-9-+.]", bd[k]) if t]
                if k == 'axis':
                    rot = Matrix()
                    for ang, basis in zip(s[:3], s[3].upper()):
                        rot = Matrix.Rotation(radians(float(ang)),
                                              4, basis) * rot
                    bd['axis'] = rot
                elif k == 'direction':
                    bd[k] = Vector([float(n) for n in s])
                elif k == 'length':
                    bd[k] = float(s[0]) * self.scale
                elif k == 'dof':
                    bd[k] = [a[1].upper() for a in s]  # only rotations
                elif k == 'limits':
                    bd[k] = s
            if 'axis' in bd:
                bd['axis_inv'] = bd['axis'].inverted()
            self.bones[bd['name']] = bd

        if 'hierarchy' not in sections:
            raise ValueError("Hierarchy section not found.")
        hm = DataStructure.block.search(sections['hierarchy'])
        if not hm:
            raise ValueError("Hierarchy section malformed.")
        self.hierarchy = {}
        for l in hm.group(1).splitlines():
            t = l.strip().split()
            self.hierarchy[t[0]] = t[1:]

    def scan_motion_capture(self, filename, skip=5):
        """
            Parse an Acclaim Motion Capture file and iterates over the data
        """
        amc = open(filename, encoding="utf-8", errors="replace")
        l = ' '
        while l and not l[0].isdigit():
            l = amc.readline().strip()
        while l:
            frame = int(l)
            bdefs = []
            while True:
                l = amc.readline().strip()
                if not l or l[0].isdigit():
                    break
                bdefs.append(l.split())
            if (frame - 1) % skip != 0:
                continue
            self.pose_def = {}
            for b in bdefs:
                vs = [float(v) for v in b[1:]]
                if b[0] == 'root':
                    loc = Vector(vs[:3]) * self.scale
                    vs = vs[3:]
                rot = Matrix()
                if 'dof' not in self.bones[b[0]]:
                    # If 'dof' isn't defined it probably means the AMC comes
                    # from a different origin than the ASF, such as the
                    # AMC exporter in this package. Assume XYZ order.
                    self.bones[b[0]]['dof'] = ['X', 'Y', 'Z']
                for dof, ang in zip(self.bones[b[0]]['dof'], vs):
                    rot = Matrix.Rotation(radians(ang), 4, dof) * rot
                self.pose_def[b[0]] = rot
            pose = self.calculate_pose(Matrix.Translation(loc))
            yield(frame / skip + 1, pose)
        amc.close()

    def calculate_pose(self, parent, bone='root'):
        """
            Calculate each bone transform iteratively
        """
        bd = self.bones[bone]
        tail = Matrix.Translation(bd['direction'] * bd['length'])
        if bone in self.pose_def:
            tail = bd['axis'] * self.pose_def[bone] * bd['axis_inv'] * tail
        world = parent * tail
        local = tail
        yield(bone, world, local)
        if bone in self.hierarchy:
            for child in self.hierarchy[bone]:
                for b, w, l in self.calculate_pose(world, child):
                    yield(b, w, l)


class StructureBuilder(DataStructure):
    def __init__(self, file_path, name="Skel", scale=1.):
        """
            Setup instance data and load the skeleton
        """
        self.file_path = file_path
        self.name = name
        self.user_def_scale = scale
        DataStructure.__init__(self, file_path, scale)

    def create_armature(self):
        """
            Create the armature and leave it in edit mode
        """
        bpy.context.scene.objects.active = None
        bpy.ops.object.add(type='ARMATURE', enter_editmode=True)
        self.object = bpy.context.scene.objects.active
        self.armature = self.object.data
        self.object.name = self.name
        self.armature.name = self.name
        self.armature.draw_type = 'STICK'
        self.object['source_file_path'] = self.file_path
        self.object['source_scale'] = self.user_def_scale
        self.object['MhxArmature'] = 'Daz'

    def load_armature(self, obj):
        """
            Assign the armature object to be used for loading motion
        """
        self.object = obj

    def build_structure(self, use_limits=False):
        """
            Create the root bone and start the recursion, exit edit mode
        """
        self.use_limits = use_limits
        bpy.ops.armature.bone_primitive_add(name='root')
        root_dir = Vector((0, 0.1 * self.scale, 0))
        bpy.ops.transform.translate(value=root_dir + Vector((.0, .0, -1.0)))
        self.recursive_add_bones()
        bpy.ops.armature.select_all(action='DESELECT')
        bpy.ops.object.mode_set(mode='OBJECT')

    def recursive_add_bones(self, parent_name='root'):
        """
            Traverse the hierarchy creating bones and constraints
        """
        if  parent_name not in self.hierarchy:
            return
        for name in self.hierarchy[parent_name]:
            self.add_bone(name, parent_name)
            if self.use_limits:
                self.add_limit_constraint(name)
            self.recursive_add_bones(name)

    def add_bone(self, name, parent_name):
        """
            Extrude a bone from the specified parent, and configure it
        """
        bone_def = self.bones[name]
        bpy.ops.armature.select_all(action='DESELECT')
        # select tail of parent bone
        self.armature.edit_bones[parent_name].select_tail = True
        # extrude and name the new bone
        bpy.ops.armature.extrude()
        self.armature.edit_bones[-1].name = name
        # translate the tail of the new bone
        tail = bone_def['direction'] * bone_def['length']
        bpy.ops.transform.translate(value=tail)
        # align the bone to the rotation axis
        axis = bone_def['axis'].to_3x3()
        vec = axis * Vector((.0, .0, -1.0))
        self.armature.edit_bones[-1].align_roll(vector=vec)

    def add_limit_constraint(self, name):
        """
            Create the limit rotation constraint of the specified bone
        """
        bpy.ops.object.mode_set(mode='POSE')
        bone_def = self.bones[name]
        dof = bone_def['dof'] if 'dof' in bone_def else ''
        pb = self.object.pose.bones[name]
        self.armature.bones.active = self.armature.bones[name]
        bpy.ops.pose.constraint_add(type='LIMIT_ROTATION')
        constr = pb.constraints[-1]
        constr.owner_space = 'LOCAL'
        constr.use_limit_x = True
        constr.use_limit_y = True
        constr.use_limit_z = True
        if dof:
            limits = (radians(float(v)) for v in bone_def['limits'])
            if 'X' in dof:
                constr.min_x = next(limits)
                constr.max_x = next(limits)
            if 'Y' in dof:
                constr.max_z = -next(limits)
                constr.min_z = -next(limits)
            if 'Z' in dof:
                constr.min_y = next(limits)
                constr.max_y = next(limits)
        bpy.ops.object.mode_set(mode='EDIT')

    def load_motion_capture(self, filename, frame_skip=5, use_frame_no=False):
        """
            Create the keyframes for a motion capture file
        """
        bpy.context.active_object.animation_data_clear()
        bpy.ops.object.mode_set(mode='POSE')
        bpy.ops.pose.select_all(action='SELECT')
        bpy.ops.pose.rot_clear()
        bpy.ops.pose.loc_clear()
        self.rest = {}
        for b in self.object.pose.bones:
            self.rest[b.name] = (b,
                                 b.matrix.to_3x3(),
                                 b.matrix.to_3x3().inverted(),
                                 )
        self.fno = 1  # default Blender scene start frame
        self.use_frame_no = use_frame_no
        self.motion = iter(self.scan_motion_capture(filename, frame_skip))

    def apply_next_frame(self):
        try:
            frame, bones = next(self.motion)
        except StopIteration:
            return False
        regframe = frame if self.use_frame_no else self.fno
        self.fno += 1
        for name, w, l in bones:
            b, P, Pi = self.rest[name]
            if name == 'root':
                b.location = w.to_translation()
                b.keyframe_insert('location', -1, regframe, name)
            T = Pi * l.to_3x3() * P
            b.rotation_quaternion = T.to_quaternion()
            b.keyframe_insert('rotation_quaternion', -1, regframe, name)
        return True


class AsfImporter(bpy.types.Operator):
    #
    "Load an Acclaim Skeleton File"
    #
    bl_idname = "import_anim.asf"
    bl_label = "Import ASF"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    armature_name = StringProperty(
            name="Armature Name", maxlen=63,
            default="Skeleton",
            description="Name of the new object",
            )
    use_limits = BoolProperty(
            name="Use Limits", default=False,
            description="Create bone constraints for limits",
            )
    scale = FloatProperty(
            name="Scale",
            default=1.0,
            description="Scale the armature by this value",
            min=0.0001, max=1000000.0,
            soft_min=0.001, soft_max=100.0,
            )
    from_inches = BoolProperty(
            name="Convert from inches to metric",
            default=False, description="Scale by 2.54/100",
            )
    use_rot_x = BoolProperty(
            name="Rotate X 90 degrees",
            default=False,
            description="Correct orientation",
            )
    use_rot_z = BoolProperty(
            name="Rotate Z 90 degrees",
            default=False,
            description="Correct orientation",
            )

    filter_glob = StringProperty(default="*.asf", options={'HIDDEN'})

    def execute(self, context):
        uscale = (0.0254 if self.from_inches else 1.0)
        sb = StructureBuilder(self.filepath,
                              self.armature_name,
                              self.scale * uscale,
                              )
        sb.create_armature()
        sb.build_structure(self.use_limits)
        if self.use_rot_x:
            bpy.ops.transform.rotate(value=radians(90.0), axis=(1, 0, 0))
        if self.use_rot_z:
            bpy.ops.transform.rotate(value=radians(90.0), axis=(0, 0, 1))
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class AmcAnimator(bpy.types.Operator):
    """
        Load an Acclaim Motion Capture
    """
    bl_idname = "import_anim.amc_animate"
    bl_label = "Animate AMC"

    sb = None
    timer = None

    def modal(self, context, event):
        if event.type == 'ESC':
            self.cancel(context)
            return {'CANCELLED'}
        if event.type == 'TIMER':
            if not self.sb.apply_next_frame():
                self.cancel(context)
                return {'FINISHED'}
        return {'PASS_THROUGH'}

    def execute(self, context):
        self.timer = context.window_manager.\
            event_timer_add(0.001, context.window)
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def cancel(self, context):
        bpy.context.scene.frame_set(bpy.context.scene.frame_current)
        context.window_manager.event_timer_remove(self.timer)
        bpy.ops.object.mode_set(mode='OBJECT')


class AmcImporter(bpy.types.Operator):
    #
    "Load an Acclaim Motion Capture"
    #
    bl_idname = "import_anim.amc"
    bl_label = "Import AMC"

    filepath = StringProperty(
            subtype='FILE_PATH',
            )
    frame_skip = IntProperty(
            name="Fps divisor",
            default=4,
            # usually the sample rate is 120, so the default 4 gives you 30fps
            description="Frame supersampling factor",
            min=1,
            )
    use_frame_no = BoolProperty(
            name="Use frame numbers",
            default=False,
            description="Offset start of animation according to the source",
            )

    filter_glob = StringProperty(default="*.amc", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'ARMATURE' and 'source_file_path' in ob)

    def execute(self, context):
        ob = context.active_object
        sb = StructureBuilder(
            ob['source_file_path'],
            ob.name,
            ob['source_scale'])
        sb.load_armature(ob)
        sb.load_motion_capture(self.filepath,
                               self.frame_skip,
                               self.use_frame_no)
        AmcAnimator.sb = sb
        bpy.ops.import_anim.amc_animate()
        return {'FINISHED'}

    def invoke(self, context, event):
        ob = context.active_object
        import os
        if not os.path.exists(ob['source_file_path']):
            self.report({'ERROR'},
                "Original Armature source file not found... was it moved?")
            return {'CANCELLED'}
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


class AmcExporter(bpy.types.Operator):
    #
    "Save an animation in Acclaim format"
    #
    bl_idname = "export_anim.amc"
    bl_label = "Export AMC"

    filepath = StringProperty(
            subtype='FILE_PATH'
            )
    use_scale = BoolProperty(
            name="Use original armature scale",
            default=True,
            description="Scale movement to original scale if available",
            )

    filter_glob = StringProperty(default="*.amc", options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return (ob and ob.type == 'ARMATURE' and 'source_file_path' in ob)

    def execute(self, context):
        ob = context.active_object
        scn = context.scene
        out = open(self.filepath, "w")
        out.write(":FULLY-SPECIFIED\n:DEGREES\n")
        ds = DataStructure(ob['source_file_path'], ob['source_scale'])
        scale = ds.scale if self.use_scale else 1
        for frame in range(scn.frame_start, scn.frame_end + 1):
            out.write("{}\n".format(frame))
            scn.frame_set(frame)
            for bone in ob.pose.bones:
                out.write("{} ".format(bone.name))
                if bone.name == "root":
                    loc = bone.location / scale
                    out.write(" ".join(str(v) for v in loc) + " ")
                    rot = bone.matrix.to_euler()
                else:
                    A = ds.bones[bone.name]['axis'].to_3x3()
                    R = ob.data.bones[bone.name].matrix_local.to_3x3()
                    AiR = A.transposed() * R
                    AiR_i = AiR.inverted()
                    rot = (AiR * bone.matrix_basis.to_3x3() * AiR_i).to_euler()
                out.write(" ".join(str(degrees(v)) for v in rot) + "\n")
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func_s(self, context):
    self.layout.operator(AsfImporter.bl_idname,
                         text="Acclaim Skeleton File (.asf)")


def menu_func_mi(self, context):
    self.layout.operator(AmcImporter.bl_idname,
                         text="Acclaim Motion Capture (.amc)")


def menu_func_me(self, context):
    self.layout.operator(AmcExporter.bl_idname,
                         text="Acclaim Motion Capture (.amc)")


def register():
    bpy.utils.register_module(__name__)
    bpy.types.INFO_MT_file_import.append(menu_func_s)
    bpy.types.INFO_MT_file_import.append(menu_func_mi)
    bpy.types.INFO_MT_file_export.append(menu_func_me)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_import.remove(menu_func_s)
    bpy.types.INFO_MT_file_import.remove(menu_func_mi)
    bpy.types.INFO_MT_file_export.remove(menu_func_me)


if __name__ == "__main__":
    register()
