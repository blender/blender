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

bl_info = {
    "name": "Copy Attributes Menu",
    "author": "Bassam Kurdali, Fabian Fricke, Adam Wiseman",
    "version": (0, 4, 8),
    "blender": (2, 63, 0),
    "location": "View3D > Ctrl-C",
    "description": "Copy Attributes Menu from Blender 2.4",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/3D_interaction/Copy_Attributes_Menu",
    "category": "3D View",
}

import bpy
from mathutils import Matrix
from bpy.types import (
        Operator,
        Menu,
        )
from bpy.props import (
        BoolVectorProperty,
        StringProperty,
        )

# First part of the operator Info message
INFO_MESSAGE = "Copy Attributes: "


def build_exec(loopfunc, func):
    """Generator function that returns exec functions for operators """

    def exec_func(self, context):
        loopfunc(self, context, func)
        return {'FINISHED'}
    return exec_func


def build_invoke(loopfunc, func):
    """Generator function that returns invoke functions for operators"""

    def invoke_func(self, context, event):
        loopfunc(self, context, func)
        return {'FINISHED'}
    return invoke_func


def build_op(idname, label, description, fpoll, fexec, finvoke):
    """Generator function that returns the basic operator"""

    class myopic(Operator):
        bl_idname = idname
        bl_label = label
        bl_description = description
        execute = fexec
        poll = fpoll
        invoke = finvoke
    return myopic


def genops(copylist, oplist, prefix, poll_func, loopfunc):
    """Generate ops from the copy list and its associated functions"""
    for op in copylist:
        exec_func = build_exec(loopfunc, op[3])
        invoke_func = build_invoke(loopfunc, op[3])
        opclass = build_op(prefix + op[0], "Copy " + op[1], op[2],
           poll_func, exec_func, invoke_func)
        oplist.append(opclass)


def generic_copy(source, target, string=""):
    """Copy attributes from source to target that have string in them"""
    for attr in dir(source):
        if attr.find(string) > -1:
            try:
                setattr(target, attr, getattr(source, attr))
            except:
                pass
    return


def getmat(bone, active, context, ignoreparent):
    """Helper function for visual transform copy,
       gets the active transform in bone space
    """
    obj_act = context.active_object
    data_bone = obj_act.data.bones[bone.name]
    # all matrices are in armature space unless commented otherwise
    otherloc = active.matrix  # final 4x4 mat of target, location.
    bonemat_local = data_bone.matrix_local.copy()  # self rest matrix
    if data_bone.parent:
        parentposemat = obj_act.pose.bones[data_bone.parent.name].matrix.copy()
        parentbonemat = data_bone.parent.matrix_local.copy()
    else:
        parentposemat = parentbonemat = Matrix()
    if parentbonemat == parentposemat or ignoreparent:
        newmat = bonemat_local.inverted() * otherloc
    else:
        bonemat = parentbonemat.inverted() * bonemat_local

        newmat = bonemat.inverted() * parentposemat.inverted() * otherloc
    return newmat


def rotcopy(item, mat):
    """Copy rotation to item from matrix mat depending on item.rotation_mode"""
    if item.rotation_mode == 'QUATERNION':
        item.rotation_quaternion = mat.to_3x3().to_quaternion()
    elif item.rotation_mode == 'AXIS_ANGLE':
        rot = mat.to_3x3().to_quaternion().to_axis_angle()    # returns (Vector((x, y, z)), w)
        axis_angle = rot[1], rot[0][0], rot[0][1], rot[0][2]  # convert to w, x, y, z
        item.rotation_axis_angle = axis_angle
    else:
        item.rotation_euler = mat.to_3x3().to_euler(item.rotation_mode)


def pLoopExec(self, context, funk):
    """Loop over selected bones and execute funk on them"""
    active = context.active_pose_bone
    selected = context.selected_pose_bones
    selected.remove(active)
    for bone in selected:
        funk(bone, active, context)


# The following functions are used o copy attributes frome active to bone

def pLocLocExec(bone, active, context):
    bone.location = active.location


def pLocRotExec(bone, active, context):
    rotcopy(bone, active.matrix_basis.to_3x3())


def pLocScaExec(bone, active, context):
    bone.scale = active.scale


def pVisLocExec(bone, active, context):
    bone.location = getmat(bone, active, context, False).to_translation()


def pVisRotExec(bone, active, context):
    rotcopy(bone, getmat(bone, active,
      context, not context.active_object.data.bones[bone.name].use_inherit_rotation))


def pVisScaExec(bone, active, context):
    bone.scale = getmat(bone, active, context,
       not context.active_object.data.bones[bone.name].use_inherit_scale)\
          .to_scale()


def pDrwExec(bone, active, context):
    bone.custom_shape = active.custom_shape
    bone.use_custom_shape_bone_size = active.use_custom_shape_bone_size
    bone.custom_shape_scale = active.custom_shape_scale
    bone.bone.show_wire = active.bone.show_wire


def pLokExec(bone, active, context):
    for index, state in enumerate(active.lock_location):
        bone.lock_location[index] = state
    for index, state in enumerate(active.lock_rotation):
        bone.lock_rotation[index] = state
    bone.lock_rotations_4d = active.lock_rotations_4d
    bone.lock_rotation_w = active.lock_rotation_w
    for index, state in enumerate(active.lock_scale):
        bone.lock_scale[index] = state


def pConExec(bone, active, context):
    for old_constraint in active.constraints.values():
        new_constraint = bone.constraints.new(old_constraint.type)
        generic_copy(old_constraint, new_constraint)


def pIKsExec(bone, active, context):
    generic_copy(active, bone, "ik_")


def pBBonesExec(bone, active, context):
    object = active.id_data
    generic_copy(
        object.data.bones[active.name],
        object.data.bones[bone.name],
        "bbone_")


pose_copies = (
        ('pose_loc_loc', "Local Location",
        "Copy Location from Active to Selected", pLocLocExec),
        ('pose_loc_rot', "Local Rotation",
        "Copy Rotation from Active to Selected", pLocRotExec),
        ('pose_loc_sca', "Local Scale",
        "Copy Scale from Active to Selected", pLocScaExec),
        ('pose_vis_loc', "Visual Location",
        "Copy Location from Active to Selected", pVisLocExec),
        ('pose_vis_rot', "Visual Rotation",
        "Copy Rotation from Active to Selected", pVisRotExec),
        ('pose_vis_sca', "Visual Scale",
        "Copy Scale from Active to Selected", pVisScaExec),
        ('pose_drw', "Bone Shape",
        "Copy Bone Shape from Active to Selected", pDrwExec),
        ('pose_lok', "Protected Transform",
        "Copy Protected Tranforms from Active to Selected", pLokExec),
        ('pose_con', "Bone Constraints",
        "Copy Object Constraints from Active to Selected", pConExec),
        ('pose_iks', "IK Limits",
        "Copy IK Limits from Active to Selected", pIKsExec),
        ('bbone_settings', "BBone Settings",
        "Copy BBone Settings from Active to Selected", pBBonesExec),
        )


@classmethod
def pose_poll_func(cls, context):
    return(context.mode == 'POSE')


def pose_invoke_func(self, context, event):
    wm = context.window_manager
    wm.invoke_props_dialog(self)
    return {'RUNNING_MODAL'}


class CopySelectedPoseConstraints(Operator):
    """Copy Chosen constraints from active to selected"""
    bl_idname = "pose.copy_selected_constraints"
    bl_label = "Copy Selected Constraints"

    selection = BoolVectorProperty(
            size=32,
            options={'SKIP_SAVE'}
            )

    poll = pose_poll_func
    invoke = pose_invoke_func

    def draw(self, context):
        layout = self.layout
        for idx, const in enumerate(context.active_pose_bone.constraints):
            layout.prop(self, "selection", index=idx, text=const.name,
               toggle=True)

    def execute(self, context):
        active = context.active_pose_bone
        selected = context.selected_pose_bones[:]
        selected.remove(active)
        for bone in selected:
            for index, flag in enumerate(self.selection):
                if flag:
                    old_constraint = active.constraints[index]
                    new_constraint = bone.constraints.new(
                                        active.constraints[index].type
                                        )
                    generic_copy(old_constraint, new_constraint)
        return {'FINISHED'}


pose_ops = []  # list of pose mode copy operators
genops(pose_copies, pose_ops, "pose.copy_", pose_poll_func, pLoopExec)


class VIEW3D_MT_posecopypopup(Menu):
    bl_label = "Copy Attributes"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("view3d.copybuffer", icon="COPY_ID")
        for op in pose_copies:
            layout.operator("pose.copy_" + op[0])
        layout.operator("pose.copy_selected_constraints")
        layout.operator("pose.copy", text="copy pose")


def obLoopExec(self, context, funk):
    """Loop over selected objects and execute funk on them"""
    active = context.active_object
    selected = context.selected_objects[:]
    selected.remove(active)
    for obj in selected:
        msg = funk(obj, active, context)
    if msg:
        self.report({msg[0]}, INFO_MESSAGE + msg[1])


def world_to_basis(active, ob, context):
    """put world coords of active as basis coords of ob"""
    local = ob.parent.matrix_world.inverted() * active.matrix_world
    P = ob.matrix_basis * ob.matrix_local.inverted()
    mat = P * local
    return(mat)


# The following functions are used o copy attributes from
# active to selected object

def obLoc(ob, active, context):
    ob.location = active.location


def obRot(ob, active, context):
    rotcopy(ob, active.matrix_local.to_3x3())


def obSca(ob, active, context):
    ob.scale = active.scale


def obVisLoc(ob, active, context):
    if ob.parent:
        mat = world_to_basis(active, ob, context)
        ob.location = mat.to_translation()
    else:
        ob.location = active.matrix_world.to_translation()
    return('INFO', "Object location copied")


def obVisRot(ob, active, context):
    if ob.parent:
        mat = world_to_basis(active, ob, context)
        rotcopy(ob, mat.to_3x3())
    else:
        rotcopy(ob, active.matrix_world.to_3x3())
    return('INFO', "Object rotation copied")


def obVisSca(ob, active, context):
    if ob.parent:
        mat = world_to_basis(active, ob, context)
        ob.scale = mat.to_scale()
    else:
        ob.scale = active.matrix_world.to_scale()
    return('INFO', "Object scale copied")


def obDrw(ob, active, context):
    ob.draw_type = active.draw_type
    ob.show_axis = active.show_axis
    ob.show_bounds = active.show_bounds
    ob.draw_bounds_type = active.draw_bounds_type
    ob.show_name = active.show_name
    ob.show_texture_space = active.show_texture_space
    ob.show_transparent = active.show_transparent
    ob.show_wire = active.show_wire
    ob.show_x_ray = active.show_x_ray
    ob.empty_draw_type = active.empty_draw_type
    ob.empty_draw_size = active.empty_draw_size


def obOfs(ob, active, context):
    ob.time_offset = active.time_offset
    return('INFO', "Time offset copied")


def obDup(ob, active, context):
    generic_copy(active, ob, "dupli")
    return('INFO', "Duplication method copied")


def obCol(ob, active, context):
    ob.color = active.color


def obMas(ob, active, context):
    ob.game.mass = active.game.mass
    return('INFO', "Mass copied")


def obLok(ob, active, context):
    for index, state in enumerate(active.lock_location):
        ob.lock_location[index] = state
    for index, state in enumerate(active.lock_rotation):
        ob.lock_rotation[index] = state
    ob.lock_rotations_4d = active.lock_rotations_4d
    ob.lock_rotation_w = active.lock_rotation_w
    for index, state in enumerate(active.lock_scale):
        ob.lock_scale[index] = state
    return('INFO', "Transform locks copied")


def obCon(ob, active, context):
    # for consistency with 2.49, delete old constraints first
    for removeconst in ob.constraints:
        ob.constraints.remove(removeconst)
    for old_constraint in active.constraints.values():
        new_constraint = ob.constraints.new(old_constraint.type)
        generic_copy(old_constraint, new_constraint)
    return('INFO', "Constraints copied")


def obTex(ob, active, context):
    if 'texspace_location' in dir(ob.data) and 'texspace_location' in dir(
       active.data):
        ob.data.texspace_location[:] = active.data.texspace_location[:]
    if 'texspace_size' in dir(ob.data) and 'texspace_size' in dir(active.data):
        ob.data.texspace_size[:] = active.data.texspace_size[:]
    return('INFO', "Texture space copied")


def obIdx(ob, active, context):
    ob.pass_index = active.pass_index
    return('INFO', "Pass index copied")


def obMod(ob, active, context):
    for modifier in ob.modifiers:
        # remove existing before adding new:
        ob.modifiers.remove(modifier)
    for old_modifier in active.modifiers.values():
        new_modifier = ob.modifiers.new(name=old_modifier.name,
           type=old_modifier.type)
        generic_copy(old_modifier, new_modifier)
    return('INFO', "Modifiers copied")


def obGrp(ob, active, context):
    for grp in bpy.data.groups:
        if active.name in grp.objects and ob.name not in grp.objects:
            grp.objects.link(ob)
    return('INFO', "Groups copied")


def obWei(ob, active, context):
    me_source = active.data
    me_target = ob.data
    # sanity check: do source and target have the same amount of verts?
    if len(me_source.vertices) != len(me_target.vertices):
        return('ERROR', "objects have different vertex counts, doing nothing")
    vgroups_IndexName = {}
    for i in range(0, len(active.vertex_groups)):
        groups = active.vertex_groups[i]
        vgroups_IndexName[groups.index] = groups.name
    data = {}  # vert_indices, [(vgroup_index, weights)]
    for v in me_source.vertices:
        vg = v.groups
        vi = v.index
        if len(vg) > 0:
            vgroup_collect = []
            for i in range(0, len(vg)):
                vgroup_collect.append((vg[i].group, vg[i].weight))
            data[vi] = vgroup_collect
    # write data to target
    if ob != active:
        # add missing vertex groups
        for vgroup_name in vgroups_IndexName.values():
            # check if group already exists...
            already_present = 0
            for i in range(0, len(ob.vertex_groups)):
                if ob.vertex_groups[i].name == vgroup_name:
                    already_present = 1
            # ... if not, then add
            if already_present == 0:
                ob.vertex_groups.new(name=vgroup_name)
        # write weights
        for v in me_target.vertices:
            for vi_source, vgroupIndex_weight in data.items():
                if v.index == vi_source:

                    for i in range(0, len(vgroupIndex_weight)):
                        groupName = vgroups_IndexName[vgroupIndex_weight[i][0]]
                        groups = ob.vertex_groups
                        for vgs in range(0, len(groups)):
                            if groups[vgs].name == groupName:
                                groups[vgs].add((v.index,),
                                   vgroupIndex_weight[i][1], "REPLACE")
    return('INFO', "Weights copied")


object_copies = (
        # ('obj_loc', "Location",
        # "Copy Location from Active to Selected", obLoc),
        # ('obj_rot', "Rotation",
        # "Copy Rotation from Active to Selected", obRot),
        # ('obj_sca', "Scale",
        # "Copy Scale from Active to Selected", obSca),
        ('obj_vis_loc', "Location",
        "Copy Location from Active to Selected", obVisLoc),
        ('obj_vis_rot', "Rotation",
        "Copy Rotation from Active to Selected", obVisRot),
        ('obj_vis_sca', "Scale",
        "Copy Scale from Active to Selected", obVisSca),
        ('obj_drw', "Draw Options",
        "Copy Draw Options from Active to Selected", obDrw),
        ('obj_ofs', "Time Offset",
        "Copy Time Offset from Active to Selected", obOfs),
        ('obj_dup', "Dupli",
        "Copy Dupli from Active to Selected", obDup),
        ('obj_col', "Object Color",
        "Copy Object Color from Active to Selected", obCol),
        ('obj_mas', "Mass",
        "Copy Mass from Active to Selected", obMas),
        # ('obj_dmp', "Damping",
        # "Copy Damping from Active to Selected"),
        # ('obj_all', "All Physical Attributes",
        # "Copy Physical Attributes from Active to Selected"),
        # ('obj_prp', "Properties",
        # "Copy Properties from Active to Selected"),
        # ('obj_log', "Logic Bricks",
        # "Copy Logic Bricks from Active to Selected"),
        ('obj_lok', "Protected Transform",
        "Copy Protected Tranforms from Active to Selected", obLok),
        ('obj_con', "Object Constraints",
        "Copy Object Constraints from Active to Selected", obCon),
        # ('obj_nla', "NLA Strips",
        # "Copy NLA Strips from Active to Selected"),
        # ('obj_tex', "Texture Space",
        # "Copy Texture Space from Active to Selected", obTex),
        # ('obj_sub', "Subsurf Settings",
        # "Copy Subsurf Setings from Active to Selected"),
        # ('obj_smo', "AutoSmooth",
        # "Copy AutoSmooth from Active to Selected"),
        ('obj_idx', "Pass Index",
        "Copy Pass Index from Active to Selected", obIdx),
        ('obj_mod', "Modifiers",
        "Copy Modifiers from Active to Selected", obMod),
        ('obj_wei', "Vertex Weights",
        "Copy vertex weights based on indices", obWei),
        ('obj_grp', "Group Links",
        "Copy selected into active object's groups", obGrp)
        )


@classmethod
def object_poll_func(cls, context):
    return (len(context.selected_objects) > 1)


def object_invoke_func(self, context, event):
    wm = context.window_manager
    wm.invoke_props_dialog(self)
    return {'RUNNING_MODAL'}


class CopySelectedObjectConstraints(Operator):
    """Copy Chosen constraints from active to selected"""
    bl_idname = "object.copy_selected_constraints"
    bl_label = "Copy Selected Constraints"

    selection = BoolVectorProperty(
            size=32,
            options={'SKIP_SAVE'}
            )

    poll = object_poll_func
    invoke = object_invoke_func

    def draw(self, context):
        layout = self.layout
        for idx, const in enumerate(context.active_object.constraints):
            layout.prop(self, "selection", index=idx, text=const.name,
               toggle=True)

    def execute(self, context):
        active = context.active_object
        selected = context.selected_objects[:]
        selected.remove(active)
        for obj in selected:
            for index, flag in enumerate(self.selection):
                if flag:
                    old_constraint = active.constraints[index]
                    new_constraint = obj.constraints.new(
                                            active.constraints[index].type
                                            )
                    generic_copy(old_constraint, new_constraint)
        return{'FINISHED'}


class CopySelectedObjectModifiers(Operator):
    """Copy Chosen modifiers from active to selected"""
    bl_idname = "object.copy_selected_modifiers"
    bl_label = "Copy Selected Modifiers"

    selection = BoolVectorProperty(
            size=32,
            options={'SKIP_SAVE'}
            )

    poll = object_poll_func
    invoke = object_invoke_func

    def draw(self, context):
        layout = self.layout
        for idx, const in enumerate(context.active_object.modifiers):
            layout.prop(self, 'selection', index=idx, text=const.name,
               toggle=True)

    def execute(self, context):
        active = context.active_object
        selected = context.selected_objects[:]
        selected.remove(active)
        for obj in selected:
            for index, flag in enumerate(self.selection):
                if flag:
                    old_modifier = active.modifiers[index]
                    new_modifier = obj.modifiers.new(
                                       type=active.modifiers[index].type,
                                       name=active.modifiers[index].name
                                       )
                    generic_copy(old_modifier, new_modifier)
        return{'FINISHED'}


object_ops = []
genops(object_copies, object_ops, "object.copy_", object_poll_func, obLoopExec)


class VIEW3D_MT_copypopup(Menu):
    bl_label = "Copy Attributes"

    def draw(self, context):
        layout = self.layout

        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("view3d.copybuffer", icon="COPY_ID")

        if (len(context.selected_objects) <= 1):
            layout.separator()
            layout.label(text="Please select at least two objects", icon="INFO")
            layout.separator()

        for entry, op in enumerate(object_copies):
            if entry and entry % 4 == 0:
                layout.separator()
            layout.operator("object.copy_" + op[0])
        layout.operator("object.copy_selected_constraints")
        layout.operator("object.copy_selected_modifiers")


# Begin Mesh copy settings:

class MESH_MT_CopyFaceSettings(Menu):
    bl_label = "Copy Face Settings"

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def draw(self, context):
        mesh = context.object.data
        uv = len(mesh.uv_textures) > 1
        vc = len(mesh.vertex_colors) > 1

        layout = self.layout
        layout.operator("view3d.copybuffer", icon="COPY_ID")
        layout.operator("view3d.pastebuffer", icon="COPY_ID")

        layout.separator()

        op = layout.operator(MESH_OT_CopyFaceSettings.bl_idname,
                        text="Copy Material")
        op['layer'] = ''
        op['mode'] = 'MAT'

        if mesh.uv_textures.active:
            op = layout.operator(MESH_OT_CopyFaceSettings.bl_idname,
                        text="Copy Active UV Image")
            op['layer'] = ''
            op['mode'] = 'IMAGE'
            op = layout.operator(MESH_OT_CopyFaceSettings.bl_idname,
                        text="Copy Active UV Coords")
            op['layer'] = ''
            op['mode'] = 'UV'

        if mesh.vertex_colors.active:
            op = layout.operator(MESH_OT_CopyFaceSettings.bl_idname,
                        text="Copy Active Vertex Colors")
            op['layer'] = ''
            op['mode'] = 'VCOL'

        if uv or vc:
            layout.separator()
            if uv:
                layout.menu("MESH_MT_CopyImagesFromLayer")
                layout.menu("MESH_MT_CopyUVCoordsFromLayer")
            if vc:
                layout.menu("MESH_MT_CopyVertexColorsFromLayer")


# Data (UV map, Image and Vertex color) menus calling MESH_OT_CopyFaceSettings
# Explicitly defined as using the generator code was broken in case of Menus
# causing issues with access and registration

class MESH_MT_CopyImagesFromLayer(Menu):
    bl_label = "Copy Other UV Image Layers"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.mode == "EDIT_MESH" and len(
                obj.data.uv_layers) > 1

    def draw(self, context):
        mesh = context.active_object.data
        _buildmenu(self, mesh, 'IMAGE', "IMAGE_COL")


class MESH_MT_CopyUVCoordsFromLayer(Menu):
    bl_label = "Copy Other UV Coord Layers"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.mode == "EDIT_MESH" and len(
                obj.data.uv_layers) > 1

    def draw(self, context):
        mesh = context.active_object.data
        _buildmenu(self, mesh, 'UV', "GROUP_UVS")


class MESH_MT_CopyVertexColorsFromLayer(Menu):
    bl_label = "Copy Other Vertex Colors Layers"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.mode == "EDIT_MESH" and len(
                obj.data.vertex_colors) > 1

    def draw(self, context):
        mesh = context.active_object.data
        _buildmenu(self, mesh, 'VCOL', "GROUP_VCOL")


def _buildmenu(self, mesh, mode, icon):
    layout = self.layout
    if mode == 'VCOL':
        layers = mesh.vertex_colors
    else:
        layers = mesh.uv_textures
    for layer in layers:
        if not layer.active:
            op = layout.operator(MESH_OT_CopyFaceSettings.bl_idname,
                                 text=layer.name, icon=icon)
            op['layer'] = layer.name
            op['mode'] = mode


class MESH_OT_CopyFaceSettings(Operator):
    """Copy settings from active face to all selected faces"""
    bl_idname = 'mesh.copy_face_settings'
    bl_label = "Copy Face Settings"
    bl_options = {'REGISTER', 'UNDO'}

    mode = StringProperty(
            name="Mode",
            options={"HIDDEN"},
            )
    layer = StringProperty(
            name="Layer",
            options={"HIDDEN"},
            )

    @classmethod
    def poll(cls, context):
        return context.mode == 'EDIT_MESH'

    def execute(self, context):
        mode = getattr(self, 'mode', '')
        if mode not in {'MAT', 'VCOL', 'IMAGE', 'UV'}:
            self.report({'ERROR'}, "No mode specified or invalid mode")
            return self._end(context, {'CANCELLED'})
        layername = getattr(self, 'layer', '')
        mesh = context.object.data

        # Switching out of edit mode updates the selected state of faces and
        # makes the data from the uv texture and vertex color layers available.
        bpy.ops.object.editmode_toggle()

        polys = mesh.polygons
        if mode == 'MAT':
            to_data = from_data = polys
        else:
            if mode == 'VCOL':
                layers = mesh.vertex_colors
                act_layer = mesh.vertex_colors.active
            elif mode == 'IMAGE':
                layers = mesh.uv_textures
                act_layer = mesh.uv_textures.active
            elif mode == 'UV':
                layers = mesh.uv_layers
                act_layer = mesh.uv_layers.active
            if not layers or (layername and layername not in layers):
                self.report({'ERROR'}, "Invalid UV or color layer. Operation Cancelled")
                return self._end(context, {'CANCELLED'})
            from_data = layers[layername or act_layer.name].data
            to_data = act_layer.data
        from_index = polys.active

        for f in polys:
            if f.select:
                if to_data != from_data:
                    # Copying from another layer.
                    # from_face is to_face's counterpart from other layer.
                    from_index = f.index
                elif f.index == from_index:
                    # Otherwise skip copying a face to itself.
                    continue
                if mode == 'MAT':
                    f.material_index = polys[from_index].material_index
                    continue
                elif mode == 'IMAGE':
                    to_data[f.index].image = from_data[from_index].image
                    continue
                if len(f.loop_indices) != len(polys[from_index].loop_indices):
                    self.report({'WARNING'}, "Different number of vertices.")
                for i in range(len(f.loop_indices)):
                    to_vertex = f.loop_indices[i]
                    from_vertex = polys[from_index].loop_indices[i]
                    if mode == 'VCOL':
                        to_data[to_vertex].color = from_data[from_vertex].color
                    elif mode == 'UV':
                        to_data[to_vertex].uv = from_data[from_vertex].uv

        return self._end(context, {'FINISHED'})

    def _end(self, context, retval):
        if context.mode != 'EDIT_MESH':
            # Clean up by returning to edit mode like it was before.
            bpy.ops.object.editmode_toggle()
        return(retval)


def register():
    bpy.utils.register_module(__name__)

    # mostly to get the keymap working
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps.new(name="Object Mode")
        kmi = km.keymap_items.new('wm.call_menu', 'C', 'PRESS', ctrl=True)
        kmi.properties.name = 'VIEW3D_MT_copypopup'

        km = kc.keymaps.new(name="Pose")
        kmi = km.keymap_items.get("pose.copy")
        if kmi is not None:
            kmi.idname = 'wm.call_menu'
        else:
            kmi = km.keymap_items.new('wm.call_menu', 'C', 'PRESS', ctrl=True)
        kmi.properties.name = 'VIEW3D_MT_posecopypopup'

        km = kc.keymaps.new(name="Mesh")
        kmi = km.keymap_items.new('wm.call_menu', 'C', 'PRESS')
        kmi.ctrl = True
        kmi.properties.name = 'MESH_MT_CopyFaceSettings'


def unregister():
    # mostly to remove the keymap
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        kms = kc.keymaps.get('Pose')
        if kms is not None:
            for item in kms.keymap_items:
                if item.name == 'Call Menu' and item.idname == 'wm.call_menu' and \
                   item.properties.name == 'VIEW3D_MT_posecopypopup':
                    item.idname = 'pose.copy'
                    break

        km = kc.keymaps.get('Mesh')
        if km is not None:
            for kmi in km.keymap_items:
                if kmi.idname == 'wm.call_menu':
                    if kmi.properties.name == 'MESH_MT_CopyFaceSettings':
                        km.keymap_items.remove(kmi)

        km = kc.keymaps.get('Object Mode')
        if km is not None:
            for kmi in km.keymap_items:
                if kmi.idname == 'wm.call_menu':
                    if kmi.properties.name == 'VIEW3D_MT_copypopup':
                        km.keymap_items.remove(kmi)

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
