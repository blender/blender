# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from mathutils import Matrix
import numpy as np
from ...io.imp.user_extensions import import_user_extensions
from ...io.imp.gltf2_io_binary import BinaryData
from ...io.com.constants import DataType, ComponentType
from ...blender.com.conversion import get_attribute_type
from ..com.extras import set_extras
from ..com.gltf2_blender_utils import fast_structured_np_unique
from .material import BlenderMaterial
from .draco_compression_extension import decode_primitive


class BlenderMesh():
    """Blender Mesh."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def create(gltf, mesh_idx, skin_idx):
        """Mesh creation."""
        return create_mesh(gltf, mesh_idx, skin_idx)


# Maximum number of TEXCOORD_n/COLOR_n sets to import
UV_MAX = 8
COLOR_MAX = 8


def create_mesh(gltf, mesh_idx, skin_idx):
    pymesh = gltf.data.meshes[mesh_idx]

    import_user_extensions('gather_import_mesh_before_hook', gltf, pymesh)

    name = pymesh.name or 'Mesh_%d' % mesh_idx
    mesh = bpy.data.meshes.new(name)

    # Temporarily parent the mesh to an object.
    # This is used to set skin weights and shapekeys.
    tmp_ob = None
    try:
        tmp_ob = bpy.data.objects.new('##gltf-import:tmp-object##', mesh)
        do_primitives(gltf, mesh_idx, skin_idx, mesh, tmp_ob)
        set_extras(mesh, gltf.data.meshes[mesh_idx].extras, exclude=['targetNames'])

    finally:
        if tmp_ob:
            bpy.data.objects.remove(tmp_ob)

    import_user_extensions('gather_import_mesh_after_hook', gltf, pymesh, mesh)

    return mesh


def do_primitives(gltf, mesh_idx, skin_idx, mesh, ob):
    """Put all primitive data into the mesh."""
    pymesh = gltf.data.meshes[mesh_idx]

    # Use a class here, to be able to pass data by reference to hook (to be able to change them inside hook)
    class IMPORT_mesh_options:
        def __init__(self, skinning: bool = True, skin_into_bind_pose: bool = True):
            self.skinning = skinning
            self.skin_into_bind_pose = skin_into_bind_pose

    mesh_options = IMPORT_mesh_options()
    import_user_extensions('gather_import_mesh_options', gltf, mesh_options, pymesh, skin_idx)

    # Scan the primitives to find out what we need to create

    has_normals = False
    num_uvs = 0
    num_cols = 0
    num_joint_sets = 0
    attributes = set({})
    attribute_data = []
    attribute_type = {}
    attribute_component_type = {}

    for prim in pymesh.primitives:
        if 'POSITION' not in prim.attributes:
            continue

        if gltf.import_settings['import_shading'] == "NORMALS":
            if 'NORMAL' in prim.attributes:
                has_normals = True

        if skin_idx is not None:
            i = 0
            while ('JOINTS_%d' % i) in prim.attributes and \
                    ('WEIGHTS_%d' % i) in prim.attributes:
                i += 1
            num_joint_sets = max(i, num_joint_sets)

        i = 0
        while i < UV_MAX and ('TEXCOORD_%d' % i) in prim.attributes:
            i += 1
        num_uvs = max(i, num_uvs)

        i = 0
        while i < COLOR_MAX and ('COLOR_%d' % i) in prim.attributes:
            i += 1
        num_cols = max(i, num_cols)

        custom_attrs = [k for k in prim.attributes if k.startswith('_')]
        for attr in custom_attrs:
            if attr not in attributes:
                attribute_type[attr] = gltf.data.accessors[prim.attributes[attr]].type
                attribute_component_type[attr] = gltf.data.accessors[prim.attributes[attr]].component_type
                attribute_data.append(
                    np.empty(
                        dtype=ComponentType.to_numpy_dtype(attribute_component_type[attr]),
                        shape=(0, DataType.num_elements(attribute_type[attr])))
                )
        attributes.update(set(custom_attrs))

    num_shapekeys = sum(sk_name is not None for sk_name in pymesh.shapekey_names)

    # -------------
    # We'll process all the primitives gathering arrays to feed into the
    # various foreach_set function that create the mesh data.

    num_faces = 0  # total number of faces
    vert_locs = np.empty(dtype=np.float32, shape=(0, 3))  # coordinate for each vert
    vert_normals = np.empty(dtype=np.float32, shape=(0, 3))  # normal for each vert
    edge_vidxs = np.array([], dtype=np.uint32)  # vertex_index for each loose edge
    loop_vidxs = np.array([], dtype=np.uint32)  # vertex_index for each loop
    loop_uvs = [
        np.empty(dtype=np.float32, shape=(0, 2))  # UV for each loop for each layer
        for _ in range(num_uvs)
    ]
    loop_cols = [
        np.empty(dtype=np.float32, shape=(0, 4))  # color for each loop for each layer
        for _ in range(num_cols)
    ]
    vert_joints = [
        np.empty(dtype=np.uint32, shape=(0, 4))  # 4 joints for each vert for each set
        for _ in range(num_joint_sets)
    ]
    vert_weights = [
        np.empty(dtype=np.float32, shape=(0, 4))  # 4 weights for each vert for each set
        for _ in range(num_joint_sets)
    ]
    sk_vert_locs = [
        np.empty(dtype=np.float32, shape=(0, 3))  # coordinate for each vert for each shapekey
        for _ in range(num_shapekeys)
    ]

    # We need to detect if some non-tri primitives have some VC.
    # (Because, in that case, we will need to create vertex domain VC, instead of corner domain VC)
    has_non_tri_vcs = []
    vc_domains = []
    for i in range(num_cols):
        has_non_tri_vcs.append(any([prim.mode not in [None, 4, 5, 6] and any(
            ['COLOR_' + str(i) in attr for attr in prim.attributes]) for prim in pymesh.primitives]))
        vc_domains.append('POINT' if has_non_tri_vcs[i] else 'CORNER')

    for prim in pymesh.primitives:
        prim.num_faces = 0

        if 'POSITION' not in prim.attributes:
            continue

        vert_index_base = len(vert_locs)

        if prim.extensions is not None and 'KHR_draco_mesh_compression' in prim.extensions:

            gltf.log.info('Draco Decoder: Decode primitive {}'.format(pymesh.name or '[unnamed]'))
            decode_primitive(gltf, prim)

        import_user_extensions('gather_import_decode_primitive', gltf, pymesh, prim, skin_idx)

        if prim.indices is not None:
            indices = BinaryData.decode_accessor(gltf, prim.indices)
            indices = indices.reshape(len(indices))
        else:
            num_verts = gltf.data.accessors[prim.attributes['POSITION']].count
            indices = np.arange(0, num_verts, dtype=np.uint32)

        mode = 4 if prim.mode is None else prim.mode
        points, edges, tris = points_edges_tris(mode, indices)
        if points is not None:
            indices = points
        elif edges is not None:
            indices = edges
        else:
            indices = tris

        # We'll add one vert to the arrays for each index used in indices
        unique_indices, inv_indices = np.unique(indices, return_inverse=True)

        vs = BinaryData.decode_accessor(gltf, prim.attributes['POSITION'], cache=True)
        vert_locs = np.concatenate((vert_locs, vs[unique_indices]))

        if has_normals:
            if 'NORMAL' in prim.attributes:
                ns = BinaryData.decode_accessor(gltf, prim.attributes['NORMAL'], cache=True)
                ns = ns[unique_indices]
            else:
                ns = np.zeros((len(unique_indices), 3), dtype=np.float32)
            vert_normals = np.concatenate((vert_normals, ns))

        for i in range(num_joint_sets):
            if ('JOINTS_%d' % i) in prim.attributes and ('WEIGHTS_%d' % i) in prim.attributes:
                js = BinaryData.decode_accessor(gltf, prim.attributes['JOINTS_%d' % i], cache=True)
                ws = BinaryData.decode_accessor(gltf, prim.attributes['WEIGHTS_%d' % i], cache=True)
                js = js[unique_indices]
                ws = ws[unique_indices]
            else:
                js = np.zeros((len(unique_indices), 4), dtype=np.uint32)
                ws = np.zeros((len(unique_indices), 4), dtype=np.float32)
            vert_joints[i] = np.concatenate((vert_joints[i], js))
            vert_weights[i] = np.concatenate((vert_weights[i], ws))

        sk_i = 0
        for sk, sk_name in enumerate(pymesh.shapekey_names):
            if sk_name is None:
                continue
            if prim.targets and 'POSITION' in prim.targets[sk]:
                morph_vs = BinaryData.decode_accessor(gltf, prim.targets[sk]['POSITION'], cache=True)
                morph_vs = morph_vs[unique_indices]
            else:
                morph_vs = np.zeros((len(unique_indices), 3), dtype=np.float32)
            sk_vert_locs[sk_i] = np.concatenate((sk_vert_locs[sk_i], morph_vs))
            sk_i += 1

        # inv_indices are the indices into the verts just for this prim;
        # calculate indices into the overall verts array
        prim_vidxs = inv_indices.astype(np.uint32, copy=False)
        prim_vidxs += vert_index_base  # offset for verts from previous prims

        if edges is not None:
            edge_vidxs = np.concatenate((edge_vidxs, prim_vidxs))

        if tris is not None:
            prim.num_faces = len(indices) // 3
            num_faces += prim.num_faces

            loop_vidxs = np.concatenate((loop_vidxs, prim_vidxs))

            # UV only if we have a face
            for uv_i in range(num_uvs):
                if ('TEXCOORD_%d' % uv_i) in prim.attributes:
                    uvs = BinaryData.decode_accessor(gltf, prim.attributes['TEXCOORD_%d' % uv_i], cache=True)
                    uvs = uvs[indices]
                else:
                    uvs = np.zeros((len(indices), 2), dtype=np.float32)
                loop_uvs[uv_i] = np.concatenate((loop_uvs[uv_i], uvs))

        # We can have VC for points, lines, and tris
        for col_i in range(num_cols):
            if ('COLOR_%d' % col_i) in prim.attributes:
                cols = BinaryData.decode_accessor(gltf, prim.attributes['COLOR_%d' % col_i], cache=True)
                if vc_domains[col_i] == 'CORNER':
                    cols = cols[indices]
                else:
                    cols = cols[unique_indices]
                if cols.shape[1] == 3:
                    cols = colors_rgb_to_rgba(cols)
            else:
                if vc_domains[col_i] == 'CORNER':
                    cols = np.ones((len(indices), 4), dtype=np.float32)
                else:
                    cols = np.ones((len(unique_indices), 4), dtype=np.float32)
            loop_cols[col_i] = np.concatenate((loop_cols[col_i], cols))

        for idx, attr in enumerate(attributes):
            if attr in prim.attributes:
                attr_data = BinaryData.decode_accessor(gltf, prim.attributes[attr], cache=True)
                attribute_data[idx] = np.concatenate((attribute_data[idx], attr_data[unique_indices]))
            else:
                attr_data = np.zeros(
                    (len(unique_indices), DataType.num_elements(attribute_type[attr])),
                    dtype=ComponentType.to_numpy_dtype(attribute_component_type[attr])
                )
                attribute_data[idx] = np.concatenate((attribute_data[idx], attr_data))

    # Accessors are cached in case they are shared between primitives; clear
    # the cache now that all prims are done.
    gltf.decode_accessor_cache = {}

    if gltf.import_settings['merge_vertices']:
        vert_locs, vert_normals, vert_joints, vert_weights, \
            sk_vert_locs, loop_vidxs, edge_vidxs, attribute_data = \
            merge_duplicate_verts(
                vert_locs, vert_normals, vert_joints, vert_weights,
                sk_vert_locs, loop_vidxs, edge_vidxs, attribute_data
            )

    # ---------------
    # Convert all the arrays glTF -> Blender

    # Change from relative to absolute positions for morph locs
    for sk_locs in sk_vert_locs:
        sk_locs += vert_locs

    gltf.locs_batch_gltf_to_blender(vert_locs)
    gltf.normals_batch_gltf_to_blender(vert_normals)
    for sk_locs in sk_vert_locs:
        gltf.locs_batch_gltf_to_blender(sk_locs)

    if num_joint_sets and mesh_options.skin_into_bind_pose:
        skin_into_bind_pose(
            gltf, skin_idx, vert_joints, vert_weights,
            locs=[vert_locs] + sk_vert_locs,
            vert_normals=vert_normals,
        )

    for uvs in loop_uvs:
        uvs_gltf_to_blender(uvs)

    # ---------------
    # Start creating things

    mesh.vertices.add(len(vert_locs))
    position_attribute = attribute_ensure(mesh.attributes, 'position', 'FLOAT_VECTOR', 'POINT')
    position_attribute.data.foreach_set('vector', squish(vert_locs, np.float32))

    mesh.loops.add(len(loop_vidxs))
    corner_vert_attribute = attribute_ensure(mesh.attributes, '.corner_vert', 'INT', 'CORNER')
    corner_vert_attribute.data.foreach_set('value', squish(loop_vidxs, np.intc))

    mesh.edges.add(len(edge_vidxs) // 2)
    edge_verts_attribute = attribute_ensure(mesh.attributes, '.edge_verts', 'INT32_2D', 'EDGE')
    edge_verts_attribute.data.foreach_set('value', squish(edge_vidxs, np.intc))

    mesh.polygons.add(num_faces)

    # All polys are tris
    loop_starts = np.arange(0, 3 * num_faces, step=3)
    mesh.polygons.foreach_set('loop_start', loop_starts)

    for uv_i in range(num_uvs):
        name = 'UVMap' if uv_i == 0 else 'UVMap.%03d' % uv_i
        layer = mesh.uv_layers.new(name=name)

        if layer is None:
            gltf.log.warning("WARNING: UV map is ignored because the maximum number of UV layers has been reached.")
            break

        layer.uv.foreach_set('vector', squish(loop_uvs[uv_i], np.float32))

    for col_i in range(num_cols):
        name = 'Color' if col_i == 0 else 'Color.%03d' % col_i
        layer = mesh.color_attributes.new(name, 'BYTE_COLOR', vc_domains[col_i])

        layer.data.foreach_set('color', squish(loop_cols[col_i], np.float32))

    # Make sure the first Vertex Color Attribute is the rendered one
    if num_cols > 0:
        mesh.color_attributes.render_color_index = 0

    # Skinning
    # TODO: this is slow :/
    if num_joint_sets and mesh_options.skinning:
        pyskin = gltf.data.skins[skin_idx]
        for i, node_idx in enumerate(pyskin.joints):
            bone = gltf.vnodes[node_idx]
            ob.vertex_groups.new(name=bone.blender_bone_name)

        vgs = list(ob.vertex_groups)

        for i in range(num_joint_sets):
            js = vert_joints[i].tolist()  # tolist() is faster
            ws = vert_weights[i].tolist()
            for vi in range(len(vert_locs)):
                w0, w1, w2, w3 = ws[vi]
                j0, j1, j2, j3 = js[vi]
                if w0 != 0:
                    vgs[j0].add((vi,), w0, 'REPLACE')
                if w1 != 0:
                    vgs[j1].add((vi,), w1, 'REPLACE')
                if w2 != 0:
                    vgs[j2].add((vi,), w2, 'REPLACE')
                if w3 != 0:
                    vgs[j3].add((vi,), w3, 'REPLACE')

    # Shapekeys
    if num_shapekeys:
        ob.shape_key_add(name='Basis')
        mesh.shape_keys.name = mesh.name

        sk_i = 0
        for sk_name in pymesh.shapekey_names:
            if sk_name is None:
                continue

            key_block = ob.shape_key_add(name=sk_name)
            key_block.value = 0.0 # Starting Blender 5.0, SK are created with weight 1.0, so setting to 0.0, as 0.0 is the default glTF SK weight
            key_block.points.foreach_set('co', squish(sk_vert_locs[sk_i], np.float32))

            sk_i += 1

    # ----
    # Assign materials to faces
    has_materials = any(prim.material is not None for prim in pymesh.primitives)
    # Even if no primitive have material, we need to create slots if some primitives have some variant
    if has_materials is False:
        has_materials = any(prim.extensions is not None and 'KHR_materials_variants' in prim.extensions.keys()
                            for prim in pymesh.primitives)

    # Even if there is no material, some primitives can have some Vertex Color
    # (and we need to create a slot for them, with a default material + vertex color)
    if has_materials is False:
        has_materials = any(prim.attributes.get('COLOR_0') is not None for prim in pymesh.primitives)

    default_materials = {} # Store VC => index
    we_can_merge_slots = gltf.import_settings['import_merge_material_slots']

    if has_materials is True:
        bl_material_index_dtype = np.intc
        material_indices = np.empty(num_faces, dtype=bl_material_index_dtype)
        empty_material_slot_index = None
        f = 0

        for idx_prim, prim in enumerate(pymesh.primitives):

            has_variant = prim.extensions is not None and 'KHR_materials_variants' in prim.extensions.keys() \
                and 'mappings' in prim.extensions['KHR_materials_variants'].keys()

            if prim.material is not None:
                # Get the material
                pymaterial = gltf.data.materials[prim.material]
                vertex_color = 'COLOR_0' if ('COLOR_0' in prim.attributes) else None
                if vertex_color not in pymaterial.blender_material:
                    BlenderMaterial.create(gltf, prim.material, vertex_color)
                material_name = pymaterial.blender_material[vertex_color]

                # Put material in slot (if not there)
                # So we are going to use the existing slot if exists, or create a new one
                if we_can_merge_slots is True and not has_variant:
                    if material_name not in mesh.materials:
                        mesh.materials.append(bpy.data.materials[material_name])
                    material_index = mesh.materials.find(material_name)
                else:
                    # In case of variant, do not merge slots // or if user does not want to merge slots
                    # So we are going to create a new slot
                    mesh.materials.append(bpy.data.materials[material_name])
                    material_index = len(mesh.materials) - 1
            else:
                 # Check if the primitive has a vertex color
                    vertex_color = 'COLOR_0' if ('COLOR_0' in prim.attributes) else None
                    if vertex_color is not None:
                        if we_can_merge_slots is True and not has_variant:
                            # Check if we already have a slot for default material + this vertec color
                            if vertex_color in default_materials.keys():
                                material_index = default_materials[vertex_color]
                            else:
                                # Create a new slot for default material + this vertec color
                                name = BlenderMaterial.create(gltf, None, vertex_color)
                                mesh.materials.append(bpy.data.materials[name])
                                material_index = len(mesh.materials) - 1
                                default_materials[vertex_color] = material_index
                        else:
                            # In case of variant, do not merge slots // or if user does not want to merge slots
                            # So we are going to create a new slot if not exists already
                            # Else, create a new slot, but using the existing material
                            if vertex_color in default_materials.keys():
                                material_index = default_materials[vertex_color]
                                mesh.materials.append(bpy.data.materials[mesh.materials[material_index].name])
                                material_index = len(mesh.materials) - 1
                            else:
                                name = BlenderMaterial.create(gltf, None, vertex_color)
                                mesh.materials.append(bpy.data.materials[name])
                                material_index = len(mesh.materials) - 1
                                default_materials[vertex_color] = material_index
                    else:
                        if we_can_merge_slots is True and not has_variant:
                            # Create an empty slot if not exists already, or use the existing one
                            if empty_material_slot_index is None:
                                mesh.materials.append(None)
                                empty_material_slot_index = len(mesh.materials) - 1
                            material_index = empty_material_slot_index
                        else:
                            # In case of variant, do not merge slots // or if user does not want to merge slots
                            # So we are going to create a new slot
                            mesh.materials.append(None)
                            material_index = len(mesh.materials) - 1

            material_indices[f:f + prim.num_faces].fill(material_index)

            f += prim.num_faces

            # Manage variants
            if has_variant:

                # Store default material
                default_mat = mesh.gltf2_variant_default_materials.add()
                default_mat.material_slot_index = material_index
                default_mat.default_material = bpy.data.materials[material_name] if prim.material is not None else None

                for mapping in prim.extensions['KHR_materials_variants']['mappings']:
                    # Store, for each variant, the material link to this primitive

                    variant_primitive = mesh.gltf2_variant_mesh_data.add()
                    variant_primitive.material_slot_index = material_index
                    if 'material' not in mapping.keys():
                        # Default material
                        variant_primitive.material = None
                    else:
                        vertex_color = 'COLOR_0' if 'COLOR_0' in prim.attributes else None
                        if str(mapping['material']) + str(vertex_color) not in gltf.variant_mapping.keys():
                            BlenderMaterial.create(gltf, mapping['material'], vertex_color)
                        variant_primitive.material = gltf.variant_mapping[str(mapping['material']) + str(vertex_color)]

                    for variant in mapping['variants']:
                        vari = variant_primitive.variants.add()
                        vari.variant.variant_idx = variant

        material_index_attribute = attribute_ensure(mesh.attributes, 'material_index', 'INT', 'FACE')
        material_index_attribute.data.foreach_set('value', material_indices)

    # Custom Attributes
    for idx, attr in enumerate(attributes):

        blender_attribute_data_type = get_attribute_type(
            attribute_component_type[attr],
            attribute_type[attr]
        )

        if blender_attribute_data_type is None:
            continue

        blender_attribute = mesh.attributes.new(attr, blender_attribute_data_type, 'POINT')
        if DataType.num_elements(attribute_type[attr]) == 1:
            blender_attribute.data.foreach_set('value', attribute_data[idx].flatten())
        elif DataType.num_elements(attribute_type[attr]) > 1:
            if blender_attribute_data_type in ["BYTE_COLOR", "FLOAT_COLOR"]:
                blender_attribute.data.foreach_set('color', attribute_data[idx].flatten())
            else:
                blender_attribute.data.foreach_set('vector', attribute_data[idx].flatten())

    # ----
    # Normals

    # Set polys smooth/flat
    set_poly_smoothing(gltf, pymesh, mesh, vert_normals, loop_vidxs)
    has_loose_edges = len(edge_vidxs) != 0  # need to calc_loose_edges for them to show up
    mesh.update(calc_edges=True, calc_edges_loose=has_loose_edges) # Make sure edges are calculated (new for Blender 5.1+)
    mesh.validate()

    if has_normals:
        mesh.normals_split_custom_set_from_vertices(vert_normals)


def points_edges_tris(mode, indices):
    points = None
    edges = None
    tris = None

    if mode == 0:
        # POINTS
        points = indices

    elif mode == 1:
        # LINES
        #   1   3
        #  /   /
        # 0   2
        edges = indices

    elif mode == 2:
        # LINE LOOP
        #   1---2
        #  /     \
        # 0-------3
        # in:  0123
        # out: 01122330
        edges = np.empty(2 * len(indices), dtype=np.uint32)
        edges[[0, -1]] = indices[[0, 0]]  # 0______0
        edges[1:-1] = np.repeat(indices[1:], 2)  # 01122330

    elif mode == 3:
        # LINE STRIP
        #   1---2
        #  /     \
        # 0       3
        # in:  0123
        # out: 011223
        edges = np.empty(2 * len(indices) - 2, dtype=np.uint32)
        edges[[0, -1]] = indices[[0, -1]]  # 0____3
        edges[1:-1] = np.repeat(indices[1:-1], 2)  # 011223

    elif mode == 4:
        # TRIANGLES
        #   2     3
        #  / \   / \
        # 0---1 4---5
        tris = indices

    elif mode == 5:
        # TRIANGLE STRIP
        # 0---2---4
        #  \ / \ /
        #   1---3
        # in: 01234
        # out: 012132234
        # out (viewed as triplets): 012, 132, 234
        tris = np.empty((len(indices) - 2) * 3, dtype=np.uint32)
        # 012__
        first_indices = indices[:-2]
        # _123_
        second_indices = indices[1:-1]
        # __234
        third_indices = indices[2:]

        # Each triplet starts with the first index
        # 0__, 1__, 2__ <- 012__
        tris[0::3] = first_indices

        # Even triplets end with the next two indices in order
        # _1_, ___, _3_ <- _1_3_ <- _123_
        # 01_, 1__, 23_
        tris[1::6] = second_indices[0::2]
        # __2, ___, __4 <- __2_4 <- __234
        # 012, 1__, 234
        tris[2::6] = third_indices[0::2]

        # Odd triplets end with the next two indices in reverse order
        # ___, _3_, ___ <- ___3_ <- __234
        # 012, 13_, 234
        tris[4::6] = third_indices[1::2]
        # ___, __2, ___ <- __2__ <- _123_
        # 012, 132, 234
        tris[5::6] = second_indices[1::2]

    elif mode == 6:
        # TRIANGLE FAN
        #   3---2
        #  / \ / \
        # 4---0---1
        # in: 01234
        # out: 012023034
        # out (viewed as triplets): 012, 023, 034
        # Start filled with the first index
        # 000, 000, 000
        tris = np.full((len(indices) - 2) * 3, indices[0], dtype=np.uint32)
        # _1_, _2_, _3_ <- _123_
        # 010, 020, 030
        tris[1::3] = indices[1:-1]
        # __2, __3, __4 <- __234
        # 012, 023, 034
        tris[2::3] = indices[2:]

    else:
        raise Exception('primitive mode unimplemented: %d' % mode)

    return points, edges, tris


def squish(array, dtype=None):
    """Squish nD array into a C-contiguous (required for faster access with the buffer protocol in foreach_set) 1D array
    (required by foreach_set). Optionally converting the array to a different dtype."""
    return np.ascontiguousarray(array, dtype=dtype).reshape(array.size)


def colors_rgb_to_rgba(rgb):
    rgba = np.ones((len(rgb), 4), dtype=np.float32)
    rgba[:, :3] = rgb
    return rgba


def uvs_gltf_to_blender(uvs):
    # u,v -> u,1-v
    uvs[:, 1] *= -1
    uvs[:, 1] += 1


def skin_into_bind_pose(gltf, skin_idx, vert_joints, vert_weights, locs, vert_normals):
    # Skin each position/normal using the bind pose.
    # Skinning equation: vert' = sum_(j,w) w * joint_mat[j] * vert
    # where the sum is over all (joint,weight) pairs.

    # Calculate joint matrices
    joint_mats = []
    pyskin = gltf.data.skins[skin_idx]
    if pyskin.inverse_bind_matrices is not None:
        inv_binds = BinaryData.get_data_from_accessor(gltf, pyskin.inverse_bind_matrices)
        inv_binds = [gltf.matrix_gltf_to_blender(m) for m in inv_binds]
    else:
        inv_binds = [Matrix.Identity(4) for i in range(len(pyskin.joints))]
    bind_mats = [gltf.vnodes[joint].bind_arma_mat for joint in pyskin.joints]
    joint_mats = [bind_mat @ inv_bind for bind_mat, inv_bind in zip(bind_mats, inv_binds)]

    # TODO: check if joint_mats are all (approximately) 1, and skip skinning

    joint_mats = np.array(joint_mats, dtype=np.float32)

    # Compute the skinning matrices for every vert
    num_verts = len(locs[0])
    skinning_mats = np.zeros((num_verts, 4, 4), dtype=np.float32)
    weight_sums = np.zeros(num_verts, dtype=np.float32)
    for js, ws in zip(vert_joints, vert_weights):
        for i in range(4):
            skinning_mats += ws[:, i].reshape(len(ws), 1, 1) * joint_mats[js[:, i]]
            weight_sums += ws[:, i]

    # Some invalid files have 0 weight sum.
    # To avoid to have this vertices at 0.0 / 0.0 / 0.0
    # We set all weight ( aka 1.0 ) to the first bone
    zeros_indices = np.where(weight_sums == 0)[0]
    if zeros_indices.shape[0] > 0:
        gltf.log.error('File is invalid: Some vertices are not assigned to bone(s) ')
        vert_weights[0][:, 0][zeros_indices] = 1.0  # Assign to first bone with all weight

        # Reprocess IBM for these vertices
        skinning_mats[zeros_indices] = np.zeros((4, 4), dtype=np.float32)
        for js, ws in zip(vert_joints, vert_weights):
            for i in range(4):
                skinning_mats[zeros_indices] += ws[:,
                                                   i][zeros_indices].reshape(len(ws[zeros_indices]), 1, 1) * joint_mats[js[:, i][zeros_indices]]
                weight_sums[zeros_indices] += ws[:, i][zeros_indices]

    skinning_mats /= weight_sums.reshape(num_verts, 1, 1)

    skinning_mats_3x3 = skinning_mats[:, :3, :3]
    skinning_trans = skinning_mats[:, :3, 3]

    for vs in locs:
        vs[:] = mul_mats_vecs(skinning_mats_3x3, vs)
        vs[:] += skinning_trans

    if len(vert_normals) != 0:
        vert_normals[:] = mul_mats_vecs(skinning_mats_3x3, vert_normals)
        # Don't translate normals!
        normalize_vecs(vert_normals)


def mul_mats_vecs(mats, vecs):
    """Given [m1,m2,...] and [v1,v2,...], returns [m1@v1,m2@v2,...]. 3D only."""
    return np.matmul(mats, vecs.reshape(len(vecs), 3, 1)).reshape(len(vecs), 3)


def normalize_vecs(vectors):
    norms = np.linalg.norm(vectors, axis=1, keepdims=True)
    np.divide(vectors, norms, out=vectors, where=norms != 0)


def attribute_ensure(attributes, name, data_type, domain):
    attribute = attributes.get(name)
    if attribute is None:
        return attributes.new(name, data_type, domain)
    if attribute.domain == domain and attribute.data_type == data_type:
        return attribute
    # There is an existing attribute, but it has the wrong domain or data_type.
    attributes.remove(attribute)
    return attributes.new(name, data_type, domain)


def set_poly_smoothing(gltf, pymesh, mesh, vert_normals, loop_vidxs):
    num_polys = len(mesh.polygons)

    if gltf.import_settings['import_shading'] == "FLAT":
        # Polys are smooth by default, setting to flat
        mesh.shade_flat()
        return

    if gltf.import_settings['import_shading'] == "SMOOTH":
        poly_sharps = np.full(num_polys, False)
        f = 0
        for prim in pymesh.primitives:
            if 'NORMAL' not in prim.attributes:
                # Primitives with no NORMALs should use flat shading
                poly_sharps[f:f + prim.num_faces].fill(True)
            f += prim.num_faces
        sharp_face_attribute = attribute_ensure(mesh.attributes, 'sharp_face', 'BOOLEAN', 'FACE')
        sharp_face_attribute.data.foreach_set('value', poly_sharps)
        return

    assert gltf.import_settings['import_shading'] == "NORMALS"

    # Try to guess which polys should be flat based on the fact that all the
    # loop normals for a flat poly are = the poly's normal.

    poly_sharps = np.empty(num_polys, dtype=bool)

    poly_normals = np.empty(num_polys * 3, dtype=np.float32)
    mesh.polygon_normals.foreach_get('vector', poly_normals)
    poly_normals = poly_normals.reshape(num_polys, 3)

    f = 0
    for prim in pymesh.primitives:
        if 'NORMAL' not in prim.attributes:
            # Primitives with no NORMALs should use flat shading
            poly_sharps[f:f + prim.num_faces].fill(True)
            f += prim.num_faces
            continue

        # Check the normals at the three corners against the poly normal.
        # Two normals are equal iff their dot product is 1.

        poly_ns = poly_normals[f:f + prim.num_faces]

        # Dot product against the first vertex normal in the tri
        vert_ns = vert_normals[loop_vidxs[3 * f:3 * (f + prim.num_faces):3]]
        dot_prods = np.sum(vert_ns * poly_ns, axis=1)  # dot product
        smooth = (dot_prods <= 0.9999999)

        # Same for the second vertex, etc.
        vert_ns = vert_normals[loop_vidxs[3 * f + 1:3 * (f + prim.num_faces):3]]
        dot_prods = np.sum(vert_ns * poly_ns, axis=1)
        np.logical_or(smooth, dot_prods <= 0.9999999, out=smooth)

        vert_ns = vert_normals[loop_vidxs[3 * f + 2:3 * (f + prim.num_faces):3]]
        dot_prods = np.sum(vert_ns * poly_ns, axis=1)
        np.logical_or(smooth, dot_prods <= 0.9999999, out=smooth)

        np.logical_not(smooth, out=poly_sharps[f:f + prim.num_faces])

        f += prim.num_faces

    sharp_face_attribute = attribute_ensure(mesh.attributes, 'sharp_face', 'BOOLEAN', 'FACE')
    sharp_face_attribute.data.foreach_set('value', poly_sharps)


def merge_duplicate_verts(
        vert_locs,
        vert_normals,
        vert_joints,
        vert_weights,
        sk_vert_locs,
        loop_vidxs,
        edge_vidxs,
        attribute_data):
    # This function attempts to invert the splitting done when exporting to
    # glTF. Welds together verts with the same per-vert data (but possibly
    # different per-loop data).
    #
    # Ideally normals would be treated as per-loop data, but that has problems,
    # so we currently treat the normal as per-vert.
    #
    # Strategy is simple: put all the per-vert data into an array of structs
    # ("dots"), dedupe with np.unique, then take all the data back out.

    # Very often two verts that "morally" should be merged will have normals
    # with very small differences. Round off the normals to smooth this over.
    if len(vert_normals) != 0:
        vert_normals *= 50000
        vert_normals[:] = np.trunc(vert_normals)
        vert_normals *= (1 / 50000)

    dot_fields = [('x', np.float32), ('y', np.float32), ('z', np.float32)]
    if len(vert_normals) != 0:
        dot_fields += [('nx', np.float32), ('ny', np.float32), ('nz', np.float32)]
    for i, _ in enumerate(vert_joints):
        dot_fields += [
            ('joint%dx' % i, np.uint32), ('joint%dy' % i, np.uint32),
            ('joint%dz' % i, np.uint32), ('joint%dw' % i, np.uint32),
            ('weight%dx' % i, np.float32), ('weight%dy' % i, np.float32),
            ('weight%dz' % i, np.float32), ('weight%dw' % i, np.float32),
        ]
    for i, _ in enumerate(sk_vert_locs):
        dot_fields += [
            ('sk%dx' % i, np.float32), ('sk%dy' % i, np.float32), ('sk%dz' % i, np.float32),
        ]
    dots = np.empty(len(vert_locs), dtype=np.dtype(dot_fields))

    dots['x'] = vert_locs[:, 0]
    dots['y'] = vert_locs[:, 1]
    dots['z'] = vert_locs[:, 2]
    if len(vert_normals) != 0:
        dots['nx'] = vert_normals[:, 0]
        dots['ny'] = vert_normals[:, 1]
        dots['nz'] = vert_normals[:, 2]
    for i, (joints, weights) in enumerate(zip(vert_joints, vert_weights)):
        dots['joint%dx' % i] = joints[:, 0]
        dots['joint%dy' % i] = joints[:, 1]
        dots['joint%dz' % i] = joints[:, 2]
        dots['joint%dw' % i] = joints[:, 3]
        dots['weight%dx' % i] = weights[:, 0]
        dots['weight%dy' % i] = weights[:, 1]
        dots['weight%dz' % i] = weights[:, 2]
        dots['weight%dw' % i] = weights[:, 3]
    for i, locs in enumerate(sk_vert_locs):
        dots['sk%dx' % i] = locs[:, 0]
        dots['sk%dy' % i] = locs[:, 1]
        dots['sk%dz' % i] = locs[:, 2]

    unique_dots, unique_ind, inv_indices = fast_structured_np_unique(dots, return_index=True, return_inverse=True)

    loop_vidxs = inv_indices[loop_vidxs]
    edge_vidxs = inv_indices[edge_vidxs]

    # We don't split vertices only because of custom attribute
    # If 2 vertices have same data (pos, normals, etc...) except custom attribute, we
    # keep 1 custom attribute, arbitrary
    for idx, i in enumerate(attribute_data):
        attribute_data[idx] = attribute_data[idx][unique_ind]

    vert_locs = np.empty((len(unique_dots), 3), dtype=np.float32)
    vert_locs[:, 0] = unique_dots['x']
    vert_locs[:, 1] = unique_dots['y']
    vert_locs[:, 2] = unique_dots['z']
    if len(vert_normals) != 0:
        vert_normals = np.empty((len(unique_dots), 3), dtype=np.float32)
        vert_normals[:, 0] = unique_dots['nx']
        vert_normals[:, 1] = unique_dots['ny']
        vert_normals[:, 2] = unique_dots['nz']
    for i in range(len(vert_joints)):
        vert_joints[i] = np.empty((len(unique_dots), 4), dtype=np.uint32)
        vert_joints[i][:, 0] = unique_dots['joint%dx' % i]
        vert_joints[i][:, 1] = unique_dots['joint%dy' % i]
        vert_joints[i][:, 2] = unique_dots['joint%dz' % i]
        vert_joints[i][:, 3] = unique_dots['joint%dw' % i]
        vert_weights[i] = np.empty((len(unique_dots), 4), dtype=np.float32)
        vert_weights[i][:, 0] = unique_dots['weight%dx' % i]
        vert_weights[i][:, 1] = unique_dots['weight%dy' % i]
        vert_weights[i][:, 2] = unique_dots['weight%dz' % i]
        vert_weights[i][:, 3] = unique_dots['weight%dw' % i]
    for i in range(len(sk_vert_locs)):
        sk_vert_locs[i] = np.empty((len(unique_dots), 3), dtype=np.float32)
        sk_vert_locs[i][:, 0] = unique_dots['sk%dx' % i]
        sk_vert_locs[i][:, 1] = unique_dots['sk%dy' % i]
        sk_vert_locs[i][:, 2] = unique_dots['sk%dz' % i]

    return vert_locs, vert_normals, vert_joints, vert_weights, sk_vert_locs, loop_vidxs, edge_vidxs, attribute_data
