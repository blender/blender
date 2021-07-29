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

# <pep8-80 compliant>

bl_info = {
    "name": "Corrective shape keys",
    "author": "Ivo Grigull (loolarge), Tal Trachtman",
    "version": (1, 0),
    "blender": (2, 57, 0),
    "location": "Object Data > Shape Keys (Search: corrective) ",
    "description": "Creates a corrective shape key for the current pose",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Animation/Corrective_Shape_Key",
    "category": "Animation",
}

"""
This script transfer the shape from an object (base mesh without
modifiers) to another object with modifiers (i.e. posed Armature).
Only two objects must be selected.
The first selected object will be added to the second selected
object as a new shape key.

- Original 2.4x script by Brecht
- Unpose-function reused from a script by Tal Trachtman in 2007
  http://www.apexbow.com/randd.html
- Converted to Blender 2.5 by Ivo Grigull

Limitations:
- Target mesh may not have any transformation at object level,
  it will be set to zero.
- Fast/Armature method does not work with Bone envelopes or dual quaternions,
  both settings will be disabled in the modifier
"""


import bpy
from mathutils import Vector, Matrix


iterations = 20
threshold = 1e-16


def reset_transform(ob):
    ob.matrix_local.identity()


# this version is for shape_key data
def extract_vert_coords(ob, verts):
    return [v.co.copy() for v in verts]


def extract_mapped_coords(ob, shape_verts):
    totvert = len(shape_verts)

    mesh = ob.to_mesh(bpy.context.scene, True, 'PREVIEW')

    # cheating, the original mapped verts happen
    # to be at the end of the vertex array
    verts = mesh.vertices
    arr = [verts[i].co.copy() for i in range(len(verts) - totvert, len(verts))]

    mesh.user_clear()
    bpy.data.meshes.remove(mesh)

    return arr


def apply_vert_coords(ob, mesh, x):
    for i, v in enumerate(mesh):
        v.co = x[i]
    ob.data.update()


def func_add_corrective_pose_shape(source, target):

    ob_1 = target
    mesh_1 = target.data
    ob_2 = source
    mesh_2 = source.data

    reset_transform(target)

    # If target object doesn't have Basis shape key, create it.
    if not mesh_1.shape_keys:
        basis = ob_1.shape_key_add()
        basis.name = "Basis"
        ob_1.data.update()

    key_index = ob_1.active_shape_key_index
    # Insert new shape key
    if key_index == 0:
        new_shapekey = ob_1.shape_key_add()
        new_shapekey.name = "Shape_" + ob_2.name

        key_index = len(mesh_1.shape_keys.key_blocks) - 1
        ob_1.active_shape_key_index = key_index

    # else, the active shape will be used (updated)

    ob_1.show_only_shape_key = True

    vgroup = ob_1.active_shape_key.vertex_group
    ob_1.active_shape_key.vertex_group = ""

    mesh_1_key_verts = mesh_1.shape_keys.key_blocks[key_index].data

    x = extract_vert_coords(ob_1, mesh_1_key_verts)

    targetx = extract_vert_coords(ob_2, mesh_2.vertices)

    for iteration in range(0, iterations):
        dx = [[], [], [], [], [], []]

        mapx = extract_mapped_coords(ob_1, mesh_1_key_verts)

        # finite differencing in X/Y/Z to get approximate gradient
        for i in range(0, len(mesh_1.vertices)):
            epsilon = (targetx[i] - mapx[i]).length

            if epsilon < threshold:
                epsilon = 0.0

            dx[0] += [x[i] + 0.5 * epsilon * Vector((1, 0, 0))]
            dx[1] += [x[i] + 0.5 * epsilon * Vector((-1, 0, 0))]
            dx[2] += [x[i] + 0.5 * epsilon * Vector((0, 1, 0))]
            dx[3] += [x[i] + 0.5 * epsilon * Vector((0, -1, 0))]
            dx[4] += [x[i] + 0.5 * epsilon * Vector((0, 0, 1))]
            dx[5] += [x[i] + 0.5 * epsilon * Vector((0, 0, -1))]

        for j in range(0, 6):
            apply_vert_coords(ob_1, mesh_1_key_verts, dx[j])
            dx[j] = extract_mapped_coords(ob_1, mesh_1_key_verts)

        # take a step in the direction of the gradient
        for i in range(0, len(mesh_1.vertices)):
            epsilon = (targetx[i] - mapx[i]).length

            if epsilon >= threshold:
                Gx = list((dx[0][i] - dx[1][i]) / epsilon)
                Gy = list((dx[2][i] - dx[3][i]) / epsilon)
                Gz = list((dx[4][i] - dx[5][i]) / epsilon)
                G = Matrix((Gx, Gy, Gz))
                x[i] += G * (targetx[i] - mapx[i])

        apply_vert_coords(ob_1, mesh_1_key_verts, x)

    ob_1.active_shape_key.vertex_group = vgroup

    # set the new shape key value to 1.0, so we see the result instantly
    ob_1.active_shape_key.value = 1.0

    #mesh_1.update()
    ob_1.show_only_shape_key = False


class add_corrective_pose_shape(bpy.types.Operator):
    """Adds first object as shape to second object for the current pose """ \
    """while maintaining modifiers """ \
    """(i.e. anisculpt, avoiding crazy space) Beware of slowness!"""

    bl_idname = "object.add_corrective_pose_shape"
    bl_label = "Add object as corrective pose shape"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        selection = context.selected_objects
        if len(selection) != 2:
            self.report({'ERROR'}, "Select source and target objects")
            return {'CANCELLED'}

        target = context.active_object
        if context.active_object == selection[0]:
            source = selection[1]
        else:
            source = selection[0]

        func_add_corrective_pose_shape(source, target)

        return {'FINISHED'}


def func_object_duplicate_flatten_modifiers(scene, obj):
    mesh = obj.to_mesh(scene, True, 'PREVIEW')
    name = obj.name + "_clean"
    new_object = bpy.data.objects.new(name, mesh)
    new_object.data = mesh
    scene.objects.link(new_object)
    return new_object


class object_duplicate_flatten_modifiers(bpy.types.Operator):
    """Duplicates the selected object with modifiers applied"""

    bl_idname = "object.object_duplicate_flatten_modifiers"
    bl_label = "Duplicate and apply all"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        scene = context.scene
        obj_act = context.active_object

        new_object = func_object_duplicate_flatten_modifiers(scene, obj_act)

        # setup the context
        bpy.ops.object.select_all(action='DESELECT')

        scene.objects.active = new_object
        new_object.select = True

        return {'FINISHED'}


def unposeMesh(meshObToUnpose, meshObToUnposeWeightSrc, armatureOb):
    psdMeshData = meshObToUnpose

    psdMesh = psdMeshData
    I = Matrix()  # identity matrix

    meshData = meshObToUnposeWeightSrc.data
    mesh = meshData

    armData = armatureOb.data

    pose = armatureOb.pose
    pbones = pose.bones

    for index, v in enumerate(mesh.vertices):
        # above is python shortcut for:index goes up from 0 to tot num of
        # verts in mesh, with index incrementing by 1 each iteration

        psdMeshVert = psdMesh[index]

        listOfBoneNameWeightPairs = []
        for n in mesh.vertices[index].groups:
            try:
                name = meshObToUnposeWeightSrc.vertex_groups[n.group].name
                weight = n.weight
                is_bone = False
                for i in armData.bones:
                    if i.name == name:
                        is_bone = True
                        break
                # ignore non-bone vertex groups
                if is_bone:
                    listOfBoneNameWeightPairs.append([name, weight])
            except:
                print('error')
                pass

        weightedAverageDictionary = {}
        totalWeight = 0
        for pair in listOfBoneNameWeightPairs:
            totalWeight += pair[1]

        for pair in listOfBoneNameWeightPairs:
            if totalWeight > 0:  # avoid divide by zero!
                weightedAverageDictionary[pair[0]] = pair[1] / totalWeight
            else:
                weightedAverageDictionary[pair[0]] = 0

        # Matrix filled with zeros
        sigma = Matrix()
        sigma.zero()

        list = []
        for n in pbones:
            list.append(n)
        list.reverse()

        for pbone in list:
            if pbone.name in weightedAverageDictionary:
                #~ print("found key %s", pbone.name)
                vertexWeight = weightedAverageDictionary[pbone.name]
                m = pbone.matrix_channel.copy()
                #m.transpose()
                sigma += (m - I) * vertexWeight

            else:
                pass
                #~ print("no key for bone " + pbone.name)

        sigma = I + sigma
        sigma.invert()
        psdMeshVert.co = psdMeshVert.co * sigma


def func_add_corrective_pose_shape_fast(source, target):

    reset_transform(target)

    # If target object doesn't have Basis shape key, create it.
    if not target.data.shape_keys:
        basis = target.shape_key_add()
        basis.name = "Basis"
        target.data.update()

    key_index = target.active_shape_key_index

    if key_index == 0:

        # Insert new shape key
        new_shapekey = target.shape_key_add()
        new_shapekey.name = "Shape_" + source.name

        key_index = len(target.data.shape_keys.key_blocks) - 1
        target.active_shape_key_index = key_index

    # else, the active shape will be used (updated)

    target.show_only_shape_key = True

    shape_key_verts = target.data.shape_keys.key_blocks[key_index].data

    try:
        vgroup = target.active_shape_key.vertex_group
        target.active_shape_key.vertex_group = ''
    except:
        pass

    # copy the local vertex positions to the new shape
    verts = source.data.vertices
    for n in range(len(verts)):
        shape_key_verts[n].co = verts[n].co

    # go to all armature modifies and unpose the shape
    for n in target.modifiers:
        if n.type == 'ARMATURE' and n.show_viewport:
            #~ print("got one")
            n.use_bone_envelopes = False
            n.use_deform_preserve_volume = False
            n.use_vertex_groups = True
            armature = n.object
            unposeMesh(shape_key_verts, target, armature)
            break

    # set the new shape key value to 1.0, so we see the result instantly
    target.active_shape_key.value = 1.0

    try:
        target.active_shape_key.vertex_group = vgroup
    except:
        pass

    target.show_only_shape_key = False
    target.data.update()


class add_corrective_pose_shape_fast(bpy.types.Operator):
    """Adds 1st object as shape to 2nd object as pose shape
    (only 1 armature)"""

    bl_idname = "object.add_corrective_pose_shape_fast"
    bl_label = "Add object as corrective shape faster"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        selection = context.selected_objects
        if len(selection) != 2:
            self.report({'ERROR'}, "Select source and target objects")
            return {'CANCELLED'}

        target = context.active_object
        if context.active_object == selection[0]:
            source = selection[1]
        else:
            source = selection[0]

        func_add_corrective_pose_shape_fast(source, target)

        return {'FINISHED'}


# -----------------------------------------------------------------------------
# GUI

def vgroups_draw(self, context):
    layout = self.layout

    layout.operator("object.object_duplicate_flatten_modifiers",
                    text='Create duplicate for editing')
    layout.operator("object.add_corrective_pose_shape_fast",
                    text='Add as corrective pose-shape (fast, armatures only)',
                    icon='COPY_ID')  # icon is not ideal
    layout.operator("object.add_corrective_pose_shape",
                    text='Add as corrective pose-shape (slow, all modifiers)',
                    icon='COPY_ID')  # icon is not ideal


def modifiers_draw(self, context):
    pass


def register():
    bpy.utils.register_module(__name__)

    bpy.types.MESH_MT_shape_key_specials.append(vgroups_draw)
    bpy.types.DATA_PT_modifiers.append(modifiers_draw)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
