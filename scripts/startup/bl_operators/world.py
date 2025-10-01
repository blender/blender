# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import bmesh


class WORLD_OT_convert_volume_to_mesh(bpy.types.Operator):
    """Convert the volume of a world to a mesh. """ \
        """The world's volume used to be rendered by EEVEE Legacy. Conversion is needed for it to render properly"""
    bl_label = "Convert Volume"
    bl_options = {'REGISTER', 'UNDO'}
    bl_idname = "world.convert_volume_to_mesh"

    @classmethod
    def poll(cls, context):
        world = cls._world_get(context)
        if not world:
            return False

        ntree = world.node_tree
        node = ntree.get_output_node('EEVEE')
        return bool(node.inputs["Volume"].links)

    def execute(self, context):
        cls = self.__class__
        world = cls._world_get(context)
        view_layer = context.view_layer

        world_tree = world.node_tree
        world_output = world_tree.get_output_node('EEVEE')
        name = "{:s}_volume".format(world.name)

        collection = bpy.data.collections.new(name)
        view_layer.layer_collection.collection.children.link(collection)

        # Add World Volume Mesh object to scene
        mesh = bpy.data.meshes.new(name)
        object = bpy.data.objects.new(name, mesh)
        object.display.show_shadows = False

        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=0, radius=1e5)
        bm.to_mesh(mesh)
        bm.free()

        # Remove all non-essential attributes
        for attribute in mesh.attributes:
            if attribute.is_internal or attribute.is_required:
                continue
            mesh.attributes.remove(attribute)

        material = bpy.data.materials.new(name)
        mesh.materials.append(material)
        volume_tree = material.node_tree
        volume_tree.nodes.clear()
        volume_tree.nodes.new("ShaderNodeOutputMaterial")
        volume_output = volume_tree.get_output_node('EEVEE')

        links_to_add = []
        self._sync_rna_properties(volume_output, world_output)
        self._sync_node_input(
            volume_tree,
            volume_output,
            volume_output.inputs["Volume"],
            world_output,
            world_output.inputs["Volume"],
            links_to_add,
        )
        self._sync_links(volume_tree, links_to_add)

        # Add transparent volume for other render engines
        if volume_output.target == 'EEVEE':
            all_output = volume_tree.nodes.new(type="ShaderNodeOutputMaterial")
            transparent = volume_tree.nodes.new(type="ShaderNodeBsdfTransparent")
            volume_tree.links.new(transparent.outputs[0], all_output.inputs[0])

        # Remove all volume links from the world node tree.
        for link in world_output.inputs["Volume"].links:
            world_tree.links.remove(link)

        collection.objects.link(object)
        object.select_set(True)
        view_layer.objects.active = object

        world.use_eevee_finite_volume = False

        return {'FINISHED'}

    @staticmethod
    def _world_get(context):
        if world := getattr(context, "world", None):
            return world
        return context.scene.world

    def _sync_node_input(
        self,
        dst_tree,  # bpy.types.NodeTree
        dst_node,  # bpy.types.Node
        dst_socket,  # bpy.types.NodeSocket
        src_node,  # bpy.types.Node
        src_socket,  # bpy.types.NodeSocket
        links_to_add,
    ):  # -> None
        self._sync_rna_properties(dst_socket, src_socket)
        for src_link in src_socket.links:
            src_linked_node = src_link.from_node
            dst_linked_node = self._sync_node(dst_tree, src_linked_node, links_to_add)

            from_socket_index = src_node.outputs.find(src_link.from_socket.name)
            dst_tree.links.new(
                dst_linked_node.outputs[from_socket_index],
                dst_socket,
            )

    def _sync_node(
        self,
        dst_tree,  # bpy.types.NodeTree
        src_node,  # bpy.types.Node
        links_to_add,
    ):  # -> bpy.types.Node
        """
        Find the counter part of the src_node in dst_tree. When found return the counter part. When not found
        create the counter part, sync it and return the created node.
        """
        if src_node.name in dst_tree.nodes:
            return dst_tree.nodes[src_node.name]

        dst_node = dst_tree.nodes.new(src_node.bl_idname)

        self._sync_rna_properties(dst_node, src_node)
        self._sync_node_inputs(dst_tree, dst_node, src_node, links_to_add)
        return dst_node

    def _sync_rna_properties(self, dst, src):  # -> None
        for rna_prop in src.bl_rna.properties:
            if rna_prop.is_readonly:
                continue

            attr_name = rna_prop.identifier
            if attr_name in {"bl_idname", "bl_static_type"}:
                continue
            setattr(dst, attr_name, getattr(src, attr_name))

    def _sync_node_inputs(
        self,
        dst_tree,  # bpy.types.NodeTree
        dst_node,  # bpy.types.Node
        src_node,  # bpy.types.Node
        links_to_add,
    ):  # -> None
        for index in range(len(src_node.inputs)):
            src_socket = src_node.inputs[index]
            dst_socket = dst_node.inputs[index]
            self._sync_node_input(dst_tree, dst_node, dst_socket, src_node, src_socket, links_to_add)

    def _sync_links(
        self,
        dst_tree,  # bpy.types.NodeTree
        links_to_add,
    ):  # -> None
        pass


classes = (
    WORLD_OT_convert_volume_to_mesh,
)
