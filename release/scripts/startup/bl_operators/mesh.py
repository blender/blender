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

import bpy
from bpy.types import Operator

from bpy.props import (
    EnumProperty,
    IntProperty,
)


class MeshMirrorUV(Operator):
    """Copy mirror UV coordinates on the X axis based on a mirrored mesh"""
    bl_idname = "mesh.faces_mirror_uv"
    bl_label = "Copy Mirrored UV coords"
    bl_options = {'REGISTER', 'UNDO'}

    direction = EnumProperty(
            name="Axis Direction",
            items=(('POSITIVE', "Positive", ""),
                   ('NEGATIVE', "Negative", "")),
            )

    precision = IntProperty(
            name="Precision",
            description=("Tolerance for finding vertex duplicates"),
            min=1, max=16,
            soft_min=1, soft_max=16,
            default=3,
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and obj.data.uv_textures.active)

    def execute(self, context):
        DIR = (self.direction == 'NEGATIVE')
        precision = self.precision
        double_warn = 0

        ob = context.active_object
        is_editmode = (ob.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        mesh = ob.data

        # mirror lookups
        mirror_gt = {}
        mirror_lt = {}

        vcos = (v.co.to_tuple(precision) for v in mesh.vertices)

        for i, co in enumerate(vcos):
            if co[0] >= 0.0:
                double_warn += co in mirror_gt
                mirror_gt[co] = i
            if co[0] <= 0.0:
                double_warn += co in mirror_lt
                mirror_lt[co] = i

        vmap = {}
        for mirror_a, mirror_b in ((mirror_gt, mirror_lt),
                                   (mirror_lt, mirror_gt)):
            for co, i in mirror_a.items():
                nco = (-co[0], co[1], co[2])
                j = mirror_b.get(nco)
                if j is not None:
                    vmap[i] = j

        polys = mesh.polygons
        loops = mesh.loops
        uv_loops = mesh.uv_layers.active.data
        nbr_polys = len(polys)

        mirror_pm = {}
        pmap = {}
        puvs = [None] * nbr_polys
        puvs_cpy = [None] * nbr_polys
        puvsel = [None] * nbr_polys
        pcents = [None] * nbr_polys
        vidxs = [None] * nbr_polys
        for i, p in enumerate(polys):
            lstart = lend = p.loop_start
            lend += p.loop_total
            puvs[i] = tuple(uv.uv for uv in uv_loops[lstart:lend])
            puvs_cpy[i] = tuple(uv.copy() for uv in puvs[i])
            puvsel[i] = (False not in
                         (uv.select for uv in uv_loops[lstart:lend]))
            # Vert idx of the poly.
            vidxs[i] = tuple(l.vertex_index for l in loops[lstart:lend])
            pcents[i] = p.center
            # Preparing next step finding matching polys.
            mirror_pm[tuple(sorted(vidxs[i]))] = i

        for i in range(nbr_polys):
            # Find matching mirror poly.
            tvidxs = [vmap.get(j) for j in vidxs[i]]
            if None not in tvidxs:
                tvidxs.sort()
                j = mirror_pm.get(tuple(tvidxs))
                if j is not None:
                    pmap[i] = j

        for i, j in pmap.items():
            if not puvsel[i] or not puvsel[j]:
                continue
            elif DIR == 0 and pcents[i][0] < 0.0:
                continue
            elif DIR == 1 and pcents[i][0] > 0.0:
                continue

            # copy UVs
            uv1 = puvs[i]
            uv2 = puvs_cpy[j]

            # get the correct rotation
            v1 = vidxs[j]
            v2 = tuple(vmap[k] for k in vidxs[i])

            if len(v1) == len(v2):
                for k in range(len(v1)):
                    k_map = v1.index(v2[k])
                    uv1[k].xy = - (uv2[k_map].x - 0.5) + 0.5, uv2[k_map].y

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        if double_warn:
            self.report({'WARNING'},
                        "%d duplicates found, mirror may be incomplete" %
                        double_warn)

        return {'FINISHED'}


class MeshSelectNext(Operator):
    """Select the next element (using selection order)"""
    bl_idname = "mesh.select_next_item"
    bl_label = "Select Next Element"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.mode == 'EDIT_MESH')

    def execute(self, context):
        import bmesh
        from .bmesh import find_adjacent

        obj = context.active_object
        me = obj.data
        bm = bmesh.from_edit_mesh(me)

        if find_adjacent.select_next(bm, self.report):
            bm.select_flush_mode()
            bmesh.update_edit_mesh(me, False)

        return {'FINISHED'}


class MeshSelectPrev(Operator):
    """Select the previous element (using selection order)"""
    bl_idname = "mesh.select_prev_item"
    bl_label = "Select Previous Element"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.mode == 'EDIT_MESH')

    def execute(self, context):
        import bmesh
        from .bmesh import find_adjacent

        obj = context.active_object
        me = obj.data
        bm = bmesh.from_edit_mesh(me)

        if find_adjacent.select_prev(bm, self.report):
            bm.select_flush_mode()
            bmesh.update_edit_mesh(me, False)

        return {'FINISHED'}


# XXX This is hackish (going forth and back from Object mode...), to be redone once we have proper support of
#     custom normals in BMesh/edit mode.
class MehsSetNormalsFromFaces(Operator):
    """Set the custom vertex normals from the selected faces ones"""
    bl_idname = "mesh.set_normals_from_faces"
    bl_label = "Set Normals From Faces"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.mode == 'EDIT_MESH' and context.edit_object.data.polygons)

    def execute(self, context):
        import mathutils

        bpy.ops.object.mode_set(mode='OBJECT')
        obj = context.active_object
        me = obj.data

        v2nors = {}
        for p in me.polygons:
            if not p.select:
                continue
            for lidx, vidx in zip(p.loop_indices, p.vertices):
                assert(me.loops[lidx].vertex_index == vidx)
                v2nors.setdefault(vidx, []).append(p.normal)

        for nors in v2nors.values():
            nors[:] = [sum(nors, mathutils.Vector((0, 0, 0))).normalized()]

        if not me.has_custom_normals:
            me.create_normals_split()
        me.calc_normals_split()

        normals = []
        for l in me.loops:
            nor = v2nors.get(l.vertex_index, [None])[0]
            if nor is None:
                nor = l.normal
            normals.append(nor.to_tuple())

        me.normals_split_custom_set(normals)

        me.free_normals_split()
        bpy.ops.object.mode_set(mode='EDIT')

        return {'FINISHED'}


classes = (
    MehsSetNormalsFromFaces,
    MeshMirrorUV,
    MeshSelectNext,
    MeshSelectPrev,
)
