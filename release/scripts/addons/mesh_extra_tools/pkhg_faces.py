# gpl author: PHKG

bl_info = {
    "name": "PKHG faces",
    "author": "PKHG",
    "version": (0, 0, 6),
    "blender": (2, 7, 1),
    "location": "View3D > Tools > PKHG (tab)",
    "description": "Faces selected will become added faces of different style",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh",
}

import bpy
import bmesh
from bpy.types import Operator
from mathutils import Vector
from bpy.props import (
        BoolProperty,
        StringProperty,
        IntProperty,
        FloatProperty,
        EnumProperty,
        )


class MESH_OT_add_faces_to_object(Operator):
    bl_idname = "mesh.add_faces_to_object"
    bl_label = "Face Extrude"
    bl_description = "Set parameters and build object with added faces"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    reverse_faces = BoolProperty(
            name="Reverse Faces",
            default=False,
            description="Revert the normals of selected faces"
            )
    name_source_object = StringProperty(
            name="Mesh",
            description="Choose a Source Mesh",
            default="Cube"
            )
    remove_start_faces = BoolProperty(
            name="Remove Start Faces",
            default=True,
            description="Make a choice about removal of Original Faces"
            )
    base_height = FloatProperty(
            name="Base Height",
            min=-20,
            soft_max=10, max=20,
            default=0.2,
            description="Set general Base Height"
            )
    use_relative_base_height = BoolProperty(
            name="Relative Base Height",
            default=False,
            description="Relative or absolute Base Height"
            )
    second_height = FloatProperty(
            name="2nd height", min=-5,
            soft_max=5, max=20,
            default=0.2,
            description="Second height for various shapes"
            )
    width = FloatProperty(
            name="Width Faces",
            min=-20, max=20,
            default=0.5,
            description="Set general width"
            )
    repeat_extrude = IntProperty(
            name="Repeat",
            min=1,
            soft_max=5, max=20,
            description="For longer base"
            )
    move_inside = FloatProperty(
            name="Move Inside",
            min=0.0,
            max=1.0,
            default=0.5,
            description="How much move to inside"
            )
    thickness = FloatProperty(
            name="Thickness",
            soft_min=0.01, min=0,
            soft_max=5.0, max=20.0,
            default=0
            )
    depth = FloatProperty(
            name="Depth",
            min=-5,
            soft_max=5.0, max=20.0,
            default=0
            )
    collapse_edges = BoolProperty(
            name="Make Point",
            default=False,
            description="Collapse the vertices of edges"
            )
    spike_base_width = FloatProperty(
            name="Spike Base Width",
            default=0.4,
            min=-4.0,
            soft_max=1, max=20,
            description="Base width of a spike"
            )
    base_height_inset = FloatProperty(
            name="Base Height Inset",
            default=0.0,
            min=-5, max=5,
            description="To elevate or drop the Base height Inset"
            )
    top_spike = FloatProperty(
            name="Top Spike",
            default=1.0,
            min=-10.0, max=10.0,
            description="The Base Height of a spike"
            )
    top_extra_height = FloatProperty(
            name="Top Extra Height",
            default=0.0,
            min=-10.0, max=10.0,
            description="Add extra height"
            )
    step_with_real_spike = BoolProperty(
            name="Step with Real Spike",
            default=False,
            description="In stepped, use a real spike"
            )
    use_relative = BoolProperty(
            name="Use Relative",
            default=False,
            description="Change size using area, min or max"
            )
    face_types = EnumProperty(
            name="Face Types",
            description="Different types of Faces",
            default="no",
            items=[
                ('no', "Pick an Option", "Choose one of the available options"),
                ('open_inset', "Open Inset", "Inset without closing faces (holes)"),
                ('with_base', "With Base", "Base and ..."),
                ('clsd_vertical', "Closed Vertical", "Closed Vertical"),
                ('open_vertical', "Open Vertical", "Open Vertical"),
                ('spiked', "Spiked", "Spike"),
                ('stepped', "Stepped", "Stepped"),
                ('boxed', "Boxed", "Boxed"),
                ('bar', "Bar", "Bar"),
                ]
            )
    strange_boxed_effect = BoolProperty(
            name="Strange Effect",
            default=False,
            description="Do not show one extrusion"
            )
    use_boundary = BoolProperty(
            name="Use Boundary",
            default=True
            )
    use_even_offset = BoolProperty(
            name="Even Offset",
            default=True
            )
    use_relative_offset = BoolProperty(
            name="Relative Offset",
            default=True
            )
    use_edge_rail = BoolProperty(
            name="Edge Rail",
            default=False
            )
    use_outset = BoolProperty(
            name="Outset",
            default=False
            )
    use_select_inset = BoolProperty(
            name="Inset",
            default=False
            )
    use_interpolate = BoolProperty(
            name="Interpolate",
            default=True
            )

    @classmethod
    def poll(cls, context):
        result = False
        active_object = context.active_object
        if active_object:
            mesh_objects_name = [el.name for el in bpy.data.objects if el.type == "MESH"]
            if active_object.name in mesh_objects_name:
                result = True

        return result

    def draw(self, context):
        layout = self.layout
        col = layout.column()

        col.separator()
        col.label(text="Using Active Object", icon="INFO")
        col.separator()
        col.label("Face Types:")
        col.prop(self, "face_types", text="")
        col.separator()
        col.prop(self, "use_relative")

        if self.face_types == "open_inset":
            col.prop(self, "move_inside")
            col.prop(self, "base_height")

        elif self.face_types == "with_base":
            col.prop(self, "move_inside")
            col.prop(self, "base_height")
            col.prop(self, "second_height")
            col.prop(self, "width")

        elif self.face_types == "clsd_vertical":
            col.prop(self, "base_height")

        elif self.face_types == "open_vertical":
            col.prop(self, "base_height")

        elif self.face_types == "boxed":
            col.prop(self, "move_inside")
            col.prop(self, "base_height")
            col.prop(self, "top_spike")
            col.prop(self, "strange_boxed_effect")

        elif self.face_types == "spiked":
            col.prop(self, "spike_base_width")
            col.prop(self, "base_height_inset")
            col.prop(self, "top_spike")

        elif self.face_types == "bar":
            col.prop(self, "spike_base_width")
            col.prop(self, "top_spike")
            col.prop(self, "top_extra_height")

        elif self.face_types == "stepped":
            col.prop(self, "spike_base_width")
            col.prop(self, "base_height_inset")
            col.prop(self, "top_extra_height")
            col.prop(self, "second_height")
            col.prop(self, "step_with_real_spike")

    def execute(self, context):
        obj_name = self.name_source_object
        face_type = self.face_types

        is_selected = check_is_selected()

        if not is_selected:
            self.report({'WARNING'},
                        "Operation Cancelled. No selected Faces found on the Active Object")
            return {'CANCELLED'}

        if face_type == "spiked":
            Spiked(spike_base_width=self.spike_base_width,
                   base_height_inset=self.base_height_inset,
                   top_spike=self.top_spike, top_relative=self.use_relative)

        elif face_type == "boxed":
            startinfo = prepare(self, context, self.remove_start_faces)
            bm = startinfo['bm']
            top = self.top_spike
            obj = startinfo['obj']
            obj_matrix_local = obj.matrix_local

            distance = None
            base_heights = None
            t = self.move_inside
            areas = startinfo['areas']
            base_height = self.base_height

            if self.use_relative:
                distance = [min(t * area, 1.0) for i, area in enumerate(areas)]
                base_heights = [base_height * area for i, area in enumerate(areas)]
            else:
                distance = [t] * len(areas)
                base_heights = [base_height] * len(areas)

            rings = startinfo['rings']
            centers = startinfo['centers']
            normals = startinfo['normals']
            for i in range(len(rings)):
                make_one_inset(self, context, bm=bm, ringvectors=rings[i],
                               center=centers[i], normal=normals[i],
                               t=distance[i], base_height=base_heights[i])
                bpy.ops.mesh.select_mode(type="EDGE")
                bpy.ops.mesh.select_more()
                bpy.ops.mesh.select_more()
            bpy.ops.object.mode_set(mode='OBJECT')
            # PKHG>INFO base extrusion done and set to the mesh

            # PKHG>INFO if the extrusion is NOT  done ... it'll look strange soon!
            if not self.strange_boxed_effect:
                bpy.ops.object.mode_set(mode='EDIT')
                obj = context.active_object
                bm = bmesh.from_edit_mesh(obj.data)
                bmfaces = [face for face in bm.faces if face.select]
                res = extrude_faces(self, context, bm=bm, face_l=bmfaces)
                ring_edges = [face.edges[:] for face in res]

            bpy.ops.object.mode_set(mode='OBJECT')

            # PKHG>INFO now the extruded facec have to move in normal direction
            bpy.ops.object.mode_set(mode='EDIT')
            obj = bpy.context.scene.objects.active
            bm = bmesh.from_edit_mesh(obj.data)
            todo_faces = [face for face in bm.faces if face.select]
            for face in todo_faces:
                bmesh.ops.translate(bm, vec=face.normal * top, space=obj_matrix_local,
                                    verts=face.verts)
            bpy.ops.object.mode_set(mode='OBJECT')

        elif face_type == "stepped":
            Stepped(spike_base_width=self.spike_base_width,
                    base_height_inset=self.base_height_inset,
                    top_spike=self.second_height,
                    top_extra_height=self.top_extra_height,
                    use_relative_offset=self.use_relative, with_spike=self.step_with_real_spike)

        elif face_type == "open_inset":
            startinfo = prepare(self, context, self.remove_start_faces)
            bm = startinfo['bm']

            # PKHG>INFO adjust for relative, via areas
            t = self.move_inside
            areas = startinfo['areas']
            base_height = self.base_height
            base_heights = None
            distance = None
            if self.use_relative:
                distance = [min(t * area, 1.0) for i, area in enumerate(areas)]
                base_heights = [base_height * area for i, area in enumerate(areas)]
            else:
                distance = [t] * len(areas)
                base_heights = [base_height] * len(areas)

            rings = startinfo['rings']
            centers = startinfo['centers']
            normals = startinfo['normals']
            for i in range(len(rings)):
                make_one_inset(self, context, bm=bm, ringvectors=rings[i],
                               center=centers[i], normal=normals[i],
                               t=distance[i], base_height=base_heights[i])
            bpy.ops.object.mode_set(mode='OBJECT')

        elif face_type == "with_base":
            startinfo = prepare(self, context, self.remove_start_faces)
            bm = startinfo['bm']
            obj = startinfo['obj']
            object_matrix = obj.matrix_local

            # PKHG>INFO for relative (using areas)
            t = self.move_inside
            areas = startinfo['areas']
            base_height = self.base_height
            distance = None
            base_heights = None

            if self.use_relative:
                distance = [min(t * area, 1.0) for i, area in enumerate(areas)]
                base_heights = [base_height * area for i, area in enumerate(areas)]
            else:
                distance = [t] * len(areas)
                base_heights = [base_height] * len(areas)

            next_rings = []
            rings = startinfo['rings']
            centers = startinfo['centers']
            normals = startinfo['normals']
            for i in range(len(rings)):
                next_rings.append(make_one_inset(self, context, bm=bm, ringvectors=rings[i],
                                                 center=centers[i], normal=normals[i],
                                                 t=distance[i], base_height=base_heights[i]))

            prepare_ring = extrude_edges(self, context, bm=bm, edge_l_l=next_rings)

            second_height = self.second_height
            width = self.width
            vectors = [[ele.verts[:] for ele in edge] for edge in prepare_ring]
            n_ring_vecs = []

            for rings in vectors:
                v = []
                for edgv in rings:
                    v.extend(edgv)
                # PKHF>INFO no double verts allowed, coming from two adjacents edges!
                bm.verts.ensure_lookup_table()
                vv = list(set([ele.index for ele in v]))

                vvv = [bm.verts[i].co for i in vv]
                n_ring_vecs.append(vvv)

            for i, ring in enumerate(n_ring_vecs):
                make_one_inset(self, context, bm=bm, ringvectors=ring,
                               center=centers[i], normal=normals[i],
                               t=width, base_height=base_heights[i] + second_height)
            bpy.ops.object.mode_set(mode='OBJECT')

        else:
            if face_type == "clsd_vertical":
                obj_name = context.active_object.name
                ClosedVertical(name=obj_name, base_height=self.base_height,
                               use_relative_base_height=self.use_relative)

            elif face_type == "open_vertical":
                obj_name = context.active_object.name
                OpenVertical(name=obj_name, base_height=self.base_height,
                             use_relative_base_height=self.use_relative)

            elif face_type == "bar":
                startinfo = prepare(self, context, self.remove_start_faces)

                result = []
                bm = startinfo['bm']
                rings = startinfo['rings']
                centers = startinfo['centers']
                normals = startinfo['normals']
                spike_base_width = self.spike_base_width
                for i, ring in enumerate(rings):
                    result.append(make_one_inset(self, context, bm=bm,
                                                 ringvectors=ring, center=centers[i],
                                                 normal=normals[i], t=spike_base_width))

                next_ring_edges_list = extrude_edges(self, context, bm=bm,
                                                     edge_l_l=result)
                top_spike = self.top_spike
                fac = top_spike
                object_matrix = startinfo['obj'].matrix_local
                for i in range(len(next_ring_edges_list)):
                    translate_ONE_ring(
                            self, context, bm=bm,
                            object_matrix=object_matrix,
                            ring_edges=next_ring_edges_list[i],
                            normal=normals[i], distance=fac
                            )
                next_ring_edges_list_2 = extrude_edges(self, context, bm=bm,
                                                       edge_l_l=next_ring_edges_list)

                top_extra_height = self.top_extra_height
                for i in range(len(next_ring_edges_list_2)):
                    move_corner_vecs_outside(
                            self, context, bm=bm,
                            edge_list=next_ring_edges_list_2[i],
                            center=centers[i], normal=normals[i],
                            base_height_erlier=fac + top_extra_height,
                            distance=fac
                            )
                bpy.ops.mesh.select_mode(type="VERT")
                bpy.ops.mesh.select_more()

                bpy.ops.object.mode_set(mode='OBJECT')

        return {'FINISHED'}


def find_one_ring(sel_vertices):
    ring0 = sel_vertices.pop(0)
    to_delete = []

    for i, edge in enumerate(sel_vertices):
        len_nu = len(ring0)
        if len(ring0 - edge) < len_nu:
            to_delete.append(i)
            ring0 = ring0.union(edge)

    to_delete.reverse()

    for el in to_delete:
        sel_vertices.pop(el)

    return (ring0, sel_vertices)


class Stepped:
    def __init__(self, spike_base_width=0.5, base_height_inset=0.0, top_spike=0.2,
                 top_relative=False, top_extra_height=0, use_relative_offset=False,
                 with_spike=False):

        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.inset(
                use_boundary=True, use_even_offset=True, use_relative_offset=False,
                use_edge_rail=False, thickness=spike_base_width, depth=0, use_outset=True,
                use_select_inset=False, use_individual=True, use_interpolate=True
                )
        bpy.ops.mesh.inset(
                use_boundary=True, use_even_offset=True, use_relative_offset=use_relative_offset,
                use_edge_rail=False, thickness=top_extra_height, depth=base_height_inset,
                use_outset=True, use_select_inset=False, use_individual=True, use_interpolate=True
                )
        bpy.ops.mesh.inset(
                use_boundary=True, use_even_offset=True, use_relative_offset=use_relative_offset,
                use_edge_rail=False, thickness=spike_base_width, depth=0, use_outset=True,
                use_select_inset=False, use_individual=True, use_interpolate=True
                )
        bpy.ops.mesh.inset(
                use_boundary=True, use_even_offset=True, use_relative_offset=False,
                use_edge_rail=False, thickness=0, depth=top_spike, use_outset=True,
                use_select_inset=False, use_individual=True, use_interpolate=True
                )
        if with_spike:
            bpy.ops.mesh.merge(type='COLLAPSE')

        bpy.ops.object.mode_set(mode='OBJECT')


class Spiked:
    def __init__(self, spike_base_width=0.5, base_height_inset=0.0, top_spike=0.2, top_relative=False):

        obj = bpy.context.active_object
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.inset(
                use_boundary=True, use_even_offset=True, use_relative_offset=False,
                use_edge_rail=False, thickness=spike_base_width, depth=base_height_inset,
                use_outset=True, use_select_inset=False, use_individual=True, use_interpolate=True
                )
        bpy.ops.mesh.inset(
                use_boundary=True, use_even_offset=True, use_relative_offset=top_relative,
                use_edge_rail=False, thickness=0, depth=top_spike, use_outset=True,
                use_select_inset=False, use_individual=True, use_interpolate=True
                )

        bm = bmesh.from_edit_mesh(obj.data)
        bpy.ops.mesh.merge(type='COLLAPSE')
        bpy.ops.object.mode_set(mode='OBJECT')


class ClosedVertical:
    def __init__(self, name="Plane", base_height=1, use_relative_base_height=False):
        obj = bpy.data.objects[name]

        bm = bmesh.new()
        bm.from_mesh(obj.data)
        # PKHG>INFO deselect chosen faces
        sel = [f for f in bm.faces if f.select]
        for f in sel:
            f.select = False
        res = bmesh.ops.extrude_discrete_faces(bm, faces=sel)
        # PKHG>INFO select extruded faces
        for f in res['faces']:
            f.select = True

        factor = base_height
        for face in res['faces']:
            if use_relative_base_height:
                area = face.calc_area()
                factor = area * base_height
            else:
                factor = base_height
            for el in face.verts:
                tmp = el.co + face.normal * factor
                el.co = tmp

        me = bpy.data.meshes[name]
        bm.to_mesh(me)
        bm.free()


class OpenVertical:
    def __init__(self, name="Plane", base_height=1, use_relative_base_height=False):

        obj = bpy.data.objects[name]
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        # PKHG>INFO deselect chosen faces
        sel = [f for f in bm.faces if f.select]
        for f in sel:
            f.select = False
        res = bmesh.ops.extrude_discrete_faces(bm, faces=sel)
        # PKHG>INFO select extruded faces
        for f in res['faces']:
            f.select = True

        # PKHG>INFO adjust extrusion by a vector
        factor = base_height
        for face in res['faces']:
            if use_relative_base_height:
                area = face.calc_area()
                factor = area * base_height
            else:
                factor = base_height
            for el in face.verts:
                tmp = el.co + face.normal * factor
                el.co = tmp

        me = bpy.data.meshes[name]
        bm.to_mesh(me)
        bm.free()

        bpy.ops.object.editmode_toggle()
        bpy.ops.mesh.delete(type='FACE')
        bpy.ops.object.editmode_toggle()


class StripFaces:
    def __init__(self, use_boundary=True, use_even_offset=True, use_relative_offset=False,
                 use_edge_rail=True, thickness=0.0, depth=0.0, use_outset=False,
                 use_select_inset=False, use_individual=True, use_interpolate=True):

        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.inset(
                use_boundary=use_boundary, use_even_offset=True, use_relative_offset=False,
                use_edge_rail=True, thickness=thickness, depth=depth, use_outset=use_outset,
                use_select_inset=use_select_inset, use_individual=use_individual,
                use_interpolate=use_interpolate
                )

        bpy.ops.object.mode_set(mode='OBJECT')

        # PKHG>IMFO only 3 parameters inc execution context supported!!
        if False:
            bpy.ops.mesh.inset(
                    use_boundary, use_even_offset, use_relative_offset, use_edge_rail,
                    thickness, depth, use_outset, use_select_inset, use_individual,
                    use_interpolate
                    )
        elif type == 0:
            bpy.ops.mesh.inset(
                    use_boundary=True, use_even_offset=True, use_relative_offset=False,
                    use_edge_rail=True, thickness=thickness, depth=depth, use_outset=False,
                    use_select_inset=False, use_individual=True, use_interpolate=True
                    )
        elif type == 1:
            bpy.ops.mesh.inset(
                    use_boundary=True, use_even_offset=True, use_relative_offset=False,
                    use_edge_rail=True, thickness=thickness, depth=depth, use_outset=False,
                    use_select_inset=False, use_individual=True, use_interpolate=False
                    )
            bpy.ops.mesh.delete(type='FACE')

        elif type == 2:
            bpy.ops.mesh.inset(
                    use_boundary=True, use_even_offset=False, use_relative_offset=True,
                    use_edge_rail=True, thickness=thickness, depth=depth, use_outset=False,
                    use_select_inset=False, use_individual=True, use_interpolate=False
                    )

            bpy.ops.mesh.delete(type='FACE')

        elif type == 3:
            bpy.ops.mesh.inset(
                    use_boundary=True, use_even_offset=False, use_relative_offset=True,
                    use_edge_rail=True, thickness=depth, depth=thickness, use_outset=False,
                    use_select_inset=False, use_individual=True, use_interpolate=True
                    )
            bpy.ops.mesh.delete(type='FACE')
        elif type == 4:
            bpy.ops.mesh.inset(
                    use_boundary=True, use_even_offset=False, use_relative_offset=True,
                    use_edge_rail=True, thickness=thickness, depth=depth, use_outset=True,
                    use_select_inset=False, use_individual=True, use_interpolate=True
                    )
            bpy.ops.mesh.inset(
                    use_boundary=True, use_even_offset=False, use_relative_offset=True,
                    use_edge_rail=True, thickness=thickness, depth=depth, use_outset=True,
                    use_select_inset=False, use_individual=True, use_interpolate=True
                    )
        bpy.ops.mesh.delete(type='FACE')

        bpy.ops.object.mode_set(mode='OBJECT')


def check_is_selected():
    is_selected = False
    for face in bpy.context.active_object.data.polygons:
        if face.select:
            is_selected = True
            break
    return is_selected


def prepare(self, context, remove_start_faces=True):
    """
       Start for a face selected change of faces
       select an object of type mesh, with activated several (all) faces
    """
    obj = bpy.context.scene.objects.active
    bpy.ops.object.mode_set(mode='OBJECT')
    selectedpolygons = [el for el in obj.data.polygons if el.select]

    # PKHG>INFO copies of the vectors are needed, otherwise Blender crashes!
    centers = [face.center for face in selectedpolygons]
    centers_copy = [Vector((el[0], el[1], el[2])) for el in centers]
    normals = [face.normal for face in selectedpolygons]
    normals_copy = [Vector((el[0], el[1], el[2])) for el in normals]

    vertindicesofpolgons = [
            [vert for vert in face.vertices] for face in selectedpolygons
            ]
    vertVectorsOfSelectedFaces = [
            [obj.data.vertices[ind].co for ind in vertIndiceofface] for
            vertIndiceofface in vertindicesofpolgons
            ]
    vertVectorsOfSelectedFaces_copy = [
            [Vector((el[0], el[1], el[2])) for el in listofvecs] for
            listofvecs in vertVectorsOfSelectedFaces
            ]

    bpy.ops.object.mode_set(mode='EDIT')
    bm = bmesh.from_edit_mesh(obj.data)
    selected_bm_faces = [ele for ele in bm.faces if ele.select]

    selected_edges_per_face_ind = [
            [ele.index for ele in face.edges] for face in selected_bm_faces
            ]
    indices = [el.index for el in selectedpolygons]
    selected_faces_areas = [bm.faces[:][i] for i in indices]
    tmp_area = [el.calc_area() for el in selected_faces_areas]

    # PKHG>INFO, selected faces are removed, only their edges are used!
    if remove_start_faces:
        bpy.ops.mesh.delete(type='ONLY_FACE')
        bpy.ops.object.mode_set(mode='OBJECT')
        obj.data.update()
        bpy.ops.object.mode_set(mode='EDIT')
        bm = bmesh.from_edit_mesh(obj.data)
        bm.verts.ensure_lookup_table()
        bm.faces.ensure_lookup_table()

    start_ring_raw = [
            [bm.verts[ind].index for ind in vertIndiceofface] for
            vertIndiceofface in vertindicesofpolgons
            ]
    start_ring = []

    for el in start_ring_raw:
        start_ring.append(set(el))
    bm.edges.ensure_lookup_table()

    bm_selected_edges_l_l = [
            [bm.edges[i] for i in bm_ind_list] for
            bm_ind_list in selected_edges_per_face_ind
            ]
    result = {
            'obj': obj, 'centers': centers_copy, 'normals': normals_copy,
            'rings': vertVectorsOfSelectedFaces_copy, 'bm': bm,
            'areas': tmp_area, 'startBMRingVerts': start_ring,
            'base_edges': bm_selected_edges_l_l
            }

    return result


def make_one_inset(self, context, bm=None, ringvectors=None, center=None,
                   normal=None, t=None, base_height=0):
    # a face will get 'inserted' faces to create (normaly) a hole if t is > 0 and < 1)
    tmp = []

    for el in ringvectors:
        tmp.append((el * (1 - t) + center * t) + normal * base_height)

    tmp = [bm.verts.new(v) for v in tmp]  # the new corner bmvectors
    # PKHG>INFO so to say sentinells, to use ONE for ...
    tmp.append(tmp[0])
    vectorsFace_i = [bm.verts.new(v) for v in ringvectors]
    vectorsFace_i.append(vectorsFace_i[0])
    myres = []
    for ii in range(len(vectorsFace_i) - 1):
        # PKHG>INFO next line: sequence is important! for added edge
        bmvecs = [vectorsFace_i[ii], vectorsFace_i[ii + 1], tmp[ii + 1], tmp[ii]]
        res = bm.faces.new(bmvecs)
        myres.append(res.edges[2])
        myres[-1].select = True  # PKHG>INFO to be used later selected!
    return (myres)


def extrude_faces(self, context, bm=None, face_l=None):
    # to make a ring extrusion
    res = bmesh.ops.extrude_discrete_faces(bm, faces=face_l)['faces']

    for face in res:
        face.select = True
    return res


def extrude_edges(self, context, bm=None, edge_l_l=None):
    # to make a ring extrusion
    all_results = []
    for edge_l in edge_l_l:
        for edge in edge_l:
            edge.select = False
        res = bmesh.ops.extrude_edge_only(bm, edges=edge_l)
        tmp = [ele for ele in res['geom'] if isinstance(ele, bmesh.types.BMEdge)]
        for edge in tmp:
            edge.select = True
        all_results.append(tmp)
    return all_results


def translate_ONE_ring(self, context, bm=None, object_matrix=None, ring_edges=None,
                       normal=(0, 0, 1), distance=0.5):
    # translate a ring in given (normal?!) direction with given (global) amount
    tmp = []
    for edge in ring_edges:
        tmp.extend(edge.verts[:])
    # PKHG>INFO no double vertices allowed by bmesh!
    tmp = set(tmp)
    tmp = list(tmp)
    bmesh.ops.translate(bm, vec=normal * distance, space=object_matrix, verts=tmp)
    # PKHG>INFO relevant edges will stay selected
    return ring_edges


def move_corner_vecs_outside(self, context, bm=None, edge_list=None, center=None,
                             normal=None, base_height_erlier=0.5, distance=0.5):
    # move corners (outside meant mostly) dependent on the parameters
    tmp = []
    for edge in edge_list:
        tmp.extend([ele for ele in edge.verts if isinstance(ele, bmesh.types.BMVert)])
    # PKHG>INFO to remove vertices, they are all used twice in the ring!
    tmp = set(tmp)
    tmp = list(tmp)

    for i in range(len(tmp)):
        vec = tmp[i].co
        direction = vec + (vec - (normal * base_height_erlier + center)) * distance
        tmp[i].co = direction


def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
