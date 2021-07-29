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

# All Operator

import bpy
import bmesh
from bpy.types import Operator
from bpy.props import (
        IntProperty,
        FloatProperty,
        )

from . import (
        mesh_helpers,
        report,
        )


def clean_float(text):
    # strip trailing zeros: 0.000 -> 0.0
    index = text.rfind(".")
    if index != -1:
        index += 2
        head, tail = text[:index], text[index:]
        tail = tail.rstrip("0")
        text = head + tail
    return text

# ---------
# Mesh Info


class Print3DInfoVolume(Operator):
    """Report the volume of the active mesh"""
    bl_idname = "mesh.print3d_info_volume"
    bl_label = "Print3D Info Volume"

    def execute(self, context):
        scene = context.scene
        unit = scene.unit_settings
        scale = 1.0 if unit.system == 'NONE' else unit.scale_length
        obj = context.active_object

        bm = mesh_helpers.bmesh_copy_from_object(obj, apply_modifiers=True)
        volume = bm.calc_volume()
        bm.free()

        info = []
        if unit.system == 'METRIC':
            info.append(("Volume: %s cm³" % clean_float("%.4f" % ((volume * (scale ** 3.0)) / (0.01 ** 3.0))), None))
        elif unit.system == 'IMPERIAL':
            info.append(("Volume: %s \"³" % clean_float("%.4f" % ((volume * (scale ** 3.0)) / (0.0254 ** 3.0))), None))
        else:
            info.append(("Volume: %s³" % clean_float("%.8f" % volume), None))

        report.update(*info)
        return {'FINISHED'}


class Print3DInfoArea(Operator):
    """Report the surface area of the active mesh"""
    bl_idname = "mesh.print3d_info_area"
    bl_label = "Print3D Info Area"

    def execute(self, context):
        scene = context.scene
        unit = scene.unit_settings
        scale = 1.0 if unit.system == 'NONE' else unit.scale_length
        obj = context.active_object

        bm = mesh_helpers.bmesh_copy_from_object(obj, apply_modifiers=True)
        area = mesh_helpers.bmesh_calc_area(bm)
        bm.free()

        info = []
        if unit.system == 'METRIC':
            info.append(("Area: %s cm²" % clean_float("%.4f" % ((area * (scale ** 2.0)) / (0.01 ** 2.0))), None))
        elif unit.system == 'IMPERIAL':
            info.append(("Area: %s \"²" % clean_float("%.4f" % ((area * (scale ** 2.0)) / (0.0254 ** 2.0))), None))
        else:
            info.append(("Area: %s²" % clean_float("%.8f" % area), None))

        report.update(*info)
        return {'FINISHED'}


# ---------------
# Geometry Checks

def execute_check(self, context):
    obj = context.active_object

    info = []
    self.main_check(obj, info)
    report.update(*info)

    multiple_obj_warning(self, context)

    return {'FINISHED'}


def multiple_obj_warning(self, context):
    if len(context.selected_objects) > 1:
        self.report({"INFO"}, "Multiple selected objects. Only the active one will be evaluated")


class Print3DCheckSolid(Operator):
    """Check for geometry is solid (has valid inside/outside) and correct normals"""
    bl_idname = "mesh.print3d_check_solid"
    bl_label = "Print3D Check Solid"

    @staticmethod
    def main_check(obj, info):
        import array

        bm = mesh_helpers.bmesh_copy_from_object(obj, transform=False, triangulate=False)

        edges_non_manifold = array.array('i', (i for i, ele in enumerate(bm.edges)
                if not ele.is_manifold))
        edges_non_contig = array.array('i', (i for i, ele in enumerate(bm.edges)
                if ele.is_manifold and (not ele.is_contiguous)))

        info.append(("Non Manifold Edge: %d" % len(edges_non_manifold),
                    (bmesh.types.BMEdge, edges_non_manifold)))

        info.append(("Bad Contig. Edges: %d" % len(edges_non_contig),
                    (bmesh.types.BMEdge, edges_non_contig)))

        bm.free()

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckIntersections(Operator):
    """Check geometry for self intersections"""
    bl_idname = "mesh.print3d_check_intersect"
    bl_label = "Print3D Check Intersections"

    @staticmethod
    def main_check(obj, info):
        faces_intersect = mesh_helpers.bmesh_check_self_intersect_object(obj)
        info.append(("Intersect Face: %d" % len(faces_intersect),
                    (bmesh.types.BMFace, faces_intersect)))

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckDegenerate(Operator):
    """Check for degenerate geometry that may not print properly """ \
    """(zero area faces, zero length edges)"""
    bl_idname = "mesh.print3d_check_degenerate"
    bl_label = "Print3D Check Degenerate"

    @staticmethod
    def main_check(obj, info):
        import array
        scene = bpy.context.scene
        print_3d = scene.print_3d
        threshold = print_3d.threshold_zero

        bm = mesh_helpers.bmesh_copy_from_object(obj, transform=False, triangulate=False)

        faces_zero = array.array('i', (i for i, ele in enumerate(bm.faces) if ele.calc_area() <= threshold))
        edges_zero = array.array('i', (i for i, ele in enumerate(bm.edges) if ele.calc_length() <= threshold))

        info.append(("Zero Faces: %d" % len(faces_zero),
                    (bmesh.types.BMFace, faces_zero)))

        info.append(("Zero Edges: %d" % len(edges_zero),
                    (bmesh.types.BMEdge, edges_zero)))

        bm.free()

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckDistorted(Operator):
    """Check for non-flat faces """
    bl_idname = "mesh.print3d_check_distort"
    bl_label = "Print3D Check Distorted Faces"

    @staticmethod
    def main_check(obj, info):
        import array

        scene = bpy.context.scene
        print_3d = scene.print_3d
        angle_distort = print_3d.angle_distort

        def face_is_distorted(ele):
            no = ele.normal
            angle_fn = no.angle
            for loop in ele.loops:
                loopno = loop.calc_normal()
                if loopno.dot(no) < 0.0:
                    loopno.negate()
                if angle_fn(loopno, 1000.0) > angle_distort:
                    return True
            return False

        bm = mesh_helpers.bmesh_copy_from_object(obj, transform=True, triangulate=False)
        bm.normal_update()

        faces_distort = array.array('i', (i for i, ele in enumerate(bm.faces) if face_is_distorted(ele)))

        info.append(("Non-Flat Faces: %d" % len(faces_distort),
                    (bmesh.types.BMFace, faces_distort)))

        bm.free()

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckThick(Operator):
    """Check geometry is above the minimum thickness preference """ \
    """(relies on correct normals)"""
    bl_idname = "mesh.print3d_check_thick"
    bl_label = "Print3D Check Thickness"

    @staticmethod
    def main_check(obj, info):
        scene = bpy.context.scene
        print_3d = scene.print_3d

        faces_error = mesh_helpers.bmesh_check_thick_object(obj, print_3d.thickness_min)

        info.append(("Thin Faces: %d" % len(faces_error),
                    (bmesh.types.BMFace, faces_error)))

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckSharp(Operator):
    """Check edges are below the sharpness preference"""
    bl_idname = "mesh.print3d_check_sharp"
    bl_label = "Print3D Check Sharp"

    @staticmethod
    def main_check(obj, info):
        scene = bpy.context.scene
        print_3d = scene.print_3d
        angle_sharp = print_3d.angle_sharp

        bm = mesh_helpers.bmesh_copy_from_object(obj, transform=True, triangulate=False)
        bm.normal_update()

        edges_sharp = [ele.index for ele in bm.edges
                       if ele.is_manifold and ele.calc_face_angle_signed() > angle_sharp]

        info.append(("Sharp Edge: %d" % len(edges_sharp),
                    (bmesh.types.BMEdge, edges_sharp)))
        bm.free()

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckOverhang(Operator):
    """Check faces don't overhang past a certain angle"""
    bl_idname = "mesh.print3d_check_overhang"
    bl_label = "Print3D Check Overhang"

    @staticmethod
    def main_check(obj, info):
        import math
        from mathutils import Vector

        scene = bpy.context.scene
        print_3d = scene.print_3d
        angle_overhang = (math.pi / 2.0) - print_3d.angle_overhang

        if angle_overhang == math.pi:
            info.append(("Skipping Overhang", ()))
            return

        bm = mesh_helpers.bmesh_copy_from_object(obj, transform=True, triangulate=False)
        bm.normal_update()

        z_down = Vector((0, 0, -1.0))
        z_down_angle = z_down.angle

        # 4.0 ignores zero area faces
        faces_overhang = [ele.index for ele in bm.faces
                          if z_down_angle(ele.normal, 4.0) < angle_overhang]

        info.append(("Overhang Face: %d" % len(faces_overhang),
                    (bmesh.types.BMFace, faces_overhang)))
        bm.free()

    def execute(self, context):
        return execute_check(self, context)


class Print3DCheckAll(Operator):
    """Run all checks"""
    bl_idname = "mesh.print3d_check_all"
    bl_label = "Print3D Check All"

    check_cls = (
        Print3DCheckSolid,
        Print3DCheckIntersections,
        Print3DCheckDegenerate,
        Print3DCheckDistorted,
        Print3DCheckThick,
        Print3DCheckSharp,
        Print3DCheckOverhang,
        )

    def execute(self, context):
        obj = context.active_object

        info = []
        for cls in self.check_cls:
            cls.main_check(obj, info)

        report.update(*info)

        multiple_obj_warning(self, context)

        return {'FINISHED'}


class Print3DCleanIsolated(Operator):
    """Cleanup isolated vertices and edges"""
    bl_idname = "mesh.print3d_clean_isolated"
    bl_label = "Print3D Clean Isolated "
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        bm = mesh_helpers.bmesh_from_object(obj)

        info = []
        change = False

        def face_is_isolated(ele):
            for loop in ele.loops:
                loop_next = loop.link_loop_radial_next
                if loop is not loop_next:
                    return False
            return True

        def edge_is_isolated(ele):
            return ele.is_wire

        def vert_is_isolated(ele):
            return (not bool(ele.link_edges))

        # --- face
        elems_remove = [ele for ele in bm.faces if face_is_isolated(ele)]
        remove = bm.faces.remove
        for ele in elems_remove:
            remove(ele)
        change |= bool(elems_remove)
        info.append(("Faces Removed: %d" % len(elems_remove),
                    None))
        del elems_remove
        # --- edge
        elems_remove = [ele for ele in bm.edges if edge_is_isolated(ele)]
        remove = bm.edges.remove
        for ele in elems_remove:
            remove(ele)
        change |= bool(elems_remove)
        info.append(("Edge Removed: %d" % len(elems_remove),
                    None))
        del elems_remove
        # --- vert
        elems_remove = [ele for ele in bm.verts if vert_is_isolated(ele)]
        remove = bm.verts.remove
        for ele in elems_remove:
            remove(ele)
        change |= bool(elems_remove)
        info.append(("Verts Removed: %d" % len(elems_remove),
                    None))
        del elems_remove
        # ---

        report.update(*info)

        if change:
            mesh_helpers.bmesh_to_object(obj, bm)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class Print3DCleanDistorted(Operator):
    """Tessellate distorted faces"""
    bl_idname = "mesh.print3d_clean_distorted"
    bl_label = "Print3D Clean Distorted"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = bpy.context.scene
        print_3d = scene.print_3d
        angle_distort = print_3d.angle_distort

        def face_is_distorted(ele):
            no = ele.normal
            angle_fn = no.angle
            for loop in ele.loops:
                if angle_fn(loop.calc_normal(), 1000.0) > angle_distort:
                    return True
            return False

        obj = context.active_object
        bm = mesh_helpers.bmesh_from_object(obj)
        bm.normal_update()
        elems_triangulate = [ele for ele in bm.faces if face_is_distorted(ele)]

        # edit
        if elems_triangulate:
            bmesh.ops.triangulate(bm, faces=elems_triangulate)
            mesh_helpers.bmesh_to_object(obj, bm)
            return {'FINISHED'}
        else:
            return {'CANCELLED'}


class Print3DCleanNonManifold(Operator):
    """Cleanup problems, like holes, non-manifold vertices, and inverted normals"""
    bl_idname = "mesh.print3d_clean_non_manifold"
    bl_label = "Print3D Clean Non-Manifold and Inverted"
    bl_options = {'REGISTER', 'UNDO'}

    threshold = bpy.props.FloatProperty(
            name="threshold",
            description="Minimum distance between elements to merge",
            default=0.0001,
            )
    sides = bpy.props.IntProperty(
            name="sides",
            description="Number of sides in hole required to fill",
            default=4,
            )

    def execute(self, context):
        self.context = context
        mode_orig = context.mode

        self.setup_environment()
        bm_key_orig = self.elem_count(context)

        self.delete_loose()
        self.remove_doubles(self.threshold)
        self.dissolve_degenerate(self.threshold)

        # may take a while
        self.fix_non_manifold(context, self.sides)

        self.make_normals_consistently_outwards()

        bm_key = self.elem_count(context)

        if mode_orig != 'EDIT_MESH':
            bpy.ops.object.mode_set(mode='OBJECT')

        self.report(
                {'INFO'},
                "Modified Verts:%+d, Edges:%+d, Faces:%+d" %
                (bm_key[0] - bm_key_orig[0],
                 bm_key[1] - bm_key_orig[1],
                 bm_key[2] - bm_key_orig[2],
                 ))

        return {'FINISHED'}

    @staticmethod
    def elem_count(context):
        bm = bmesh.from_edit_mesh(context.edit_object.data)
        return len(bm.verts), len(bm.edges), len(bm.faces)

    @staticmethod
    def setup_environment():
        """set the mode as edit, select mode as vertices, and reveal hidden vertices"""
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_mode(type='VERT')
        bpy.ops.mesh.reveal()

    @staticmethod
    def remove_doubles(threshold):
        """remove duplicate vertices"""
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.remove_doubles(threshold=threshold)

    @staticmethod
    def delete_loose():
        """delete loose vertices/edges/faces"""
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.delete_loose()

    @staticmethod
    def dissolve_degenerate(threshold):
        """dissolve zero area faces and zero length edges"""
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.dissolve_degenerate(threshold=threshold)

    @staticmethod
    def make_normals_consistently_outwards():
        """have all normals face outwards"""
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.normals_make_consistent()

    @classmethod
    def fix_non_manifold(cls, context, sides):
        """naive iterate-until-no-more approach for fixing manifolds"""
        total_non_manifold = cls.count_non_manifold_verts(context)

        if not total_non_manifold:
            return

        bm_states = set()
        bm_key = cls.elem_count(context)
        bm_states.add(bm_key)

        while True:
            cls.fill_non_manifold(sides)

            cls.delete_newly_generated_non_manifold_verts()

            bm_key = cls.elem_count(context)
            if bm_key in bm_states:
                break
            else:
                bm_states.add(bm_key)

    @staticmethod
    def select_non_manifold_verts(
            use_wire=False,
            use_boundary=False,
            use_multi_face=False,
            use_non_contiguous=False,
            use_verts=False,
            ):
        """select non-manifold vertices"""
        bpy.ops.mesh.select_non_manifold(
                extend=False,
                use_wire=use_wire,
                use_boundary=use_boundary,
                use_multi_face=use_multi_face,
                use_non_contiguous=use_non_contiguous,
                use_verts=use_verts,
                )

    @classmethod
    def count_non_manifold_verts(cls, context):
        """return a set of coordinates of non-manifold vertices"""
        cls.select_non_manifold_verts(
                use_wire=True,
                use_boundary=True,
                use_verts=True,
                )

        bm = bmesh.from_edit_mesh(context.edit_object.data)
        return sum((1 for v in bm.verts if v.select))

    @classmethod
    def fill_non_manifold(cls, sides):
        """fill holes and then fill in any remnant non-manifolds"""
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.fill_holes(sides=sides)

        # fill selected edge faces, which could be additional holes
        cls.select_non_manifold_verts(use_boundary=True)
        bpy.ops.mesh.fill()

    @classmethod
    def delete_newly_generated_non_manifold_verts(cls):
        """delete any newly generated vertices from the filling repair"""
        cls.select_non_manifold_verts(use_wire=True, use_verts=True)
        bpy.ops.mesh.delete(type='VERT')


class Print3DCleanThin(Operator):
    """Ensure minimum thickness"""
    bl_idname = "mesh.print3d_clean_thin"
    bl_label = "Print3D Clean Thin"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # TODO

        return {'FINISHED'}


# -------------
# Select Report
# ... helper function for info UI

class Print3DSelectReport(Operator):
    """Select the data associated with this report"""
    bl_idname = "mesh.print3d_select_report"
    bl_label = "Print3D Select Report"
    bl_options = {'INTERNAL'}

    index = IntProperty()

    _type_to_mode = {
        bmesh.types.BMVert: 'VERT',
        bmesh.types.BMEdge: 'EDGE',
        bmesh.types.BMFace: 'FACE',
        }

    _type_to_attr = {
        bmesh.types.BMVert: "verts",
        bmesh.types.BMEdge: "edges",
        bmesh.types.BMFace: "faces",
        }

    def execute(self, context):
        obj = context.edit_object
        info = report.info()
        text, data = info[self.index]
        bm_type, bm_array = data

        bpy.ops.mesh.reveal()
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.mesh.select_mode(type=self._type_to_mode[bm_type])

        bm = bmesh.from_edit_mesh(obj.data)
        elems = getattr(bm, Print3DSelectReport._type_to_attr[bm_type])[:]

        try:
            for i in bm_array:
                elems[i].select_set(True)
        except:
            # possible arrays are out of sync
            self.report({'WARNING'}, "Report is out of date, re-run check")

        # cool, but in fact annoying
        # bpy.ops.view3d.view_selected(use_all_regions=False)

        return {'FINISHED'}


# -----------
# Scale to...

def _scale(scale, report=None, report_suffix=""):
    if scale != 1.0:
        bpy.ops.transform.resize(value=(scale,) * 3,
                                 mirror=False, proportional='DISABLED',
                                 snap=False,
                                 texture_space=False)
    if report is not None:
        report({'INFO'}, "Scaled by %s%s" % (clean_float("%.6f" % scale), report_suffix))


class Print3DScaleToVolume(Operator):
    """Scale edit-mesh or selected-objects to a set volume"""
    bl_idname = "mesh.print3d_scale_to_volume"
    bl_label = "Scale to Volume"
    bl_options = {'REGISTER', 'UNDO'}

    volume_init = FloatProperty(
            options={'HIDDEN'},
            )
    volume = FloatProperty(
            name="Volume",
            unit='VOLUME',
            min=0.0, max=100000.0,
            )

    def execute(self, context):
        import math
        scale = math.pow(self.volume, 1 / 3) / math.pow(self.volume_init, 1 / 3)
        self.report({'INFO'}, "Scaled by %s" % clean_float("%.6f" % scale))
        _scale(scale, self.report)
        return {'FINISHED'}

    def invoke(self, context, event):

        def calc_volume(obj):
            bm = mesh_helpers.bmesh_copy_from_object(obj, apply_modifiers=True)
            volume = bm.calc_volume(signed=True)
            bm.free()
            return volume

        if context.mode == 'EDIT_MESH':
            volume = calc_volume(context.edit_object)
        else:
            volume = sum(calc_volume(obj) for obj in context.selected_editable_objects
                         if obj.type == 'MESH')

        if volume == 0.0:
            self.report({'WARNING'}, "Object has zero volume")
            return {'CANCELLED'}

        self.volume_init = self.volume = abs(volume)

        wm = context.window_manager
        return wm.invoke_props_dialog(self)


class Print3DScaleToBounds(Operator):
    """Scale edit-mesh or selected-objects to fit within a maximum length"""
    bl_idname = "mesh.print3d_scale_to_bounds"
    bl_label = "Scale to Bounds"
    bl_options = {'REGISTER', 'UNDO'}

    length_init = FloatProperty(
            options={'HIDDEN'},
            )
    axis_init = IntProperty(
            options={'HIDDEN'},
            )
    length = FloatProperty(
            name="Length Limit",
            unit='LENGTH',
            min=0.0, max=100000.0,
            )

    def execute(self, context):
        scale = self.length / self.length_init
        _scale(scale,
               report=self.report,
               report_suffix=", Clamping %s-Axis" % "XYZ"[self.axis_init])
        return {'FINISHED'}

    def invoke(self, context, event):
        from mathutils import Vector

        def calc_length(vecs):
            return max(((max(v[i] for v in vecs) - min(v[i] for v in vecs)), i) for i in range(3))

        if context.mode == 'EDIT_MESH':
            length, axis = calc_length([Vector(v) * obj.matrix_world
                                        for obj in [context.edit_object]
                                        for v in obj.bound_box])
        else:
            length, axis = calc_length([Vector(v) * obj.matrix_world
                                        for obj in context.selected_editable_objects
                                        if obj.type == 'MESH' for v in obj.bound_box])

        if length == 0.0:
            self.report({'WARNING'}, "Object has zero bounds")
            return {'CANCELLED'}

        self.length_init = self.length = length
        self.axis_init = axis

        wm = context.window_manager
        return wm.invoke_props_dialog(self)


# ------
# Export

class Print3DExport(Operator):
    """Export active object using print3d settings"""
    bl_idname = "mesh.print3d_export"
    bl_label = "Print3D Export"

    def execute(self, context):
        from . import export

        info = []
        ret = export.write_mesh(context, info, self.report)
        report.update(*info)

        if ret:
            return {'FINISHED'}
        else:
            return {'CANCELLED'}
