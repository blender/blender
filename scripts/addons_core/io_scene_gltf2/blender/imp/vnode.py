# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from itertools import chain
from mathutils import Vector, Quaternion, Matrix
from ...io.imp.gltf2_io_binary import BinaryData
from ..com.gltf2_blender_math import scale_rot_swap_matrix, nearby_signed_perm_matrix


def compute_vnodes(gltf):
    """Computes the tree of virtual nodes.
    Copies the glTF nodes into a tree of VNodes, then performs a series of
    passes to transform it into a form that we can import into Blender.
    """
    init_vnodes(gltf)
    mark_bones_and_armas(gltf)
    move_skinned_meshes(gltf)
    fixup_multitype_nodes(gltf)
    correct_cameras_and_lights(gltf)
    pick_bind_pose(gltf)
    prettify_bones(gltf)
    calc_bone_matrices(gltf)


class VNode:
    """A "virtual" node.
    These are what eventually get turned into nodes
    in the Blender scene.
    """
    # Types
    Object = 0
    Bone = 1
    DummyRoot = 2
    Inst = 3

    def __init__(self):
        self.name = None
        self.default_name = 'Node'  # fallback when no name
        self.children = []
        self.parent = None
        self.type = VNode.Object
        self.is_arma = False
        self.base_trs = (
            Vector((0, 0, 0)),
            Quaternion((1, 0, 0, 0)),
            Vector((1, 1, 1)),
        )
        # Additional rotations before/after the base TRS.
        # Allows per-vnode axis adjustment. See local_rotation.
        self.rotation_after = Quaternion((1, 0, 0, 0))
        self.rotation_before = Quaternion((1, 0, 0, 0))

        # Indices of the glTF node where the mesh, etc. came from.
        # (They can get moved around.)
        self.mesh_node_idx = None
        self.camera_node_idx = None
        self.light_node_idx = None

        # Store in which glTF scene(s) this node is used.
        self.scenes = []

    def trs(self):
        # (final TRS) = (rotation after) (base TRS) (rotation before)
        t, r, s = self.base_trs
        m = scale_rot_swap_matrix(self.rotation_before)
        return (
            self.rotation_after @ t,
            self.rotation_after @ r @ self.rotation_before,
            m @ s,
        )

    def base_locs_to_final_locs(self, base_locs):
        ra = self.rotation_after
        return [ra @ loc for loc in base_locs]

    def base_rots_to_final_rots(self, base_rots):
        ra, rb = self.rotation_after, self.rotation_before
        return [ra @ rot @ rb for rot in base_rots]

    def base_scales_to_final_scales(self, base_scales):
        m = scale_rot_swap_matrix(self.rotation_before)
        return [m @ scale for scale in base_scales]


def local_rotation(gltf, vnode_id, rot):
    """Appends a local rotation to vnode's world transform:
    (new world transform) = (old world transform) @ (rot)
    without changing the world transform of vnode's children.

    For correctness, rot must be a signed permutation of the axes
    (eg. (X Y Z)->(X -Z Y)) OR vnode's scale must always be uniform.
    """
    gltf.vnodes[vnode_id].rotation_before @= rot

    # Append the inverse rotation after children's TRS to cancel it out.
    rot_inv = rot.conjugated()
    for child in gltf.vnodes[vnode_id].children:
        gltf.vnodes[child].rotation_after = \
            rot_inv @ gltf.vnodes[child].rotation_after


def init_vnodes(gltf):
    # Map of all VNodes. The keys are arbitrary IDs.
    # Nodes coming from glTF use the index into gltf.data.nodes for an ID.
    gltf.vnodes = {}

    for i, pynode in enumerate(gltf.data.nodes or []):
        vnode = VNode()
        gltf.vnodes[i] = vnode
        vnode.name = pynode.name
        vnode.default_name = 'Node_%d' % i
        vnode.children = list(pynode.children or [])
        vnode.base_trs = get_node_trs(gltf, pynode)
        if pynode.mesh is not None:
            # Check if there is gpu_instancing extension
            if pynode.extensions and "EXT_mesh_gpu_instancing" in pynode.extensions.keys():
                manage_gpu_instancing(gltf, vnode, i, pynode.extensions['EXT_mesh_gpu_instancing'], pynode.mesh)
            else:
                vnode.mesh_node_idx = i
        if pynode.camera is not None:
            vnode.camera_node_idx = i
        if 'KHR_lights_punctual' in (pynode.extensions or {}):
            vnode.light_node_idx = i

    for id in gltf.vnodes:
        for child in gltf.vnodes[id].children:
            assert gltf.vnodes[child].parent is None
            gltf.vnodes[child].parent = id

    # Map the node/scene

    # Create a recursive function to find all the nodes in a scene
    # add assign the nodes to the scene(s)
    def add_nodes_to_scene(idx_scene, node):
        gltf.vnodes[node].scenes.append(idx_scene)
        for child in gltf.vnodes[node].children:
            add_nodes_to_scene(idx_scene, child)

    for idx_scene, scene in enumerate(gltf.data.scenes or []):
        for node in scene.nodes or []:
            add_nodes_to_scene(idx_scene, node)

    # Create a map of all scene / blender collections
    gltf.blender_collections = {}
    gltf.active_collection = bpy.context.collection
    gltf.blender_scenes = {}

    # Create needed scenes
    for idx_scene, scene in enumerate(gltf.data.scenes or []):
        # Create a new scene for all not default scenes
        if idx_scene != (gltf.data.scene or 0):
            new_scene = bpy.data.scenes.new(name=scene.name or "Scene %d" % idx_scene)
            gltf.blender_scenes[idx_scene] = new_scene
        else:
            gltf.blender_scenes[idx_scene] = bpy.context.scene


    # If we have only 1 scene, we can use the active collection
    # If we have multiple scenes, we create a collection for each scene (as child of active collection)
    # And if some nodes are orphan, we create a collection for them too
    if len(gltf.data.scenes or []) == 1:
        gltf.blender_collections[gltf.data.scene or 0] = bpy.context.collection
    elif len(gltf.data.scenes or []) > 1:
        for idx_scene, scene in enumerate(gltf.data.scenes or []):
            if gltf.import_settings['import_scene_as_collection'] is True:
                # Create a new collection for the scene
                collection = bpy.data.collections.new(gltf.data.scenes[idx_scene].name or "Scene %d" % idx_scene)
                # Link the collection to the active collection or the collection of the scene
                # Collection on current scene
                gltf.active_collection.children.link(collection)
                # Add the collection to the map
                gltf.blender_collections[idx_scene] = collection
            else:
                if idx_scene == gltf.data.scene:
                    gltf.blender_collections[idx_scene] = gltf.active_collection
                # No collection creation, so no linking
                # Link between glTF scene and blender scene is already done


    # Check if we have orphan nodes
    orphan_nodes = [node for node in gltf.vnodes if len(gltf.vnodes[node].scenes) == 0]
    if len(orphan_nodes) > 0:
        # Create a new collection for the orphan nodes
        orphan_collection = bpy.data.collections.new("Orphan Nodes")
        # Link the collection to the active collection
        gltf.active_collection.children.link(orphan_collection)
        # Add the collection to the map
        gltf.blender_collections[None] = orphan_collection


    # Inserting a root node will simplify things.
    roots = [id for id in gltf.vnodes if gltf.vnodes[id].parent is None]
    gltf.vnodes['root'] = VNode()
    gltf.vnodes['root'].type = VNode.DummyRoot
    gltf.vnodes['root'].default_name = 'Root'
    gltf.vnodes['root'].children = roots
    for root in roots:
        gltf.vnodes[root].parent = 'root'


def manage_gpu_instancing(gltf, vnode, i, ext, mesh_id):

    trans_list = BinaryData.get_data_from_accessor(gltf, ext['attributes'].get('TRANSLATION', None)) \
        if ext['attributes'].get('TRANSLATION', None) is not None else None

    rot_list = BinaryData.get_data_from_accessor(gltf, ext['attributes'].get('ROTATION', None)) \
        if ext['attributes'].get('ROTATION', None) is not None else None

    scale_list = BinaryData.get_data_from_accessor(gltf, ext['attributes'].get('SCALE', None)) \
        if ext['attributes'].get('SCALE', None) is not None else None

    # Retrieve the first available attribute to get the number of children
    val = next((elem for elem in [
        trans_list,
        rot_list,
        scale_list,
    ] if elem is not None), None)

    # Wwe can't have only custom properties
    if not val:
        return

    length = len(val)

    if trans_list is None:
        trans_list = [None] * length
    if rot_list is None:
        rot_list = [None] * length
    if scale_list is None:
        scale_list = [None] * length

    assert len(trans_list) == len(rot_list) == len(scale_list)

    for inst in range(length):
        inst_id = '%d' % i + "." + '%d' % inst
        inst_vnode = VNode()
        inst_vnode.type = VNode.Inst
        gltf.vnodes[inst_id] = inst_vnode
        inst_vnode.name = None
        inst_vnode.default_name = 'Node_' + inst_id
        inst_vnode.children = []
        inst_vnode.base_trs = get_inst_trs(gltf, trans_list[inst], rot_list[inst], scale_list[inst])
        inst_vnode.mesh_idx = mesh_id
        # Do not set scenes here, this will be handle later by recursive add_nodes_to_scene

        vnode.children.append(inst_id)


def get_inst_trs(gltf, trans, rot, scale):
    t = gltf.loc_gltf_to_blender(trans or [0, 0, 0])
    r = gltf.quaternion_gltf_to_blender(rot or [0, 0, 0, 1])
    s = gltf.scale_gltf_to_blender(scale or [1, 1, 1])
    return t, r, s


def get_node_trs(gltf, pynode):
    if pynode.matrix is not None:
        m = gltf.matrix_gltf_to_blender(pynode.matrix)
        return m.decompose()

    t = gltf.loc_gltf_to_blender(pynode.translation or [0, 0, 0])
    r = gltf.quaternion_gltf_to_blender(pynode.rotation or [0, 0, 0, 1])
    s = gltf.scale_gltf_to_blender(pynode.scale or [1, 1, 1])
    return t, r, s


def mark_bones_and_armas(gltf):
    """
    Mark nodes as armatures so that every node that is used as joint is a
    descendant of an armature. Mark everything between an armature and a
    joint as a bone.
    """
    for skin in gltf.data.skins or []:
        descendants = list(skin.joints)
        if skin.skeleton is not None:
            descendants.append(skin.skeleton)
        arma_id = deepest_common_ancestor(gltf, descendants)

        if arma_id in skin.joints:
            arma_id = gltf.vnodes[arma_id].parent

        if gltf.vnodes[arma_id].type != VNode.Bone:
            gltf.vnodes[arma_id].type = VNode.Object
            gltf.vnodes[arma_id].is_arma = True
            gltf.vnodes[arma_id].arma_name = skin.name or 'Armature'
            # Because the dummy root node is no more an dummy node, but a real armature object,
            # We need to set the scenes on the vnode
            gltf.vnodes[arma_id].scenes = list(
                set(chain.from_iterable(
                    gltf.vnodes[joint].scenes for joint in skin.joints
                ))
            )

        for joint in skin.joints:
            while joint != arma_id:
                gltf.vnodes[joint].type = VNode.Bone
                gltf.vnodes[joint].is_arma = False
                joint = gltf.vnodes[joint].parent

    # Mark the armature each bone is a descendant of.

    def visit(vnode_id, cur_arma):  # Depth-first walk
        vnode = gltf.vnodes[vnode_id]

        if vnode.is_arma:
            cur_arma = vnode_id
        elif vnode.type == VNode.Bone:
            vnode.bone_arma = cur_arma
        else:
            cur_arma = None

        for child in vnode.children:
            visit(child, cur_arma)

    visit('root', cur_arma=None)


def deepest_common_ancestor(gltf, vnode_ids):
    """Find the deepest (improper) ancestor of a set of vnodes."""
    path_to_ancestor = []  # path to deepest ancestor so far
    for vnode_id in vnode_ids:
        path = path_from_root(gltf, vnode_id)
        if not path_to_ancestor:
            path_to_ancestor = path
        else:
            path_to_ancestor = longest_common_prefix(path, path_to_ancestor)
    return path_to_ancestor[-1]


def path_from_root(gltf, vnode_id):
    """Returns the ids of all vnodes from the root to vnode_id."""
    path = []
    while vnode_id is not None:
        path.append(vnode_id)
        vnode_id = gltf.vnodes[vnode_id].parent
    path.reverse()
    return path


def longest_common_prefix(list1, list2):
    i = 0
    while i != min(len(list1), len(list2)):
        if list1[i] != list2[i]:
            break
        i += 1
    return list1[:i]


def move_skinned_meshes(gltf):
    """
    In glTF, where in the node hierarchy a skinned mesh is instantiated has
    no effect on its world space position: only the world transforms of the
    joints in its skin affect it.

    To do this in Blender:
     * Move a skinned mesh to become a child of the armature that skins it.
       Have to ensure the mesh and arma have the same world transform.
     * When we do mesh creation, we will also need to put all the verts in
       the bind pose in arma space.
    """
    ids = list(gltf.vnodes.keys())
    for id in ids:
        vnode = gltf.vnodes[id]

        if vnode.mesh_node_idx is None:
            continue

        skin = gltf.data.nodes[vnode.mesh_node_idx].skin
        if skin is None:
            continue

        pyskin = gltf.data.skins[skin]
        arma = gltf.vnodes[pyskin.joints[0]].bone_arma

        # First try moving the whole node if we can do it without
        # messing anything up.
        is_animated = (
            gltf.data.animations and
            isinstance(id, int) and
            gltf.data.nodes[id].animations
        )
        ok_to_move = (
            not is_animated and
            vnode.type == VNode.Object and
            not vnode.is_arma and
            not vnode.children and
            vnode.camera_node_idx is None and
            vnode.light_node_idx is None
        )
        if ok_to_move:
            reparent(gltf, id, new_parent=arma)
            vnode.base_trs = (
                Vector((0, 0, 0)),
                Quaternion((1, 0, 0, 0)),
                Vector((1, 1, 1)),
            )
            continue

        # Otherwise, create a new child of the arma and move
        # the mesh instance there, leaving the node behind.
        new_id = str(id) + '.skinned'
        gltf.vnodes[new_id] = VNode()
        gltf.vnodes[new_id].parent = arma
        gltf.vnodes[arma].children.append(new_id)
        gltf.vnodes[new_id].mesh_node_idx = vnode.mesh_node_idx
        gltf.vnodes[new_id].scenes = vnode.scenes
        vnode.mesh_node_idx = None


def reparent(gltf, vnode_id, new_parent):
    """Moves a VNode to a new parent."""
    vnode = gltf.vnodes[vnode_id]
    if vnode.parent == new_parent:
        return
    if vnode.parent is not None:
        parent_vnode = gltf.vnodes[vnode.parent]
        index = parent_vnode.children.index(vnode_id)
        del parent_vnode.children[index]
    vnode.parent = new_parent
    gltf.vnodes[new_parent].children.append(vnode_id)


def fixup_multitype_nodes(gltf):
    """
    Blender only lets each object have one of: an armature, a mesh, a
    camera, a light. Also bones cannot have any of these either. Find any
    nodes like this and move the mesh/camera/light onto new children.
    """
    ids = list(gltf.vnodes.keys())
    for id in ids:
        vnode = gltf.vnodes[id]

        needs_move = False

        if vnode.is_arma or vnode.type == VNode.Bone:
            needs_move = True

        if vnode.mesh_node_idx is not None:
            if needs_move:
                new_id = str(id) + '.mesh'
                gltf.vnodes[new_id] = VNode()
                gltf.vnodes[new_id].mesh_node_idx = vnode.mesh_node_idx
                gltf.vnodes[new_id].parent = id
                gltf.vnodes[new_id].scenes = vnode.scenes
                vnode.children.append(new_id)
                vnode.mesh_node_idx = None
            needs_move = True

        if vnode.camera_node_idx is not None:
            if needs_move:
                new_id = str(id) + '.camera'
                gltf.vnodes[new_id] = VNode()
                gltf.vnodes[new_id].camera_node_idx = vnode.camera_node_idx
                gltf.vnodes[new_id].parent = id
                gltf.vnodes[new_id].scenes = vnode.scenes
                vnode.children.append(new_id)
                vnode.camera_node_idx = None
            needs_move = True

        if vnode.light_node_idx is not None:
            if needs_move:
                new_id = str(id) + '.light'
                gltf.vnodes[new_id] = VNode()
                gltf.vnodes[new_id].light_node_idx = vnode.light_node_idx
                gltf.vnodes[new_id].parent = id
                gltf.vnodes[new_id].scenes = vnode.scenes
                vnode.children.append(new_id)
                vnode.light_node_idx = None
            needs_move = True


def correct_cameras_and_lights(gltf):
    """
    Depending on the coordinate change, lights and cameras might need to be
    rotated to match Blender conventions for which axes they point along.
    """
    if gltf.camera_correction is None:
        return

    for id, vnode in gltf.vnodes.items():
        needs_correction = \
            vnode.camera_node_idx is not None or \
            vnode.light_node_idx is not None

        if needs_correction:
            local_rotation(gltf, id, gltf.camera_correction)


def pick_bind_pose(gltf):
    """
    Pick the bind pose for all bones. Skinned meshes will be retargeted onto
    this bind pose during mesh creation.
    """
    if gltf.import_settings['guess_original_bind_pose']:
        # Record inverse bind matrices. We're going to milk them for information
        # about the original bind pose.
        inv_binds = {'root': Matrix.Identity(4)}
        for skin in gltf.data.skins or []:
            if skin.inverse_bind_matrices is None:
                continue

            # Assume inverse bind matrices are calculated relative to the skeleton
            skel = skin.skeleton
            if skel is not None:
                if skel in skin.joints:
                    skel = gltf.vnodes[skel].parent
                if skel not in inv_binds:
                    inv_binds[skel] = Matrix.Identity(4)

            skin_inv_binds = BinaryData.get_data_from_accessor(gltf, skin.inverse_bind_matrices)
            skin_inv_binds = [gltf.matrix_gltf_to_blender(m) for m in skin_inv_binds]
            for i, joint in enumerate(skin.joints):
                inv_binds[joint] = skin_inv_binds[i]

    for vnode_id in gltf.vnodes:
        vnode = gltf.vnodes[vnode_id]
        if vnode.type == VNode.Bone:
            # Initialize bind pose to default pose (from gltf.data.nodes)
            vnode.bind_trans = Vector(vnode.base_trs[0])
            vnode.bind_rot = Quaternion(vnode.base_trs[1])

            if gltf.import_settings['guess_original_bind_pose']:
                # Try to guess bind pose from inverse bind matrices
                if vnode_id in inv_binds and vnode.parent in inv_binds:
                    # (bind matrix) = (parent bind matrix) (bind local). Solve for bind local...
                    bind_local = inv_binds[vnode.parent] @ inv_binds[vnode_id].inverted_safe()
                    t, r, _s = bind_local.decompose()
                    vnode.bind_trans = t
                    vnode.bind_rot = r

            # Initialize editbones to match bind pose
            vnode.editbone_trans = Vector(vnode.bind_trans)
            vnode.editbone_rot = Quaternion(vnode.bind_rot)


def prettify_bones(gltf):
    """
    Prettify bone lengths/directions.
    """
    def visit(vnode_id, parent_rot=None):  # Depth-first walk
        vnode = gltf.vnodes[vnode_id]
        rot = None

        if vnode.type == VNode.Bone:
            vnode.bone_length = pick_bone_length(gltf, vnode_id)
            rot = pick_bone_rotation(gltf, vnode_id, parent_rot)
            if rot is not None:
                rotate_edit_bone(gltf, vnode_id, rot)

        for child in vnode.children:
            visit(child, parent_rot=rot)

    visit('root')


MIN_BONE_LENGTH = 0.004  # too small and bones get deleted


def pick_bone_length(gltf, bone_id):
    """Heuristic for bone length."""
    vnode = gltf.vnodes[bone_id]

    child_locs = [
        gltf.vnodes[child].editbone_trans
        for child in vnode.children
        if gltf.vnodes[child].type == VNode.Bone
    ]
    child_locs = [loc for loc in child_locs if loc.length > MIN_BONE_LENGTH]
    if child_locs:
        return min(loc.length for loc in child_locs)

    if gltf.vnodes[vnode.parent].type == VNode.Bone:
        return gltf.vnodes[vnode.parent].bone_length

    if vnode.editbone_trans.length > MIN_BONE_LENGTH:
        return vnode.editbone_trans.length

    return 1


def pick_bone_rotation(gltf, bone_id, parent_rot):
    """Heuristic for bone rotation.
    A bone's tip lies on its local +Y axis so rotating a bone let's us
    adjust the bone direction.
    """
    if bpy.app.debug_value == 100:
        return None

    if gltf.import_settings['bone_heuristic'] == 'BLENDER':
        return Quaternion((2**0.5 / 2, 2**0.5 / 2, 0, 0))
    elif gltf.import_settings['bone_heuristic'] in ['TEMPERANCE', 'FORTUNE']:
        return temperance(gltf, bone_id, parent_rot)


def temperance(gltf, bone_id, parent_rot):
    vnode = gltf.vnodes[bone_id]

    # Try to put our tip at the centroid of our children
    child_locs = [
        gltf.vnodes[child].editbone_trans
        for child in vnode.children
        if gltf.vnodes[child].type == VNode.Bone
    ]
    child_locs = [loc for loc in child_locs if loc.length > MIN_BONE_LENGTH]
    if child_locs:
        centroid = sum(child_locs, Vector((0, 0, 0)))
        rot = Vector((0, 1, 0)).rotation_difference(centroid)
        if gltf.import_settings['bone_heuristic'] == 'TEMPERANCE':
            # Snap to the local axes; required for local_rotation to be
            # accurate when vnode has a non-uniform scaling.
            # FORTUNE skips this, so it may look better, but may have errors.
            rot = nearby_signed_perm_matrix(rot).to_quaternion()
        return rot

    return parent_rot


def rotate_edit_bone(gltf, bone_id, rot):
    """Rotate one edit bone without affecting anything else."""
    gltf.vnodes[bone_id].editbone_rot @= rot
    # Cancel out the rotation so children aren't affected.
    rot_inv = rot.conjugated()
    for child_id in gltf.vnodes[bone_id].children:
        child = gltf.vnodes[child_id]
        if child.type == VNode.Bone:
            child.editbone_trans = rot_inv @ child.editbone_trans
            child.editbone_rot = rot_inv @ child.editbone_rot
    # Need to rotate the bone's final TRS by the same amount so skinning
    # isn't affected.
    local_rotation(gltf, bone_id, rot)


def calc_bone_matrices(gltf):
    """
    Calculate the transformations from bone space to arma space in the bind
    pose and in the edit bone pose.
    """
    def visit(vnode_id):  # Depth-first walk
        vnode = gltf.vnodes[vnode_id]
        if vnode.type == VNode.Bone:
            if gltf.vnodes[vnode.parent].type == VNode.Bone:
                parent_bind_mat = gltf.vnodes[vnode.parent].bind_arma_mat
                parent_editbone_mat = gltf.vnodes[vnode.parent].editbone_arma_mat
            else:
                parent_bind_mat = Matrix.Identity(4)
                parent_editbone_mat = Matrix.Identity(4)

            t, r = vnode.bind_trans, vnode.bind_rot
            local_to_parent = Matrix.Translation(t) @ Quaternion(r).to_matrix().to_4x4()
            vnode.bind_arma_mat = parent_bind_mat @ local_to_parent

            t, r = vnode.editbone_trans, vnode.editbone_rot
            local_to_parent = Matrix.Translation(t) @ Quaternion(r).to_matrix().to_4x4()
            vnode.editbone_arma_mat = parent_editbone_mat @ local_to_parent

        for child in vnode.children:
            visit(child)

    visit('root')
