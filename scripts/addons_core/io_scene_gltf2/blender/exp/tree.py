# SPDX-FileCopyrightText: 2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import uuid
import numpy as np
from mathutils import Quaternion, Matrix, Vector
from ...io.exp.user_extensions import export_user_extensions
from ...io.com import gltf2_io
from ...io.imp.gltf2_io_binary import BinaryData
from ...io.com import constants as gltf2_io_constants
from ...io.exp import binary_data as gltf2_io_binary_data
from ..com.blender_default import BLENDER_GLTF_SPECIAL_COLLECTION
from . import accessors as gltf2_blender_gather_accessors


class VExportNode:

    OBJECT = 1
    ARMATURE = 2
    BONE = 3
    LIGHT = 4
    CAMERA = 5
    COLLECTION = 6
    INSTANCE = 7  # For instances of GN

    INST_COLLECTION = 8

    # Parent type, to be set on child regarding its parent
    NO_PARENT = 54
    PARENT_OBJECT = 50
    PARENT_BONE = 51
    PARENT_BONE_RELATIVE = 52
    PARENT_ROOT_BONE = 53
    PARENT_BONE_BONE = 55

    # Children type
    # Is used to split instance collection into 2 categories:
    CHILDREN_REAL = 90
    CHILDREN_IS_IN_COLLECTION = 91

    def __init__(self):
        self.children = []
        self.children_type = {}  # Used for children of instance collection
        self.blender_type = None
        self.matrix_world = None
        self.parent_type = None

        self.blender_object = None
        self.blender_bone = None
        self.leaf_reference = None  # For leaf bones only

        self.default_hide_viewport = False  # Need to store the default value for meshes in case of animation baking on armature

        self.force_as_empty = False  # Used for instancer display

        # Only for bone/bone and object parented to bone
        self.parent_bone_uuid = None

        # Only for bones
        self.use_deform = None

        # Only for armature
        self.bones = {}

        # For deformed object
        self.armature = None  # for deformed object and for bone
        self.skin = None

        # glTF
        self.node = None

        # For mesh instance data of GN instances
        self.data = None
        self.materials = None

        self.is_instancer = False

    def add_child(self, uuid):
        self.children.append(uuid)

    def set_blender_data(self, blender_object, blender_bone):
        self.blender_object = blender_object
        self.blender_bone = blender_bone

    def recursive_display(self, tree, mode):
        if mode == "simple":
            for c in self.children:
                print(
                    tree.nodes[c].uuid,
                    self.blender_object.name if self.blender_object is not None else "GN" +
                    self.data.name,
                    "/",
                    self.blender_bone.name if self.blender_bone else "",
                    "-->",
                    tree.nodes[c].blender_object.name if tree.nodes[c].blender_object else "GN" +
                    tree.nodes[c].data.name,
                    "/",
                    tree.nodes[c].blender_bone.name if tree.nodes[c].blender_bone else "")
                tree.nodes[c].recursive_display(tree, mode)


class VExportTree:
    def __init__(self, export_settings):
        self.nodes = {}
        self.roots = []

        self.export_settings = export_settings

        self.tree_troncated = False

        self.axis_basis_change = Matrix.Identity(4)
        if self.export_settings['gltf_yup']:
            self.axis_basis_change = Matrix(
                ((1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, -1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))

    def add_node(self, node):
        self.nodes[node.uuid] = node

    def add_children(self, uuid_parent, uuid_child):
        self.nodes[uuid_parent].add_child(uuid_child)

    def construct(self, blender_scene):
        bpy.context.window.scene = blender_scene

        # Make sure the active object is in object mode
        if bpy.context.active_object and bpy.context.active_object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')

        depsgraph = bpy.context.evaluated_depsgraph_get()

        # Gather parent/children information once, as calling bobj.children is
        #   very expensive operation : takes O(len(bpy.data.objects)) time.
        # TODO : In case of full collection export, we should add children / collection in the same way
        blender_children = dict()
        for bobj in bpy.data.objects:
            bparent = bobj.parent
            blender_children.setdefault(bobj, [])
            blender_children.setdefault(bparent, []).append(bobj)

        if self.export_settings['gltf_hierarchy_full_collections'] is False:
            scene_eval = blender_scene.evaluated_get(depsgraph=depsgraph)
            for blender_object in [obj.original for obj in scene_eval.objects if obj.parent is None]:
                self.recursive_node_traverse(blender_object, None, None, Matrix.Identity(4), False, blender_children)
        else:
            if self.export_settings['gltf_collection']:
                # Collection exporter
                self.recursive_node_traverse(
                    bpy.data.collections[self.export_settings['gltf_collection']],
                    None,
                    None,
                    Matrix.Identity(4),
                    False,
                    blender_children,
                    is_collection=True)
            else:
                # Scene / classic export
                self.recursive_node_traverse(
                    blender_scene.collection,
                    None,
                    None,
                    Matrix.Identity(4),
                    False,
                    blender_children,
                    is_collection=True)

    def recursive_node_traverse(
            self,
            blender_object,
            blender_bone,
            parent_uuid,
            parent_coll_matrix_world,
            delta,
            blender_children,
            armature_uuid=None,
            dupli_world_matrix=None,
            data=None,
            original_object=None,
            is_collection=False,
            is_children_in_collection=False):
        node = VExportNode()
        node.uuid = str(uuid.uuid4())
        node.parent_uuid = parent_uuid
        node.set_blender_data(blender_object, blender_bone)
        if blender_object is None:
            node.data = data
            node.original_object = original_object

        # add to parent if needed
        if parent_uuid is not None:
            self.add_children(parent_uuid, node.uuid)

            # 2 cases where we will need to store the fact that children are in collection or a real children
            #       1. GN instance
            #       2. Old Dupli vertices feature
            # For any other case, children are real children
            if (self.nodes[parent_uuid].blender_type == VExportNode.INST_COLLECTION or original_object is not None) or (self.nodes[parent_uuid].blender_type !=
                                                                                                                        VExportNode.COLLECTION and self.nodes[parent_uuid].blender_object is not None and self.nodes[parent_uuid].blender_object.is_instancer is True):
                self.nodes[parent_uuid].children_type[node.uuid] = VExportNode.CHILDREN_IS_IN_COLLECTION if is_children_in_collection is True else VExportNode.CHILDREN_REAL
            else:
                # We are in a regular case where children are real children
                self.nodes[parent_uuid].children_type[node.uuid] = VExportNode.CHILDREN_REAL
        else:
            self.roots.append(node.uuid)

        # Set blender type
        if blender_object is None:  # GN instance
            node.blender_type = VExportNode.INSTANCE
        elif blender_bone is not None:
            node.blender_type = VExportNode.BONE
            self.nodes[armature_uuid].bones[blender_bone.name] = node.uuid
            node.use_deform = blender_bone.id_data.data.bones[blender_bone.name].use_deform
        elif is_collection is True:
            node.blender_type = VExportNode.COLLECTION
        elif blender_object.type == "ARMATURE":
            node.blender_type = VExportNode.ARMATURE
            node.default_hide_viewport = blender_object.hide_viewport
        elif blender_object.type == "CAMERA":
            node.blender_type = VExportNode.CAMERA
        elif blender_object.type == "LIGHT":
            node.blender_type = VExportNode.LIGHT
        elif blender_object.instance_type == "COLLECTION":
            node.blender_type = VExportNode.INST_COLLECTION
            node.default_hide_viewport = blender_object.hide_viewport
        else:
            node.blender_type = VExportNode.OBJECT
            node.default_hide_viewport = blender_object.hide_viewport

        # For meshes with armature modifier (parent is armature), keep armature uuid
        if node.blender_type == VExportNode.OBJECT:
            modifiers = {m.type: m for m in blender_object.modifiers}
            if "ARMATURE" in modifiers and modifiers["ARMATURE"].object is not None:
                if parent_uuid is None or not self.nodes[parent_uuid].blender_type == VExportNode.ARMATURE:
                    # correct workflow is to parent skinned mesh to armature, but ...
                    # all users don't use correct workflow
                    self.export_settings['log'].warning(
                        "Armature must be the parent of skinned mesh"
                        "Armature is selected by its name, but may be false in case of instances"
                    )
                    # Search an armature by name, and use the first found
                    # This will be done after all objects are setup
                    node.armature_needed = modifiers["ARMATURE"].object.name
                else:
                    node.armature = parent_uuid

        # For bones, store uuid of armature
        if blender_bone is not None:
            node.armature = armature_uuid

        # for bone/bone parenting, store parent, this will help armature tree management
        if parent_uuid is not None and self.nodes[parent_uuid].blender_type == VExportNode.BONE and node.blender_type == VExportNode.BONE:
            node.parent_bone_uuid = parent_uuid

        # Objects parented to bone
        if parent_uuid is not None and self.nodes[parent_uuid].blender_type == VExportNode.BONE and node.blender_type != VExportNode.BONE:
            node.parent_bone_uuid = parent_uuid

        # World Matrix

        # Delta is used when rest transforms are used for armatures
        # Any children of objects parented to bones must have this delta (for grandchildren, etc...)
        new_delta = False

        # Store World Matrix for objects
        if dupli_world_matrix is not None:
            node.matrix_world = dupli_world_matrix
        elif node.blender_type in [VExportNode.OBJECT, VExportNode.COLLECTION, VExportNode.INST_COLLECTION, VExportNode.ARMATURE, VExportNode.CAMERA, VExportNode.LIGHT]:
            # Matrix World of object is expressed based on collection instance objects are
            # So real world matrix is collection world_matrix @ "world_matrix" of object
            if is_collection:
                node.matrix_world = parent_coll_matrix_world.copy()
            else:
                node.matrix_world = parent_coll_matrix_world @ blender_object.matrix_world.copy()

            # If object is parented to bone, and Rest pose is used for Armature, we need to keep the world matrix transformed relative relative to rest pose,
            # not the current world matrix (relation to pose)
            if parent_uuid and self.nodes[parent_uuid].blender_type == VExportNode.BONE and self.export_settings['gltf_rest_position_armature'] is True:
                _blender_bone = self.nodes[parent_uuid].blender_bone
                _pose = self.nodes[self.nodes[parent_uuid].armature].matrix_world @ _blender_bone.matrix @ self.axis_basis_change
                _rest = self.nodes[self.nodes[parent_uuid].armature].matrix_world @ _blender_bone.bone.matrix_local @ self.axis_basis_change
                _delta = _pose.inverted_safe() @ node.matrix_world
                node.original_matrix_world = node.matrix_world.copy()
                node.matrix_world = _rest @ _delta
                new_delta = True

            if node.blender_type == VExportNode.CAMERA and self.export_settings['gltf_cameras']:
                if self.export_settings['gltf_yup']:
                    correction = Quaternion((2**0.5 / 2, -2**0.5 / 2, 0.0, 0.0))
                else:
                    correction = Matrix.Identity(4).to_quaternion()
                node.matrix_world @= correction.to_matrix().to_4x4()
            elif node.blender_type == VExportNode.LIGHT and self.export_settings['gltf_lights']:
                if self.export_settings['gltf_yup']:
                    correction = Quaternion((2**0.5 / 2, -2**0.5 / 2, 0.0, 0.0))
                else:
                    correction = Matrix.Identity(4).to_quaternion()
                node.matrix_world @= correction.to_matrix().to_4x4()
        elif node.blender_type == VExportNode.BONE:
            if self.export_settings['gltf_rest_position_armature'] is False:
                # Use pose bone for TRS
                node.matrix_world = self.nodes[node.armature].matrix_world @ blender_bone.matrix
                if self.export_settings['gltf_leaf_bone'] is True:
                    node.matrix_world_tail = self.nodes[node.armature].matrix_world @ Matrix.Translation(
                        blender_bone.tail)
                    node.matrix_world_tail = node.matrix_world_tail @ self.axis_basis_change
            else:
                # Use edit bone for TRS --> REST pose will be used
                node.matrix_world = self.nodes[node.armature].matrix_world @ blender_bone.bone.matrix_local
                # Tail will be set after, as we need to be in edit mode
            node.matrix_world = node.matrix_world @ self.axis_basis_change

        if delta is True:
            _pose_parent = self.nodes[parent_uuid].original_matrix_world
            _rest_parent = self.nodes[parent_uuid].matrix_world
            _delta = _pose_parent.inverted_safe() @ node.matrix_world
            node.original_matrix_world = node.matrix_world.copy()
            node.matrix_world = _rest_parent @ _delta

        # Force empty ?
        # For duplis, if instancer is not display, we should create an empty
        if blender_object and is_collection is False and blender_object.is_instancer is True and blender_object.show_instancer_for_render is False:
            node.force_as_empty = True

        # Storing this node
        self.add_node(node)

        ###### Manage children ######

        # GN instance have no children
        if blender_object is None:
            return

        # standard children (of object, or of instance collection)
        if blender_bone is None and is_collection is False and blender_object.is_instancer is False:
            for child_object in blender_children[blender_object]:
                if child_object.parent_bone and child_object.parent_type in ("BONE", "BONE_RELATIVE"):
                    # Object parented to bones
                    # Will be manage later
                    continue
                else:
                    # Classic parenting

                    # If we export full collection hierarchy, we need to ignore children that
                    # are not in the same collection
                    if self.export_settings['gltf_hierarchy_full_collections'] is True:
                        if child_object.users_collection[0].name != blender_object.users_collection[0].name:
                            continue

                    self.recursive_node_traverse(
                        child_object,
                        None,
                        node.uuid,
                        parent_coll_matrix_world,
                        new_delta or delta,
                        blender_children)

        # Collections
        if is_collection is False and (blender_object.instance_type ==
                                       'COLLECTION' and blender_object.instance_collection):
            if self.export_settings['gltf_hierarchy_full_collections'] is False:
                for dupli_object in blender_object.instance_collection.all_objects:
                    if dupli_object.parent is not None:
                        continue
                    self.recursive_node_traverse(
                        dupli_object,
                        None,
                        node.uuid,
                        node.matrix_world,
                        new_delta or delta,
                        blender_children,
                        is_children_in_collection=True)

                # Some objects are parented to instance collection
                for child in blender_children[blender_object]:
                    self.recursive_node_traverse(child, None, node.uuid, node.matrix_world,
                                                 new_delta or delta, blender_children)

            else:
                # Manage children objects
                for child in blender_object.instance_collection.objects:
                    if child.users_collection[0].name != blender_object.name:
                        continue
                    self.recursive_node_traverse(child, None, node.uuid, node.matrix_world,
                                                 new_delta or delta, blender_children)
                # Manage children collections
                for child in blender_object.instance_collection.children:
                    self.recursive_node_traverse(
                        child,
                        None,
                        node.uuid,
                        node.matrix_world,
                        new_delta or delta,
                        blender_children,
                        is_collection=True)

        if is_collection is True:  # Only for gltf_hierarchy_full_collections == True
            # Manage children objects
            for child in blender_object.objects:
                if child.users_collection[0].name != blender_object.name:
                    continue

                # Keep only object if it has no parent, or parent is not in the collection
                if not (child.parent is None or child.parent.users_collection[0].name != blender_object.name):
                    continue

                self.recursive_node_traverse(child, None, node.uuid, node.matrix_world,
                                             new_delta or delta, blender_children)
            # Manage children collections
            for child in blender_object.children:
                self.recursive_node_traverse(
                    child,
                    None,
                    node.uuid,
                    node.matrix_world,
                    new_delta or delta,
                    blender_children,
                    is_collection=True)

        # Armature : children are bones with no parent
        if is_collection is False and blender_object.type == "ARMATURE" and blender_bone is None:
            for b in [b for b in blender_object.pose.bones if b.parent is None]:
                self.recursive_node_traverse(
                    blender_object,
                    b,
                    node.uuid,
                    parent_coll_matrix_world,
                    new_delta or delta,
                    blender_children,
                    node.uuid)

        # Bones
        if is_collection is False and blender_object.type == "ARMATURE" and blender_bone is not None:
            for b in blender_bone.children:
                self.recursive_node_traverse(
                    blender_object,
                    b,
                    node.uuid,
                    parent_coll_matrix_world,
                    new_delta or delta,
                    blender_children,
                    armature_uuid)

        # Object parented to bone
        if is_collection is False and blender_bone is not None:
            for child_object in [c for c in blender_children[blender_object] if c.parent_type ==
                                 "BONE" and c.parent_bone is not None and c.parent_bone == blender_bone.name]:
                self.recursive_node_traverse(
                    child_object,
                    None,
                    node.uuid,
                    parent_coll_matrix_world,
                    new_delta or delta,
                    blender_children)

        # Duplis
        if is_collection is False and blender_object.is_instancer is True and blender_object.instance_type != 'COLLECTION':
            depsgraph = bpy.context.evaluated_depsgraph_get()
            for (
                dupl,
                mat) in [
                (dup.object.original,
                 dup.matrix_world.copy()) for dup in depsgraph.object_instances if dup.parent and id(
                    dup.parent.original) == id(blender_object)]:
                self.recursive_node_traverse(
                    dupl,
                    None,
                    node.uuid,
                    parent_coll_matrix_world,
                    new_delta or delta,
                    blender_children,
                    dupli_world_matrix=mat)

        # Geometry Nodes instances
        # Make sure to not check instances for instanced collection, because we
        # will export what's inside the collection twice
        if self.export_settings['gltf_gn_mesh'] is True and node.blender_type == VExportNode.OBJECT:
            # Do not force export as empty
            # Because GN graph can have both geometry and instances
            depsgraph = bpy.context.evaluated_depsgraph_get()
            eval = blender_object.evaluated_get(depsgraph)
            for inst in depsgraph.object_instances:  # use only as iterator
                if inst.parent == eval:
                    if not inst.is_instance:
                        continue
                    if type(inst.object.data).__name__ == "Mesh" and len(inst.object.data.vertices) == 0:
                        continue  # This is nested instances, and this mesh has no vertices, so is an instancier for other instances
                    node.is_instancer = True
                    self.recursive_node_traverse(
                        None,
                        None,
                        node.uuid,
                        parent_coll_matrix_world,
                        new_delta or delta,
                        blender_children,
                        dupli_world_matrix=inst.matrix_world.copy(),
                        data=inst.object.data,
                        original_object=blender_object,
                        is_children_in_collection=True)

    def get_all_objects(self):
        return [n.uuid for n in self.nodes.values() if n.blender_type != VExportNode.BONE]

    def get_all_bones(self, uuid):  # For armature only
        if not hasattr(self.nodes[uuid], "all_bones"):
            if self.nodes[uuid].blender_type == VExportNode.ARMATURE:
                def recursive_get_all_bones(uuid):
                    total = []
                    if self.nodes[uuid].blender_type == VExportNode.BONE:
                        total.append(uuid)
                        for child_uuid in self.nodes[uuid].children:
                            total.extend(recursive_get_all_bones(child_uuid))

                    return total

                tot = []
                for c_uuid in self.nodes[uuid].children:
                    tot.extend(recursive_get_all_bones(c_uuid))
                self.nodes[uuid].all_bones = tot
                return tot  # Not really needed to return, we are just baking it before export really starts
            else:
                self.nodes[uuid].all_bones = []
                return []
        else:
            return self.nodes[uuid].all_bones

    def get_root_bones_uuid(self, uuid, cache=True):  # For armature only
        if not hasattr(self.nodes[uuid], "root_bones_uuid"):
            if self.nodes[uuid].blender_type == VExportNode.ARMATURE:
                all_armature_children = self.nodes[uuid].children
                root_bones_uuid = [
                    c for c in all_armature_children if self.nodes[c].blender_type == VExportNode.BONE]
                if self.export_settings['gltf_def_bones'] is True:
                    root_bones_uuid = [
                        c for c in root_bones_uuid if self.nodes[c].use_deform is True]
                if cache:
                    self.nodes[uuid].root_bones_uuid = root_bones_uuid
                # in case of caching (first call), we return the value even if not needed
                # (because we call the function only to cache the value)
                return root_bones_uuid
            else:
                self.nodes[uuid].root_bones_uuid = []
                return []
        else:
            return self.nodes[uuid].root_bones_uuid

    def get_all_node_of_type(self, node_type):
        return [n.uuid for n in self.nodes.values() if n.blender_type == node_type]

    def display(self, mode):
        if mode == "simple":
            for n in self.roots:
                print(
                    self.nodes[n].uuid,
                    "Root",
                    self.nodes[n].blender_object.name if self.nodes[n].blender_object else "GN instance",
                    "/",
                    self.nodes[n].blender_bone.name if self.nodes[n].blender_bone else "")
                self.nodes[n].recursive_display(self, mode)

    def filter_tag(self):
        roots = self.roots.copy()
        for r in roots:
            self.recursive_filter_tag(r, None)

    def filter_perform(self):
        roots = self.roots.copy()
        for r in roots:
            self.recursive_filter(r, None)  # Root, so no parent

    def filter(self):
        self.filter_tag()
        export_user_extensions('gather_tree_filter_tag_hook', self.export_settings, self)
        self.filter_perform()
        self.remove_empty_collections()  # Used only when exporting full collection hierarchy
        self.remove_filtered_nodes()

    def recursive_filter_tag(self, uuid, parent_keep_tag):
        # parent_keep_tag is for collection instance
        # some properties (selection, visibility, renderability)
        # are defined at collection level, and we need to use these values
        # for all objects of the collection instance.
        # But some properties (camera, lamp ...) are not defined at collection level
        if parent_keep_tag is None:
            self.nodes[uuid].keep_tag = self.node_filter_not_inheritable_is_kept(
                uuid) and self.node_filter_inheritable_is_kept(uuid)
        elif parent_keep_tag is True:
            self.nodes[uuid].keep_tag = self.node_filter_not_inheritable_is_kept(uuid)
        elif parent_keep_tag is False:
            self.nodes[uuid].keep_tag = False
        else:
            self.export_settings['log'].error("This should not happen")

        for child in self.nodes[uuid].children:
            if self.nodes[uuid].blender_type == VExportNode.INST_COLLECTION or self.nodes[uuid].is_instancer:
                # We need to split children into 2 categories: real children, and objects inside the collection
                if self.nodes[uuid].children_type[child] == VExportNode.CHILDREN_IS_IN_COLLECTION:
                    self.recursive_filter_tag(child, self.nodes[uuid].keep_tag)
                else:
                    self.recursive_filter_tag(child, parent_keep_tag)
            else:
                self.recursive_filter_tag(child, parent_keep_tag)

    def recursive_filter(self, uuid, parent_kept_uuid):
        children = self.nodes[uuid].children.copy()

        new_parent_kept_uuid = None
        if self.nodes[uuid].keep_tag is False:
            new_parent_kept_uuid = parent_kept_uuid
            # Need to modify tree
            if self.nodes[uuid].parent_uuid is not None:
                self.nodes[self.nodes[uuid].parent_uuid].children.remove(uuid)
            else:
                # Remove from root
                self.roots.remove(uuid)
        else:
            new_parent_kept_uuid = uuid

            # If parent_uuid is not parent_kept_uuid, we need to modify children list of parent_kept_uuid
            if parent_kept_uuid != self.nodes[uuid].parent_uuid and parent_kept_uuid is not None:
                self.tree_troncated = True
                self.nodes[parent_kept_uuid].children.append(uuid)

            # If parent_kept_uuid is None, and parent_uuid was not, add to root list
            if self.nodes[uuid].parent_uuid is not None and parent_kept_uuid is None:
                self.tree_troncated = True
                self.roots.append(uuid)

            # Modify parent uuid
            self.nodes[uuid].parent_uuid = parent_kept_uuid

        for child in children:
            self.recursive_filter(child, new_parent_kept_uuid)

    def node_filter_not_inheritable_is_kept(self, uuid):
        # Export Camera or not
        if self.nodes[uuid].blender_type == VExportNode.CAMERA:
            if self.export_settings['gltf_cameras'] is False:
                return False

        # Export Lamp or not
        if self.nodes[uuid].blender_type == VExportNode.LIGHT:
            if self.export_settings['gltf_lights'] is False:
                return False

        # Export deform bones only
        if self.nodes[uuid].blender_type == VExportNode.BONE:
            if self.export_settings['gltf_def_bones'] is True and self.nodes[uuid].use_deform is False:
                # Check if bone has some objected parented to bone. We need to keep it in
                # that case, even if this is not a def bone
                if len([c for c in self.nodes[uuid].children if self.nodes[c].blender_type != VExportNode.BONE]) != 0:
                    return True
                return False

        return True

    def node_filter_inheritable_is_kept(self, uuid):

        if self.nodes[uuid].blender_object is None:
            # geometry node instances
            return True

        if self.nodes[uuid].blender_type == VExportNode.COLLECTION:
            # Collections, can't be filtered => we always keep them
            return True

        if self.export_settings['gltf_selected'] and self.nodes[uuid].blender_object.select_get() is False:
            return False

        if self.export_settings['gltf_visible']:
            # The eye in outliner (object)
            if self.nodes[uuid].blender_object.visible_get() is False:
                return False

            # The screen in outliner (object)
            if self.nodes[uuid].blender_object.hide_viewport is True:
                return False

            # The screen in outliner (collections)
            if all([c.hide_viewport for c in self.nodes[uuid].blender_object.users_collection]):
                return False

        # The camera in outliner (object)
        if self.export_settings['gltf_renderable']:
            if self.nodes[uuid].blender_object.hide_render is True:
                return False

            # The camera in outliner (collections)
            if all([c.hide_render for c in self.nodes[uuid].blender_object.users_collection]):
                return False

        # If we are given a collection, use all objects from it
        if self.export_settings['gltf_collection']:
            local_collection = bpy.data.collections.get((self.export_settings['gltf_collection'], None))
            if not local_collection:
                return False
            found = any(x == self.nodes[uuid].blender_object for x in local_collection.all_objects)
            if not found:
                return False
        else:
            if self.export_settings['gltf_active_collection'] and not self.export_settings['gltf_active_collection_with_nested']:
                found = any(x == self.nodes[uuid].blender_object for x in bpy.context.collection.objects)
                if not found:
                    return False

            if self.export_settings['gltf_active_collection'] and self.export_settings['gltf_active_collection_with_nested']:
                found = any(x == self.nodes[uuid].blender_object for x in bpy.context.collection.all_objects)
                if not found:
                    return False

        if BLENDER_GLTF_SPECIAL_COLLECTION in bpy.data.collections and self.nodes[uuid].blender_object.name in \
                bpy.data.collections[BLENDER_GLTF_SPECIAL_COLLECTION].objects:
            return False

        if self.export_settings['gltf_armature_object_remove'] is True:
            # If we remove the Armature object
            if self.nodes[uuid].blender_type == VExportNode.ARMATURE:
                self.nodes[uuid].arma_exported = True
                return False

        return True

    def remove_filtered_nodes(self):
        if self.export_settings['gltf_armature_object_remove'] is True:
            # If we remove the Armature object
            self.nodes = {k: n for (k, n) in self.nodes.items() if n.keep_tag is True or (
                n.keep_tag is False and n.blender_type == VExportNode.ARMATURE)}
        else:
            self.nodes = {k: n for (k, n) in self.nodes.items() if n.keep_tag is True}

    def remove_empty_collections(self):
        def recursive_remove_empty_collections(uuid):
            if self.nodes[uuid].blender_type == VExportNode.COLLECTION:
                if len(self.nodes[uuid].children) == 0:
                    if self.nodes[uuid].parent_uuid is not None:
                        self.nodes[self.nodes[uuid].parent_uuid].children.remove(uuid)
                    else:
                        self.roots.remove(uuid)
                    self.nodes[uuid].keep_tag = False
                else:
                    for c in self.nodes[uuid].children:
                        recursive_remove_empty_collections(c)

        roots = self.roots.copy()
        for r in roots:
            recursive_remove_empty_collections(r)

    def search_missing_armature(self):
        for n in [n for n in self.nodes.values() if hasattr(n, "armature_needed") is True]:
            candidates = [i for i in self.nodes.values() if i.blender_type ==
                          VExportNode.ARMATURE and i.blender_object.name == n.armature_needed]
            if len(candidates) > 0:
                n.armature = candidates[0].uuid
            del n.armature_needed

    def bake_armature_bone_list(self):

        if self.export_settings['gltf_leaf_bone'] is True:
            self.add_leaf_bones()

        # Used to store data in armature vnode
        # If armature is removed from export
        # Data are still available, even if armature is not exported (so bones are re-parented)
        for n in [n for n in self.nodes.values() if n.blender_type == VExportNode.ARMATURE]:

            self.get_all_bones(n.uuid)
            self.get_root_bones_uuid(n.uuid)

    def add_leaf_bones(self):

        # If we are using rest pose, we need to get tail of editbone, going to edit mode for each armature
        if self.export_settings['gltf_rest_position_armature'] is True:
            for obj_uuid in [n for n in self.nodes if self.nodes[n].blender_type == VExportNode.ARMATURE]:
                armature = self.nodes[obj_uuid].blender_object
                bpy.context.view_layer.objects.active = armature
                bpy.ops.object.mode_set(mode="EDIT")

                for bone in armature.data.edit_bones:
                    if len(bone.children) == 0:

                        # If we are exporting only deform bones, we need to check if this bone is a def bone
                        if self.export_settings['gltf_def_bones'] is True \
                            and bone.use_deform is False:
                                continue

                        self.nodes[self.nodes[obj_uuid].bones[bone.name]
                                   ].matrix_world_tail = armature.matrix_world @ Matrix.Translation(bone.tail) @ self.axis_basis_change

                bpy.ops.object.mode_set(mode="OBJECT")

        for bone_uuid in [n for n in self.nodes if self.nodes[n].blender_type == VExportNode.BONE
                          and len(self.nodes[n].children) == 0]:

            # If we are exporting only deform bones, we need to check if this bone is a def bone
            if self.export_settings['gltf_def_bones'] is True \
                and self.nodes[bone_uuid].use_deform is False:
                    continue

            bone_node = self.nodes[bone_uuid]

            # Add a new node
            node = VExportNode()
            node.uuid = str(uuid.uuid4())
            node.parent_uuid = bone_uuid
            node.parent_bone_uuid = bone_uuid
            node.blender_object = bone_node.blender_object
            node.armature = bone_node.armature
            node.blender_type = VExportNode.BONE
            node.leaf_reference = bone_uuid
            node.keep_tag = True

            node.matrix_world = bone_node.matrix_world_tail.copy()

            self.add_children(bone_uuid, node.uuid)
            self.add_node(node)

    def add_neutral_bones(self):
        added_armatures = []
        for n in [n for n in self.nodes.values() if
                  n.armature is not None and
                  n.armature in self.nodes and
                  n.blender_type == VExportNode.OBJECT and
                  n.blender_object.type == "MESH" and
                  hasattr(self.nodes[n.armature], "need_neutral_bone")]:  # all skin meshes objects where neutral bone is needed
            # Only for meshes, as curve can't have skin data (no weights pain available)

            # Be sure to add it to really exported meshes
            if n.node.skin is None:
                self.export_settings['log'].warning(
                    "{} has no skin, skipping adding neutral bone data on it.".format(
                        n.blender_object.name))
                continue

            if n.armature not in added_armatures:

                added_armatures.append(n.armature)  # Make sure to not insert 2 times the neural bone

                # First add a new node
                trans, rot, sca = self.axis_basis_change.decompose()
                translation, rotation, scale = (None, None, None)
                if trans[0] != 0.0 or trans[1] != 0.0 or trans[2] != 0.0:
                    translation = [trans[0], trans[1], trans[2]]
                if rot[0] != 1.0 or rot[1] != 0.0 or rot[2] != 0.0 or rot[3] != 0.0:
                    rotation = [rot[1], rot[2], rot[3], rot[0]]
                if sca[0] != 1.0 or sca[1] != 1.0 or sca[2] != 1.0:
                    scale = [sca[0], sca[1], sca[2]]
                neutral_bone = gltf2_io.Node(
                    camera=None,
                    children=None,
                    extensions=None,
                    extras=None,
                    matrix=None,
                    mesh=None,
                    name='neutral_bone',
                    rotation=rotation,
                    scale=scale,
                    skin=None,
                    translation=translation,
                    weights=None
                )
                # Add it to child list of armature
                self.nodes[n.armature].node.children.append(neutral_bone)

                # Add it to joint list
                n.node.skin.joints.append(neutral_bone)

                # Need to add an InverseBindMatrix
                array = BinaryData.decode_accessor_internal(n.node.skin.inverse_bind_matrices)

                inverse_bind_matrix = (
                    self.axis_basis_change @ self.nodes[n.armature].matrix_world_armature).inverted_safe()

                matrix = []
                for column in range(0, 4):
                    for row in range(0, 4):
                        matrix.append(inverse_bind_matrix[row][column])

                array = np.append(array, np.array([matrix]), axis=0)
                binary_data = gltf2_io_binary_data.BinaryData.from_list(
                    array.flatten(), gltf2_io_constants.ComponentType.Float)
                n.node.skin.inverse_bind_matrices = gltf2_blender_gather_accessors.gather_accessor(
                    binary_data,
                    gltf2_io_constants.ComponentType.Float,
                    len(array.flatten()) // gltf2_io_constants.DataType.num_elements(gltf2_io_constants.DataType.Mat4),
                    None,
                    None,
                    gltf2_io_constants.DataType.Mat4,
                    self.export_settings
                )

    def get_unused_skins(self):
        from .skins import gather_skin
        skins = []
        for n in [n for n in self.nodes.values() if n.blender_type == VExportNode.ARMATURE]:
            if self.export_settings['gltf_armature_object_remove'] is True:
                if hasattr(n, "arma_exported") is False:
                    continue
            if len([m for m in self.nodes.values() if m.keep_tag is True and m.blender_type ==
                   VExportNode.OBJECT and m.armature == n.uuid]) == 0:
                skin = gather_skin(n.uuid, self.export_settings)
                skins.append(skin)
        return skins

    def variants_reset_to_original(self):
        # Only if Variants are displayed and exported
        if bpy.context.preferences.addons['io_scene_gltf2'].preferences.KHR_materials_variants_ui is False:
            return
        objects = [self.nodes[o].blender_object for o in self.get_all_node_of_type(VExportNode.OBJECT) if self.nodes[o].blender_object.type == "MESH"
                   and self.nodes[o].blender_object.data.gltf2_variant_default_materials is not None]
        for obj in objects:
            # loop on material slots ( primitives )
            for mat_slot_idx, s in enumerate(obj.material_slots):
                # Check if there is a default material for this slot
                for i in obj.data.gltf2_variant_default_materials:
                    if i.material_slot_index == mat_slot_idx:
                        s.material = i.default_material
                        break

            # If not found, keep current material as default

    def break_bone_hierarchy(self):
        # Can be useful when matrix is not decomposable
        for arma in self.get_all_node_of_type(VExportNode.ARMATURE):
            bones = self.get_all_bones(arma)
            for bone in bones:
                if self.nodes[bone].parent_uuid is not None and self.nodes[bone].parent_uuid != arma:
                    self.nodes[self.nodes[bone].parent_uuid].children.remove(bone)
                    self.nodes[bone].parent_uuid = arma
                    self.nodes[arma].children.append(bone)
                    self.nodes[arma].children_type[bone] = VExportNode.CHILDREN_REAL
                    self.nodes[bone].parent_bone_uuid = None

    def break_obj_hierarchy(self):
        # Can be useful when matrix is not decomposable
        # TODO: if we get real collection one day, we probably need to adapt this code
        for obj in self.get_all_objects():
            if self.nodes[obj].armature is not None and self.nodes[obj].parent_uuid == self.nodes[obj].armature:
                continue  # Keep skined meshs as children of armature
            if self.nodes[obj].parent_uuid is not None:
                self.nodes[self.nodes[obj].parent_uuid].children.remove(obj)
                self.nodes[obj].parent_uuid = None
                self.roots.append(obj)

    def check_if_we_can_remove_armature(self):
        # If user requested to remove armature, we need to check if it is possible
        # If is impossible to remove it if armature has multiple root bones. (glTF validator error)
        # Currently, we manage it at export level, not at each armature level
        for arma_uuid in [n for n in self.nodes.keys() if self.nodes[n].blender_type == VExportNode.ARMATURE]:
            # Do not cache bones here, as we will filter them later, so the cache will be wrong
            if len(self.get_root_bones_uuid(arma_uuid, cache=False)) > 1:
                # We can't remove armature
                self.export_settings['gltf_armature_object_remove'] = False
                self.export_settings['log'].warning(
                    "We can't remove armature object because some armatures have multiple root bones.")
                break

    def calculate_collection_center(self):
        # Because we already filtered the tree, we can use all objects
        # to calculate the center of the scene
        # Are taken into account all objects that are direct root in the exported collection
        centers = []

        for node in [
            n for n in self.nodes.values() if n.parent_uuid is None and n.blender_type in [
                VExportNode.OBJECT,
                VExportNode.ARMATURE,
                VExportNode.LIGHT,
                VExportNode.CAMERA]]:
            if node.matrix_world is not None:
                centers.append(node.matrix_world.translation)

        if len(centers) == 0:
            self.export_settings['gltf_collection_center'] = Vector((0.0, 0.0, 0.0))
            return

        self.export_settings['gltf_collection_center'] = sum(centers, Vector()) / len(centers)
