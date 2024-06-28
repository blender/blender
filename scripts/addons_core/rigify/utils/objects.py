# SPDX-FileCopyrightText: 2019-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from typing import TYPE_CHECKING
from bpy.types import LayerCollection, Collection, Object

from .misc import ArmatureObject
from .naming import strip_org

from mathutils import Matrix

if TYPE_CHECKING:
    from ..generate import Generator
    from ..base_rig import BaseRig


# noinspection SpellCheckingInspection
def create_object_data(obj_type, name):
    if obj_type == 'EMPTY':
        return None
    if obj_type == 'MESH':
        return bpy.data.meshes.new(name)
    if obj_type in ('CURVE', 'SURFACE', 'FONT'):
        return bpy.data.curves.new(name, obj_type)
    if obj_type == 'META':
        return bpy.data.metaballs.new(name)
    if obj_type == 'CURVES':
        return bpy.data.hair_curves.new(name)
    if obj_type == 'POINTCLOUD':
        return bpy.data.pointclouds.new(name)
    if obj_type == 'VOLUME':
        return bpy.data.volumes.new(name)
    if obj_type == 'GREASEPENCIL':
        return bpy.data.grease_pencils.new(name)
    if obj_type == 'ARMATURE':
        return bpy.data.armatures.new(name)
    if obj_type == 'LATTICE':
        return bpy.data.lattices.new(name)
    raise ValueError(f"Invalid object type {obj_type}")


class ArtifactManager:
    generator: 'Generator'

    collection: Collection | None
    layer_collection: LayerCollection | None

    used_artifacts: list[Object]
    temp_artifacts: list[Object]

    artifact_reuse_table: dict[tuple[str, ...], Object]

    def __init__(self, generator: 'Generator'):
        self.generator = generator
        self.collection = None
        self.layer_collection = None
        self.used_artifacts = []
        self.temp_artifacts = []
        self.artifact_reuse_table = {}

    def _make_name(self, owner: 'BaseRig', name: str):
        return self.generator.obj.name + ":" + strip_org(owner.base_bone) + ":" + name

    def create_new(self, owner: 'BaseRig', obj_type: str, name: str):
        """
        Creates an artifact object of the specified type and name. If it already exists, all
        references are updated to point to the new instance, and the existing one is deleted.

        Parameters:
            owner: rig component that requests the object.
            obj_type: type of the object to create.
            name: unique name of the object within the rig component.
        Returns:
            Object that was created.
        """
        return self.find_or_create(owner, obj_type, name, recreate=True)[1]

    def find_or_create(self, owner: 'BaseRig', obj_type: str, name: str, *, recreate=False):
        """
        Creates or reuses an artifact object of the specified type.

        Parameters:
            owner: rig component that requests the object.
            obj_type: type of the object to create.
            name: unique name of the object within the rig component.
            recreate: instructs that the object should be re-created from scratch even if it exists.
        Returns:
            (bool, Object) tuple, with the boolean specifying if the object already existed.
        """

        obj_name = self._make_name(owner, name)
        key = (owner.base_bone, name)

        obj = self.artifact_reuse_table.get(key)

        # If the existing object has incorrect type, delete it
        if obj and obj.type != obj_type:
            if obj in self.used_artifacts:
                owner.raise_error(f"duplicate reuse of artifact object {obj.name}")

            print(f"RIGIFY: incompatible artifact object {obj.name} type: {obj.type} instead of {obj_type}")
            del self.artifact_reuse_table[key]
            bpy.data.objects.remove(obj)
            obj = None

        # Reuse the existing object
        if obj:
            if obj in self.used_artifacts:
                owner.raise_error(f"duplicate reuse of artifact object {obj.name}")

            if recreate:
                # Forcefully re-create and replace the existing object
                obj.name += '-OLD'
                if data := obj.data:
                    data.name += '-OLD'

                new_obj = bpy.data.objects.new(obj_name, create_object_data(obj_type, obj_name))

                obj.user_remap(new_obj)
                self.artifact_reuse_table[key] = new_obj
                bpy.data.objects.remove(obj)
                obj = new_obj

            # Ensure the existing object is visible
            obj.hide_viewport = False
            obj.hide_set(False, view_layer=self.generator.view_layer)

            if not obj.visible_get(view_layer=self.generator.view_layer):
                owner.raise_error(f"could not un-hide existing artifact object {obj.name}")

            # Try renaming the existing object
            obj.name = obj_name
            if data := obj.data:
                data.name = obj_name

            found = True

        # Create an object from scratch
        else:
            obj = bpy.data.objects.new(obj_name, create_object_data(obj_type, obj_name))

            self.generator.collection.objects.link(obj)
            self.artifact_reuse_table[key] = obj

            found = False

        self.used_artifacts.append(obj)

        obj.rigify_owner_rig = self.generator.obj
        obj["rigify_artifact_id"] = key

        obj.parent = self.generator.obj
        obj.parent_type = 'OBJECT'
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.matrix_basis = Matrix.Identity(4)

        return found, obj

    def new_temporary(self, owner: 'BaseRig', obj_type: str, name="temp"):
        """
        Creates a new temporary object of the specified type.
        The object will be removed after generation finishes.
        """
        obj_name = "TEMP:" + self._make_name(owner, name)
        obj = bpy.data.objects.new(obj_name, create_object_data(obj_type, obj_name))
        obj.rigify_owner_rig = self.generator.obj
        obj["rigify_artifact_id"] = 'temporary'
        self.generator.collection.objects.link(obj)
        self.temp_artifacts.append(obj)
        return obj

    def remove_temporary(self, obj):
        """
        Immediately removes a temporary object previously created using new_temporary.
        """
        self.temp_artifacts.remove(obj)
        bpy.data.objects.remove(obj)

    def generate_init_existing(self, armature: ArmatureObject):
        for obj in bpy.data.objects:
            if obj.rigify_owner_rig != armature:
                continue

            aid = obj["rigify_artifact_id"]
            if isinstance(aid, list) and all(isinstance(x, str) for x in aid):
                self.artifact_reuse_table[tuple(aid)] = obj
            else:
                print(f"RIGIFY: removing orphan artifact {obj.name}")
                bpy.data.objects.remove(obj)

    def generate_cleanup(self):
        for obj in self.temp_artifacts:
            bpy.data.objects.remove(obj)

        self.temp_artifacts = []

        for key, obj in self.artifact_reuse_table.items():
            if obj in self.used_artifacts:
                obj.hide_viewport = True
                obj.hide_render = True
            else:
                del self.artifact_reuse_table[key]
                bpy.data.objects.remove(obj)
