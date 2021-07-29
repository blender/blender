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

import itertools
from collections import defaultdict

import bpy
import bmesh
from bpy.props import BoolProperty, StringProperty, FloatProperty, IntProperty
from mathutils import Matrix

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, match_long_repeat, fullList
from sverchok.utils.sv_bmesh_utils import bmesh_from_pydata, pydata_from_bmesh
from sverchok.utils.sv_viewer_utils import (
    greek_alphabet, matrix_sanitizer, remove_non_updated_objects
)


def process_mesh_into_features(skin_vertices, edge_keys, assume_unique=True):
    """
    be under no illusion that this function is an attempt at an optimized tip/junction finder.
    primarily it exists to test an assumption about foreach_set and obj.data.skin_verts edge keys.

        # this works for a edgenet mesh (polyline) that has no disjoint elements
        # but I think all it does it set the last vertex as the root.
        obj.data.skin_vertices[0].data.foreach_set('use_root', all_yes)

    disjoint elements each need 1 vertex set to root
    """

    # need a set of sorted keys
    if not assume_unique:
        try:
            edge_keys = set(edge_keys)
        except:
            # this will be slower..but it should catch most input
            edge_keys = set(tuple(sorted(key)) for key in edge_keys)
    else:
        edge_keys = set(edge_keys)

    # iterate and accumulate
    ndA = defaultdict(set)
    for key in edge_keys:
        lowest, highest = key
        ndA[lowest].add(highest)
        ndA[highest].add(lowest)

    ndB = defaultdict(set)
    ndC = {k: len(ndA[k]) for k in sorted(ndA.keys()) if len(ndA[k]) == 1 or len(ndA[k]) >=3}
    for k, v in ndC.items():
        ndB[v].add(k)

    # in heavily branching input, there will be a lot of redundant use_root pushing.
    for k in sorted(ndB.keys()):
        for index in ndB[k]:
            skin_vertices[index].use_root = True


def set_data_for_layer(bm, data, layer):
    for i in range(len(bm.verts)):
        bm.verts[i][data] = layer[i] or 0.1


def shrink_geometry(bm, dist, layers):
    vlayers = bm.verts.layers
    made_layers = []
    for idx, layer in enumerate(layers):
        first_element = layer[0] or 0.2
        if isinstance(first_element, float):
            data_layer = vlayers.float.new('float_layer' + str(idx))

        made_layers.append(data_layer)
        bm.verts.ensure_lookup_table()
        set_data_for_layer(bm, data_layer, layer)

    bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=dist)
    bm.verts.ensure_lookup_table()
    
    verts, edges, faces = pydata_from_bmesh(bm)
    data_out = [verts, edges, faces]
    for layer_name in made_layers:
        data_out.append([bm.verts[i][layer_name] for i in range(len(bm.verts))])

    return data_out


def assign_empty_mesh(idx):
    meshes = bpy.data.meshes
    mt_name = 'empty_skin_mesh_sv.' + str("%04d" % idx)
    if mt_name in meshes:
        return meshes[mt_name]
    else:
        return meshes.new(mt_name)


def force_pydata(mesh, verts, edges):

    mesh.vertices.add(len(verts))
    f_v = list(itertools.chain.from_iterable(verts))
    mesh.vertices.foreach_set('co', f_v)
    mesh.update()

    mesh.edges.add(len(edges))
    f_e = list(itertools.chain.from_iterable(edges))
    mesh.edges.foreach_set('vertices', f_e)
    mesh.update(calc_edges=True)


def make_bmesh_geometry(node, context, geometry, idx, layers):
    scene = context.scene
    meshes = bpy.data.meshes
    objects = bpy.data.objects
    verts, edges, matrix, _, _ = geometry
    name = node.basemesh_name + '.' + str("%04d" % idx)

    # remove object
    if name in objects:
        obj = objects[name]
        # assign the object an empty mesh, this allows the current mesh
        # to be uncoupled and removed from bpy.data.meshes
        obj.data = assign_empty_mesh(idx)

        # remove uncoupled mesh, and add it straight back.
        if name in meshes:
            meshes.remove(meshes[name])
        mesh = meshes.new(name)
        obj.data = mesh
    else:
        # this is only executed once, upon the first run.
        mesh = meshes.new(name)
        obj = objects.new(name, mesh)
        scene.objects.link(obj)

    # at this point the mesh is always fresh and empty
    obj['idx'] = idx
    obj['basename'] = node.basemesh_name

    data_layers = None
    if node.distance_doubles > 0.0:
        bm = bmesh_from_pydata(verts, edges, [])
        verts, edges, faces, d1, d2 = shrink_geometry(bm, node.distance_doubles, layers)
        data_layers = d1, d2

    force_pydata(obj.data, verts, edges)
    obj.update_tag(refresh={'OBJECT', 'DATA'})

    if node.live_updates:

        if 'sv_skin' in obj.modifiers:
            sk = obj.modifiers['sv_skin']
            obj.modifiers.remove(sk)

        if 'sv_subsurf' in obj.modifiers:
            sd = obj.modifiers['sv_subsurf']
            obj.modifiers.remove(sd)

        _ = obj.modifiers.new(type='SKIN', name='sv_skin')
        b = obj.modifiers.new(type='SUBSURF', name='sv_subsurf')
        b.levels = node.levels
        b.render_levels = node.render_levels

    if matrix:
        matrix = matrix_sanitizer(matrix)
        obj.matrix_local = matrix
    else:
        obj.matrix_local = Matrix.Identity(4)

    return obj, data_layers


class SvSkinmodViewOpMK1b(bpy.types.Operator):

    bl_idname = "node.sv_callback_skinmod_viewer_mk1b"
    bl_label = "Sverchok skinmod general callback 1b"
    bl_options = {'REGISTER', 'UNDO'}

    fn_name = StringProperty(default='')

    def skin_ops(self, context, type_op):
        n = context.node
        k = n.basemesh_name + "_"

        if type_op == 'add_material':
            mat = bpy.data.materials.new('sv_material')
            mat.use_nodes = True
            mat.use_fake_user = True  # usually handy
            n.material = mat.name

    def execute(self, context):
        self.skin_ops(context, self.fn_name)
        return {'FINISHED'}


class SvSkinViewerNodeMK1b(bpy.types.Node, SverchCustomTreeNode):

    bl_idname = 'SvSkinViewerNodeMK1b'
    bl_label = 'Skin Mesher mk1b'
    bl_icon = 'OUTLINER_OB_EMPTY'

    basemesh_name = StringProperty(
        default='Alpha',
        update=updateNode,
        description="sets which base name the object will use, "
        "use N-panel to pick alternative random names")

    live_updates = BoolProperty(
        default=0,
        update=updateNode,
        description="This auto updates the modifier (by removing and adding it)")

    general_radius_x = FloatProperty(
        name='general_radius_x',
        default=0.25,
        description='value used to uniformly set the radii of skin vertices x',
        min=0.0001, step=0.05,
        update=updateNode)

    general_radius_y = FloatProperty(
        name='general_radius_y',
        default=0.25,
        description='value used to uniformly set the radii of skin vertices y',
        min=0.0001, step=0.05,
        update=updateNode)

    levels = IntProperty(min=0, default=1, max=3, update=updateNode)
    render_levels = IntProperty(min=0, default=1, max=3, update=updateNode)

    distance_doubles = FloatProperty(
        default=0.0, min=0.0,
        name='doubles distance',
        description="removes coinciding verts, also aims to remove double radii data",
        update=updateNode)

    material = StringProperty(default='', update=updateNode)
    use_root = BoolProperty(default=True, update=updateNode)
    use_slow_root = BoolProperty(default=False, update=updateNode)


    def sv_init(self, context):
        gai = bpy.context.scene.SvGreekAlphabet_index
        self.basemesh_name = greek_alphabet[gai]
        bpy.context.scene.SvGreekAlphabet_index += 1
        self.use_custom_color = True
        self.inputs.new('VerticesSocket', 'vertices')
        self.inputs.new('StringsSocket', 'edges')
        self.inputs.new('MatrixSocket', 'matrix')
        self.inputs.new('StringsSocket', 'radii_x').prop_name = "general_radius_x"
        self.inputs.new('StringsSocket', 'radii_y').prop_name = "general_radius_y"


    def draw_buttons(self, context, layout):

        r = layout.row(align=True)
        r.prop(self, "live_updates", text="Live", toggle=True)
        r.prop(self, "basemesh_name", text="", icon='OUTLINER_OB_MESH')

        r3 = layout.column(align=True)
        r3.prop(self, 'levels', text="div View")
        r3.prop(self, 'render_levels', text="div Render")
        r3.prop(self, 'distance_doubles', text='doubles distance')

        sh = "node.sv_callback_skinmod_viewer_mk1b"
        r5 = layout.row(align=True)
        r5.prop_search(
            self, 'material', bpy.data, 'materials', text='',
            icon='MATERIAL_DATA')
        r5.operator(sh, text='', icon='ZOOMIN').fn_name = 'add_material'


    def draw_buttons_ext(self, context, layout):
        k = layout.box()
        r = k.row(align=True)
        r.label("setting roots")
        r = k.row(align=True)
        r.prop(self, "use_root", text="mark all", toggle=True)
        r.prop(self, "use_slow_root", text="mark some", toggle=True)



    def get_geometry_from_sockets(self):
        i = self.inputs
        mverts = i['vertices'].sv_get(default=[])
        medges = i['edges'].sv_get(default=[])
        mmtrix = i['matrix'].sv_get(default=[None])
        mradiix = i['radii_x'].sv_get()
        mradiiy = i['radii_y'].sv_get()
        return mverts, medges, mmtrix, mradiix, mradiiy


    def process(self):
        if not self.live_updates:
            return

        # only interested in the first
        geometry_full = self.get_geometry_from_sockets()

        # pad all input to longest
        maxlen = max(*(map(len, geometry_full)))
        fullList(geometry_full[0], maxlen)
        fullList(geometry_full[1], maxlen)
        fullList(geometry_full[2], maxlen)
        fullList(geometry_full[3], maxlen)
        fullList(geometry_full[4], maxlen)

        catch_idx = 0
        for idx, (geometry) in enumerate(zip(*geometry_full)):
            catch_idx = idx
            self.unit_generator(idx, geometry)

        # remove stail objects
        remove_non_updated_objects(self, catch_idx)


    def unit_generator(self, idx, geometry):
        verts, _, _, radiix, radiiy = geometry
        ntimes = len(verts)
        radiix, _ = match_long_repeat([radiix, verts])
        radiiy, _ = match_long_repeat([radiiy, verts])

        # assign radii after creation
        obj, data_layers = make_bmesh_geometry(self, bpy.context, geometry, idx, [radiix, radiiy])

        if data_layers and self.distance_doubles > 0.0:
            # This sets the modified geometry with radius x and radius y.
            f_r = list(itertools.chain(*zip(data_layers[0], data_layers[1])))
            f_r = [abs(f) for f in f_r]
            obj.data.skin_vertices[0].data.foreach_set('radius', f_r)   
            all_yes = list(itertools.repeat(True, len(obj.data.vertices)))
            obj.data.skin_vertices[0].data.foreach_set('use_root', all_yes)

        elif len(radiix) == len(verts):
            f_r = list(itertools.chain(*zip(radiix, radiiy)))
            f_r = [abs(f) for f in f_r]
            obj.data.skin_vertices[0].data.foreach_set('radius', f_r)

        if self.use_root:        
            # set all to root
            all_yes = list(itertools.repeat(True, ntimes))
            obj.data.skin_vertices[0].data.foreach_set('use_root', all_yes)
        elif self.use_slow_root:
            process_mesh_into_features(obj.data.skin_vertices[0].data, obj.data.edge_keys)

        # truthy if self.material is in .materials
        if bpy.data.materials.get(self.material):
            self.set_corresponding_materials([obj])


    def set_corresponding_materials(self, objs):
        for obj in objs:
            obj.active_material = bpy.data.materials[self.material]

    def flip_roots_or_junctions_only(self, data):
        ...



def register():
    bpy.utils.register_class(SvSkinmodViewOpMK1b)
    bpy.utils.register_class(SvSkinViewerNodeMK1b)


def unregister():
    bpy.utils.unregister_class(SvSkinViewerNodeMK1b)
    bpy.utils.unregister_class(SvSkinmodViewOpMK1b)
