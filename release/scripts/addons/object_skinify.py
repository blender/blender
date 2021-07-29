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

bl_info = {
    "name": "Skinify Rig",
    "author": "Albert Makac (karab44)",
    "version": (0, 11, 0),
    "blender": (2, 7, 9),
    "location": "Properties > Bone > Skinify Rig (visible on pose mode only)",
    "description": "Creates a mesh object from selected bones",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/"
                "Py/Scripts/Object/Skinify",
    "category": "Object"}

import bpy
from bpy.props import (
        FloatProperty,
        IntProperty,
        BoolProperty,
        PointerProperty,
        )
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        )
from mathutils import (
        Vector,
        Euler,
        )
from bpy.app.handlers import persistent
from enum import Enum

# can the armature data properties group_prop and row be fetched directly from the rigify script?
horse_data = \
        (1, 5), (2, 4), (3, 0), (4, 3), (5, 4), (1, 0), (1, 0), (7, 2), (8, 5), (9, 4), \
        (7, 2), (8, 5), (9, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), \
        (13, 6), (1, 4), (14, 6), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (14, 1),

shark_data = \
        (1, 5), (2, 4), (1, 0), (3, 3), (4, 4), (5, 6), (6, 5), (7, 4), (6, 5), (7, 4), \
        (8, 3), (9, 4), (1, 0), (1, 6), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), \
        (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (14, 1),

bird_data = \
        (1, 6), (2, 4), (1, 0), (3, 3), (4, 4), (1, 0), (1, 0), (6, 5), (8, 0), (7, 4), (6, 5), \
        (8, 0), (7, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), (1, 0), (1, 0), \
        (13, 6), (14, 4), (1, 0), (8, 6), (1, 0), (1, 0), (1, 0), (14, 1),

cat_data = \
        (1, 5), (2, 2), (2, 3), (3, 3), (4, 4), (5, 6), (6, 4), (7, 2), (8, 5), (9, 4), (7, 2), \
        (8, 5), (9, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), (13, 3), (14, 4), \
        (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (16, 1),

biped_data = \
        (1, 0), (1, 0), (1, 0), (3, 3), (4, 4), (1, 0), (1, 0), (7, 2), (8, 5), (9, 4), (7, 2), \
        (8, 5), (9, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), (1, 0), (1, 0), \
        (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (14, 1),

human_data = \
        (1, 5), (2, 2), (2, 3), (3, 3), (4, 4), (5, 6), (6, 4), (7, 2), (8, 5), (9, 4), (7, 2), \
        (8, 5), (9, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), (1, 0), (1, 0), \
        (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (14, 1),

wolf_data = \
        (1, 5), (2, 2), (2, 3), (3, 3), (4, 4), (5, 6), (6, 4), (7, 2), (8, 5), (9, 4), (7, 2), \
        (8, 5), (9, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), (13, 6), (1, 0), \
        (13, 0), (13, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (14, 1),

quadruped_data = \
        (1, 0), (2, 0), (2, 0), (3, 3), (4, 4), (5, 0), (6, 0), (7, 2), (8, 5), (9, 4), \
        (7, 2), (8, 5), (9, 4), (10, 2), (11, 5), (12, 4), (10, 2), (11, 5), (12, 4), (13, 6), \
        (1, 0), (13, 0), (13, 0), (1, 0), (1, 0), (1, 0), (1, 0), (1, 0), (14, 1),

human_legacy_data = \
        (1, None), (1, None), (2, None), (1, None), (3, None), (3, None), (4, None), (5, None), \
        (6, None), (4, None), (5, None), (6, None), (7, None), (8, None), (9, None), (7, None), \
        (8, None), (9, None), (1, None), (1, None), (1, None), (1, None), (1, None), (1, None), \
        (1, None), (1, None), (1, None), (1, None),

pitchipoy_data = \
        (1, None), (2, None), (2, None), (3, None), (4, None), (5, None), (6, None), (7, None), \
        (8, None), (9, None), (7, None), (8, None), (9, None), (10, None), (11, None), (12, None), \
        (10, None), (11, None), (12, None), (1, None), (1, None), (1, None), (1, None), (1, None), \
        (1, None), (1, None), (1, None), (1, None),

rigify_data = horse_data, shark_data, bird_data, cat_data, biped_data, human_data, \
              wolf_data, quadruped_data, human_legacy_data, pitchipoy_data


class Rig_type(Enum):
    HORSE = 0
    SHARK = 1
    BIRD = 2
    CAT = 3
    BIPED = 4
    HUMAN = 5
    WOLF = 6
    QUAD = 7
    LEGACY = 8
    PITCHIPOY = 9
    OTHER = 10


rig_type = Rig_type.OTHER


class Idx_Store(object):
    def __init__(self, rig_type):
        self.rig_type = rig_type
        self.hand_r_merge = []
        self.hand_l_merge = []
        self.hands_pretty = []
        self.root = []

        if not self.rig_type == Rig_type.LEGACY and \
                not self.rig_type == Rig_type.HUMAN and \
                not self.rig_type == Rig_type.PITCHIPOY:
            return

        if self.rig_type == Rig_type.LEGACY:
            self.hand_l_merge = [7, 12, 16, 21, 26, 27]
            self.hand_r_merge = [30, 31, 36, 40, 45, 50]
            self.hands_pretty = [6, 29]
            self.root = [59]

        if self.rig_type == Rig_type.HUMAN or self.rig_type == Rig_type.PITCHIPOY:
            self.hand_l_merge = [9, 10, 15, 19, 24, 29]
            self.hand_r_merge = [32, 33, 37, 42, 47, 52]
            self.hands_pretty = [8, 31]
            self.root = [56]

    def get_all_idx(self):
        return self.hand_l_merge, self.hand_r_merge, self.hands_pretty, self.root

    def get_hand_l_merge_idx(self):
        return self.hand_l_merge

    def get_hand_r_merge_idx(self):
        return self.hand_r_merge

    def get_hands_pretty_idx(self):
        return self.hands_pretty

    def get_root_idx(self):
        return self.root


# initialize properties
def init_props():
    # additional check - this should be a rare case if the handler
    # wasn't removed for some reason and the add-on is not toggled on/off
    if hasattr(bpy.types.Scene, "skinify"):
        scn = bpy.context.scene.skinify

        scn.connect_mesh = False
        scn.connect_parents = False
        scn.generate_all = False
        scn.thickness = 0.8
        scn.finger_thickness = 0.25
        scn.apply_mod = True
        scn.parent_armature = True
        scn.sub_level = 1


# selects vertices
def select_vertices(mesh_obj, idx):
    bpy.context.scene.objects.active = mesh_obj
    mode = mesh_obj.mode
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.object.mode_set(mode='OBJECT')

    for i in idx:
        mesh_obj.data.vertices[i].select = True

    selectedVerts = [v.index for v in mesh_obj.data.vertices if v.select]

    bpy.ops.object.mode_set(mode=mode)
    return selectedVerts


def identify_rig():
    if 'rigify_layers' not in bpy.context.object.data:
        return Rig_type.OTHER  # non recognized

    LEGACY_LAYERS_SIZE = 28
    layers = bpy.context.object.data['rigify_layers']

    for type, rig in enumerate(rigify_data):
        index = 0

        for props in layers:
            if len(layers) == LEGACY_LAYERS_SIZE and 'group_prop' not in props:

                if props['row'] != rig[index][0] or rig[index][1] is not None:
                    break

            elif (props['row'] != rig[index][0]) or (props['group_prop'] != rig[index][1]):
                break

            # SUCCESS if reach the end
            if index == len(layers) - 1:
                return Rig_type(type)

            index = index + 1

    return Rig_type.OTHER


def prepare_ignore_list(rig_type, bones):
    # detect the head, face, hands, breast, heels or other exceptionary bones to exclusion or customization
    common_ignore_list = ['eye', 'heel', 'breast', 'root']

    # edit these lists to suits your taste

    horse_ignore_list = ['chest', 'belly', 'pelvis', 'jaw', 'nose', 'skull', 'ear.']

    shark_ignore_list = ['jaw']

    bird_ignore_list = [
            'face', 'pelvis', 'nose', 'lip', 'jaw', 'chin', 'ear.', 'brow',
            'lid', 'forehead', 'temple', 'cheek', 'teeth', 'tongue', 'beak'
            ]
    cat_ignore_list = [
            'face', 'belly' 'pelvis.C', 'nose', 'lip', 'jaw', 'chin', 'ear.', 'brow',
            'lid', 'forehead', 'temple', 'cheek', 'teeth', 'tongue'
            ]
    biped_ignore_list = ['pelvis']

    human_ignore_list = [
            'face', 'pelvis', 'nose', 'lip', 'jaw', 'chin', 'ear.', 'brow',
            'lid', 'forehead', 'temple', 'cheek', 'teeth', 'tongue'
            ]
    wolf_ignore_list = [
            'face', 'pelvis', 'nose', 'lip', 'jaw', 'chin', 'ear.', 'brow',
            'lid', 'forehead', 'temple', 'cheek', 'teeth', 'tongue'
            ]
    quad_ignore_list = [
            'face', 'pelvis', 'nose', 'lip', 'jaw', 'chin', 'ear.', 'brow',
            'lid', 'forehead', 'temple', 'cheek', 'teeth', 'tongue'
            ]
    rigify_legacy_ignore_list = []

    pitchipoy_ignore_list = [
            'face', 'pelvis', 'nose', 'lip', 'jaw', 'chin', 'ear.', 'brow',
            'lid', 'forehead', 'temple', 'cheek', 'teeth', 'tongue'
            ]

    other_ignore_list = []

    ignore_list = common_ignore_list

    if rig_type == Rig_type.HORSE:
        ignore_list = ignore_list + horse_ignore_list
        print("RIDER OF THE APOCALYPSE")
    elif rig_type == Rig_type.SHARK:
        ignore_list = ignore_list + shark_ignore_list
        print("DEADLY JAWS")
    elif rig_type == Rig_type.BIRD:
        ignore_list = ignore_list + bird_ignore_list
        print("WINGS OF LIBERTY")
    elif rig_type == Rig_type.CAT:
        ignore_list = ignore_list + cat_ignore_list
        print("MEOW")
    elif rig_type == Rig_type.BIPED:
        ignore_list = ignore_list + biped_ignore_list
        print("HUMANOID")
    elif rig_type == Rig_type.HUMAN:
        ignore_list = ignore_list + human_ignore_list
        print("JUST A HUMAN AFTER ALL")
    elif rig_type == Rig_type.WOLF:
        ignore_list = ignore_list + wolf_ignore_list
        print("WHITE FANG")
    elif rig_type == Rig_type.QUAD:
        ignore_list = ignore_list + quad_ignore_list
        print("MYSTERIOUS CREATURE")
    elif rig_type == Rig_type.LEGACY:
        ignore_list = ignore_list + rigify_legacy_ignore_list
        print("LEGACY RIGIFY")
    elif rig_type == Rig_type.PITCHIPOY:
        ignore_list = ignore_list + pitchipoy_ignore_list
        print("PITCHIPOY")
    elif rig_type == Rig_type.OTHER:
        ignore_list = ignore_list + other_ignore_list
        print("rig non recognized...")

    return ignore_list


# generates edges from vertices used by skin modifier
def generate_edges(mesh, shape_object, bones, scale, connect_mesh=False, connect_parents=False,
                   head_ornaments=False, generate_all=False):
    """
    This function adds vertices for all heads and tails
    """
    # scene preferences

    alternate_scale_list = []

    me = mesh
    verts = []
    edges = []
    idx = 0
    alternate_scale_idx_list = list()

    rig_type = identify_rig()
    ignore_list = prepare_ignore_list(rig_type, bones)

    # edge generator loop
    for b in bones:
        # look for rig's hands and their childs
        if 'hand' in b.name.lower():
            # prepare the list
            for c in b.children_recursive:
                alternate_scale_list.append(c.name)

        found = False

        for i in ignore_list:
            if i in b.name.lower():
                found = True
                break

        if found and generate_all is False:
            continue

        # fix for drawing rootbone and relationship lines
        if 'root' in b.name.lower() and generate_all is False:
            continue

        # ignore any head ornaments
        if head_ornaments is False:
            if b.parent is not None:

                if 'head' in b.parent.name.lower() and not rig_type == Rig_type.HUMAN:
                    continue

                if 'face' in b.parent.name.lower() and rig_type == Rig_type.HUMAN:
                    continue

        if connect_parents:
            if b.parent is not None and b.parent.bone.select is True and b.bone.use_connect is False:
                if 'root' in b.parent.name.lower() and generate_all is False:
                    continue
                # ignore shoulder
                if 'shoulder' in b.name.lower() and connect_mesh is True:
                    continue
                # connect the upper arm directly with chest ommiting shoulders
                if 'shoulder' in b.parent.name.lower() and connect_mesh is True:
                    vert1 = b.head
                    vert2 = b.parent.parent.tail

                else:
                    vert1 = b.head
                    vert2 = b.parent.tail

                verts.append(vert1)
                verts.append(vert2)
                edges.append([idx, idx + 1])

                # also make list of edges made of gaps between the bones
                for a in alternate_scale_list:
                    if b.name == a:
                        alternate_scale_idx_list.append(idx)
                        alternate_scale_idx_list.append(idx + 1)

                idx = idx + 2
            # for bvh free floating hips and hips correction for rigify and pitchipoy
            if ((generate_all is False and 'hip' in b.name.lower()) or
              (generate_all is False and (b.name == 'hips' and rig_type == Rig_type.LEGACY) or
              (b.name == 'spine' and rig_type == Rig_type.PITCHIPOY) or (b.name == 'spine' and
               rig_type == Rig_type.HUMAN) or (b.name == 'spine' and rig_type == Rig_type.BIPED))):
                continue

        vert1 = b.head
        vert2 = b.tail
        verts.append(vert1)
        verts.append(vert2)

        edges.append([idx, idx + 1])

        for a in alternate_scale_list:
            if b.name == a:
                alternate_scale_idx_list.append(idx)
                alternate_scale_idx_list.append(idx + 1)

        idx = idx + 2

    # Create mesh from given verts, faces
    me.from_pydata(verts, edges, [])
    # Update mesh with new data
    me.update()

    # set object scale exact as armature's scale
    shape_object.scale = scale

    return alternate_scale_idx_list, rig_type


def generate_mesh(shape_object, size, thickness=0.8, finger_thickness=0.25, sub_level=1,
                  connect_mesh=False, connect_parents=False, generate_all=False, apply_mod=True,
                  alternate_scale_idx_list=[], rig_type=0, bones=[]):
    """
    This function adds modifiers for generated edges
    """
    total_bones_num = len(bpy.context.object.pose.bones.keys())
    selected_bones_num = len(bones)

    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')

    # add skin modifier
    shape_object.modifiers.new("Skin", 'SKIN')
    bpy.ops.mesh.select_all(action='SELECT')

    override = bpy.context.copy()
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            for region in area.regions:
                if region.type == 'WINDOW':
                    override['area'] = area
                    override['region'] = region
                    override['edit_object'] = bpy.context.edit_object
                    override['scene'] = bpy.context.scene
                    override['active_object'] = shape_object
                    override['object'] = shape_object
                    override['modifier'] = bpy.context.object.modifiers
                    break

    # calculate optimal thickness for defaults
    bpy.ops.object.skin_root_mark(override)
    bpy.ops.transform.skin_resize(override,
            value=(1 * thickness * (size / 10), 1 * thickness * (size / 10), 1 * thickness * (size / 10)),
            constraint_axis=(False, False, False), constraint_orientation='GLOBAL',
            mirror=False, proportional='DISABLED', proportional_edit_falloff='SMOOTH',
            proportional_size=1
            )
    shape_object.modifiers["Skin"].use_smooth_shade = True
    shape_object.modifiers["Skin"].use_x_symmetry = True

    # select finger vertices and calculate optimal thickness for fingers to fix proportions
    if len(alternate_scale_idx_list) > 0:
        select_vertices(shape_object, alternate_scale_idx_list)

        bpy.ops.object.skin_loose_mark_clear(override, action='MARK')
        # by default set fingers thickness to 25 percent of body thickness
        bpy.ops.transform.skin_resize(override,
                    value=(finger_thickness, finger_thickness, finger_thickness),
                    constraint_axis=(False, False, False), constraint_orientation='GLOBAL',
                    mirror=False, proportional='DISABLED', proportional_edit_falloff='SMOOTH',
                    proportional_size=1
                    )
        # make loose hands only for better topology

    # bpy.ops.mesh.select_all(action='DESELECT')

    if connect_mesh:
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.remove_doubles()

    idx_store = Idx_Store(rig_type)

    # fix rigify and pitchipoy hands topology
    if connect_mesh and connect_parents and generate_all is False and \
            (rig_type == Rig_type.LEGACY or rig_type == Rig_type.PITCHIPOY or rig_type == Rig_type.HUMAN) and \
            selected_bones_num == total_bones_num:
        # thickness will set palm vertex for both hands look pretty
        corrective_thickness = 2.5
        # left hand verts
        merge_idx = idx_store.get_hand_l_merge_idx()

        select_vertices(shape_object, merge_idx)
        bpy.ops.mesh.merge(type='CENTER')
        bpy.ops.transform.skin_resize(override,
                value=(corrective_thickness, corrective_thickness, corrective_thickness),
                constraint_axis=(False, False, False), constraint_orientation='GLOBAL',
                mirror=False, proportional='DISABLED', proportional_edit_falloff='SMOOTH',
                proportional_size=1
                )
        bpy.ops.mesh.select_all(action='DESELECT')

        # right hand verts
        merge_idx = idx_store.get_hand_r_merge_idx()

        select_vertices(shape_object, merge_idx)
        bpy.ops.mesh.merge(type='CENTER')
        bpy.ops.transform.skin_resize(override,
                value=(corrective_thickness, corrective_thickness, corrective_thickness),
                constraint_axis=(False, False, False), constraint_orientation='GLOBAL',
                mirror=False, proportional='DISABLED', proportional_edit_falloff='SMOOTH',
                proportional_size=1
                )

        # making hands even more pretty
        bpy.ops.mesh.select_all(action='DESELECT')
        hands_idx = idx_store.get_hands_pretty_idx()

        select_vertices(shape_object, hands_idx)
        # change the thickness to make hands look less blocky and more sexy
        corrective_thickness = 0.7
        bpy.ops.transform.skin_resize(override,
                value=(corrective_thickness, corrective_thickness, corrective_thickness),
                constraint_axis=(False, False, False), constraint_orientation='GLOBAL',
                mirror=False, proportional='DISABLED', proportional_edit_falloff='SMOOTH',
                proportional_size=1
                )
        bpy.ops.mesh.select_all(action='DESELECT')

    # todo optionally take root from rig's hip tail or head depending on scenario

    root_idx = idx_store.get_root_idx()

    if selected_bones_num == total_bones_num:
        root_idx = [0]

    if len(root_idx) > 0:
        select_vertices(shape_object, root_idx)
        bpy.ops.object.skin_root_mark(override)
    # skin in edit mode
    # add Subsurf modifier
    shape_object.modifiers.new("Subsurf", 'SUBSURF')
    shape_object.modifiers["Subsurf"].levels = sub_level
    shape_object.modifiers["Subsurf"].render_levels = sub_level

    bpy.ops.object.mode_set(mode='OBJECT')

    # object mode apply all modifiers
    if apply_mod:
        bpy.ops.object.modifier_apply(override, apply_as='DATA', modifier="Skin")
        bpy.ops.object.modifier_apply(override, apply_as='DATA', modifier="Subsurf")

    return {'FINISHED'}


def main(context):
    """
    This script will create a custome shape
    """

    # ### Check if selection is OK ###
    if len(context.selected_pose_bones) == 0 or \
            len(context.selected_objects) == 0 or \
            context.selected_objects[0].type != 'ARMATURE':
        return {'CANCELLED'}, "No bone selected or the Armature is hidden"

    scn = bpy.context.scene
    sknfy = scn.skinify

    # initialize the mesh object
    mesh_name = context.selected_objects[0].name + "_mesh"
    obj_name = context.selected_objects[0].name + "_object"
    armature_object = context.object

    origin = context.object.location
    bone_selection = context.selected_pose_bones
    oldLocation = None
    oldRotation = None
    oldScale = None
    armature_object = scn.objects.active
    armature_object.select = True

    old_pose_pos = armature_object.data.pose_position
    bpy.ops.object.mode_set(mode='OBJECT')
    oldLocation = Vector(armature_object.location)
    oldRotation = Euler(armature_object.rotation_euler)
    oldScale = Vector(armature_object.scale)

    bpy.ops.object.rotation_clear(clear_delta=False)
    bpy.ops.object.location_clear(clear_delta=False)
    bpy.ops.object.scale_clear(clear_delta=False)
    if sknfy.apply_mod and sknfy.parent_armature:
        armature_object.data.pose_position = 'REST'

    scale = bpy.context.object.scale
    size = bpy.context.object.dimensions[2]

    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.select_all(action='DESELECT')

    bpy.ops.object.add(type='MESH', enter_editmode=False, location=origin)

    # get the mesh object
    ob = scn.objects.active
    ob.name = obj_name
    me = ob.data
    me.name = mesh_name

    # this way we fit mesh and bvh with armature modifier correctly

    alternate_scale_idx_list, rig_type = generate_edges(
                                                me, ob, bone_selection, scale, sknfy.connect_mesh,
                                                sknfy.connect_parents, sknfy.head_ornaments,
                                                sknfy.generate_all
                                                )

    generate_mesh(ob, size, sknfy.thickness, sknfy.finger_thickness, sknfy.sub_level,
                  sknfy.connect_mesh, sknfy.connect_parents, sknfy.generate_all,
                  sknfy.apply_mod, alternate_scale_idx_list, rig_type, bone_selection)

    # parent mesh with armature only if modifiers are applied
    if sknfy.apply_mod and sknfy.parent_armature:
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
        ob.select = True
        armature_object.select = True
        scn.objects.active = armature_object

        bpy.ops.object.parent_set(type='ARMATURE_AUTO')
        armature_object.data.pose_position = old_pose_pos
        armature_object.select = False
    else:
        bpy.ops.object.mode_set(mode='OBJECT')
        ob.location = oldLocation
        ob.rotation_euler = oldRotation
        ob.scale = oldScale
        ob.select = False
        armature_object.select = True
        scn.objects.active = armature_object

    armature_object.location = oldLocation
    armature_object.rotation_euler = oldRotation
    armature_object.scale = oldScale
    bpy.ops.object.mode_set(mode='POSE')

    return {'FINISHED'}, me


class BONE_OT_custom_shape(Operator):
    bl_idname = "object.skinify_rig"
    bl_label = "Skinify Rig"
    bl_description = "Creates a mesh object at the selected bones positions"
    bl_options = {'UNDO', 'INTERNAL'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        Mesh = main(context)
        if Mesh[0] == {'CANCELLED'}:
            self.report({'WARNING'}, Mesh[1])
            return {'CANCELLED'}
        else:
            self.report({'INFO'}, Mesh[1].name + " has been created")

            return {'FINISHED'}


class BONE_PT_custom_shape(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "bone"
    bl_label = "Skinify Rig"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.mode == 'POSE' and context.bone

    def draw(self, context):
        layout = self.layout
        scn = context.scene.skinify

        row = layout.row()
        row.operator("object.skinify_rig", text="Add Shape", icon='BONE_DATA')

        split = layout.split(percentage=0.3)
        split.label("Thickness:")
        split.prop(scn, "thickness", text="Body", icon='MOD_SKIN')
        split.prop(scn, "finger_thickness", text="Fingers", icon='HAND')

        split = layout.split(percentage=0.3)
        split.label("Mesh Density:")
        split.prop(scn, "sub_level", icon='MESH_ICOSPHERE')

        row = layout.row()
        row.prop(scn, "connect_mesh", icon='EDITMODE_HLT')
        row.prop(scn, "connect_parents", icon='CONSTRAINT_BONE')
        row = layout.row()
        row.prop(scn, "head_ornaments", icon='GROUP_BONE')
        row.prop(scn, "generate_all", icon='GROUP_BONE')
        row = layout.row()
        row.prop(scn, "apply_mod", icon='FILE_TICK')
        if scn.apply_mod:
            row = layout.row()
            row.prop(scn, "parent_armature", icon='POSE_HLT')


# define the scene properties in a group - call them with context.scene.skinify
class Skinify_Properties(PropertyGroup):
    sub_level = IntProperty(
            name="Sub level",
            min=0, max=4,
            default=1,
            description="Mesh density"
            )
    thickness = FloatProperty(
            name="Thickness",
            min=0.01,
            default=0.8,
            description="Adjust shape thickness"
            )
    finger_thickness = FloatProperty(
            name="Finger Thickness",
            min=0.01, max=1.0,
            default=0.25,
            description="Adjust finger thickness relative to body"
            )
    connect_mesh = BoolProperty(
            name="Solid Shape",
            default=False,
            description="Makes solid shape from bone chains"
            )
    connect_parents = BoolProperty(
            name="Fill Gaps",
            default=False,
            description="Fills the gaps between parented bones"
            )
    generate_all = BoolProperty(
            name="All Shapes",
            default=False,
            description="Generates shapes from all bones"
            )
    head_ornaments = BoolProperty(
            name="Head Ornaments",
            default=False,
            description="Includes head ornaments"
            )
    apply_mod = BoolProperty(
            name="Apply Modifiers",
            default=True,
            description="Applies Modifiers to mesh"
            )
    parent_armature = BoolProperty(
            name="Parent Armature",
            default=True,
            description="Applies mesh to Armature"
            )


# startup defaults

@persistent
def startup_init(dummy):
    init_props()


def register():
    bpy.utils.register_class(BONE_OT_custom_shape)
    bpy.utils.register_class(BONE_PT_custom_shape)
    bpy.utils.register_class(Skinify_Properties)

    bpy.types.Scene.skinify = PointerProperty(
                                    type=Skinify_Properties
                                    )
    # startup defaults
    bpy.app.handlers.load_post.append(startup_init)


def unregister():
    bpy.utils.unregister_class(BONE_OT_custom_shape)
    bpy.utils.unregister_class(BONE_PT_custom_shape)
    bpy.utils.unregister_class(Skinify_Properties)

    # cleanup the handler
    bpy.app.handlers.load_post.remove(startup_init)

    del bpy.types.Scene.skinify


if __name__ == "__main__":
    register()
