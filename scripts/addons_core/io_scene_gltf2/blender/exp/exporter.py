# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import re
import os
from typing import List

from ... import get_version_string
from ...io.com import gltf2_io, gltf2_io_extensions
from ...io.com.path import path_to_uri, uri_to_path
from ...io.com.constants import ComponentType, DataType
from ...io.exp import binary_data as gltf2_io_binary_data, buffer as gltf2_io_buffer, image_data as gltf2_io_image_data
from ...io.exp.user_extensions import export_user_extensions
from .accessors import gather_accessor
from .material.image import get_gltf_image_from_blender_image


class AdditionalData:
    def __init__(self):
        additional_textures = []


class GlTF2Exporter:
    """
    The glTF exporter flattens a scene graph to a glTF serializable format.

    Any child properties are replaced with references where necessary
    """

    def __init__(self, export_settings):
        self.export_settings = export_settings
        self.__finalized = False

        copyright = export_settings['gltf_copyright'] or None
        asset = gltf2_io.Asset(
            copyright=copyright,
            extensions=None,
            extras=None,
            generator='Khronos glTF Blender I/O v' + get_version_string(),
            min_version=None,
            version='2.0')

        export_user_extensions('gather_asset_hook', export_settings, asset)

        self.__gltf = gltf2_io.Gltf(
            accessors=[],
            animations=[],
            asset=asset,
            buffers=[],
            buffer_views=[],
            cameras=[],
            extensions={},
            extensions_required=[],
            extensions_used=[],
            extras=None,
            images=[],
            materials=[],
            meshes=[],
            nodes=[],
            samplers=[],
            scene=-1,
            scenes=[],
            skins=[],
            textures=[]
        )

        self.additional_data = AdditionalData()

        self.__buffer = gltf2_io_buffer.Buffer()
        self.__images = {}

        # mapping of all glTFChildOfRootProperty types to their corresponding root level arrays
        self.__childOfRootPropertyTypeLookup = {
            gltf2_io.Accessor: self.__gltf.accessors,
            gltf2_io.Animation: self.__gltf.animations,
            gltf2_io.Buffer: self.__gltf.buffers,
            gltf2_io.BufferView: self.__gltf.buffer_views,
            gltf2_io.Camera: self.__gltf.cameras,
            gltf2_io.Image: self.__gltf.images,
            gltf2_io.Material: self.__gltf.materials,
            gltf2_io.Mesh: self.__gltf.meshes,
            gltf2_io.Node: self.__gltf.nodes,
            gltf2_io.Sampler: self.__gltf.samplers,
            gltf2_io.Scene: self.__gltf.scenes,
            gltf2_io.Skin: self.__gltf.skins,
            gltf2_io.Texture: self.__gltf.textures
        }

        self.__propertyTypeLookup = [
            gltf2_io.AccessorSparseIndices,
            gltf2_io.AccessorSparse,
            gltf2_io.AccessorSparseValues,
            gltf2_io.AnimationChannel,
            gltf2_io.AnimationChannelTarget,
            gltf2_io.AnimationSampler,
            gltf2_io.Asset,
            gltf2_io.CameraOrthographic,
            gltf2_io.CameraPerspective,
            gltf2_io.MeshPrimitive,
            gltf2_io.TextureInfo,
            gltf2_io.MaterialPBRMetallicRoughness,
            gltf2_io.MaterialNormalTextureInfoClass,
            gltf2_io.MaterialOcclusionTextureInfoClass
        ]

        self.__traverse(asset)

    @property
    def glTF(self):
        if not self.__finalized:
            raise RuntimeError("glTF requested, but buffers are not finalized yet")
        return self.__gltf

    def finalize_buffer(self, output_path=None, buffer_name=None, is_glb=False):
        """Finalize the glTF and write buffers."""
        if self.__finalized:
            raise RuntimeError("Tried to finalize buffers for finalized glTF file")

        if self.__buffer.byte_length > 0:
            if is_glb:
                uri = None
            elif output_path and buffer_name:
                with open(output_path + uri_to_path(buffer_name), 'wb') as f:
                    f.write(self.__buffer.to_bytes())
                uri = buffer_name
            else:
                uri = self.__buffer.to_embed_string()

            buffer = gltf2_io.Buffer(
                byte_length=self.__buffer.byte_length,
                extensions=None,
                extras=None,
                name=None,
                uri=uri
            )
            self.__gltf.buffers.append(buffer)

        self.__finalized = True

        if is_glb:
            return self.__buffer.to_bytes()

    def add_draco_extension(self):
        """
        Register Draco extension as *used* and *required*.

        :return:
        """
        self.__gltf.extensions_required.append('KHR_draco_mesh_compression')
        self.__gltf.extensions_used.append('KHR_draco_mesh_compression')

    def finalize_images(self):
        """
        Write all images.
        """
        output_path = self.export_settings['gltf_texturedirectory']

        if self.__images:
            os.makedirs(output_path, exist_ok=True)

        for name, image in self.__images.items():
            dst_path = output_path + "/" + name
            with open(dst_path, 'wb') as f:
                f.write(image.data)

    def manage_gpu_instancing(self, node, also_mesh=False):
        instances = {}
        for child_idx in node.children:
            child = self.__gltf.nodes[child_idx]
            if child.children:
                continue
            if child.mesh is not None and child.mesh not in instances.keys():
                instances[child.mesh] = []
            if child.mesh is not None:
                instances[child.mesh].append(child_idx)

        # For now, manage instances only if there are all children of same object
        # And this instances don't have any children
        instances = {k: v for k, v in instances.items() if len(v) > 1}

        holders = []
        if len(instances.keys()) == 1 and also_mesh is False:
            # There is only 1 set of instances. So using the parent as instance holder
            holder = node
            holders = [node]
        elif len(instances.keys()) > 1 or (len(instances.keys()) == 1 and also_mesh is True):
            for h in range(len(instances.keys())):
                # Create a new node
                n = gltf2_io.Node(
                    camera=None,
                    children=[],
                    extensions=None,
                    extras=None,
                    matrix=None,
                    mesh=None,
                    name=node.name + "." + str(h),
                    rotation=None,
                    scale=None,
                    skin=None,
                    translation=None,
                    weights=None,
                )
                n = self.__traverse_property(n)
                idx = self.__to_reference(n)

                # Add it to original empty
                node.children.append(idx)
                holders.append(self.__gltf.nodes[idx])

        for idx, inst_key in enumerate(instances.keys()):
            insts = instances[inst_key]
            holder = holders[idx]

            # Let's retrieve TRS of instances
            translation = []
            rotation = []
            scale = []
            for inst_node_idx in insts:
                inst_node = self.__gltf.nodes[inst_node_idx]
                t = inst_node.translation if inst_node.translation is not None else [0, 0, 0]
                r = inst_node.rotation if inst_node.rotation is not None else [0, 0, 0, 1]
                s = inst_node.scale if inst_node.scale is not None else [1, 1, 1]
                for i in t:
                    translation.append(i)
                for i in r:
                    rotation.append(i)
                for i in s:
                    scale.append(i)

            # Create Accessors for the extension
            ext = {}
            ext['attributes'] = {}
            ext['attributes']['TRANSLATION'] = gather_accessor(
                gltf2_io_binary_data.BinaryData.from_list(translation, ComponentType.Float),
                ComponentType.Float,
                len(translation) // 3,
                None,
                None,
                DataType.Vec3,
                None
            )
            ext['attributes']['ROTATION'] = gather_accessor(
                gltf2_io_binary_data.BinaryData.from_list(rotation, ComponentType.Float),
                ComponentType.Float,
                len(rotation) // 4,
                None,
                None,
                DataType.Vec4,
                None
            )
            ext['attributes']['SCALE'] = gather_accessor(
                gltf2_io_binary_data.BinaryData.from_list(scale, ComponentType.Float),
                ComponentType.Float,
                len(scale) // 3,
                None,
                None,
                DataType.Vec3,
                None
            )

            # Add extension to the Node, and traverse it
            if not holder.extensions:
                holder.extensions = {}
            holder.extensions["EXT_mesh_gpu_instancing"] = gltf2_io_extensions.Extension(
                'EXT_mesh_gpu_instancing', ext, False)
            holder.mesh = inst_key
            self.__traverse(holder.extensions)

            # Remove children from original Empty
            new_children = []
            for child_idx in node.children:
                if child_idx not in insts:
                    new_children.append(child_idx)
            node.children = new_children

            self.nodes_idx_to_remove.extend(insts)

        for child_idx in node.children:
                child = self.__gltf.nodes[child_idx]
                self.manage_gpu_instancing(child, also_mesh=child.mesh is not None)

    def manage_gpu_instancing_nodes(self, export_settings):
        if export_settings['gltf_gpu_instances'] is True:
            for scene_num in range(len(self.__gltf.scenes)):
                # Modify the scene data in case of EXT_mesh_gpu_instancing export

                self.nodes_idx_to_remove = []
                for node_idx in self.__gltf.scenes[scene_num].nodes:
                    node = self.__gltf.nodes[node_idx]
                    if node.mesh is None:
                        self.manage_gpu_instancing(node)
                    else:
                        self.manage_gpu_instancing(node, also_mesh=True)

                # Slides other nodes index

                self.nodes_idx_to_remove.sort()
                for node_idx in self.__gltf.scenes[scene_num].nodes:
                    self.recursive_slide_node_idx(node_idx)

                new_node_list = []
                for node_idx in self.__gltf.scenes[scene_num].nodes:
                    len_ = len([i for i in self.nodes_idx_to_remove if i < node_idx])
                    new_node_list.append(node_idx - len_)
                self.__gltf.scenes[scene_num].nodes = new_node_list

                for skin in self.__gltf.skins:
                    new_joint_list = []
                    for node_idx in skin.joints:
                        len_ = len([i for i in self.nodes_idx_to_remove if i < node_idx])
                        new_joint_list.append(node_idx - len_)
                    skin.joints = new_joint_list
                    if skin.skeleton is not None:
                        len_ = len([i for i in self.nodes_idx_to_remove if i < skin.skeleton])
                        skin.skeleton = skin.skeleton - len_

            # Remove animation channels that was targeting a node that will be removed
            new_animation_list = []
            for animation in self.__gltf.animations:
                new_channel_list = []
                for channel in animation.channels:
                    if channel.target.node not in self.nodes_idx_to_remove:
                        new_channel_list.append(channel)
                animation.channels = new_channel_list
                if len(animation.channels) > 0:
                    new_animation_list.append(animation)
            self.__gltf.animations = new_animation_list

            # TODO: remove unused animation accessors?

            # And now really remove nodes
            self.__gltf.nodes = [node for idx, node in enumerate(
                self.__gltf.nodes) if idx not in self.nodes_idx_to_remove]

    def add_scene(self, scene: gltf2_io.Scene, active: bool = False, export_settings=None):
        """
        Add a scene to the glTF.

        The scene should be built up with the generated glTF classes
        :param scene: gltf2_io.Scene type. Root node of the scene graph
        :param active: If true, sets the glTD.scene index to the added scene
        :return: nothing
        """
        if self.__finalized:
            raise RuntimeError("Tried to add scene to finalized glTF file")

        scene_num = self.__traverse(scene)
        if active:
            self.__gltf.scene = scene_num

    def recursive_slide_node_idx(self, node_idx):
        node = self.__gltf.nodes[node_idx]

        new_node_children = []
        for child_idx in node.children:
            len_ = len([i for i in self.nodes_idx_to_remove if i < child_idx])
            new_node_children.append(child_idx - len_)

        for child_idx in node.children:
            self.recursive_slide_node_idx(child_idx)

        node.children = new_node_children

    def traverse_unused_skins(self, skins):
        for s in skins:
            self.__traverse(s)

    def traverse_additional_textures(self):
        if self.export_settings['gltf_unused_textures'] is True:
            tab = []
            for tex in self.export_settings['additional_texture_export']:
                res = self.__traverse(tex)
                tab.append(res)

            self.additional_data.additional_textures = tab

    def traverse_additional_images(self):
        if self.export_settings['gltf_unused_images']:
            for img in [img for img in bpy.data.images if img.source != "VIEWER"]:
                # TODO manage full / partial / custom via hook ...
                if img.name not in self.export_settings['exported_images'].keys():
                    self.__traverse(get_gltf_image_from_blender_image(img.name, self.export_settings))

    def add_animation(self, animation: gltf2_io.Animation):
        """
        Add an animation to the glTF.

        :param animation: glTF animation, with python style references (names)
        :return: nothing
        """
        if self.__finalized:
            raise RuntimeError("Tried to add animation to finalized glTF file")

        self.__traverse(animation)

    def __to_reference(self, property):
        """
        Append a child of root property to its respective list and return a reference into said list.

        If the property is not child of root, the property itself is returned.
        :param property: A property type object that should be converted to a reference
        :return: a reference or the object itself if it is not child or root
        """
        gltf_list = self.__childOfRootPropertyTypeLookup.get(type(property), None)
        if gltf_list is None:
            # The object is not of a child of root --> don't convert to reference
            return property

        return self.__append_unique_and_get_index(gltf_list, property)

    @staticmethod
    def __append_unique_and_get_index(target: list, obj):
        if obj in target:
            return target.index(obj)
        else:
            index = len(target)
            target.append(obj)
            return index

    def __add_image(self, image: gltf2_io_image_data.ImageData):
        name = image.adjusted_name()
        count = 1
        regex = re.compile(r"-\d+$")
        while name + image.file_extension in self.__images.keys():
            regex_found = re.findall(regex, name)
            if regex_found:
                name = re.sub(regex, "-" + str(count), name)
            else:
                name += "-" + str(count)

            count += 1
        # TODO: allow embedding of images (base64)

        self.__images[name + image.file_extension] = image

        texture_dir = self.export_settings['gltf_texturedirectory']
        abs_path = os.path.join(texture_dir, name + image.file_extension)
        rel_path = os.path.relpath(
            abs_path,
            start=self.export_settings['gltf_filedirectory'],
        )
        return path_to_uri(rel_path)

    @classmethod
    def __get_key_path(cls, d: dict, keypath: List[str], default):
        """Create if necessary and get the element at key path from a dict"""
        key = keypath.pop(0)

        if len(keypath) == 0:
            v = d.get(key, default)
            d[key] = v
            return v

        d_key = d.get(key, {})
        d[key] = d_key
        return cls.__get_key_path(d[key], keypath, default)

    def traverse_extensions(self):
        self.__traverse(self.__gltf.extensions)

    def __traverse_property(self, node):
        for member_name in [a for a in dir(node) if not a.startswith('__') and not callable(getattr(node, a))]:
            new_value = self.__traverse(getattr(node, member_name))
            setattr(node, member_name, new_value)  # usually this is the same as before

            # # TODO: maybe with extensions hooks we can find a more elegant solution
            # if member_name == "extensions" and new_value is not None:
            #     for extension_name in new_value.keys():
            #         self.__append_unique_and_get_index(self.__gltf.extensions_used, extension_name)
            #         self.__append_unique_and_get_index(self.__gltf.extensions_required, extension_name)

        if self.export_settings['gltf_trs_w_animation_pointer'] is True:
            if type(node) == gltf2_io.AnimationChannelTarget:
                if node.path in ["translation", "rotation", "scale", "weights"]:
                    if node.extensions is None:
                        node.extensions = {}
                    node.extensions["KHR_animation_pointer"] = {"pointer": "/nodes/" + str(node.node) + "/" + node.path}
                    node.node = None
                    node.path = "pointer"
                    self.__append_unique_and_get_index(self.__gltf.extensions_used, "KHR_animation_pointer")

        if type(node) == gltf2_io.AnimationChannelTarget:
            if node.path not in ["translation", "rotation", "scale", "weights"] and node.path != "pointer":
                if node.extensions is None:
                    node.extensions = {}
                node.extensions["KHR_animation_pointer"] = {"pointer": node.path.replace("XXX", str(node.node))}
                node.node = None
                node.path = "pointer"
                self.__append_unique_and_get_index(self.__gltf.extensions_used, "KHR_animation_pointer")

        return node

    def __traverse(self, node):
        """
        Recursively traverse a scene graph consisting of gltf compatible elements.

        The tree is traversed downwards until a primitive is reached. Then any ChildOfRoot property
        is stored in the according list in the glTF and replaced with a index reference in the upper level.
        """
        # traverse nodes of a child of root property type and add them to the glTF root
        if type(node) in self.__childOfRootPropertyTypeLookup:
            node = self.__traverse_property(node)
            idx = self.__to_reference(node)
            # child of root properties are only present at root level --> replace with index in upper level
            return idx

        # traverse lists, such as children and replace them with indices
        if isinstance(node, list):
            for i in range(len(node)):
                node[i] = self.__traverse(node[i])
            return node

        if isinstance(node, dict):
            for key in node.keys():
                node[key] = self.__traverse(node[key])
            return node

        # traverse into any other property
        if type(node) in self.__propertyTypeLookup:
            return self.__traverse_property(node)

        # binary data needs to be moved to a buffer and referenced with a buffer view
        if isinstance(node, gltf2_io_binary_data.BinaryData):
            buffer_view = self.__buffer.add_and_get_view(node)
            return self.__to_reference(buffer_view)

        # image data needs to be saved to file
        if isinstance(node, gltf2_io_image_data.ImageData):
            image = self.__add_image(node)
            return image

        # extensions
        # I don't know why, but after reloading script, this condition failed
        # So using name comparison, instead of isinstance
        # if isinstance(node, gltf2_io_extensions.Extension):
        if isinstance(node, gltf2_io_extensions.Extension) \
                or (node and hasattr(type(node), "extension")):
            extension = self.__traverse(node.extension)
            self.__append_unique_and_get_index(self.__gltf.extensions_used, node.name)
            if node.required:
                self.__append_unique_and_get_index(self.__gltf.extensions_required, node.name)

            # extensions that lie in the root of the glTF.
            # They need to be converted to a reference at place of occurrence
            if isinstance(node, gltf2_io_extensions.ChildOfRootExtension):
                root_extension_list = self.__get_key_path(self.__gltf.extensions, [node.name] + node.path, [])
                idx = self.__append_unique_and_get_index(root_extension_list, extension)
                return idx

            return extension

        # do nothing for any type that does not match a glTF schema (primitives)
        return node
