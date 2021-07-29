# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; version 2
#  of the License.
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
    "name": "Bsurfaces GPL Edition",
    "author": "Eclectiel",
    "version": (1, 5, 1),
    "blender": (2, 76, 0),
    "location": "View3D > EditMode > ToolShelf",
    "description": "Modeling and retopology tool",
    "wiki_url": "https://wiki.blender.org/index.php/Dev:Ref/Release_Notes/2.64/Bsurfaces_1.5",
    "category": "Mesh",
}


import bpy
import bmesh

import operator
from mathutils import Vector
from mathutils.geometry import (
        intersect_line_line,
        intersect_point_line,
        )
from math import (
        degrees,
        pi,
        sqrt,
        )
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
        PointerProperty,
        )
from bpy.types import (
        Operator,
        Panel,
        PropertyGroup,
        AddonPreferences,
        )


class VIEW3D_PT_tools_SURFSK_mesh(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_category = 'Tools'
    bl_context = "mesh_edit"
    bl_label = "Bsurfaces"

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout
        scn = context.scene.bsurfaces

        col = layout.column(align=True)
        row = layout.row()
        row.separator()
        col.operator("gpencil.surfsk_add_surface", text="Add Surface")
        col.operator("gpencil.surfsk_edit_strokes", text="Edit Strokes")
        col.prop(scn, "SURFSK_cyclic_cross")
        col.prop(scn, "SURFSK_cyclic_follow")
        col.prop(scn, "SURFSK_loops_on_strokes")
        col.prop(scn, "SURFSK_automatic_join")
        col.prop(scn, "SURFSK_keep_strokes")


class VIEW3D_PT_tools_SURFSK_curve(Panel):
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'TOOLS'
    bl_context = "curve_edit"
    bl_category = 'Tools'
    bl_label = "Bsurfaces"

    @classmethod
    def poll(cls, context):
        return context.active_object

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        row = layout.row()
        row.separator()
        col.operator("curve.surfsk_first_points", text="Set First Points")
        col.operator("curve.switch_direction", text="Switch Direction")
        col.operator("curve.surfsk_reorder_splines", text="Reorder Splines")


# Returns the type of strokes used
def get_strokes_type(main_object):
    strokes_type = ""
    strokes_num = 0

    # Check if they are grease pencil
    try:
        # Get the active grease pencil layer
        strokes_num = len(main_object.grease_pencil.layers.active.active_frame.strokes)

        if strokes_num > 0:
            strokes_type = "GP_STROKES"
    except:
        pass

    # Check if they are curves, if there aren't grease pencil strokes
    if strokes_type == "":
        if len(bpy.context.selected_objects) == 2:
            for ob in bpy.context.selected_objects:
                if ob != bpy.context.scene.objects.active and ob.type == "CURVE":
                    strokes_type = "EXTERNAL_CURVE"
                    strokes_num = len(ob.data.splines)

                    # Check if there is any non-bezier spline
                    for i in range(len(ob.data.splines)):
                        if ob.data.splines[i].type != "BEZIER":
                            strokes_type = "CURVE_WITH_NON_BEZIER_SPLINES"
                            break

                elif ob != bpy.context.scene.objects.active and ob.type != "CURVE":
                    strokes_type = "EXTERNAL_NO_CURVE"
        elif len(bpy.context.selected_objects) > 2:
            strokes_type = "MORE_THAN_ONE_EXTERNAL"

    # Check if there is a single stroke without any selection in the object
    if strokes_num == 1 and main_object.data.total_vert_sel == 0:
        if strokes_type == "EXTERNAL_CURVE":
            strokes_type = "SINGLE_CURVE_STROKE_NO_SELECTION"
        elif strokes_type == "GP_STROKES":
            strokes_type = "SINGLE_GP_STROKE_NO_SELECTION"

    if strokes_num == 0 and main_object.data.total_vert_sel > 0:
        strokes_type = "SELECTION_ALONE"

    if strokes_type == "":
        strokes_type = "NO_STROKES"

    return strokes_type


# Surface generator operator
class GPENCIL_OT_SURFSK_add_surface(Operator):
    bl_idname = "gpencil.surfsk_add_surface"
    bl_label = "Bsurfaces add surface"
    bl_description = "Generates surfaces from grease pencil strokes, bezier curves or loose edges"
    bl_options = {'REGISTER', 'UNDO'}

    edges_U = IntProperty(
                    name="Cross",
                    description="Number of face-loops crossing the strokes",
                    default=1,
                    min=1,
                    max=200
                    )
    edges_V = IntProperty(
                    name="Follow",
                    description="Number of face-loops following the strokes",
                    default=1,
                    min=1,
                    max=200
                    )
    cyclic_cross = BoolProperty(
                    name="Cyclic Cross",
                    description="Make cyclic the face-loops crossing the strokes",
                    default=False
                    )
    cyclic_follow = BoolProperty(
                    name="Cyclic Follow",
                    description="Make cyclic the face-loops following the strokes",
                    default=False
                    )
    loops_on_strokes = BoolProperty(
                    name="Loops on strokes",
                    description="Make the loops match the paths of the strokes",
                    default=False
                    )
    automatic_join = BoolProperty(
                    name="Automatic join",
                    description="Join automatically vertices of either surfaces generated "
                                "by crosshatching, or from the borders of closed shapes",
                    default=False
                    )
    join_stretch_factor = FloatProperty(
                    name="Stretch",
                    description="Amount of stretching or shrinking allowed for "
                                "edges when joining vertices automatically",
                    default=1,
                    min=0,
                    max=3,
                    subtype='FACTOR'
                    )

    def draw(self, context):
        layout = self.layout
        col = layout.column(align=True)
        row = layout.row()

        if not self.is_fill_faces:
            row.separator()
            if not self.is_crosshatch:
                if not self.selection_U_exists:
                    col.prop(self, "edges_U")
                    row.separator()

                if not self.selection_V_exists:
                    col.prop(self, "edges_V")
                    row.separator()

                row.separator()

                if not self.selection_U_exists:
                    if not (
                          (self.selection_V_exists and not self.selection_V_is_closed) or
                          (self.selection_V2_exists and not self.selection_V2_is_closed)
                          ):
                        col.prop(self, "cyclic_cross")

                if not self.selection_V_exists:
                    if not (
                          (self.selection_U_exists and not self.selection_U_is_closed) or
                          (self.selection_U2_exists and not self.selection_U2_is_closed)
                          ):
                        col.prop(self, "cyclic_follow")

                col.prop(self, "loops_on_strokes")

            col.prop(self, "automatic_join")

            if self.automatic_join:
                row.separator()
                col.separator()
                row.separator()
                col.prop(self, "join_stretch_factor")

    # Get an ordered list of a chain of vertices
    def get_ordered_verts(self, ob, all_selected_edges_idx, all_selected_verts_idx,
                          first_vert_idx, middle_vertex_idx, closing_vert_idx):
        # Order selected vertices.
        verts_ordered = []
        if closing_vert_idx is not None:
            verts_ordered.append(ob.data.vertices[closing_vert_idx])

        verts_ordered.append(ob.data.vertices[first_vert_idx])
        prev_v = first_vert_idx
        prev_ed = None
        finish_while = False
        while True:
            edges_non_matched = 0
            for i in all_selected_edges_idx:
                if ob.data.edges[i] != prev_ed and ob.data.edges[i].vertices[0] == prev_v and \
                   ob.data.edges[i].vertices[1] in all_selected_verts_idx:

                    verts_ordered.append(ob.data.vertices[ob.data.edges[i].vertices[1]])
                    prev_v = ob.data.edges[i].vertices[1]
                    prev_ed = ob.data.edges[i]
                elif ob.data.edges[i] != prev_ed and ob.data.edges[i].vertices[1] == prev_v and \
                     ob.data.edges[i].vertices[0] in all_selected_verts_idx:

                    verts_ordered.append(ob.data.vertices[ob.data.edges[i].vertices[0]])
                    prev_v = ob.data.edges[i].vertices[0]
                    prev_ed = ob.data.edges[i]
                else:
                    edges_non_matched += 1

                    if edges_non_matched == len(all_selected_edges_idx):
                        finish_while = True

            if finish_while:
                break

        if closing_vert_idx is not None:
            verts_ordered.append(ob.data.vertices[closing_vert_idx])

        if middle_vertex_idx is not None:
            verts_ordered.append(ob.data.vertices[middle_vertex_idx])
            verts_ordered.reverse()

        return tuple(verts_ordered)

    # Calculates length of a chain of points.
    def get_chain_length(self, object, verts_ordered):
        matrix = object.matrix_world

        edges_lengths = []
        edges_lengths_sum = 0
        for i in range(0, len(verts_ordered)):
            if i == 0:
                prev_v_co = matrix * verts_ordered[i].co
            else:
                v_co = matrix * verts_ordered[i].co

                v_difs = [prev_v_co[0] - v_co[0], prev_v_co[1] - v_co[1], prev_v_co[2] - v_co[2]]
                edge_length = abs(sqrt(v_difs[0] * v_difs[0] + v_difs[1] * v_difs[1] + v_difs[2] * v_difs[2]))

                edges_lengths.append(edge_length)
                edges_lengths_sum += edge_length

                prev_v_co = v_co

        return edges_lengths, edges_lengths_sum

    # Calculates the proportion of the edges of a chain of edges, relative to the full chain length.
    def get_edges_proportions(self, edges_lengths, edges_lengths_sum, use_boundaries, fixed_edges_num):
        edges_proportions = []
        if use_boundaries:
            verts_count = 1
            for l in edges_lengths:
                edges_proportions.append(l / edges_lengths_sum)
                verts_count += 1
        else:
            verts_count = 1
            for n in range(0, fixed_edges_num):
                edges_proportions.append(1 / fixed_edges_num)
                verts_count += 1

        return edges_proportions

    # Calculates the angle between two pairs of points in space
    def orientation_difference(self, points_A_co, points_B_co):
        # each parameter should be a list with two elements,
        # and each element should be a x,y,z coordinate
        vec_A = points_A_co[0] - points_A_co[1]
        vec_B = points_B_co[0] - points_B_co[1]

        angle = vec_A.angle(vec_B)

        if angle > 0.5 * pi:
            angle = abs(angle - pi)

        return angle

    # Calculate the which vert of verts_idx list is the nearest one
    # to the point_co coordinates, and the distance
    def shortest_distance(self, object, point_co, verts_idx):
        matrix = object.matrix_world

        for i in range(0, len(verts_idx)):
            dist = (point_co - matrix * object.data.vertices[verts_idx[i]].co).length
            if i == 0:
                prev_dist = dist
                nearest_vert_idx = verts_idx[i]
                shortest_dist = dist

            if dist < prev_dist:
                prev_dist = dist
                nearest_vert_idx = verts_idx[i]
                shortest_dist = dist

        return nearest_vert_idx, shortest_dist

    # Returns the index of the opposite vert tip in a chain, given a vert tip index
    # as parameter, and a multidimentional list with all pairs of tips
    def opposite_tip(self, vert_tip_idx, all_chains_tips_idx):
        opposite_vert_tip_idx = None
        for i in range(0, len(all_chains_tips_idx)):
            if vert_tip_idx == all_chains_tips_idx[i][0]:
                opposite_vert_tip_idx = all_chains_tips_idx[i][1]
            if vert_tip_idx == all_chains_tips_idx[i][1]:
                opposite_vert_tip_idx = all_chains_tips_idx[i][0]

        return opposite_vert_tip_idx

    # Simplifies a spline and returns the new points coordinates
    def simplify_spline(self, spline_coords, segments_num):
        simplified_spline = []
        points_between_segments = round(len(spline_coords) / segments_num)

        simplified_spline.append(spline_coords[0])
        for i in range(1, segments_num):
            simplified_spline.append(spline_coords[i * points_between_segments])

        simplified_spline.append(spline_coords[len(spline_coords) - 1])

        return simplified_spline

    # Cleans up the scene and gets it the same it was at the beginning,
    # in case the script is interrupted in the middle of the execution
    def cleanup_on_interruption(self):
        # If the original strokes curve comes from conversion
        # from grease pencil and wasn't made by hand, delete it
        if not self.using_external_curves:
            try:
                bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.data.objects[self.original_curve.name].select = True
                bpy.context.scene.objects.active = bpy.data.objects[self.original_curve.name]

                bpy.ops.object.delete()
            except:
                pass

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_object.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]
        else:
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.original_curve.name].select = True
            bpy.data.objects[self.main_object.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

    # Returns a list with the coords of the points distributed over the splines
    # passed to this method according to the proportions parameter
    def distribute_pts(self, surface_splines, proportions):

        # Calculate the length of each final surface spline
        surface_splines_lengths = []
        surface_splines_parsed = []

        for sp_idx in range(0, len(surface_splines)):
            # Calculate spline length
            surface_splines_lengths.append(0)

            for i in range(0, len(surface_splines[sp_idx].bezier_points)):
                if i == 0:
                    prev_p = surface_splines[sp_idx].bezier_points[i]
                else:
                    p = surface_splines[sp_idx].bezier_points[i]
                    edge_length = (prev_p.co - p.co).length
                    surface_splines_lengths[sp_idx] += edge_length

                    prev_p = p

        # Calculate vertex positions with appropriate edge proportions, and ordered, for each spline
        for sp_idx in range(0, len(surface_splines)):
            surface_splines_parsed.append([])
            surface_splines_parsed[sp_idx].append(surface_splines[sp_idx].bezier_points[0].co)

            prev_p_co = surface_splines[sp_idx].bezier_points[0].co
            p_idx = 0

            for prop_idx in range(len(proportions) - 1):
                target_length = surface_splines_lengths[sp_idx] * proportions[prop_idx]
                partial_segment_length = 0
                finish_while = False

                while True:
                    # if not it'll pass the p_idx as an index bellow and crash
                    if p_idx < len(surface_splines[sp_idx].bezier_points):
                        p_co = surface_splines[sp_idx].bezier_points[p_idx].co
                        new_dist = (prev_p_co - p_co).length

                    # The new distance that could have the partial segment if
                    # it is still shorter than the target length
                    potential_segment_length = partial_segment_length + new_dist

                    # If the potential is still shorter, keep adding
                    if potential_segment_length < target_length:
                        partial_segment_length = potential_segment_length

                        p_idx += 1
                        prev_p_co = p_co

                    # If the potential is longer than the target, calculate the target
                    # (a point between the last two points), and assign
                    elif potential_segment_length > target_length:
                        remaining_dist = target_length - partial_segment_length
                        vec = p_co - prev_p_co
                        vec.normalize()
                        intermediate_co = prev_p_co + (vec * remaining_dist)

                        surface_splines_parsed[sp_idx].append(intermediate_co)

                        partial_segment_length += remaining_dist
                        prev_p_co = intermediate_co

                        finish_while = True

                    # If the potential is equal to the target, assign
                    elif potential_segment_length == target_length:
                        surface_splines_parsed[sp_idx].append(p_co)
                        prev_p_co = p_co

                        finish_while = True

                    if finish_while:
                        break

            # last point of the spline
            surface_splines_parsed[sp_idx].append(
                    surface_splines[sp_idx].bezier_points[len(surface_splines[sp_idx].bezier_points) - 1].co
                    )

        return surface_splines_parsed

    # Counts the number of faces that belong to each edge
    def edge_face_count(self, ob):
        ed_keys_count_dict = {}

        for face in ob.data.polygons:
            for ed_keys in face.edge_keys:
                if ed_keys not in ed_keys_count_dict:
                    ed_keys_count_dict[ed_keys] = 1
                else:
                    ed_keys_count_dict[ed_keys] += 1

        edge_face_count = []
        for i in range(len(ob.data.edges)):
            edge_face_count.append(0)

        for i in range(len(ob.data.edges)):
            ed = ob.data.edges[i]

            v1 = ed.vertices[0]
            v2 = ed.vertices[1]

            if (v1, v2) in ed_keys_count_dict:
                edge_face_count[i] = ed_keys_count_dict[(v1, v2)]
            elif (v2, v1) in ed_keys_count_dict:
                edge_face_count[i] = ed_keys_count_dict[(v2, v1)]

        return edge_face_count

    # Fills with faces all the selected vertices which form empty triangles or quads
    def fill_with_faces(self, object):
        all_selected_verts_count = self.main_object_selected_verts_count

        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='OBJECT')

        # Calculate average length of selected edges
        all_selected_verts = []
        original_sel_edges_count = 0
        for ed in object.data.edges:
            if object.data.vertices[ed.vertices[0]].select and object.data.vertices[ed.vertices[1]].select:
                coords = []
                coords.append(object.data.vertices[ed.vertices[0]].co)
                coords.append(object.data.vertices[ed.vertices[1]].co)

                original_sel_edges_count += 1

                if not ed.vertices[0] in all_selected_verts:
                    all_selected_verts.append(ed.vertices[0])

                if not ed.vertices[1] in all_selected_verts:
                    all_selected_verts.append(ed.vertices[1])

        tuple(all_selected_verts)

        # Check if there is any edge selected. If not, interrupt the script
        if original_sel_edges_count == 0 and all_selected_verts_count > 0:
            return 0

        # Get all edges connected to selected verts
        all_edges_around_sel_verts = []
        edges_connected_to_sel_verts = {}
        verts_connected_to_every_vert = {}
        for ed_idx in range(len(object.data.edges)):
            ed = object.data.edges[ed_idx]
            include_edge = False

            if ed.vertices[0] in all_selected_verts:
                if not ed.vertices[0] in edges_connected_to_sel_verts:
                    edges_connected_to_sel_verts[ed.vertices[0]] = []

                edges_connected_to_sel_verts[ed.vertices[0]].append(ed_idx)
                include_edge = True

            if ed.vertices[1] in all_selected_verts:
                if not ed.vertices[1] in edges_connected_to_sel_verts:
                    edges_connected_to_sel_verts[ed.vertices[1]] = []

                edges_connected_to_sel_verts[ed.vertices[1]].append(ed_idx)
                include_edge = True

            if include_edge is True:
                all_edges_around_sel_verts.append(ed_idx)

            # Get all connected verts to each vert
            if not ed.vertices[0] in verts_connected_to_every_vert:
                verts_connected_to_every_vert[ed.vertices[0]] = []

            if not ed.vertices[1] in verts_connected_to_every_vert:
                verts_connected_to_every_vert[ed.vertices[1]] = []

            verts_connected_to_every_vert[ed.vertices[0]].append(ed.vertices[1])
            verts_connected_to_every_vert[ed.vertices[1]].append(ed.vertices[0])

        # Get all verts connected to faces
        all_verts_part_of_faces = []
        all_edges_faces_count = []
        all_edges_faces_count += self.edge_face_count(object)

        # Get only the selected edges that have faces attached.
        count_faces_of_edges_around_sel_verts = {}
        selected_verts_with_faces = []
        for ed_idx in all_edges_around_sel_verts:
            count_faces_of_edges_around_sel_verts[ed_idx] = all_edges_faces_count[ed_idx]

            if all_edges_faces_count[ed_idx] > 0:
                ed = object.data.edges[ed_idx]

                if not ed.vertices[0] in selected_verts_with_faces:
                    selected_verts_with_faces.append(ed.vertices[0])

                if not ed.vertices[1] in selected_verts_with_faces:
                    selected_verts_with_faces.append(ed.vertices[1])

                all_verts_part_of_faces.append(ed.vertices[0])
                all_verts_part_of_faces.append(ed.vertices[1])

        tuple(selected_verts_with_faces)

        # Discard unneeded verts from calculations
        participating_verts = []
        movable_verts = []
        for v_idx in all_selected_verts:
            vert_has_edges_with_one_face = False

            # Check if the actual vert has at least one edge connected to only one face
            for ed_idx in edges_connected_to_sel_verts[v_idx]:
                if count_faces_of_edges_around_sel_verts[ed_idx] == 1:
                    vert_has_edges_with_one_face = True

            # If the vert has two or less edges connected and the vert is not part of any face.
            # Or the vert is part of any face and at least one of
            # the connected edges has only one face attached to it.
            if (len(edges_connected_to_sel_verts[v_idx]) == 2 and
               v_idx not in all_verts_part_of_faces) or \
               len(edges_connected_to_sel_verts[v_idx]) == 1 or \
               (v_idx in all_verts_part_of_faces and
               vert_has_edges_with_one_face):

                participating_verts.append(v_idx)

                if v_idx not in all_verts_part_of_faces:
                    movable_verts.append(v_idx)

        # Remove from movable verts list those that are part of closed geometry (ie: triangles, quads)
        for mv_idx in movable_verts:
            freeze_vert = False
            mv_connected_verts = verts_connected_to_every_vert[mv_idx]

            for actual_v_idx in all_selected_verts:
                count_shared_neighbors = 0
                checked_verts = []

                for mv_conn_v_idx in mv_connected_verts:
                    if mv_idx != actual_v_idx:
                        if mv_conn_v_idx in verts_connected_to_every_vert[actual_v_idx] and \
                           mv_conn_v_idx not in checked_verts:
                            count_shared_neighbors += 1
                            checked_verts.append(mv_conn_v_idx)

                            if actual_v_idx in mv_connected_verts:
                                freeze_vert = True
                                break

                        if count_shared_neighbors == 2:
                            freeze_vert = True
                            break

                if freeze_vert:
                    break

            if freeze_vert:
                movable_verts.remove(mv_idx)

        # Calculate merge distance for participating verts
        shortest_edge_length = None
        for ed in object.data.edges:
            if ed.vertices[0] in movable_verts and ed.vertices[1] in movable_verts:
                v1 = object.data.vertices[ed.vertices[0]]
                v2 = object.data.vertices[ed.vertices[1]]

                length = (v1.co - v2.co).length

                if shortest_edge_length is None:
                    shortest_edge_length = length
                else:
                    if length < shortest_edge_length:
                        shortest_edge_length = length

        if shortest_edge_length is not None:
            edges_merge_distance = shortest_edge_length * 0.5
        else:
            edges_merge_distance = 0

        # Get together the verts near enough. They will be merged later
        remaining_verts = []
        remaining_verts += participating_verts
        for v1_idx in participating_verts:
            if v1_idx in remaining_verts and v1_idx in movable_verts:
                verts_to_merge = []
                coords_verts_to_merge = {}

                verts_to_merge.append(v1_idx)

                v1_co = object.data.vertices[v1_idx].co
                coords_verts_to_merge[v1_idx] = (v1_co[0], v1_co[1], v1_co[2])

                for v2_idx in remaining_verts:
                    if v1_idx != v2_idx:
                        v2_co = object.data.vertices[v2_idx].co

                        dist = (v1_co - v2_co).length

                        if dist <= edges_merge_distance:  # Add the verts which are near enough
                            verts_to_merge.append(v2_idx)

                            coords_verts_to_merge[v2_idx] = (v2_co[0], v2_co[1], v2_co[2])

                for vm_idx in verts_to_merge:
                    remaining_verts.remove(vm_idx)

                if len(verts_to_merge) > 1:
                    # Calculate middle point of the verts to merge.
                    sum_x_co = 0
                    sum_y_co = 0
                    sum_z_co = 0
                    movable_verts_to_merge_count = 0
                    for i in range(len(verts_to_merge)):
                        if verts_to_merge[i] in movable_verts:
                            v_co = object.data.vertices[verts_to_merge[i]].co

                            sum_x_co += v_co[0]
                            sum_y_co += v_co[1]
                            sum_z_co += v_co[2]

                            movable_verts_to_merge_count += 1

                    middle_point_co = [
                                sum_x_co / movable_verts_to_merge_count,
                                sum_y_co / movable_verts_to_merge_count,
                                sum_z_co / movable_verts_to_merge_count
                                ]

                    # Check if any vert to be merged is not movable
                    shortest_dist = None
                    are_verts_not_movable = False
                    verts_not_movable = []
                    for v_merge_idx in verts_to_merge:
                        if v_merge_idx in participating_verts and v_merge_idx not in movable_verts:
                            are_verts_not_movable = True
                            verts_not_movable.append(v_merge_idx)

                    if are_verts_not_movable:
                        # Get the vert connected to faces, that is nearest to
                        # the middle point of the movable verts
                        shortest_dist = None
                        for vcf_idx in verts_not_movable:
                                dist = abs((object.data.vertices[vcf_idx].co -
                                            Vector(middle_point_co)).length)

                                if shortest_dist is None:
                                    shortest_dist = dist
                                    nearest_vert_idx = vcf_idx
                                else:
                                    if dist < shortest_dist:
                                        shortest_dist = dist
                                        nearest_vert_idx = vcf_idx

                        coords = object.data.vertices[nearest_vert_idx].co
                        target_point_co = [coords[0], coords[1], coords[2]]
                    else:
                        target_point_co = middle_point_co

                    # Move verts to merge to the middle position
                    for v_merge_idx in verts_to_merge:
                        if v_merge_idx in movable_verts:  # Only move the verts that are not part of faces
                            object.data.vertices[v_merge_idx].co[0] = target_point_co[0]
                            object.data.vertices[v_merge_idx].co[1] = target_point_co[1]
                            object.data.vertices[v_merge_idx].co[2] = target_point_co[2]

        # Perform "Remove Doubles" to weld all the disconnected verts
        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='EDIT')
        bpy.ops.mesh.remove_doubles(threshold=0.0001)

        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='OBJECT')

        # Get all the definitive selected edges, after weldding
        selected_edges = []
        edges_per_vert = {}  # Number of faces of each selected edge
        for ed in object.data.edges:
            if object.data.vertices[ed.vertices[0]].select and object.data.vertices[ed.vertices[1]].select:
                selected_edges.append(ed.index)

                # Save all the edges that belong to each vertex.
                if not ed.vertices[0] in edges_per_vert:
                    edges_per_vert[ed.vertices[0]] = []

                if not ed.vertices[1] in edges_per_vert:
                    edges_per_vert[ed.vertices[1]] = []

                edges_per_vert[ed.vertices[0]].append(ed.index)
                edges_per_vert[ed.vertices[1]].append(ed.index)

        # Check if all the edges connected to each vert have two faces attached to them.
        # To discard them later and make calculations faster
        a = []
        a += self.edge_face_count(object)
        tuple(a)
        verts_surrounded_by_faces = {}
        for v_idx in edges_per_vert:
            edges = edges_per_vert[v_idx]
            edges_with_two_faces_count = 0

            for ed_idx in edges_per_vert[v_idx]:
                if a[ed_idx] == 2:
                    edges_with_two_faces_count += 1

            if edges_with_two_faces_count == len(edges_per_vert[v_idx]):
                verts_surrounded_by_faces[v_idx] = True
            else:
                verts_surrounded_by_faces[v_idx] = False

        # Get all the selected vertices
        selected_verts_idx = []
        for v in object.data.vertices:
            if v.select:
                selected_verts_idx.append(v.index)

        # Get all the faces of the object
        all_object_faces_verts_idx = []
        for face in object.data.polygons:
            face_verts = []
            face_verts.append(face.vertices[0])
            face_verts.append(face.vertices[1])
            face_verts.append(face.vertices[2])

            if len(face.vertices) == 4:
                face_verts.append(face.vertices[3])

            all_object_faces_verts_idx.append(face_verts)

        # Deselect all vertices
        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='EDIT')
        bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='OBJECT')

        # Make a dictionary with the verts related to each vert
        related_key_verts = {}
        for ed_idx in selected_edges:
            ed = object.data.edges[ed_idx]

            if not verts_surrounded_by_faces[ed.vertices[0]]:
                if not ed.vertices[0] in related_key_verts:
                    related_key_verts[ed.vertices[0]] = []

                if not ed.vertices[1] in related_key_verts[ed.vertices[0]]:
                    related_key_verts[ed.vertices[0]].append(ed.vertices[1])

            if not verts_surrounded_by_faces[ed.vertices[1]]:
                if not ed.vertices[1] in related_key_verts:
                    related_key_verts[ed.vertices[1]] = []

                if not ed.vertices[0] in related_key_verts[ed.vertices[1]]:
                    related_key_verts[ed.vertices[1]].append(ed.vertices[0])

        # Get groups of verts forming each face
        faces_verts_idx = []
        for v1 in related_key_verts:      # verts-1 ....
            for v2 in related_key_verts:  # verts-2
                if v1 != v2:
                    related_verts_in_common = []
                    v2_in_rel_v1 = False
                    v1_in_rel_v2 = False
                    for rel_v1 in related_key_verts[v1]:
                        # Check if related verts of verts-1 are related verts of verts-2
                        if rel_v1 in related_key_verts[v2]:
                            related_verts_in_common.append(rel_v1)

                    if v2 in related_key_verts[v1]:
                        v2_in_rel_v1 = True

                    if v1 in related_key_verts[v2]:
                        v1_in_rel_v2 = True

                    repeated_face = False
                    # If two verts have two related verts in common, they form a quad
                    if len(related_verts_in_common) == 2:
                        # Check if the face is already saved
                        all_faces_to_check_idx = faces_verts_idx + all_object_faces_verts_idx

                        for f_verts in all_faces_to_check_idx:
                            repeated_verts = 0

                            if len(f_verts) == 4:
                                if v1 in f_verts:
                                    repeated_verts += 1
                                if v2 in f_verts:
                                    repeated_verts += 1
                                if related_verts_in_common[0] in f_verts:
                                    repeated_verts += 1
                                if related_verts_in_common[1] in f_verts:
                                    repeated_verts += 1

                                if repeated_verts == len(f_verts):
                                    repeated_face = True
                                    break

                        if not repeated_face:
                            faces_verts_idx.append(
                                    [v1, related_verts_in_common[0], v2, related_verts_in_common[1]]
                                    )

                    # If Two verts have one related vert in common and
                    # they are related to each other, they form a triangle
                    elif v2_in_rel_v1 and v1_in_rel_v2 and len(related_verts_in_common) == 1:
                        # Check if the face is already saved.
                        all_faces_to_check_idx = faces_verts_idx + all_object_faces_verts_idx

                        for f_verts in all_faces_to_check_idx:
                            repeated_verts = 0

                            if len(f_verts) == 3:
                                if v1 in f_verts:
                                    repeated_verts += 1
                                if v2 in f_verts:
                                    repeated_verts += 1
                                if related_verts_in_common[0] in f_verts:
                                    repeated_verts += 1

                                if repeated_verts == len(f_verts):
                                    repeated_face = True
                                    break

                        if not repeated_face:
                            faces_verts_idx.append([v1, related_verts_in_common[0], v2])

        # Keep only the faces that don't overlap by ignoring quads
        # that overlap with two adjacent triangles
        faces_to_not_include_idx = []  # Indices of faces_verts_idx to eliminate
        all_faces_to_check_idx = faces_verts_idx + all_object_faces_verts_idx
        for i in range(len(faces_verts_idx)):
            for t in range(len(all_faces_to_check_idx)):
                if i != t:
                    verts_in_common = 0

                    if len(faces_verts_idx[i]) == 4 and len(all_faces_to_check_idx[t]) == 3:
                        for v_idx in all_faces_to_check_idx[t]:
                            if v_idx in faces_verts_idx[i]:
                                verts_in_common += 1
                        # If it doesn't have all it's vertices repeated in the other face
                        if verts_in_common == 3:
                            if i not in faces_to_not_include_idx:
                                faces_to_not_include_idx.append(i)

        # Build faces discarding the ones in faces_to_not_include
        me = object.data
        bm = bmesh.new()
        bm.from_mesh(me)

        num_faces_created = 0
        for i in range(len(faces_verts_idx)):
            if i not in faces_to_not_include_idx:
                bm.faces.new([bm.verts[v] for v in faces_verts_idx[i]])

                num_faces_created += 1

        bm.to_mesh(me)
        bm.free()

        for v_idx in selected_verts_idx:
            self.main_object.data.vertices[v_idx].select = True

        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='EDIT')
        bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.object.mode_set('INVOKE_REGION_WIN', mode='OBJECT')

        return num_faces_created

    # Crosshatch skinning
    def crosshatch_surface_invoke(self, ob_original_splines):
        self.is_crosshatch = False
        self.crosshatch_merge_distance = 0

        objects_to_delete = []  # duplicated strokes to be deleted.

        # If the main object uses modifiers deactivate them temporarily until the surface is joined
        # (without this the surface verts merging with the main object doesn't work well)
        self.modifiers_prev_viewport_state = []
        if len(self.main_object.modifiers) > 0:
            for m_idx in range(len(self.main_object.modifiers)):
                self.modifiers_prev_viewport_state.append(
                                    self.main_object.modifiers[m_idx].show_viewport
                                    )
                self.main_object.modifiers[m_idx].show_viewport = False

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[ob_original_splines.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[ob_original_splines.name]

        if len(ob_original_splines.data.splines) >= 2:
            bpy.ops.object.duplicate('INVOKE_REGION_WIN')
            ob_splines = bpy.context.object
            ob_splines.name = "SURFSKIO_NE_STR"

            # Get estimative merge distance (sum up the distances from the first point to
            # all other points, then average them and then divide them)
            first_point_dist_sum = 0
            first_dist = 0
            second_dist = 0
            coords_first_pt = ob_splines.data.splines[0].bezier_points[0].co
            for i in range(len(ob_splines.data.splines)):
                sp = ob_splines.data.splines[i]

                if coords_first_pt != sp.bezier_points[0].co:
                    first_dist = (coords_first_pt - sp.bezier_points[0].co).length

                if coords_first_pt != sp.bezier_points[len(sp.bezier_points) - 1].co:
                    second_dist = (coords_first_pt - sp.bezier_points[len(sp.bezier_points) - 1].co).length

                first_point_dist_sum += first_dist + second_dist

                if i == 0:
                    if first_dist != 0:
                        shortest_dist = first_dist
                    elif second_dist != 0:
                        shortest_dist = second_dist

                if shortest_dist > first_dist and first_dist != 0:
                    shortest_dist = first_dist

                if shortest_dist > second_dist and second_dist != 0:
                    shortest_dist = second_dist

            self.crosshatch_merge_distance = shortest_dist / 20

            # Recalculation of merge distance

            bpy.ops.object.duplicate('INVOKE_REGION_WIN')

            ob_calc_merge_dist = bpy.context.object
            ob_calc_merge_dist.name = "SURFSKIO_CALC_TMP"

            objects_to_delete.append(ob_calc_merge_dist)

            # Smooth out strokes a little to improve crosshatch detection
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='SELECT')

            for i in range(4):
                bpy.ops.curve.smooth('INVOKE_REGION_WIN')

            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Convert curves into mesh
            ob_calc_merge_dist.data.resolution_u = 12
            bpy.ops.object.convert(target='MESH', keep_original=False)

            # Find "intersection-nodes"
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='SELECT')
            bpy.ops.mesh.remove_doubles('INVOKE_REGION_WIN',
                                        threshold=self.crosshatch_merge_distance)
            bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Remove verts with less than three edges
            verts_edges_count = {}
            for ed in ob_calc_merge_dist.data.edges:
                v = ed.vertices

                if v[0] not in verts_edges_count:
                    verts_edges_count[v[0]] = 0

                if v[1] not in verts_edges_count:
                    verts_edges_count[v[1]] = 0

                verts_edges_count[v[0]] += 1
                verts_edges_count[v[1]] += 1

            nodes_verts_coords = []
            for v_idx in verts_edges_count:
                v = ob_calc_merge_dist.data.vertices[v_idx]

                if verts_edges_count[v_idx] < 3:
                    v.select = True

            # Remove them
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.mesh.delete('INVOKE_REGION_WIN', type='VERT')
            bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='SELECT')

            # Remove doubles to discard very near verts from calculations of distance
            bpy.ops.mesh.remove_doubles(
                        'INVOKE_REGION_WIN',
                        threshold=self.crosshatch_merge_distance * 4.0
                        )
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Get all coords of the resulting nodes
            nodes_verts_coords = [(v.co[0], v.co[1], v.co[2]) for
                                   v in ob_calc_merge_dist.data.vertices]

            # Check if the strokes are a crosshatch
            if len(nodes_verts_coords) >= 3:
                self.is_crosshatch = True

                shortest_dist = None
                for co_1 in nodes_verts_coords:
                    for co_2 in nodes_verts_coords:
                        if co_1 != co_2:
                            dist = (Vector(co_1) - Vector(co_2)).length

                            if shortest_dist is not None:
                                if dist < shortest_dist:
                                    shortest_dist = dist
                            else:
                                shortest_dist = dist

                self.crosshatch_merge_distance = shortest_dist / 3

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[ob_splines.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[ob_splines.name]

            # Deselect all points
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Smooth splines in a localized way, to eliminate "saw-teeth"
            # like shapes when there are many points
            for sp in ob_splines.data.splines:
                angle_sum = 0

                angle_limit = 2  # Degrees
                for t in range(len(sp.bezier_points)):
                    # Because on each iteration it checks the "next two points"
                    # of the actual. This way it doesn't go out of range
                    if t <= len(sp.bezier_points) - 3:
                        p1 = sp.bezier_points[t]
                        p2 = sp.bezier_points[t + 1]
                        p3 = sp.bezier_points[t + 2]

                        vec_1 = p1.co - p2.co
                        vec_2 = p2.co - p3.co

                        if p2.co != p1.co and p2.co != p3.co:
                            angle = vec_1.angle(vec_2)
                            angle_sum += degrees(angle)

                            if angle_sum >= angle_limit:  # If sum of angles is grater than the limit
                                if (p1.co - p2.co).length <= self.crosshatch_merge_distance:
                                    p1.select_control_point = True
                                    p1.select_left_handle = True
                                    p1.select_right_handle = True

                                    p2.select_control_point = True
                                    p2.select_left_handle = True
                                    p2.select_right_handle = True

                                if (p1.co - p2.co).length <= self.crosshatch_merge_distance:
                                    p3.select_control_point = True
                                    p3.select_left_handle = True
                                    p3.select_right_handle = True

                                angle_sum = 0

                sp.bezier_points[0].select_control_point = False
                sp.bezier_points[0].select_left_handle = False
                sp.bezier_points[0].select_right_handle = False

                sp.bezier_points[len(sp.bezier_points) - 1].select_control_point = False
                sp.bezier_points[len(sp.bezier_points) - 1].select_left_handle = False
                sp.bezier_points[len(sp.bezier_points) - 1].select_right_handle = False

            # Smooth out strokes a little to improve crosshatch detection
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            for i in range(15):
                bpy.ops.curve.smooth('INVOKE_REGION_WIN')

            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Simplify the splines
            for sp in ob_splines.data.splines:
                angle_sum = 0

                sp.bezier_points[0].select_control_point = True
                sp.bezier_points[0].select_left_handle = True
                sp.bezier_points[0].select_right_handle = True

                sp.bezier_points[len(sp.bezier_points) - 1].select_control_point = True
                sp.bezier_points[len(sp.bezier_points) - 1].select_left_handle = True
                sp.bezier_points[len(sp.bezier_points) - 1].select_right_handle = True

                angle_limit = 15  # Degrees
                for t in range(len(sp.bezier_points)):
                    # Because on each iteration it checks the "next two points"
                    # of the actual. This way it doesn't go out of range
                    if t <= len(sp.bezier_points) - 3:
                        p1 = sp.bezier_points[t]
                        p2 = sp.bezier_points[t + 1]
                        p3 = sp.bezier_points[t + 2]

                        vec_1 = p1.co - p2.co
                        vec_2 = p2.co - p3.co

                        if p2.co != p1.co and p2.co != p3.co:
                            angle = vec_1.angle(vec_2)
                            angle_sum += degrees(angle)
                            # If sum of angles is grater than the limit
                            if angle_sum >= angle_limit:
                                p1.select_control_point = True
                                p1.select_left_handle = True
                                p1.select_right_handle = True

                                p2.select_control_point = True
                                p2.select_left_handle = True
                                p2.select_right_handle = True

                                p3.select_control_point = True
                                p3.select_left_handle = True
                                p3.select_right_handle = True

                                angle_sum = 0

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.select_all(action='INVERT')

            bpy.ops.curve.delete(type='VERT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            objects_to_delete.append(ob_splines)

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Check if the strokes are a crosshatch
            if self.is_crosshatch:
                all_points_coords = []
                for i in range(len(ob_splines.data.splines)):
                    all_points_coords.append([])

                    all_points_coords[i] = [Vector((x, y, z)) for
                                            x, y, z in [bp.co for
                                            bp in ob_splines.data.splines[i].bezier_points]]

                all_intersections = []
                checked_splines = []
                for i in range(len(all_points_coords)):

                    for t in range(len(all_points_coords[i]) - 1):
                        bp1_co = all_points_coords[i][t]
                        bp2_co = all_points_coords[i][t + 1]

                        for i2 in range(len(all_points_coords)):
                            if i != i2 and i2 not in checked_splines:
                                for t2 in range(len(all_points_coords[i2]) - 1):
                                    bp3_co = all_points_coords[i2][t2]
                                    bp4_co = all_points_coords[i2][t2 + 1]

                                    intersec_coords = intersect_line_line(
                                                            bp1_co, bp2_co, bp3_co, bp4_co
                                                            )
                                    if intersec_coords is not None:
                                        dist = (intersec_coords[0] - intersec_coords[1]).length

                                        if dist <= self.crosshatch_merge_distance * 1.5:
                                            temp_co, percent1 = intersect_point_line(
                                                                    intersec_coords[0], bp1_co, bp2_co
                                                                    )
                                            if (percent1 >= -0.02 and percent1 <= 1.02):
                                                temp_co, percent2 = intersect_point_line(
                                                                        intersec_coords[1], bp3_co, bp4_co
                                                                        )
                                                if (percent2 >= -0.02 and percent2 <= 1.02):
                                                    # Format: spline index, first point index from
                                                    # corresponding segment, percentage from first point of
                                                    # actual segment, coords of intersection point
                                                    all_intersections.append(
                                                            (i, t, percent1,
                                                            ob_splines.matrix_world * intersec_coords[0])
                                                            )
                                                    all_intersections.append(
                                                            (i2, t2, percent2,
                                                            ob_splines.matrix_world * intersec_coords[1])
                                                            )

                        checked_splines.append(i)
                # Sort list by spline, then by corresponding first point index of segment,
                # and then by percentage from first point of segment: elements 0 and 1 respectively
                all_intersections.sort(key=operator.itemgetter(0, 1, 2))

                self.crosshatch_strokes_coords = {}
                for i in range(len(all_intersections)):
                    if not all_intersections[i][0] in self.crosshatch_strokes_coords:
                        self.crosshatch_strokes_coords[all_intersections[i][0]] = []

                    self.crosshatch_strokes_coords[all_intersections[i][0]].append(
                                                                        all_intersections[i][3]
                                                                        )  # Save intersection coords
            else:
                self.is_crosshatch = False

        # Delete all duplicates
        for o in objects_to_delete:
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[o.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[o.name]
            bpy.ops.object.delete()

        # If the main object has modifiers, turn their "viewport view status" to
        # what it was before the forced deactivation above
        if len(self.main_object.modifiers) > 0:
            for m_idx in range(len(self.main_object.modifiers)):
                self.main_object.modifiers[m_idx].show_viewport = self.modifiers_prev_viewport_state[m_idx]

        return

    # Part of the Crosshatch process that is repeated when the operator is tweaked
    def crosshatch_surface_execute(self):
        # If the main object uses modifiers deactivate them temporarily until the surface is joined
        # (without this the surface verts merging with the main object doesn't work well)
        self.modifiers_prev_viewport_state = []
        if len(self.main_object.modifiers) > 0:
            for m_idx in range(len(self.main_object.modifiers)):
                self.modifiers_prev_viewport_state.append(self.main_object.modifiers[m_idx].show_viewport)

                self.main_object.modifiers[m_idx].show_viewport = False

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        me_name = "SURFSKIO_STK_TMP"
        me = bpy.data.meshes.new(me_name)

        all_verts_coords = []
        all_edges = []
        for st_idx in self.crosshatch_strokes_coords:
            for co_idx in range(len(self.crosshatch_strokes_coords[st_idx])):
                coords = self.crosshatch_strokes_coords[st_idx][co_idx]

                all_verts_coords.append(coords)

                if co_idx > 0:
                    all_edges.append((len(all_verts_coords) - 2, len(all_verts_coords) - 1))

        me.from_pydata(all_verts_coords, all_edges, [])

        me.update()

        ob = bpy.data.objects.new(me_name, me)
        ob.data = me
        bpy.context.scene.objects.link(ob)

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[ob.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[ob.name]

        # Get together each vert and its nearest, to the middle position
        verts = ob.data.vertices
        checked_verts = []
        for i in range(len(verts)):
            shortest_dist = None

            if i not in checked_verts:
                for t in range(len(verts)):
                    if i != t and t not in checked_verts:
                        dist = (verts[i].co - verts[t].co).length

                        if shortest_dist is not None:
                            if dist < shortest_dist:
                                shortest_dist = dist
                                nearest_vert = t
                        else:
                            shortest_dist = dist
                            nearest_vert = t

                middle_location = (verts[i].co + verts[nearest_vert].co) / 2

                verts[i].co = middle_location
                verts[nearest_vert].co = middle_location

                checked_verts.append(i)
                checked_verts.append(nearest_vert)

        # Calculate average length between all the generated edges
        ob = bpy.context.object
        lengths_sum = 0
        for ed in ob.data.edges:
            v1 = ob.data.vertices[ed.vertices[0]]
            v2 = ob.data.vertices[ed.vertices[1]]

            lengths_sum += (v1.co - v2.co).length

        edges_count = len(ob.data.edges)
        # possible division by zero here
        average_edge_length = lengths_sum / edges_count if edges_count != 0 else 0.0001

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='SELECT')
        bpy.ops.mesh.remove_doubles('INVOKE_REGION_WIN',
                                    threshold=average_edge_length / 15.0)
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        final_points_ob = bpy.context.scene.objects.active

        # Make a dictionary with the verts related to each vert
        related_key_verts = {}
        for ed in final_points_ob.data.edges:
            if not ed.vertices[0] in related_key_verts:
                related_key_verts[ed.vertices[0]] = []

            if not ed.vertices[1] in related_key_verts:
                related_key_verts[ed.vertices[1]] = []

            if not ed.vertices[1] in related_key_verts[ed.vertices[0]]:
                related_key_verts[ed.vertices[0]].append(ed.vertices[1])

            if not ed.vertices[0] in related_key_verts[ed.vertices[1]]:
                related_key_verts[ed.vertices[1]].append(ed.vertices[0])

        # Get groups of verts forming each face
        faces_verts_idx = []
        for v1 in related_key_verts:      # verts-1 ....
            for v2 in related_key_verts:  # verts-2
                if v1 != v2:
                    related_verts_in_common = []
                    v2_in_rel_v1 = False
                    v1_in_rel_v2 = False
                    for rel_v1 in related_key_verts[v1]:
                        # Check if related verts of verts-1 are related verts of verts-2
                        if rel_v1 in related_key_verts[v2]:
                            related_verts_in_common.append(rel_v1)

                    if v2 in related_key_verts[v1]:
                        v2_in_rel_v1 = True

                    if v1 in related_key_verts[v2]:
                        v1_in_rel_v2 = True

                    repeated_face = False
                    # If two verts have two related verts in common, they form a quad
                    if len(related_verts_in_common) == 2:
                        # Check if the face is already saved
                        for f_verts in faces_verts_idx:
                            repeated_verts = 0

                            if len(f_verts) == 4:
                                if v1 in f_verts:
                                    repeated_verts += 1
                                if v2 in f_verts:
                                    repeated_verts += 1
                                if related_verts_in_common[0] in f_verts:
                                    repeated_verts += 1
                                if related_verts_in_common[1] in f_verts:
                                    repeated_verts += 1

                                if repeated_verts == len(f_verts):
                                    repeated_face = True
                                    break

                        if not repeated_face:
                            faces_verts_idx.append([v1, related_verts_in_common[0],
                                                    v2, related_verts_in_common[1]])

                    # If Two verts have one related vert in common and they are
                    # related to each other, they form a triangle
                    elif v2_in_rel_v1 and v1_in_rel_v2 and len(related_verts_in_common) == 1:
                        # Check if the face is already saved.
                        for f_verts in faces_verts_idx:
                            repeated_verts = 0

                            if len(f_verts) == 3:
                                if v1 in f_verts:
                                    repeated_verts += 1
                                if v2 in f_verts:
                                    repeated_verts += 1
                                if related_verts_in_common[0] in f_verts:
                                    repeated_verts += 1

                                if repeated_verts == len(f_verts):
                                    repeated_face = True
                                    break

                        if not repeated_face:
                            faces_verts_idx.append([v1, related_verts_in_common[0], v2])

        # Keep only the faces that don't overlap by ignoring
        # quads that overlap with two adjacent triangles
        faces_to_not_include_idx = []  # Indices of faces_verts_idx to eliminate
        for i in range(len(faces_verts_idx)):
            for t in range(len(faces_verts_idx)):
                if i != t:
                    verts_in_common = 0

                    if len(faces_verts_idx[i]) == 4 and len(faces_verts_idx[t]) == 3:
                        for v_idx in faces_verts_idx[t]:
                            if v_idx in faces_verts_idx[i]:
                                verts_in_common += 1
                        # If it doesn't have all it's vertices repeated in the other face
                        if verts_in_common == 3:
                            if i not in faces_to_not_include_idx:
                                faces_to_not_include_idx.append(i)

        # Build surface
        all_surface_verts_co = []
        verts_idx_translation = {}
        for i in range(len(final_points_ob.data.vertices)):
            coords = final_points_ob.data.vertices[i].co
            all_surface_verts_co.append([coords[0], coords[1], coords[2]])

        # Verts of each face.
        all_surface_faces = []
        for i in range(len(faces_verts_idx)):
            if i not in faces_to_not_include_idx:
                face = []
                for v_idx in faces_verts_idx[i]:
                    face.append(v_idx)

                all_surface_faces.append(face)

        # Build the mesh
        surf_me_name = "SURFSKIO_surface"
        me_surf = bpy.data.meshes.new(surf_me_name)

        me_surf.from_pydata(all_surface_verts_co, [], all_surface_faces)

        me_surf.update()

        ob_surface = bpy.data.objects.new(surf_me_name, me_surf)
        bpy.context.scene.objects.link(ob_surface)

        # Delete final points temporal object
        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[final_points_ob.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[final_points_ob.name]

        bpy.ops.object.delete()

        # Delete isolated verts if there are any
        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[ob_surface.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[ob_surface.name]

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.mesh.select_face_by_sides(type='NOTEQUAL')
        bpy.ops.mesh.delete()
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        # Join crosshatch results with original mesh

        # Calculate a distance to merge the verts of the crosshatch surface to the main object
        edges_length_sum = 0
        for ed in ob_surface.data.edges:
            edges_length_sum += (
                                ob_surface.data.vertices[ed.vertices[0]].co -
                                ob_surface.data.vertices[ed.vertices[1]].co
                                ).length

        if len(ob_surface.data.edges) > 0:
            average_surface_edges_length = edges_length_sum / len(ob_surface.data.edges)
        else:
            average_surface_edges_length = 0.0001

        # Make dictionary with all the verts connected to each vert, on the new surface object.
        surface_connected_verts = {}
        for ed in ob_surface.data.edges:
            if not ed.vertices[0] in surface_connected_verts:
                surface_connected_verts[ed.vertices[0]] = []

            surface_connected_verts[ed.vertices[0]].append(ed.vertices[1])

            if ed.vertices[1] not in surface_connected_verts:
                surface_connected_verts[ed.vertices[1]] = []

            surface_connected_verts[ed.vertices[1]].append(ed.vertices[0])

        # Duplicate the new surface object, and use shrinkwrap to
        # calculate later the nearest verts to the main object
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bpy.ops.object.duplicate('INVOKE_REGION_WIN')

        final_ob_duplicate = bpy.context.scene.objects.active

        bpy.ops.object.modifier_add('INVOKE_REGION_WIN', type='SHRINKWRAP')
        final_ob_duplicate.modifiers["Shrinkwrap"].wrap_method = "NEAREST_VERTEX"
        final_ob_duplicate.modifiers["Shrinkwrap"].target = self.main_object

        bpy.ops.object.modifier_apply('INVOKE_REGION_WIN', apply_as='DATA', modifier='Shrinkwrap')

        # Make list with verts of original mesh as index and coords as value
        main_object_verts_coords = []
        for v in self.main_object.data.vertices:
            coords = self.main_object.matrix_world * v.co

            # To avoid problems when taking "-0.00" as a different value as "0.00"
            for c in range(len(coords)):
                if "%.3f" % coords[c] == "-0.00":
                    coords[c] = 0

            main_object_verts_coords.append(["%.3f" % coords[0], "%.3f" % coords[1], "%.3f" % coords[2]])

        tuple(main_object_verts_coords)

        # Determine which verts will be merged, snap them to the nearest verts
        # on the original verts, and get them selected
        crosshatch_verts_to_merge = []
        if self.automatic_join:
            for i in range(len(ob_surface.data.vertices)):
                # Calculate the distance from each of the connected verts to the actual vert,
                # and compare it with the distance they would have if joined.
                # If they don't change much, that vert can be joined
                merge_actual_vert = True
                if len(surface_connected_verts[i]) < 4:
                    for c_v_idx in surface_connected_verts[i]:
                        points_original = []
                        points_original.append(ob_surface.data.vertices[c_v_idx].co)
                        points_original.append(ob_surface.data.vertices[i].co)

                        points_target = []
                        points_target.append(ob_surface.data.vertices[c_v_idx].co)
                        points_target.append(final_ob_duplicate.data.vertices[i].co)

                        vec_A = points_original[0] - points_original[1]
                        vec_B = points_target[0] - points_target[1]

                        dist_A = (points_original[0] - points_original[1]).length
                        dist_B = (points_target[0] - points_target[1]).length

                        if not (
                           points_original[0] == points_original[1] or
                           points_target[0] == points_target[1]
                           ):  # If any vector's length is zero

                            angle = vec_A.angle(vec_B) / pi
                        else:
                            angle = 0

                        # Set a range of acceptable variation in the connected edges
                        if dist_B > dist_A * 1.7 * self.join_stretch_factor or \
                           dist_B < dist_A / 2 / self.join_stretch_factor or \
                           angle >= 0.15 * self.join_stretch_factor:

                            merge_actual_vert = False
                            break
                else:
                    merge_actual_vert = False

                if merge_actual_vert:
                    coords = final_ob_duplicate.data.vertices[i].co
                    # To avoid problems when taking "-0.000" as a different value as "0.00"
                    for c in range(len(coords)):
                        if "%.3f" % coords[c] == "-0.00":
                            coords[c] = 0

                    comparison_coords = ["%.3f" % coords[0], "%.3f" % coords[1], "%.3f" % coords[2]]

                    if comparison_coords in main_object_verts_coords:
                        # Get the index of the vert with those coords in the main object
                        main_object_related_vert_idx = main_object_verts_coords.index(comparison_coords)

                        if self.main_object.data.vertices[main_object_related_vert_idx].select is True or \
                           self.main_object_selected_verts_count == 0:

                            ob_surface.data.vertices[i].co = final_ob_duplicate.data.vertices[i].co
                            ob_surface.data.vertices[i].select = True
                            crosshatch_verts_to_merge.append(i)

                            # Make sure the vert in the main object is selected,
                            # in case it wasn't selected and the "join crosshatch" option is active
                            self.main_object.data.vertices[main_object_related_vert_idx].select = True

        # Delete duplicated object
        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[final_ob_duplicate.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[final_ob_duplicate.name]
        bpy.ops.object.delete()

        # Join crosshatched surface and main object
        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[ob_surface.name].select = True
        bpy.data.objects[self.main_object.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

        bpy.ops.object.join('INVOKE_REGION_WIN')

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        # Perform Remove doubles to merge verts
        if not (self.automatic_join is False and self.main_object_selected_verts_count == 0):
            bpy.ops.mesh.remove_doubles(threshold=0.0001)

        bpy.ops.mesh.select_all(action='DESELECT')

        # If the main object has modifiers, turn their "viewport view status"
        # to what it was before the forced deactivation above
        if len(self.main_object.modifiers) > 0:
            for m_idx in range(len(self.main_object.modifiers)):
                self.main_object.modifiers[m_idx].show_viewport = self.modifiers_prev_viewport_state[m_idx]

        return {'FINISHED'}

    def rectangular_surface(self):
        # Selected edges
        all_selected_edges_idx = []
        all_selected_verts = []
        all_verts_idx = []
        for ed in self.main_object.data.edges:
            if ed.select:
                all_selected_edges_idx.append(ed.index)

                # Selected vertices
                if not ed.vertices[0] in all_selected_verts:
                    all_selected_verts.append(self.main_object.data.vertices[ed.vertices[0]])
                if not ed.vertices[1] in all_selected_verts:
                    all_selected_verts.append(self.main_object.data.vertices[ed.vertices[1]])

                # All verts (both from each edge) to determine later
                # which are at the tips (those not repeated twice)
                all_verts_idx.append(ed.vertices[0])
                all_verts_idx.append(ed.vertices[1])

        # Identify the tips and "middle-vertex" that separates U from V, if there is one
        all_chains_tips_idx = []
        for v_idx in all_verts_idx:
            if all_verts_idx.count(v_idx) < 2:
                all_chains_tips_idx.append(v_idx)

        edges_connected_to_tips = []
        for ed in self.main_object.data.edges:
            if (ed.vertices[0] in all_chains_tips_idx or ed.vertices[1] in all_chains_tips_idx) and \
               not (ed.vertices[0] in all_verts_idx and ed.vertices[1] in all_verts_idx):

                edges_connected_to_tips.append(ed)

        # Check closed selections
        # List with groups of three verts, where the first element of the pair is
        # the unselected vert of a closed selection and the other two elements are the
        # selected neighbor verts (it will be useful to determine which selection chain
        # the unselected vert belongs to, and determine the "middle-vertex")
        single_unselected_verts_and_neighbors = []

        # To identify a "closed" selection (a selection that is a closed chain except
        # for one vertex) find the vertex in common that have the edges connected to tips.
        # If there is a vertex in common, that one is the unselected vert that closes
        # the selection or is a "middle-vertex"
        single_unselected_verts = []
        for ed in edges_connected_to_tips:
            for ed_b in edges_connected_to_tips:
                if ed != ed_b:
                    if ed.vertices[0] == ed_b.vertices[0] and \
                       not self.main_object.data.vertices[ed.vertices[0]].select and \
                       ed.vertices[0] not in single_unselected_verts:

                        # The second element is one of the tips of the selected
                        # vertices of the closed selection
                        single_unselected_verts_and_neighbors.append(
                                        [ed.vertices[0], ed.vertices[1], ed_b.vertices[1]]
                                        )
                        single_unselected_verts.append(ed.vertices[0])
                        break
                    elif ed.vertices[0] == ed_b.vertices[1] and \
                       not self.main_object.data.vertices[ed.vertices[0]].select and \
                       ed.vertices[0] not in single_unselected_verts:

                        single_unselected_verts_and_neighbors.append(
                                        [ed.vertices[0], ed.vertices[1], ed_b.vertices[0]]
                                        )
                        single_unselected_verts.append(ed.vertices[0])
                        break
                    elif ed.vertices[1] == ed_b.vertices[0] and \
                       not self.main_object.data.vertices[ed.vertices[1]].select and  \
                       ed.vertices[1] not in single_unselected_verts:

                        single_unselected_verts_and_neighbors.append(
                                              [ed.vertices[1], ed.vertices[0], ed_b.vertices[1]]
                                              )
                        single_unselected_verts.append(ed.vertices[1])
                        break
                    elif ed.vertices[1] == ed_b.vertices[1] and \
                       not self.main_object.data.vertices[ed.vertices[1]].select and \
                       ed.vertices[1] not in single_unselected_verts:

                        single_unselected_verts_and_neighbors.append(
                                              [ed.vertices[1], ed.vertices[0], ed_b.vertices[0]]
                                              )
                        single_unselected_verts.append(ed.vertices[1])
                        break

        middle_vertex_idx = None
        tips_to_discard_idx = []

        # Check if there is a "middle-vertex", and get its index
        for i in range(0, len(single_unselected_verts_and_neighbors)):
            actual_chain_verts = self.get_ordered_verts(
                                        self.main_object, all_selected_edges_idx,
                                        all_verts_idx, single_unselected_verts_and_neighbors[i][1],
                                        None, None
                                        )

            if single_unselected_verts_and_neighbors[i][2] != \
               actual_chain_verts[len(actual_chain_verts) - 1].index:

                middle_vertex_idx = single_unselected_verts_and_neighbors[i][0]
                tips_to_discard_idx.append(single_unselected_verts_and_neighbors[i][1])
                tips_to_discard_idx.append(single_unselected_verts_and_neighbors[i][2])

        # List with pairs of verts that belong to the tips of each selection chain (row)
        verts_tips_same_chain_idx = []
        if len(all_chains_tips_idx) >= 2:
            checked_v = []
            for i in range(0, len(all_chains_tips_idx)):
                if all_chains_tips_idx[i] not in checked_v:
                    v_chain = self.get_ordered_verts(
                                    self.main_object, all_selected_edges_idx,
                                    all_verts_idx, all_chains_tips_idx[i],
                                    middle_vertex_idx, None
                                    )

                    verts_tips_same_chain_idx.append([v_chain[0].index, v_chain[len(v_chain) - 1].index])

                    checked_v.append(v_chain[0].index)
                    checked_v.append(v_chain[len(v_chain) - 1].index)

        # Selection tips (vertices).
        verts_tips_parsed_idx = []
        if len(all_chains_tips_idx) >= 2:
            for spec_v_idx in all_chains_tips_idx:
                if (spec_v_idx not in tips_to_discard_idx):
                    verts_tips_parsed_idx.append(spec_v_idx)

        # Identify the type of selection made by the user
        if middle_vertex_idx is not None:
            # If there are 4 tips (two selection chains), and
            # there is only one single unselected vert (the middle vert)
            if len(all_chains_tips_idx) == 4 and len(single_unselected_verts_and_neighbors) == 1:
                selection_type = "TWO_CONNECTED"
            else:
                # The type of the selection was not identified, the script stops.
                self.report({'WARNING'}, "The selection isn't valid.")
                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                self.cleanup_on_interruption()
                self.stopping_errors = True

                return{'CANCELLED'}
        else:
            if len(all_chains_tips_idx) == 2:    # If there are 2 tips
                selection_type = "SINGLE"
            elif len(all_chains_tips_idx) == 4:  # If there are 4 tips
                selection_type = "TWO_NOT_CONNECTED"
            elif len(all_chains_tips_idx) == 0:
                if len(self.main_splines.data.splines) > 1:
                    selection_type = "NO_SELECTION"
                else:
                    # If the selection was not identified and there is only one stroke,
                    # there's no possibility to build a surface, so the script is interrupted
                    self.report({'WARNING'}, "The selection isn't valid.")
                    bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                    self.cleanup_on_interruption()
                    self.stopping_errors = True

                    return{'CANCELLED'}
            else:
                # The type of the selection was not identified, the script stops
                self.report({'WARNING'}, "The selection isn't valid.")

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                self.cleanup_on_interruption()

                self.stopping_errors = True

                return{'CANCELLED'}

        # If the selection type is TWO_NOT_CONNECTED and there is only one stroke, stop the script
        if selection_type == "TWO_NOT_CONNECTED" and len(self.main_splines.data.splines) == 1:
            self.report({'WARNING'},
                        "At least two strokes are needed when there are two not connected selections")
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            self.cleanup_on_interruption()
            self.stopping_errors = True

            return{'CANCELLED'}

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[self.main_splines.name].select = True
        bpy.context.scene.objects.active = bpy.context.scene.objects[self.main_splines.name]

        # Enter editmode for the new curve (converted from grease pencil strokes), to smooth it out
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.curve.smooth('INVOKE_REGION_WIN')
        bpy.ops.curve.smooth('INVOKE_REGION_WIN')
        bpy.ops.curve.smooth('INVOKE_REGION_WIN')
        bpy.ops.curve.smooth('INVOKE_REGION_WIN')
        bpy.ops.curve.smooth('INVOKE_REGION_WIN')
        bpy.ops.curve.smooth('INVOKE_REGION_WIN')
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        self.selection_U_exists = False
        self.selection_U2_exists = False
        self.selection_V_exists = False
        self.selection_V2_exists = False

        self.selection_U_is_closed = False
        self.selection_U2_is_closed = False
        self.selection_V_is_closed = False
        self.selection_V2_is_closed = False

        # Define what vertices are at the tips of each selection and are not the middle-vertex
        if selection_type == "TWO_CONNECTED":
            self.selection_U_exists = True
            self.selection_V_exists = True

            closing_vert_U_idx = None
            closing_vert_V_idx = None
            closing_vert_U2_idx = None
            closing_vert_V2_idx = None

            # Determine which selection is Selection-U and which is Selection-V
            points_A = []
            points_B = []
            points_first_stroke_tips = []

            points_A.append(
                self.main_object.matrix_world * self.main_object.data.vertices[verts_tips_parsed_idx[0]].co
                )
            points_A.append(
                self.main_object.matrix_world * self.main_object.data.vertices[middle_vertex_idx].co
                )
            points_B.append(
                self.main_object.matrix_world * self.main_object.data.vertices[verts_tips_parsed_idx[1]].co
                )
            points_B.append(
                self.main_object.matrix_world * self.main_object.data.vertices[middle_vertex_idx].co
                )
            points_first_stroke_tips.append(
                self.main_splines.data.splines[0].bezier_points[0].co
                )
            points_first_stroke_tips.append(
                self.main_splines.data.splines[0].bezier_points[
                                                    len(self.main_splines.data.splines[0].bezier_points) - 1
                                                    ].co
                )

            angle_A = self.orientation_difference(points_A, points_first_stroke_tips)
            angle_B = self.orientation_difference(points_B, points_first_stroke_tips)

            if angle_A < angle_B:
                first_vert_U_idx = verts_tips_parsed_idx[0]
                first_vert_V_idx = verts_tips_parsed_idx[1]
            else:
                first_vert_U_idx = verts_tips_parsed_idx[1]
                first_vert_V_idx = verts_tips_parsed_idx[0]

        elif selection_type == "SINGLE" or selection_type == "TWO_NOT_CONNECTED":
            first_sketched_point_first_stroke_co = self.main_splines.data.splines[0].bezier_points[0].co
            last_sketched_point_first_stroke_co = \
                    self.main_splines.data.splines[0].bezier_points[
                                                    len(self.main_splines.data.splines[0].bezier_points) - 1
                                                    ].co
            first_sketched_point_last_stroke_co = \
                    self.main_splines.data.splines[
                                            len(self.main_splines.data.splines) - 1
                                            ].bezier_points[0].co
            if len(self.main_splines.data.splines) > 1:
                first_sketched_point_second_stroke_co = self.main_splines.data.splines[1].bezier_points[0].co
                last_sketched_point_second_stroke_co = \
                    self.main_splines.data.splines[1].bezier_points[
                                            len(self.main_splines.data.splines[1].bezier_points) - 1
                                            ].co

            single_unselected_neighbors = []  # Only the neighbors of the single unselected verts
            for verts_neig_idx in single_unselected_verts_and_neighbors:
                single_unselected_neighbors.append(verts_neig_idx[1])
                single_unselected_neighbors.append(verts_neig_idx[2])

            all_chains_tips_and_middle_vert = []
            for v_idx in all_chains_tips_idx:
                if v_idx not in single_unselected_neighbors:
                    all_chains_tips_and_middle_vert.append(v_idx)

            all_chains_tips_and_middle_vert += single_unselected_verts

            all_participating_verts = all_chains_tips_and_middle_vert + all_verts_idx

            # The tip of the selected vertices nearest to the first point of the first sketched stroke
            nearest_tip_to_first_st_first_pt_idx, shortest_distance_to_first_stroke = \
                    self.shortest_distance(
                                self.main_object,
                                first_sketched_point_first_stroke_co,
                                all_chains_tips_and_middle_vert
                                )
            # If the nearest tip is not from a closed selection, get the opposite tip vertex index
            if nearest_tip_to_first_st_first_pt_idx not in single_unselected_verts or \
               nearest_tip_to_first_st_first_pt_idx == middle_vertex_idx:

                nearest_tip_to_first_st_first_pt_opposite_idx = \
                    self.opposite_tip(
                                nearest_tip_to_first_st_first_pt_idx,
                                verts_tips_same_chain_idx
                                )
            # The tip of the selected vertices nearest to the last point of the first sketched stroke
            nearest_tip_to_first_st_last_pt_idx, temp_dist = \
                    self.shortest_distance(
                                self.main_object,
                                last_sketched_point_first_stroke_co,
                                all_chains_tips_and_middle_vert
                                )
            # The tip of the selected vertices nearest to the first point of the last sketched stroke
            nearest_tip_to_last_st_first_pt_idx, shortest_distance_to_last_stroke = \
                    self.shortest_distance(
                                self.main_object,
                                first_sketched_point_last_stroke_co,
                                all_chains_tips_and_middle_vert
                                )
            if len(self.main_splines.data.splines) > 1:
                # The selected vertex nearest to the first point of the second sketched stroke
                # (This will be useful to determine the direction of the closed
                # selection V when extruding along strokes)
                nearest_vert_to_second_st_first_pt_idx, temp_dist = \
                        self.shortest_distance(
                                self.main_object,
                                first_sketched_point_second_stroke_co,
                                all_verts_idx
                                )
                # The selected vertex nearest to the first point of the second sketched stroke
                # (This will be useful to determine the direction of the closed
                # selection V2 when extruding along strokes)
                nearest_vert_to_second_st_last_pt_idx, temp_dist = \
                        self.shortest_distance(
                                self.main_object,
                                last_sketched_point_second_stroke_co,
                                all_verts_idx
                                )
            # Determine if the single selection will be treated as U or as V
            edges_sum = 0
            for i in all_selected_edges_idx:
                edges_sum += (
                    (self.main_object.matrix_world *
                     self.main_object.data.vertices[self.main_object.data.edges[i].vertices[0]].co) -
                    (self.main_object.matrix_world *
                     self.main_object.data.vertices[self.main_object.data.edges[i].vertices[1]].co)
                    ).length

            average_edge_length = edges_sum / len(all_selected_edges_idx)

            # Get shortest distance from the first point of the last stroke to any participating vertex
            temp_idx, shortest_distance_to_last_stroke = \
                        self.shortest_distance(
                                self.main_object,
                                first_sketched_point_last_stroke_co,
                                all_participating_verts
                                )
            # If the beginning of the first stroke is near enough, and its orientation
            # difference with the first edge of the nearest selection chain is not too high,
            # interpret things as an "extrude along strokes" instead of "extrude through strokes"
            if shortest_distance_to_first_stroke < average_edge_length / 4 and \
             shortest_distance_to_last_stroke < average_edge_length and \
             len(self.main_splines.data.splines) > 1:

                self.selection_U_exists = False
                self.selection_V_exists = True
                # If the first selection is not closed
                if nearest_tip_to_first_st_first_pt_idx not in single_unselected_verts or \
                  nearest_tip_to_first_st_first_pt_idx == middle_vertex_idx:
                    self.selection_V_is_closed = False
                    first_neighbor_V_idx = None
                    closing_vert_U_idx = None
                    closing_vert_U2_idx = None
                    closing_vert_V_idx = None
                    closing_vert_V2_idx = None

                    first_vert_V_idx = nearest_tip_to_first_st_first_pt_idx

                    if selection_type == "TWO_NOT_CONNECTED":
                        self.selection_V2_exists = True

                        first_vert_V2_idx = nearest_tip_to_first_st_last_pt_idx
                else:
                    self.selection_V_is_closed = True
                    closing_vert_V_idx = nearest_tip_to_first_st_first_pt_idx

                    # Get the neighbors of the first (unselected) vert of the closed selection U.
                    vert_neighbors = []
                    for verts in single_unselected_verts_and_neighbors:
                        if verts[0] == nearest_tip_to_first_st_first_pt_idx:
                            vert_neighbors.append(verts[1])
                            vert_neighbors.append(verts[2])
                            break

                    verts_V = self.get_ordered_verts(
                                    self.main_object, all_selected_edges_idx,
                                    all_verts_idx, vert_neighbors[0], middle_vertex_idx, None
                                    )

                    for i in range(0, len(verts_V)):
                        if verts_V[i].index == nearest_vert_to_second_st_first_pt_idx:
                            # If the vertex nearest to the first point of the second stroke
                            # is in the first half of the selected verts
                            if i >= len(verts_V) / 2:
                                first_vert_V_idx = vert_neighbors[1]
                                break
                            else:
                                first_vert_V_idx = vert_neighbors[0]
                                break

                if selection_type == "TWO_NOT_CONNECTED":
                    self.selection_V2_exists = True
                    # If the second selection is not closed
                    if nearest_tip_to_first_st_last_pt_idx not in single_unselected_verts or \
                      nearest_tip_to_first_st_last_pt_idx == middle_vertex_idx:

                        self.selection_V2_is_closed = False
                        first_neighbor_V2_idx = None
                        closing_vert_V2_idx = None
                        first_vert_V2_idx = nearest_tip_to_first_st_last_pt_idx

                    else:
                        self.selection_V2_is_closed = True
                        closing_vert_V2_idx = nearest_tip_to_first_st_last_pt_idx

                        # Get the neighbors of the first (unselected) vert of the closed selection U
                        vert_neighbors = []
                        for verts in single_unselected_verts_and_neighbors:
                            if verts[0] == nearest_tip_to_first_st_last_pt_idx:
                                vert_neighbors.append(verts[1])
                                vert_neighbors.append(verts[2])
                                break

                        verts_V2 = self.get_ordered_verts(
                                        self.main_object, all_selected_edges_idx,
                                        all_verts_idx, vert_neighbors[0], middle_vertex_idx, None
                                        )

                        for i in range(0, len(verts_V2)):
                            if verts_V2[i].index == nearest_vert_to_second_st_last_pt_idx:
                                # If the vertex nearest to the first point of the second stroke
                                # is in the first half of the selected verts
                                if i >= len(verts_V2) / 2:
                                    first_vert_V2_idx = vert_neighbors[1]
                                    break
                                else:
                                    first_vert_V2_idx = vert_neighbors[0]
                                    break
                else:
                    self.selection_V2_exists = False

            else:
                self.selection_U_exists = True
                self.selection_V_exists = False
                # If the first selection is not closed
                if nearest_tip_to_first_st_first_pt_idx not in single_unselected_verts or \
                  nearest_tip_to_first_st_first_pt_idx == middle_vertex_idx:
                    self.selection_U_is_closed = False
                    first_neighbor_U_idx = None
                    closing_vert_U_idx = None

                    points_tips = []
                    points_tips.append(
                            self.main_object.matrix_world *
                            self.main_object.data.vertices[nearest_tip_to_first_st_first_pt_idx].co
                            )
                    points_tips.append(
                            self.main_object.matrix_world *
                            self.main_object.data.vertices[nearest_tip_to_first_st_first_pt_opposite_idx].co
                            )
                    points_first_stroke_tips = []
                    points_first_stroke_tips.append(self.main_splines.data.splines[0].bezier_points[0].co)
                    points_first_stroke_tips.append(
                        self.main_splines.data.splines[0].bezier_points[
                                                len(self.main_splines.data.splines[0].bezier_points) - 1
                                                ].co
                        )
                    vec_A = points_tips[0] - points_tips[1]
                    vec_B = points_first_stroke_tips[0] - points_first_stroke_tips[1]

                    # Compare the direction of the selection and the first
                    # grease pencil stroke to determine which is the "first" vertex of the selection
                    if vec_A.dot(vec_B) < 0:
                        first_vert_U_idx = nearest_tip_to_first_st_first_pt_opposite_idx
                    else:
                        first_vert_U_idx = nearest_tip_to_first_st_first_pt_idx

                else:
                    self.selection_U_is_closed = True
                    closing_vert_U_idx = nearest_tip_to_first_st_first_pt_idx

                    # Get the neighbors of the first (unselected) vert of the closed selection U
                    vert_neighbors = []
                    for verts in single_unselected_verts_and_neighbors:
                        if verts[0] == nearest_tip_to_first_st_first_pt_idx:
                            vert_neighbors.append(verts[1])
                            vert_neighbors.append(verts[2])
                            break

                    points_first_and_neighbor = []
                    points_first_and_neighbor.append(
                            self.main_object.matrix_world *
                            self.main_object.data.vertices[nearest_tip_to_first_st_first_pt_idx].co
                            )
                    points_first_and_neighbor.append(
                            self.main_object.matrix_world *
                            self.main_object.data.vertices[vert_neighbors[0]].co
                            )
                    points_first_stroke_tips = []
                    points_first_stroke_tips.append(self.main_splines.data.splines[0].bezier_points[0].co)
                    points_first_stroke_tips.append(self.main_splines.data.splines[0].bezier_points[1].co)

                    vec_A = points_first_and_neighbor[0] - points_first_and_neighbor[1]
                    vec_B = points_first_stroke_tips[0] - points_first_stroke_tips[1]

                    # Compare the direction of the selection and the first grease pencil stroke to
                    # determine which is the vertex neighbor to the first vertex (unselected) of
                    # the closed selection. This will determine the direction of the closed selection
                    if vec_A.dot(vec_B) < 0:
                        first_vert_U_idx = vert_neighbors[1]
                    else:
                        first_vert_U_idx = vert_neighbors[0]

                if selection_type == "TWO_NOT_CONNECTED":
                    self.selection_U2_exists = True
                    # If the second selection is not closed
                    if nearest_tip_to_last_st_first_pt_idx not in single_unselected_verts or \
                      nearest_tip_to_last_st_first_pt_idx == middle_vertex_idx:

                        self.selection_U2_is_closed = False
                        first_neighbor_U2_idx = None
                        closing_vert_U2_idx = None
                        first_vert_U2_idx = nearest_tip_to_last_st_first_pt_idx
                    else:
                        self.selection_U2_is_closed = True
                        closing_vert_U2_idx = nearest_tip_to_last_st_first_pt_idx

                        # Get the neighbors of the first (unselected) vert of the closed selection U
                        vert_neighbors = []
                        for verts in single_unselected_verts_and_neighbors:
                            if verts[0] == nearest_tip_to_last_st_first_pt_idx:
                                vert_neighbors.append(verts[1])
                                vert_neighbors.append(verts[2])
                                break

                        points_first_and_neighbor = []
                        points_first_and_neighbor.append(
                                self.main_object.matrix_world *
                                self.main_object.data.vertices[nearest_tip_to_last_st_first_pt_idx].co
                                )
                        points_first_and_neighbor.append(
                                self.main_object.matrix_world *
                                self.main_object.data.vertices[vert_neighbors[0]].co
                                )
                        points_last_stroke_tips = []
                        points_last_stroke_tips.append(
                                self.main_splines.data.splines[
                                                        len(self.main_splines.data.splines) - 1
                                                        ].bezier_points[0].co
                                )
                        points_last_stroke_tips.append(
                                self.main_splines.data.splines[
                                                        len(self.main_splines.data.splines) - 1
                                                        ].bezier_points[1].co
                                )
                        vec_A = points_first_and_neighbor[0] - points_first_and_neighbor[1]
                        vec_B = points_last_stroke_tips[0] - points_last_stroke_tips[1]

                        # Compare the direction of the selection and the last grease pencil stroke to
                        # determine which is the vertex neighbor to the first vertex (unselected) of
                        # the closed selection. This will determine the direction of the closed selection
                        if vec_A.dot(vec_B) < 0:
                            first_vert_U2_idx = vert_neighbors[1]
                        else:
                            first_vert_U2_idx = vert_neighbors[0]
                else:
                    self.selection_U2_exists = False

        elif selection_type == "NO_SELECTION":
            self.selection_U_exists = False
            self.selection_V_exists = False

        # Get an ordered list of the vertices of Selection-U
        verts_ordered_U = []
        if self.selection_U_exists:
            verts_ordered_U = self.get_ordered_verts(
                                    self.main_object, all_selected_edges_idx,
                                    all_verts_idx, first_vert_U_idx,
                                    middle_vertex_idx, closing_vert_U_idx
                                    )
            verts_ordered_U_indices = [x.index for x in verts_ordered_U]

        # Get an ordered list of the vertices of Selection-U2
        verts_ordered_U2 = []
        if self.selection_U2_exists:
            verts_ordered_U2 = self.get_ordered_verts(
                                    self.main_object, all_selected_edges_idx,
                                    all_verts_idx, first_vert_U2_idx,
                                    middle_vertex_idx, closing_vert_U2_idx
                                    )
            verts_ordered_U2_indices = [x.index for x in verts_ordered_U2]

        # Get an ordered list of the vertices of Selection-V
        verts_ordered_V = []
        if self.selection_V_exists:
            verts_ordered_V = self.get_ordered_verts(
                                    self.main_object, all_selected_edges_idx,
                                    all_verts_idx, first_vert_V_idx,
                                    middle_vertex_idx, closing_vert_V_idx
                                    )
            verts_ordered_V_indices = [x.index for x in verts_ordered_V]

        # Get an ordered list of the vertices of Selection-V2
        verts_ordered_V2 = []
        if self.selection_V2_exists:
            verts_ordered_V2 = self.get_ordered_verts(
                                    self.main_object, all_selected_edges_idx,
                                    all_verts_idx, first_vert_V2_idx,
                                    middle_vertex_idx, closing_vert_V2_idx
                                    )
            verts_ordered_V2_indices = [x.index for x in verts_ordered_V2]

        # Check if when there are two-not-connected selections both have the same
        # number of verts. If not terminate the script
        if ((self.selection_U2_exists and len(verts_ordered_U) != len(verts_ordered_U2)) or
           (self.selection_V2_exists and len(verts_ordered_V) != len(verts_ordered_V2))):
            # Display a warning
            self.report({'WARNING'}, "Both selections must have the same number of edges")

            self.cleanup_on_interruption()
            self.stopping_errors = True

            return{'CANCELLED'}

        # Calculate edges U proportions
        # Sum selected edges U lengths
        edges_lengths_U = []
        edges_lengths_sum_U = 0

        if self.selection_U_exists:
            edges_lengths_U, edges_lengths_sum_U = self.get_chain_length(
                                                            self.main_object,
                                                            verts_ordered_U
                                                            )
        if self.selection_U2_exists:
            edges_lengths_U2, edges_lengths_sum_U2 = self.get_chain_length(
                                                            self.main_object,
                                                            verts_ordered_U2
                                                            )
        # Sum selected edges V lengths
        edges_lengths_V = []
        edges_lengths_sum_V = 0

        if self.selection_V_exists:
            edges_lengths_V, edges_lengths_sum_V = self.get_chain_length(
                                                            self.main_object,
                                                            verts_ordered_V
                                                            )
        if self.selection_V2_exists:
            edges_lengths_V2, edges_lengths_sum_V2 = self.get_chain_length(
                                                            self.main_object,
                                                            verts_ordered_V2
                                                            )

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.curve.subdivide('INVOKE_REGION_WIN',
                                number_cuts=bpy.context.scene.bsurfaces.SURFSK_precision)
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        # Proportions U
        edges_proportions_U = []
        edges_proportions_U = self.get_edges_proportions(
                                    edges_lengths_U, edges_lengths_sum_U,
                                    self.selection_U_exists, self.edges_U
                                    )
        verts_count_U = len(edges_proportions_U) + 1

        if self.selection_U2_exists:
            edges_proportions_U2 = []
            edges_proportions_U2 = self.get_edges_proportions(
                                    edges_lengths_U2, edges_lengths_sum_U2,
                                    self.selection_U2_exists, self.edges_V
                                    )
            verts_count_U2 = len(edges_proportions_U2) + 1

        # Proportions V
        edges_proportions_V = []
        edges_proportions_V = self.get_edges_proportions(
                                    edges_lengths_V, edges_lengths_sum_V,
                                    self.selection_V_exists, self.edges_V
                                    )
        verts_count_V = len(edges_proportions_V) + 1

        if self.selection_V2_exists:
            edges_proportions_V2 = []
            edges_proportions_V2 = self.get_edges_proportions(
                                    edges_lengths_V2, edges_lengths_sum_V2,
                                    self.selection_V2_exists, self.edges_V
                                    )
            verts_count_V2 = len(edges_proportions_V2) + 1

        # Cyclic Follow: simplify sketched curves, make them Cyclic, and complete
        # the actual sketched curves with a "closing segment"
        if self.cyclic_follow and not self.selection_V_exists and not \
          ((self.selection_U_exists and not self.selection_U_is_closed) or
          (self.selection_U2_exists and not self.selection_U2_is_closed)):

            simplified_spline_coords = []
            simplified_curve = []
            ob_simplified_curve = []
            splines_first_v_co = []
            for i in range(len(self.main_splines.data.splines)):
                # Create a curve object for the actual spline "cyclic extension"
                simplified_curve.append(bpy.data.curves.new('SURFSKIO_simpl_crv', 'CURVE'))
                ob_simplified_curve.append(bpy.data.objects.new('SURFSKIO_simpl_crv', simplified_curve[i]))
                bpy.context.scene.objects.link(ob_simplified_curve[i])

                simplified_curve[i].dimensions = "3D"

                spline_coords = []
                for bp in self.main_splines.data.splines[i].bezier_points:
                    spline_coords.append(bp.co)

                # Simplification
                simplified_spline_coords.append(self.simplify_spline(spline_coords, 5))

                # Get the coordinates of the first vert of the actual spline
                splines_first_v_co.append(simplified_spline_coords[i][0])

                # Generate the spline
                spline = simplified_curve[i].splines.new('BEZIER')
                # less one because one point is added when the spline is created
                spline.bezier_points.add(len(simplified_spline_coords[i]) - 1)
                for p in range(0, len(simplified_spline_coords[i])):
                    spline.bezier_points[p].co = simplified_spline_coords[i][p]

                spline.use_cyclic_u = True

                spline_bp_count = len(spline.bezier_points)

                bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.data.objects[ob_simplified_curve[i].name].select = True
                bpy.context.scene.objects.active = bpy.context.scene.objects[ob_simplified_curve[i].name]

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='SELECT')
                bpy.ops.curve.handle_type_set('INVOKE_REGION_WIN', type='AUTOMATIC')
                bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

                # Select the "closing segment", and subdivide it
                ob_simplified_curve[i].data.splines[0].bezier_points[0].select_control_point = True
                ob_simplified_curve[i].data.splines[0].bezier_points[0].select_left_handle = True
                ob_simplified_curve[i].data.splines[0].bezier_points[0].select_right_handle = True

                ob_simplified_curve[i].data.splines[0].bezier_points[spline_bp_count - 1].select_control_point = True
                ob_simplified_curve[i].data.splines[0].bezier_points[spline_bp_count - 1].select_left_handle = True
                ob_simplified_curve[i].data.splines[0].bezier_points[spline_bp_count - 1].select_right_handle = True

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                segments = sqrt(
                          (ob_simplified_curve[i].data.splines[0].bezier_points[0].co -
                           ob_simplified_curve[i].data.splines[0].bezier_points[spline_bp_count - 1].co).length /
                          self.average_gp_segment_length
                        )
                for t in range(2):
                    bpy.ops.curve.subdivide('INVOKE_REGION_WIN', number_cuts=segments)

                # Delete the other vertices and make it non-cyclic to
                # keep only the needed verts of the "closing segment"
                bpy.ops.curve.select_all(action='INVERT')
                bpy.ops.curve.delete(type='VERT')
                ob_simplified_curve[i].data.splines[0].use_cyclic_u = False
                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

                # Add the points of the "closing segment" to the original curve from grease pencil stroke
                first_new_index = len(self.main_splines.data.splines[i].bezier_points)
                self.main_splines.data.splines[i].bezier_points.add(
                                                    len(ob_simplified_curve[i].data.splines[0].bezier_points) - 1
                                                    )
                for t in range(1, len(ob_simplified_curve[i].data.splines[0].bezier_points)):
                    self.main_splines.data.splines[i].bezier_points[t - 1 + first_new_index].co = \
                            ob_simplified_curve[i].data.splines[0].bezier_points[t].co

                # Delete the temporal curve
                bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.data.objects[ob_simplified_curve[i].name].select = True
                bpy.context.scene.objects.active = bpy.context.scene.objects[ob_simplified_curve[i].name]

                bpy.ops.object.delete()

        # Get the coords of the points distributed along the sketched strokes,
        # with proportions-U of the first selection
        pts_on_strokes_with_proportions_U = self.distribute_pts(
                                                    self.main_splines.data.splines,
                                                    edges_proportions_U
                                                    )
        sketched_splines_parsed = []

        if self.selection_U2_exists:
            # Initialize the multidimensional list with the proportions of all the segments
            proportions_loops_crossing_strokes = []
            for i in range(len(pts_on_strokes_with_proportions_U)):
                proportions_loops_crossing_strokes.append([])

                for t in range(len(pts_on_strokes_with_proportions_U[0])):
                    proportions_loops_crossing_strokes[i].append(None)

            # Calculate the proportions of each segment of the loops-U from pts_on_strokes_with_proportions_U
            for lp in range(len(pts_on_strokes_with_proportions_U[0])):
                loop_segments_lengths = []

                for st in range(len(pts_on_strokes_with_proportions_U)):
                    # When on the first stroke, add the segment from the selection to the dirst stroke
                    if st == 0:
                        loop_segments_lengths.append(
                                    ((self.main_object.matrix_world * verts_ordered_U[lp].co) -
                                    pts_on_strokes_with_proportions_U[0][lp]).length
                                    )
                    # For all strokes except for the last, calculate the distance
                    # from the actual stroke to the next
                    if st != len(pts_on_strokes_with_proportions_U) - 1:
                        loop_segments_lengths.append(
                                    (pts_on_strokes_with_proportions_U[st][lp] -
                                    pts_on_strokes_with_proportions_U[st + 1][lp]).length
                                    )
                    # When on the last stroke, add the segments
                    # from the last stroke to the second selection
                    if st == len(pts_on_strokes_with_proportions_U) - 1:
                        loop_segments_lengths.append(
                                    (pts_on_strokes_with_proportions_U[st][lp] -
                                    (self.main_object.matrix_world * verts_ordered_U2[lp].co)).length
                                    )
                # Calculate full loop length
                loop_seg_lengths_sum = 0
                for i in range(len(loop_segments_lengths)):
                    loop_seg_lengths_sum += loop_segments_lengths[i]

                # Fill the multidimensional list with the proportions of all the segments
                for st in range(len(pts_on_strokes_with_proportions_U)):
                    proportions_loops_crossing_strokes[st][lp] = \
                        loop_segments_lengths[st] / loop_seg_lengths_sum

            # Calculate proportions for each stroke
            for st in range(len(pts_on_strokes_with_proportions_U)):
                actual_stroke_spline = []
                # Needs to be a list for the "distribute_pts" method
                actual_stroke_spline.append(self.main_splines.data.splines[st])

                # Calculate the proportions for the actual stroke.
                actual_edges_proportions_U = []
                for i in range(len(edges_proportions_U)):
                    proportions_sum = 0

                    # Sum the proportions of this loop up to the actual.
                    for t in range(0, st + 1):
                        proportions_sum += proportions_loops_crossing_strokes[t][i]
                    # i + 1, because proportions_loops_crossing_strokes refers to loops,
                    # and the proportions refer to edges, so we start at the element 1
                    # of proportions_loops_crossing_strokes instead of element 0
                    actual_edges_proportions_U.append(
                            edges_proportions_U[i] -
                            ((edges_proportions_U[i] - edges_proportions_U2[i]) * proportions_sum)
                            )
                points_actual_spline = self.distribute_pts(actual_stroke_spline, actual_edges_proportions_U)
                sketched_splines_parsed.append(points_actual_spline[0])
        else:
            sketched_splines_parsed = pts_on_strokes_with_proportions_U

        # If the selection type is "TWO_NOT_CONNECTED" replace the
        # points of the last spline with the points in the "target" selection
        if selection_type == "TWO_NOT_CONNECTED":
            if self.selection_U2_exists:
                for i in range(0, len(sketched_splines_parsed[len(sketched_splines_parsed) - 1])):
                    sketched_splines_parsed[len(sketched_splines_parsed) - 1][i] = \
                            self.main_object.matrix_world * verts_ordered_U2[i].co

        # Create temporary curves along the "control-points" found
        # on the sketched curves and the mesh selection
        mesh_ctrl_pts_name = "SURFSKIO_ctrl_pts"
        me = bpy.data.meshes.new(mesh_ctrl_pts_name)
        ob_ctrl_pts = bpy.data.objects.new(mesh_ctrl_pts_name, me)
        ob_ctrl_pts.data = me
        bpy.context.scene.objects.link(ob_ctrl_pts)

        cyclic_loops_U = []
        first_verts = []
        second_verts = []
        last_verts = []

        for i in range(0, verts_count_U):
            vert_num_in_spline = 1

            if self.selection_U_exists:
                ob_ctrl_pts.data.vertices.add(1)
                last_v = ob_ctrl_pts.data.vertices[len(ob_ctrl_pts.data.vertices) - 1]
                last_v.co = self.main_object.matrix_world * verts_ordered_U[i].co

                vert_num_in_spline += 1

            for t in range(0, len(sketched_splines_parsed)):
                ob_ctrl_pts.data.vertices.add(1)
                v = ob_ctrl_pts.data.vertices[len(ob_ctrl_pts.data.vertices) - 1]
                v.co = sketched_splines_parsed[t][i]

                if vert_num_in_spline > 1:
                    ob_ctrl_pts.data.edges.add(1)
                    ob_ctrl_pts.data.edges[len(ob_ctrl_pts.data.edges) - 1].vertices[0] = \
                            len(ob_ctrl_pts.data.vertices) - 2
                    ob_ctrl_pts.data.edges[len(ob_ctrl_pts.data.edges) - 1].vertices[1] = \
                            len(ob_ctrl_pts.data.vertices) - 1

                if t == 0:
                    first_verts.append(v.index)

                if t == 1:
                    second_verts.append(v.index)

                if t == len(sketched_splines_parsed) - 1:
                    last_verts.append(v.index)

                last_v = v
                vert_num_in_spline += 1

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[ob_ctrl_pts.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[ob_ctrl_pts.name]

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        # Determine which loops-U will be "Cyclic"
        for i in range(0, len(first_verts)):
            # When there is Cyclic Cross there is no need of
            # Automatic Join, (and there are at least three strokes)
            if self.automatic_join and not self.cyclic_cross and \
               selection_type != "TWO_CONNECTED" and len(self.main_splines.data.splines) >= 3:

                v = ob_ctrl_pts.data.vertices
                first_point_co = v[first_verts[i]].co
                second_point_co = v[second_verts[i]].co
                last_point_co = v[last_verts[i]].co

                # Coordinates of the point in the center of both the first and last verts.
                verts_center_co = [
                        (first_point_co[0] + last_point_co[0]) / 2,
                        (first_point_co[1] + last_point_co[1]) / 2,
                        (first_point_co[2] + last_point_co[2]) / 2
                        ]
                vec_A = second_point_co - first_point_co
                vec_B = second_point_co - Vector(verts_center_co)

                # Calculate the length of the first segment of the loop,
                # and the length it would have after moving the first vert
                # to the middle position between first and last
                length_original = (second_point_co - first_point_co).length
                length_target = (second_point_co - Vector(verts_center_co)).length

                angle = vec_A.angle(vec_B) / pi

                # If the target length doesn't stretch too much, and the
                # its angle doesn't change to much either
                if length_target <= length_original * 1.03 * self.join_stretch_factor and \
                   angle <= 0.008 * self.join_stretch_factor and not self.selection_U_exists:

                    cyclic_loops_U.append(True)
                    # Move the first vert to the center coordinates
                    ob_ctrl_pts.data.vertices[first_verts[i]].co = verts_center_co
                    # Select the last verts from Cyclic loops, for later deletion all at once
                    v[last_verts[i]].select = True
                else:
                    cyclic_loops_U.append(False)
            else:
                # If "Cyclic Cross" is active then "all" crossing curves become cyclic
                if self.cyclic_cross and not self.selection_U_exists and not \
                   ((self.selection_V_exists and not self.selection_V_is_closed) or
                   (self.selection_V2_exists and not self.selection_V2_is_closed)):

                    cyclic_loops_U.append(True)
                else:
                    cyclic_loops_U.append(False)

        # The cyclic_loops_U list needs to be reversed.
        cyclic_loops_U.reverse()

        # Delete the previously selected (last_)verts.
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.mesh.delete('INVOKE_REGION_WIN', type='VERT')
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        # Create curves from control points.
        bpy.ops.object.convert('INVOKE_REGION_WIN', target='CURVE', keep_original=False)
        ob_curves_surf = bpy.context.scene.objects.active
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.curve.spline_type_set('INVOKE_REGION_WIN', type='BEZIER')
        bpy.ops.curve.handle_type_set('INVOKE_REGION_WIN', type='AUTOMATIC')

        # Make Cyclic the splines designated as Cyclic.
        for i in range(0, len(cyclic_loops_U)):
            ob_curves_surf.data.splines[i].use_cyclic_u = cyclic_loops_U[i]

        # Get the coords of all points on first loop-U, for later comparison with its
        # subdivided version, to know which points of the loops-U are crossed by the
        # original strokes. The indices wiil be the same for the other loops-U
        if self.loops_on_strokes:
            coords_loops_U_control_points = []
            for p in ob_ctrl_pts.data.splines[0].bezier_points:
                coords_loops_U_control_points.append(["%.4f" % p.co[0], "%.4f" % p.co[1], "%.4f" % p.co[2]])

            tuple(coords_loops_U_control_points)

        # Calculate number of edges-V in case option "Loops on strokes" is active or inactive
        if self.loops_on_strokes and not self.selection_V_exists:
                edges_V_count = len(self.main_splines.data.splines) * self.edges_V
        else:
            edges_V_count = len(edges_proportions_V)

        # The Follow precision will vary depending on the number of Follow face-loops
        precision_multiplier = round(2 + (edges_V_count / 15))
        curve_cuts = bpy.context.scene.bsurfaces.SURFSK_precision * precision_multiplier

        # Subdivide the curves
        bpy.ops.curve.subdivide('INVOKE_REGION_WIN', number_cuts=curve_cuts)

        # The verts position shifting that happens with splines subdivision.
        # For later reorder splines points
        verts_position_shift = curve_cuts + 1
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        # Reorder coordinates of the points of each spline to put the first point of
        # the spline starting at the position it was the first point before sudividing
        # the curve. And make a new curve object per spline (to handle memory better later)
        splines_U_objects = []
        for i in range(len(ob_curves_surf.data.splines)):
            spline_U_curve = bpy.data.curves.new('SURFSKIO_spline_U_' + str(i), 'CURVE')
            ob_spline_U = bpy.data.objects.new('SURFSKIO_spline_U_' + str(i), spline_U_curve)
            bpy.context.scene.objects.link(ob_spline_U)

            spline_U_curve.dimensions = "3D"

            # Add points to the spline in the new curve object
            ob_spline_U.data.splines.new('BEZIER')
            for t in range(len(ob_curves_surf.data.splines[i].bezier_points)):
                if cyclic_loops_U[i] is True and not self.selection_U_exists:  # If the loop is cyclic
                    if t + verts_position_shift <= len(ob_curves_surf.data.splines[i].bezier_points) - 1:
                        point_index = t + verts_position_shift
                    else:
                        point_index = t + verts_position_shift - len(ob_curves_surf.data.splines[i].bezier_points)
                else:
                    point_index = t
                # to avoid adding the first point since it's added when the spline is created
                if t > 0:
                    ob_spline_U.data.splines[0].bezier_points.add(1)
                ob_spline_U.data.splines[0].bezier_points[t].co = \
                        ob_curves_surf.data.splines[i].bezier_points[point_index].co

            if cyclic_loops_U[i] is True and not self.selection_U_exists:  # If the loop is cyclic
                # Add a last point at the same location as the first one
                ob_spline_U.data.splines[0].bezier_points.add(1)
                ob_spline_U.data.splines[0].bezier_points[len(ob_spline_U.data.splines[0].bezier_points) - 1].co = \
                        ob_spline_U.data.splines[0].bezier_points[0].co
            else:
                ob_spline_U.data.splines[0].use_cyclic_u = False

            splines_U_objects.append(ob_spline_U)
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[ob_spline_U.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[ob_spline_U.name]

        # When option "Loops on strokes" is active each "Cross" loop will have
        # its own proportions according to where the original strokes "touch" them
        if self.loops_on_strokes:
            # Get the indices of points where the original strokes "touch" loops-U
            points_U_crossed_by_strokes = []
            for i in range(len(splines_U_objects[0].data.splines[0].bezier_points)):
                bp = splines_U_objects[0].data.splines[0].bezier_points[i]
                if ["%.4f" % bp.co[0], "%.4f" % bp.co[1], "%.4f" % bp.co[2]] in coords_loops_U_control_points:
                    points_U_crossed_by_strokes.append(i)

            # Make a dictionary with the number of the edge, in the selected chain V, corresponding to each stroke
            edge_order_number_for_splines = {}
            if self.selection_V_exists:
                # For two-connected selections add a first hypothetic stroke at the begining.
                if selection_type == "TWO_CONNECTED":
                    edge_order_number_for_splines[0] = 0

                for i in range(len(self.main_splines.data.splines)):
                    sp = self.main_splines.data.splines[i]
                    v_idx, dist_temp = self.shortest_distance(
                                                self.main_object,
                                                sp.bezier_points[0].co,
                                                verts_ordered_V_indices
                                                )
                    # Get the position (edges count) of the vert v_idx in the selected chain V
                    edge_idx_in_chain = verts_ordered_V_indices.index(v_idx)

                    # For two-connected selections the strokes go after the
                    # hypothetic stroke added before, so the index adds one per spline
                    if selection_type == "TWO_CONNECTED":
                        spline_number = i + 1
                    else:
                        spline_number = i

                    edge_order_number_for_splines[spline_number] = edge_idx_in_chain

                    # Get the first and last verts indices for later comparison
                    if i == 0:
                        first_v_idx = v_idx
                    elif i == len(self.main_splines.data.splines) - 1:
                        last_v_idx = v_idx

                if self.selection_V_is_closed:
                    # If there is no last stroke on the last vertex (same as first vertex),
                    # add a hypothetic spline at last vert order
                    if first_v_idx != last_v_idx:
                        edge_order_number_for_splines[(len(self.main_splines.data.splines) - 1) + 1] = \
                                len(verts_ordered_V_indices) - 1
                    else:
                        if self.cyclic_cross:
                            edge_order_number_for_splines[len(self.main_splines.data.splines) - 1] = \
                                    len(verts_ordered_V_indices) - 2
                            edge_order_number_for_splines[(len(self.main_splines.data.splines) - 1) + 1] = \
                                    len(verts_ordered_V_indices) - 1
                        else:
                            edge_order_number_for_splines[len(self.main_splines.data.splines) - 1] = \
                                    len(verts_ordered_V_indices) - 1

        # Get the coords of the points distributed along the
        # "crossing curves", with appropriate proportions-V
        surface_splines_parsed = []
        for i in range(len(splines_U_objects)):
            sp_ob = splines_U_objects[i]
            # If "Loops on strokes" option is active, calculate the proportions for each loop-U
            if self.loops_on_strokes:
                # Segments distances from stroke to stroke
                dist = 0
                full_dist = 0
                segments_distances = []
                for t in range(len(sp_ob.data.splines[0].bezier_points)):
                    bp = sp_ob.data.splines[0].bezier_points[t]

                    if t == 0:
                        last_p = bp.co
                    else:
                        actual_p = bp.co
                        dist += (last_p - actual_p).length

                        if t in points_U_crossed_by_strokes:
                            segments_distances.append(dist)
                            full_dist += dist

                            dist = 0

                        last_p = actual_p

                # Calculate Proportions.
                used_edges_proportions_V = []
                for t in range(len(segments_distances)):
                    if self.selection_V_exists:
                        if t == 0:
                            order_number_last_stroke = 0

                        segment_edges_length_V = 0
                        segment_edges_length_V2 = 0
                        for order in range(order_number_last_stroke, edge_order_number_for_splines[t + 1]):
                            segment_edges_length_V += edges_lengths_V[order]
                            if self.selection_V2_exists:
                                segment_edges_length_V2 += edges_lengths_V2[order]

                        for order in range(order_number_last_stroke, edge_order_number_for_splines[t + 1]):
                            # Calculate each "sub-segment" (the ones between each stroke) length
                            if self.selection_V2_exists:
                                proportion_sub_seg = (edges_lengths_V2[order] -
                                    ((edges_lengths_V2[order] - edges_lengths_V[order]) /
                                    len(splines_U_objects) * i)) / (segment_edges_length_V2 -
                                    (segment_edges_length_V2 - segment_edges_length_V) /
                                    len(splines_U_objects) * i)

                                sub_seg_dist = segments_distances[t] * proportion_sub_seg
                            else:
                                proportion_sub_seg = edges_lengths_V[order] / segment_edges_length_V
                                sub_seg_dist = segments_distances[t] * proportion_sub_seg

                            used_edges_proportions_V.append(sub_seg_dist / full_dist)

                        order_number_last_stroke = edge_order_number_for_splines[t + 1]

                    else:
                        for c in range(self.edges_V):
                            # Calculate each "sub-segment" (the ones between each stroke) length
                            sub_seg_dist = segments_distances[t] / self.edges_V
                            used_edges_proportions_V.append(sub_seg_dist / full_dist)

                actual_spline = self.distribute_pts(sp_ob.data.splines, used_edges_proportions_V)
                surface_splines_parsed.append(actual_spline[0])

            else:
                if self.selection_V2_exists:
                    used_edges_proportions_V = []
                    for p in range(len(edges_proportions_V)):
                        used_edges_proportions_V.append(
                                    edges_proportions_V2[p] -
                                    ((edges_proportions_V2[p] -
                                    edges_proportions_V[p]) / len(splines_U_objects) * i)
                                    )
                else:
                    used_edges_proportions_V = edges_proportions_V

                actual_spline = self.distribute_pts(sp_ob.data.splines, used_edges_proportions_V)
                surface_splines_parsed.append(actual_spline[0])

        # Set the verts of the first and last splines to the locations
        # of the respective verts in the selections
        if self.selection_V_exists:
            for i in range(0, len(surface_splines_parsed[0])):
                surface_splines_parsed[len(surface_splines_parsed) - 1][i] = \
                        self.main_object.matrix_world * verts_ordered_V[i].co

        if selection_type == "TWO_NOT_CONNECTED":
            if self.selection_V2_exists:
                for i in range(0, len(surface_splines_parsed[0])):
                    surface_splines_parsed[0][i] = self.main_object.matrix_world * verts_ordered_V2[i].co

        # When "Automatic join" option is active (and the selection type is not "TWO_CONNECTED"),
        # merge the verts of the tips of the loops when they are "near enough"
        if self.automatic_join and selection_type != "TWO_CONNECTED":
            # Join the tips of "Follow" loops that are near enough and must be "closed"
            if not self.selection_V_exists and len(edges_proportions_U) >= 3:
                for i in range(len(surface_splines_parsed[0])):
                    sp = surface_splines_parsed
                    loop_segment_dist = (sp[0][i] - sp[1][i]).length
                    full_loop_dist = loop_segment_dist * self.edges_U

                    verts_middle_position_co = [
                            (sp[0][i][0] + sp[len(sp) - 1][i][0]) / 2,
                            (sp[0][i][1] + sp[len(sp) - 1][i][1]) / 2,
                            (sp[0][i][2] + sp[len(sp) - 1][i][2]) / 2
                            ]
                    points_original = []
                    points_original.append(sp[1][i])
                    points_original.append(sp[0][i])

                    points_target = []
                    points_target.append(sp[1][i])
                    points_target.append(Vector(verts_middle_position_co))

                    vec_A = points_original[0] - points_original[1]
                    vec_B = points_target[0] - points_target[1]
                    # check for zero angles, not sure if it is a great fix
                    if vec_A.length != 0 and vec_B.length != 0:
                        angle = vec_A.angle(vec_B) / pi
                        edge_new_length = (Vector(verts_middle_position_co) - sp[1][i]).length
                    else:
                        angle = 0
                        edge_new_length = 0

                    # If after moving the verts to the middle point, the segment doesn't stretch too much
                    if edge_new_length <= loop_segment_dist * 1.5 * \
                       self.join_stretch_factor and angle < 0.25 * self.join_stretch_factor:

                        # Avoid joining when the actual loop must be merged with the original mesh
                        if not (self.selection_U_exists and i == 0) and \
                           not (self.selection_U2_exists and i == len(surface_splines_parsed[0]) - 1):

                            # Change the coords of both verts to the middle position
                            surface_splines_parsed[0][i] = verts_middle_position_co
                            surface_splines_parsed[len(surface_splines_parsed) - 1][i] = verts_middle_position_co

        # Delete object with control points and object from grease pencil convertion
        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[ob_ctrl_pts.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[ob_ctrl_pts.name]

        bpy.ops.object.delete()

        for sp_ob in splines_U_objects:
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[sp_ob.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[sp_ob.name]

            bpy.ops.object.delete()

        # Generate surface

        # Get all verts coords
        all_surface_verts_co = []
        for i in range(0, len(surface_splines_parsed)):
            # Get coords of all verts and make a list with them
            for pt_co in surface_splines_parsed[i]:
                all_surface_verts_co.append(pt_co)

        # Define verts for each face
        all_surface_faces = []
        for i in range(0, len(all_surface_verts_co) - len(surface_splines_parsed[0])):
            if ((i + 1) / len(surface_splines_parsed[0]) != int((i + 1) / len(surface_splines_parsed[0]))):
                all_surface_faces.append(
                            [i + 1, i, i + len(surface_splines_parsed[0]),
                            i + len(surface_splines_parsed[0]) + 1]
                            )
        # Build the mesh
        surf_me_name = "SURFSKIO_surface"
        me_surf = bpy.data.meshes.new(surf_me_name)

        me_surf.from_pydata(all_surface_verts_co, [], all_surface_faces)

        me_surf.update()

        ob_surface = bpy.data.objects.new(surf_me_name, me_surf)
        bpy.context.scene.objects.link(ob_surface)

        # Select all the "unselected but participating" verts, from closed selection
        # or double selections with middle-vertex, for later join with remove doubles
        for v_idx in single_unselected_verts:
            self.main_object.data.vertices[v_idx].select = True

        # Join the new mesh to the main object
        ob_surface.select = True
        self.main_object.select = True
        bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

        bpy.ops.object.join('INVOKE_REGION_WIN')

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bpy.ops.mesh.remove_doubles('INVOKE_REGION_WIN', threshold=0.0001)
        bpy.ops.mesh.normals_make_consistent('INVOKE_REGION_WIN', inside=False)
        bpy.ops.mesh.select_all('INVOKE_REGION_WIN', action='DESELECT')

        return{'FINISHED'}

    def execute(self, context):

        bpy.context.user_preferences.edit.use_global_undo = False

        if not self.is_fill_faces:
            bpy.ops.wm.context_set_value(data_path='tool_settings.mesh_select_mode',
                                         value='True, False, False')

            # Build splines from the "last saved splines".
            last_saved_curve = bpy.data.curves.new('SURFSKIO_last_crv', 'CURVE')
            self.main_splines = bpy.data.objects.new('SURFSKIO_last_crv', last_saved_curve)
            bpy.context.scene.objects.link(self.main_splines)

            last_saved_curve.dimensions = "3D"

            for sp in self.last_strokes_splines_coords:
                spline = self.main_splines.data.splines.new('BEZIER')
                # less one because one point is added when the spline is created
                spline.bezier_points.add(len(sp) - 1)
                for p in range(0, len(sp)):
                    spline.bezier_points[p].co = [sp[p][0], sp[p][1], sp[p][2]]

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_splines.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_splines.name]

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='SELECT')
            # Important to make it vector first and then automatic, otherwise the
            # tips handles get too big and distort the shrinkwrap results later
            bpy.ops.curve.handle_type_set(type='VECTOR')
            bpy.ops.curve.handle_type_set('INVOKE_REGION_WIN', type='AUTOMATIC')
            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            self.main_splines.name = "SURFSKIO_temp_strokes"

            if self.is_crosshatch:
                strokes_for_crosshatch = True
                strokes_for_rectangular_surface = False
            else:
                strokes_for_rectangular_surface = True
                strokes_for_crosshatch = False

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_object.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            if strokes_for_rectangular_surface:
                self.rectangular_surface()
            elif strokes_for_crosshatch:
                self.crosshatch_surface_execute()

            # Delete main splines
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_splines.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_splines.name]

            bpy.ops.object.delete()

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_object.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bpy.context.user_preferences.edit.use_global_undo = self.initial_global_undo_state

        return{'FINISHED'}

    def invoke(self, context, event):
        self.initial_global_undo_state = bpy.context.user_preferences.edit.use_global_undo

        self.main_object = bpy.context.scene.objects.active
        self.main_object_selected_verts_count = int(self.main_object.data.total_vert_sel)

        bpy.context.user_preferences.edit.use_global_undo = False
        bpy.ops.wm.context_set_value(data_path='tool_settings.mesh_select_mode',
                                     value='True, False, False')

        # Out Edit mode and In again to make sure the actual mesh selections are being taken
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bsurfaces_props = bpy.context.scene.bsurfaces
        self.cyclic_cross = bsurfaces_props.SURFSK_cyclic_cross
        self.cyclic_follow = bsurfaces_props.SURFSK_cyclic_follow
        self.automatic_join = bsurfaces_props.SURFSK_automatic_join
        self.loops_on_strokes = bsurfaces_props.SURFSK_loops_on_strokes
        self.keep_strokes = bsurfaces_props.SURFSK_keep_strokes

        self.edges_U = 5

        if self.loops_on_strokes:
            self.edges_V = 1
        else:
            self.edges_V = 5

        self.is_fill_faces = False
        self.stopping_errors = False
        self.last_strokes_splines_coords = []

        # Determine the type of the strokes
        self.strokes_type = get_strokes_type(self.main_object)

        # Check if it will be used grease pencil strokes or curves
        # If there are strokes to be used
        if self.strokes_type == "GP_STROKES" or self.strokes_type == "EXTERNAL_CURVE":
            if self.strokes_type == "GP_STROKES":
                # Convert grease pencil strokes to curve
                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                bpy.ops.gpencil.convert('INVOKE_REGION_WIN', type='CURVE', use_link_strokes=False)
                # XXX gpencil.convert now keep org object as active/selected, *not* newly created curve!
                # XXX This is far from perfect, but should work in most cases...
                # self.original_curve = bpy.context.object
                for ob in bpy.context.selected_objects:
                    if ob != bpy.context.scene.objects.active and ob.name.startswith("GP_Layer"):
                        self.original_curve = ob
                self.using_external_curves = False
            elif self.strokes_type == "EXTERNAL_CURVE":
                for ob in bpy.context.selected_objects:
                    if ob != bpy.context.scene.objects.active:
                        self.original_curve = ob
                self.using_external_curves = True

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Make sure there are no objects left from erroneous
            # executions of this operator, with the reserved names used here
            for o in bpy.data.objects:
                if o.name.find("SURFSKIO_") != -1:
                    bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                    bpy.data.objects[o.name].select = True
                    bpy.context.scene.objects.active = bpy.data.objects[o.name]

                    bpy.ops.object.delete()

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.original_curve.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.original_curve.name]

            bpy.ops.object.duplicate('INVOKE_REGION_WIN')

            self.temporary_curve = bpy.context.scene.objects.active

            # Deselect all points of the curve
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Delete splines with only a single isolated point
            for i in range(len(self.temporary_curve.data.splines)):
                sp = self.temporary_curve.data.splines[i]

                if len(sp.bezier_points) == 1:
                    sp.bezier_points[0].select_control_point = True

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.delete(type='VERT')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.temporary_curve.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.temporary_curve.name]

            # Set a minimum number of points for crosshatch
            minimum_points_num = 15

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            # Check if the number of points of each curve has at least the number of points
            # of minimum_points_num, which is a bit more than the face-loops limit.
            # If not, subdivide to reach at least that number of ponts
            for i in range(len(self.temporary_curve.data.splines)):
                sp = self.temporary_curve.data.splines[i]

                if len(sp.bezier_points) < minimum_points_num:
                    for bp in sp.bezier_points:
                        bp.select_control_point = True

                    if (len(sp.bezier_points) - 1) != 0:
                        # Formula to get the number of cuts that will make a curve
                        # of N number of points have near to "minimum_points_num"
                        # points, when subdividing with this number of cuts
                        subdivide_cuts = int(
                                    (minimum_points_num - len(sp.bezier_points)) /
                                    (len(sp.bezier_points) - 1)
                                    ) + 1
                    else:
                        subdivide_cuts = 0

                    bpy.ops.curve.subdivide('INVOKE_REGION_WIN', number_cuts=subdivide_cuts)
                    bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Detect if the strokes are a crosshatch and do it if it is
            self.crosshatch_surface_invoke(self.temporary_curve)

            if not self.is_crosshatch:
                bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.data.objects[self.temporary_curve.name].select = True
                bpy.context.scene.objects.active = bpy.data.objects[self.temporary_curve.name]

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

                # Set a minimum number of points for rectangular surfaces
                minimum_points_num = 60

                # Check if the number of points of each curve has at least the number of points
                # of minimum_points_num, which is a bit more than the face-loops limit.
                # If not, subdivide to reach at least that number of ponts
                for i in range(len(self.temporary_curve.data.splines)):
                    sp = self.temporary_curve.data.splines[i]

                    if len(sp.bezier_points) < minimum_points_num:
                        for bp in sp.bezier_points:
                            bp.select_control_point = True

                        if (len(sp.bezier_points) - 1) != 0:
                            # Formula to get the number of cuts that will make a curve of
                            # N number of points have near to "minimum_points_num" points,
                            # when subdividing with this number of cuts
                            subdivide_cuts = int(
                                        (minimum_points_num - len(sp.bezier_points)) /
                                        (len(sp.bezier_points) - 1)
                                        ) + 1
                        else:
                            subdivide_cuts = 0

                        bpy.ops.curve.subdivide('INVOKE_REGION_WIN', number_cuts=subdivide_cuts)
                        bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Save coordinates of the actual strokes (as the "last saved splines")
            for sp_idx in range(len(self.temporary_curve.data.splines)):
                self.last_strokes_splines_coords.append([])
                for bp_idx in range(len(self.temporary_curve.data.splines[sp_idx].bezier_points)):
                    coords = self.temporary_curve.matrix_world * \
                             self.temporary_curve.data.splines[sp_idx].bezier_points[bp_idx].co
                    self.last_strokes_splines_coords[sp_idx].append([coords[0], coords[1], coords[2]])

            # Check for cyclic splines, put the first and last points in the middle of their actual positions
            for sp_idx in range(len(self.temporary_curve.data.splines)):
                if self.temporary_curve.data.splines[sp_idx].use_cyclic_u is True:
                    first_p_co = self.last_strokes_splines_coords[sp_idx][0]
                    last_p_co = self.last_strokes_splines_coords[sp_idx][
                                                            len(self.last_strokes_splines_coords[sp_idx]) - 1
                                                            ]
                    target_co = [
                            (first_p_co[0] + last_p_co[0]) / 2,
                            (first_p_co[1] + last_p_co[1]) / 2,
                            (first_p_co[2] + last_p_co[2]) / 2
                            ]

                    self.last_strokes_splines_coords[sp_idx][0] = target_co
                    self.last_strokes_splines_coords[sp_idx][
                                                            len(self.last_strokes_splines_coords[sp_idx]) - 1
                                                            ] = target_co
            tuple(self.last_strokes_splines_coords)

            # Estimation of the average length of the segments between
            # each point of the grease pencil strokes.
            # Will be useful to determine whether a curve should be made "Cyclic"
            segments_lengths_sum = 0
            segments_count = 0
            random_spline = self.temporary_curve.data.splines[0].bezier_points
            for i in range(0, len(random_spline)):
                if i != 0 and len(random_spline) - 1 >= i:
                    segments_lengths_sum += (random_spline[i - 1].co - random_spline[i].co).length
                    segments_count += 1

            self.average_gp_segment_length = segments_lengths_sum / segments_count

            # Delete temporary strokes curve object
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.temporary_curve.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.temporary_curve.name]

            bpy.ops.object.delete()

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_object.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            self.execute(context)
            # Set again since "execute()" will turn it again to its initial value
            bpy.context.user_preferences.edit.use_global_undo = False

            # If "Keep strokes" option is not active, delete original strokes curve object
            if (not self.stopping_errors and not self.keep_strokes) or self.is_crosshatch:
                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.data.objects[self.original_curve.name].select = True
                bpy.context.scene.objects.active = bpy.data.objects[self.original_curve.name]

                bpy.ops.object.delete()

                bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
                bpy.data.objects[self.main_object.name].select = True
                bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

                bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            # Delete grease pencil strokes
            if self.strokes_type == "GP_STROKES" and not self.stopping_errors:
                bpy.ops.gpencil.active_frame_delete('INVOKE_REGION_WIN')

            bpy.context.user_preferences.edit.use_global_undo = self.initial_global_undo_state

            if not self.stopping_errors:
                return {"FINISHED"}
            else:
                return{"CANCELLED"}

        elif self.strokes_type == "SELECTION_ALONE":
            self.is_fill_faces = True
            created_faces_count = self.fill_with_faces(self.main_object)

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.context.user_preferences.edit.use_global_undo = self.initial_global_undo_state

            if created_faces_count == 0:
                self.report({'WARNING'}, "There aren't any strokes attatched to the object")
                return {"CANCELLED"}
            else:
                return {"FINISHED"}

        bpy.context.user_preferences.edit.use_global_undo = self.initial_global_undo_state

        if self.strokes_type == "EXTERNAL_NO_CURVE":
            self.report({'WARNING'}, "The secondary object is not a Curve.")
            return{"CANCELLED"}

        elif self.strokes_type == "MORE_THAN_ONE_EXTERNAL":
            self.report({'WARNING'}, "There shouldn't be more than one secondary object selected.")
            return{"CANCELLED"}

        elif self.strokes_type == "SINGLE_GP_STROKE_NO_SELECTION" or \
             self.strokes_type == "SINGLE_CURVE_STROKE_NO_SELECTION":

            self.report({'WARNING'}, "It's needed at least one stroke and one selection, or two strokes.")
            return{"CANCELLED"}

        elif self.strokes_type == "NO_STROKES":
            self.report({'WARNING'}, "There aren't any strokes attatched to the object")
            return{"CANCELLED"}

        elif self.strokes_type == "CURVE_WITH_NON_BEZIER_SPLINES":
            self.report({'WARNING'}, "All splines must be Bezier.")
            return{"CANCELLED"}

        else:
            return{"CANCELLED"}


# Edit strokes operator
class GPENCIL_OT_SURFSK_edit_strokes(Operator):
    bl_idname = "gpencil.surfsk_edit_strokes"
    bl_label = "Bsurfaces edit strokes"
    bl_description = "Edit the grease pencil strokes or curves used"

    def execute(self, context):
        # Determine the type of the strokes
        self.strokes_type = get_strokes_type(self.main_object)
        # Check if strokes are grease pencil strokes or a curves object
        selected_objs = bpy.context.selected_objects
        if self.strokes_type == "EXTERNAL_CURVE" or self.strokes_type == "SINGLE_CURVE_STROKE_NO_SELECTION":
            for ob in selected_objs:
                if ob != bpy.context.scene.objects.active:
                    curve_ob = ob

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[curve_ob.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[curve_ob.name]

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        elif self.strokes_type == "GP_STROKES" or self.strokes_type == "SINGLE_GP_STROKE_NO_SELECTION":
            # Convert grease pencil strokes to curve
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.gpencil.convert('INVOKE_REGION_WIN', type='CURVE', use_link_strokes=False)
            for ob in bpy.context.selected_objects:
                    if ob != bpy.context.scene.objects.active and ob.name.startswith("GP_Layer"):
                        ob_gp_strokes = ob

            # ob_gp_strokes = bpy.context.object

            # Delete grease pencil strokes
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[self.main_object.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[self.main_object.name]

            bpy.ops.gpencil.active_frame_delete('INVOKE_REGION_WIN')

            # Clean up curves
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[ob_gp_strokes.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[ob_gp_strokes.name]

            curve_crv = ob_gp_strokes.data
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.spline_type_set('INVOKE_REGION_WIN', type="BEZIER")
            bpy.ops.curve.handle_type_set('INVOKE_REGION_WIN', type="AUTOMATIC")
            bpy.data.curves[curve_crv.name].show_handles = False
            bpy.data.curves[curve_crv.name].show_normal_face = False

        elif self.strokes_type == "EXTERNAL_NO_CURVE":
            self.report({'WARNING'}, "The secondary object is not a Curve.")
            return{"CANCELLED"}

        elif self.strokes_type == "MORE_THAN_ONE_EXTERNAL":
            self.report({'WARNING'}, "There shouldn't be more than one secondary object selected.")
            return{"CANCELLED"}

        elif self.strokes_type == "NO_STROKES" or self.strokes_type == "SELECTION_ALONE":
            self.report({'WARNING'}, "There aren't any strokes attatched to the object")
            return{"CANCELLED"}

        else:
            return{"CANCELLED"}

    def invoke(self, context, event):
        self.main_object = bpy.context.object
        self.execute(context)

        return {"FINISHED"}


class CURVE_OT_SURFSK_reorder_splines(Operator):
    bl_idname = "curve.surfsk_reorder_splines"
    bl_label = "Bsurfaces reorder splines"
    bl_description = "Defines the order of the splines by using grease pencil strokes"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        objects_to_delete = []
        # Convert grease pencil strokes to curve.
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.gpencil.convert('INVOKE_REGION_WIN', type='CURVE', use_link_strokes=False)
        for ob in bpy.context.selected_objects:
            if ob != bpy.context.scene.objects.active and ob.name.startswith("GP_Layer"):
                GP_strokes_curve = ob

        # GP_strokes_curve = bpy.context.object
        objects_to_delete.append(GP_strokes_curve)

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[GP_strokes_curve.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[GP_strokes_curve.name]

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='SELECT')
        bpy.ops.curve.subdivide('INVOKE_REGION_WIN', number_cuts=100)
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bpy.ops.object.duplicate('INVOKE_REGION_WIN')
        GP_strokes_mesh = bpy.context.object
        objects_to_delete.append(GP_strokes_mesh)

        GP_strokes_mesh.data.resolution_u = 1
        bpy.ops.object.convert(target='MESH', keep_original=False)

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[self.main_curve.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[self.main_curve.name]

        bpy.ops.object.duplicate('INVOKE_REGION_WIN')
        curves_duplicate_1 = bpy.context.object
        objects_to_delete.append(curves_duplicate_1)

        minimum_points_num = 500

        # Some iterations since the subdivision operator
        # has a limit of 100 subdivisions per iteration
        for x in range(round(minimum_points_num / 100)):
            # Check if the number of points of each curve has at least the number of points
            # of minimum_points_num. If not, subdivide to reach at least that number of ponts
            for i in range(len(curves_duplicate_1.data.splines)):
                sp = curves_duplicate_1.data.splines[i]

                if len(sp.bezier_points) < minimum_points_num:
                    for bp in sp.bezier_points:
                        bp.select_control_point = True

                    if (len(sp.bezier_points) - 1) != 0:
                        # Formula to get the number of cuts that will make a curve of N
                        # number of points have near to "minimum_points_num" points,
                        # when subdividing with this number of cuts
                        subdivide_cuts = int(
                                (minimum_points_num - len(sp.bezier_points)) /
                                (len(sp.bezier_points) - 1)
                                ) + 1
                    else:
                        subdivide_cuts = 0

                    bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
                    bpy.ops.curve.subdivide('INVOKE_REGION_WIN', number_cuts=subdivide_cuts)
                    bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
                    bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        bpy.ops.object.duplicate('INVOKE_REGION_WIN')
        curves_duplicate_2 = bpy.context.object
        objects_to_delete.append(curves_duplicate_2)

        # Duplicate the duplicate and add Shrinkwrap to it, with the grease pencil strokes curve as target
        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[curves_duplicate_2.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[curves_duplicate_2.name]

        bpy.ops.object.modifier_add('INVOKE_REGION_WIN', type='SHRINKWRAP')
        curves_duplicate_2.modifiers["Shrinkwrap"].wrap_method = "NEAREST_VERTEX"
        curves_duplicate_2.modifiers["Shrinkwrap"].target = GP_strokes_mesh
        bpy.ops.object.modifier_apply('INVOKE_REGION_WIN', apply_as='DATA', modifier='Shrinkwrap')

        # Get the distance of each vert from its original position to its position with Shrinkwrap
        nearest_points_coords = {}
        for st_idx in range(len(curves_duplicate_1.data.splines)):
            for bp_idx in range(len(curves_duplicate_1.data.splines[st_idx].bezier_points)):
                bp_1_co = curves_duplicate_1.matrix_world * \
                          curves_duplicate_1.data.splines[st_idx].bezier_points[bp_idx].co

                bp_2_co = curves_duplicate_2.matrix_world * \
                          curves_duplicate_2.data.splines[st_idx].bezier_points[bp_idx].co

                if bp_idx == 0:
                    shortest_dist = (bp_1_co - bp_2_co).length
                    nearest_points_coords[st_idx] = ("%.4f" % bp_2_co[0],
                                                     "%.4f" % bp_2_co[1],
                                                     "%.4f" % bp_2_co[2])

                dist = (bp_1_co - bp_2_co).length

                if dist < shortest_dist:
                    nearest_points_coords[st_idx] = ("%.4f" % bp_2_co[0],
                                                     "%.4f" % bp_2_co[1],
                                                     "%.4f" % bp_2_co[2])
                    shortest_dist = dist

        # Get all coords of GP strokes points, for comparison
        GP_strokes_coords = []
        for st_idx in range(len(GP_strokes_curve.data.splines)):
            GP_strokes_coords.append(
                    [("%.4f" % x if "%.4f" % x != "-0.00" else "0.00",
                    "%.4f" % y if "%.4f" % y != "-0.00" else "0.00",
                    "%.4f" % z if "%.4f" % z != "-0.00" else "0.00") for
                    x, y, z in [bp.co for bp in GP_strokes_curve.data.splines[st_idx].bezier_points]]
                    )

        # Check the point of the GP strokes with the same coords as
        # the nearest points of the curves (with shrinkwrap)

        # Dictionary with GP stroke index as index, and a list as value.
        # The list has as index the point index of the GP stroke
        # nearest to the spline, and as value the spline index
        GP_connection_points = {}
        for gp_st_idx in range(len(GP_strokes_coords)):
            GPvert_spline_relationship = {}

            for splines_st_idx in range(len(nearest_points_coords)):
                if nearest_points_coords[splines_st_idx] in GP_strokes_coords[gp_st_idx]:
                    GPvert_spline_relationship[
                        GP_strokes_coords[gp_st_idx].index(nearest_points_coords[splines_st_idx])
                        ] = splines_st_idx

            GP_connection_points[gp_st_idx] = GPvert_spline_relationship

        # Get the splines new order
        splines_new_order = []
        for i in GP_connection_points:
            dict_keys = sorted(GP_connection_points[i].keys())  # Sort dictionaries by key

            for k in dict_keys:
                splines_new_order.append(GP_connection_points[i][k])

        # Reorder
        curve_original_name = self.main_curve.name

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[self.main_curve.name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[self.main_curve.name]

        self.main_curve.name = "SURFSKIO_CRV_ORD"

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        for sp_idx in range(len(self.main_curve.data.splines)):
            self.main_curve.data.splines[0].bezier_points[0].select_control_point = True

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            bpy.ops.curve.separate('EXEC_REGION_WIN')
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        # Get the names of the separated splines objects in the original order
        splines_unordered = {}
        for o in bpy.data.objects:
            if o.name.find("SURFSKIO_CRV_ORD") != -1:
                spline_order_string = o.name.partition(".")[2]

                if spline_order_string != "" and int(spline_order_string) > 0:
                    spline_order_index = int(spline_order_string) - 1
                    splines_unordered[spline_order_index] = o.name

        # Join all splines objects in final order
        for order_idx in splines_new_order:
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[splines_unordered[order_idx]].select = True
            bpy.data.objects["SURFSKIO_CRV_ORD"].select = True
            bpy.context.scene.objects.active = bpy.data.objects["SURFSKIO_CRV_ORD"]

            bpy.ops.object.join('INVOKE_REGION_WIN')

        # Go back to the original name of the curves object.
        bpy.context.object.name = curve_original_name

        # Delete all unused objects
        for o in objects_to_delete:
            bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
            bpy.data.objects[o.name].select = True
            bpy.context.scene.objects.active = bpy.data.objects[o.name]

            bpy.ops.object.delete()

        bpy.ops.object.select_all('INVOKE_REGION_WIN', action='DESELECT')
        bpy.data.objects[curve_original_name].select = True
        bpy.context.scene.objects.active = bpy.data.objects[curve_original_name]

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')

        bpy.ops.gpencil.active_frame_delete('INVOKE_REGION_WIN')

        return {"FINISHED"}

    def invoke(self, context, event):
        self.main_curve = bpy.context.object
        there_are_GP_strokes = False

        try:
            # Get the active grease pencil layer
            strokes_num = len(self.main_curve.grease_pencil.layers.active.active_frame.strokes)

            if strokes_num > 0:
                there_are_GP_strokes = True
        except:
            pass

        if there_are_GP_strokes:
            self.execute(context)
            self.report({'INFO'}, "Splines have been reordered")
        else:
            self.report({'WARNING'}, "Draw grease pencil strokes to connect splines")

        return {"FINISHED"}


class CURVE_OT_SURFSK_first_points(Operator):
    bl_idname = "curve.surfsk_first_points"
    bl_label = "Bsurfaces set first points"
    bl_description = "Set the selected points as the first point of each spline"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        splines_to_invert = []

        # Check non-cyclic splines to invert
        for i in range(len(self.main_curve.data.splines)):
            b_points = self.main_curve.data.splines[i].bezier_points

            if i not in self.cyclic_splines:  # Only for non-cyclic splines
                if b_points[len(b_points) - 1].select_control_point:
                    splines_to_invert.append(i)

        # Reorder points of cyclic splines, and set all handles to "Automatic"

        # Check first selected point
        cyclic_splines_new_first_pt = {}
        for i in self.cyclic_splines:
            sp = self.main_curve.data.splines[i]

            for t in range(len(sp.bezier_points)):
                bp = sp.bezier_points[t]
                if bp.select_control_point or bp.select_right_handle or bp.select_left_handle:
                    cyclic_splines_new_first_pt[i] = t
                    break  # To take only one if there are more

        # Reorder
        for spline_idx in cyclic_splines_new_first_pt:
            sp = self.main_curve.data.splines[spline_idx]

            spline_old_coords = []
            for bp_old in sp.bezier_points:
                coords = (bp_old.co[0], bp_old.co[1], bp_old.co[2])

                left_handle_type = str(bp_old.handle_left_type)
                left_handle_length = float(bp_old.handle_left.length)
                left_handle_xyz = (
                        float(bp_old.handle_left.x),
                        float(bp_old.handle_left.y),
                        float(bp_old.handle_left.z)
                        )
                right_handle_type = str(bp_old.handle_right_type)
                right_handle_length = float(bp_old.handle_right.length)
                right_handle_xyz = (
                        float(bp_old.handle_right.x),
                        float(bp_old.handle_right.y),
                        float(bp_old.handle_right.z)
                        )
                spline_old_coords.append(
                        [coords, left_handle_type,
                        right_handle_type, left_handle_length,
                        right_handle_length, left_handle_xyz,
                        right_handle_xyz]
                        )

            for t in range(len(sp.bezier_points)):
                bp = sp.bezier_points

                if t + cyclic_splines_new_first_pt[spline_idx] + 1 <= len(bp) - 1:
                    new_index = t + cyclic_splines_new_first_pt[spline_idx] + 1
                else:
                    new_index = t + cyclic_splines_new_first_pt[spline_idx] + 1 - len(bp)

                bp[t].co = Vector(spline_old_coords[new_index][0])

                bp[t].handle_left.length = spline_old_coords[new_index][3]
                bp[t].handle_right.length = spline_old_coords[new_index][4]

                bp[t].handle_left_type = "FREE"
                bp[t].handle_right_type = "FREE"

                bp[t].handle_left.x = spline_old_coords[new_index][5][0]
                bp[t].handle_left.y = spline_old_coords[new_index][5][1]
                bp[t].handle_left.z = spline_old_coords[new_index][5][2]

                bp[t].handle_right.x = spline_old_coords[new_index][6][0]
                bp[t].handle_right.y = spline_old_coords[new_index][6][1]
                bp[t].handle_right.z = spline_old_coords[new_index][6][2]

                bp[t].handle_left_type = spline_old_coords[new_index][1]
                bp[t].handle_right_type = spline_old_coords[new_index][2]

        # Invert the non-cyclic splines designated above
        for i in range(len(splines_to_invert)):
            bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')

            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
            self.main_curve.data.splines[splines_to_invert[i]].bezier_points[0].select_control_point = True
            bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

            bpy.ops.curve.switch_direction()

        bpy.ops.curve.select_all('INVOKE_REGION_WIN', action='DESELECT')

        # Keep selected the first vert of each spline
        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')
        for i in range(len(self.main_curve.data.splines)):
            if not self.main_curve.data.splines[i].use_cyclic_u:
                bp = self.main_curve.data.splines[i].bezier_points[0]
            else:
                bp = self.main_curve.data.splines[i].bezier_points[
                                                        len(self.main_curve.data.splines[i].bezier_points) - 1
                                                        ]

            bp.select_control_point = True
            bp.select_right_handle = True
            bp.select_left_handle = True

        bpy.ops.object.editmode_toggle('INVOKE_REGION_WIN')

        return {'FINISHED'}

    def invoke(self, context, event):
        self.main_curve = bpy.context.object

        # Check if all curves are Bezier, and detect which ones are cyclic
        self.cyclic_splines = []
        for i in range(len(self.main_curve.data.splines)):
            if self.main_curve.data.splines[i].type != "BEZIER":
                self.report({'WARNING'}, "All splines must be Bezier type")

                return {'CANCELLED'}
            else:
                if self.main_curve.data.splines[i].use_cyclic_u:
                    self.cyclic_splines.append(i)

        self.execute(context)
        self.report({'INFO'}, "First points have been set")

        return {'FINISHED'}


# Add-ons Preferences Update Panel

# Define Panel classes for updating
panels = (
        VIEW3D_PT_tools_SURFSK_mesh,
        VIEW3D_PT_tools_SURFSK_curve,
        )


def update_panel(self, context):
    message = "Bsurfaces GPL Edition: Updating Panel locations has failed"
    try:
        for panel in panels:
            if "bl_rna" in panel.__dict__:
                bpy.utils.unregister_class(panel)

        for panel in panels:
            panel.bl_category = context.user_preferences.addons[__name__].preferences.category
            bpy.utils.register_class(panel)

    except Exception as e:
        print("\n[{}]\n{}\n\nError:\n{}".format(__name__, message, e))
        pass


class BsurfPreferences(AddonPreferences):
    # this must match the addon name, use '__package__'
    # when defining this in a submodule of a python package.
    bl_idname = __name__

    category = StringProperty(
            name="Tab Category",
            description="Choose a name for the category of the panel",
            default="Tools",
            update=update_panel
            )

    def draw(self, context):
        layout = self.layout

        row = layout.row()
        col = row.column()
        col.label(text="Tab Category:")
        col.prop(self, "category", text="")


# Properties
class BsurfacesProps(PropertyGroup):
    SURFSK_cyclic_cross = BoolProperty(
                name="Cyclic Cross",
                description="Make cyclic the face-loops crossing the strokes",
                default=False
                )
    SURFSK_cyclic_follow = BoolProperty(
                name="Cyclic Follow",
                description="Make cyclic the face-loops following the strokes",
                default=False
                )
    SURFSK_keep_strokes = BoolProperty(
                name="Keep strokes",
                description="Keeps the sketched strokes or curves after adding the surface",
                default=False
                )
    SURFSK_automatic_join = BoolProperty(
                name="Automatic join",
                description="Join automatically vertices of either surfaces "
                            "generated by crosshatching, or from the borders of closed shapes",
                default=True
                )
    SURFSK_loops_on_strokes = BoolProperty(
                name="Loops on strokes",
                description="Make the loops match the paths of the strokes",
                default=True
                )
    SURFSK_precision = IntProperty(
                name="Precision",
                description="Precision level of the surface calculation",
                default=2,
                min=1,
                max=100
                )


classes = (
    VIEW3D_PT_tools_SURFSK_mesh,
    VIEW3D_PT_tools_SURFSK_curve,
    GPENCIL_OT_SURFSK_add_surface,
    GPENCIL_OT_SURFSK_edit_strokes,
    CURVE_OT_SURFSK_reorder_splines,
    CURVE_OT_SURFSK_first_points,
    BsurfPreferences,
    BsurfacesProps,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Scene.bsurfaces = PointerProperty(type=BsurfacesProps)
    update_panel(None, bpy.context)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    del bpy.types.Scene.bsurfaces


if __name__ == "__main__":
    register()
