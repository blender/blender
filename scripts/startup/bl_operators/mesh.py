# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator

from bpy.props import (
    EnumProperty,
    IntProperty,
)
from bpy.app.translations import pgettext_rpt as rpt_


class MeshMirrorUV(Operator):
    """Copy mirror UV coordinates on the X axis based on a mirrored mesh"""
    bl_idname = "mesh.faces_mirror_uv"
    bl_label = "Copy Mirrored UV Coords"
    bl_options = {'REGISTER', 'UNDO'}

    direction: EnumProperty(
        name="Axis Direction",
        items=(
            ('POSITIVE', "Positive", ""),
            ('NEGATIVE', "Negative", ""),
        ),
    )

    precision: IntProperty(
        name="Precision",
        description=("Tolerance for finding vertex duplicates"),
        min=1, max=16,
        soft_min=1, soft_max=16,
        default=3,
    )

    # Returns has_active_UV_layer, double_warn.
    def do_mesh_mirror_UV(self, mesh, DIR):
        precision = self.precision
        double_warn = 0

        if not mesh.uv_layers.active:
            # has_active_UV_layer, double_warn
            return False, 0

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
            # Vert index of the poly.
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
            if DIR == 0 and pcents[i][0] < 0.0:
                continue
            if DIR == 1 and pcents[i][0] > 0.0:
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

        # has_active_UV_layer, double_warn
        return True, double_warn

    @classmethod
    def poll(cls, context):
        obj = context.view_layer.objects.active
        return (obj and obj.type == 'MESH')

    def execute(self, context):
        DIR = (self.direction == 'NEGATIVE')

        total_no_active_UV = 0
        total_duplicates = 0
        meshes_with_duplicates = 0

        ob = context.view_layer.objects.active
        is_editmode = (ob.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        meshes = [
            ob.data for ob in context.view_layer.objects.selected
            if ob.type == 'MESH' and ob.data.library is None
        ]

        for mesh in meshes:
            mesh.tag = False

        for mesh in meshes:
            if mesh.tag:
                continue

            mesh.tag = True

            has_active_UV_layer, double_warn = self.do_mesh_mirror_UV(mesh, DIR)

            if not has_active_UV_layer:
                total_no_active_UV = total_no_active_UV + 1

            elif double_warn:
                total_duplicates += double_warn
                meshes_with_duplicates = meshes_with_duplicates + 1

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        if total_duplicates and total_no_active_UV:
            self.report(
                {'WARNING'},
                rpt_(
                    "{:d} mesh(es) with no active UV layer, "
                    "{:d} duplicates found in {:d} mesh(es), mirror may be incomplete"
                ).format(total_no_active_UV, total_duplicates, meshes_with_duplicates),
            )
        elif total_no_active_UV:
            self.report(
                {'WARNING'},
                rpt_("{:d} mesh(es) with no active UV layer").format(total_no_active_UV),
            )
        elif total_duplicates:
            self.report(
                {'WARNING'},
                rpt_(
                    "{:d} duplicates found in {:d} mesh(es), mirror may be incomplete"
                ).format(total_duplicates, meshes_with_duplicates),
            )

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
            bmesh.update_edit_mesh(me, loop_triangles=False)

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
            bmesh.update_edit_mesh(me, loop_triangles=False)

        return {'FINISHED'}


classes = (
    MeshMirrorUV,
    MeshSelectNext,
    MeshSelectPrev,
)
