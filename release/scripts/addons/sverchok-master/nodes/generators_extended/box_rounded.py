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

from math import copysign, pi, sqrt

import bpy
from bpy.props import IntProperty, FloatProperty, FloatVectorProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, fullList


#
# round_cube function taken from:
# add_mesh_round_cube.py Copyright (c) 2014, Alain Ducharme
# / with slight modifications for Sverchok.
#

def round_cube(radius=1.0, arcdiv=4, lindiv=0., size=(0., 0., 0.), div_type=0, odd_axis_align=0):

    odd_axis_align = bool(odd_axis_align)

    # subdiv bitmasks
    CORNERS, EDGES, ALL = 0, 1, 2
    subdiv = CORNERS

    if not (div_type in {0, 1, 2}):
        subdiv = CORNERS  # fallback
    else:
        subdiv = div_type

    radius = max(radius, 0.)
    if not radius:
        # No sphere
        arcdiv = 1
        odd_axis_align = False

    if arcdiv <= 0:
        arcdiv = max(round(pi * radius * lindiv * 0.5), 1)
    arcdiv = max(round(arcdiv), 1)
    if lindiv <= 0. and radius:
        lindiv = 1. / (pi / (arcdiv * 2.) * radius)
    lindiv = max(lindiv, 0.)
    if not lindiv:
        subdiv = CORNERS

    odd = arcdiv % 2  # even = arcdiv % 2 ^ 1
    step_size = 2. / arcdiv

    odd_aligned = 0
    vi = -1.
    steps = arcdiv + 1
    if odd_axis_align and odd:
        odd_aligned = 1
        vi += 0.5 * step_size
        steps = arcdiv
    axis_aligned = not odd or odd_aligned

    if arcdiv == 1 and not odd_aligned and subdiv == EDGES:
        subdiv = CORNERS

    half_chord = 0.  # ~ spherical cap base radius
    sagitta = 0.  # ~ spherical cap height
    if not axis_aligned:
        half_chord = sqrt(3.) * radius / (3. * arcdiv)
        id2 = 1. / (arcdiv * arcdiv)
        sagitta = radius - radius * sqrt(id2 * id2 / 3. - id2 + 1.)

    # Extrusion per axis
    exyz = [0. if s < 2. * (radius - sagitta) else (s - 2. * (radius - sagitta))*0.5 for s in size]
    ex, ey, ez = exyz

    dxyz = [0, 0, 0]      # extrusion divisions per axis
    dssxyz = [0., 0., 0.]  # extrusion division step sizes per axis

    for i in range(3):
        sc = 2. * (exyz[i] + half_chord)
        dxyz[i] = round(sc * lindiv) if subdiv else 0
        if dxyz[i]:
            dssxyz[i] = sc / dxyz[i]
            dxyz[i] -= 1
        else:
            dssxyz[i] = sc

    if not radius and not max(size) > 0:
        # Single vertex
        return [(0, 0, 0)], []

    # uv lookup table
    uvlt = []
    v = vi
    for j in range(1, steps + 1):
        v2 = v*v
        uvlt.append((v, v2, radius * sqrt(18. - 6. * v2) / 6.))
        v = vi + j * step_size  # v += step_size # instead of accumulating errors
        # clear fp errors / signs at axis
        if abs(v) < 1e-10:
            v = 0.0

    # Sides built left to right bottom up
    #         xp yp zp  xd  yd  zd
    sides = ((0, 2, 1, (-1,  1,  1)),   # Y+ Front
             (1, 2, 0, (-1, -1,  1)),   # X- Left
             (0, 2, 1, (1, -1,  1)),   # Y- Back
             (1, 2, 0, (1,  1,  1)),   # X+ Right
             (0, 1, 2, (-1,  1, -1)),   # Z- Bottom
             (0, 1, 2, (-1, -1,  1)))   # Z+ Top

    # side vertex index table (for sphere)
    svit = [[[] for i in range(steps)] for i in range(6)]
    # Extend svit rows for extrusion
    yer = zer = 0
    if ey:
        yer = axis_aligned + (dxyz[1] if subdiv else 0)
        svit[4].extend([[] for i in range(yer)])
        svit[5].extend([[] for i in range(yer)])
    if ez:
        zer = axis_aligned + (dxyz[2] if subdiv else 0)
        for side in range(4):
            svit[side].extend([[] for i in range(zer)])
    # Extend svit rows for odd_aligned
    if odd_aligned:
        for side in range(4):
            svit[side].append([])

    hemi = steps // 2

    # Create vertices and svit without dups
    vert = [0., 0., 0.]
    verts = []

    if arcdiv == 1 and not odd_aligned and subdiv == ALL:
        # Special case: 3D Grid Cuboid
        for side, (xp, yp, zp, dir) in enumerate(sides):
            svitc = svit[side]
            rows = len(svitc)
            if rows < dxyz[yp] + 2:
                svitc.extend([[] for i in range(dxyz[yp] + 2 - rows)])
            vert[zp] = (half_chord + exyz[zp]) * dir[zp]
            for j in range(dxyz[yp] + 2):
                vert[yp] = (j * dssxyz[yp] - half_chord - exyz[yp]) * dir[yp]
                for i in range(dxyz[xp] + 2):
                    vert[xp] = (i * dssxyz[xp] - half_chord - exyz[xp]) * dir[xp]
                    if (side == 5) or ((i < dxyz[xp] + 1 and j < dxyz[yp] + 1) and (
                            side < 4 or (i and j))):
                        svitc[j].append(len(verts))
                        verts.append(tuple(vert))
    else:
        for side, (xp, yp, zp, dir) in enumerate(sides):
            svitc = svit[side]
            exr = exyz[xp]
            eyr = exyz[yp]
            ri = 0  # row index
            rij = zer if side < 4 else yer

            for j in range(steps):  # rows
                v, v2, mv2 = uvlt[j]
                tv2mh = 1./3. * v2 - 0.5
                hv2 = 0.5 * v2

                if j == hemi and rij:
                    # Jump over non-edge row indices
                    ri += rij

                for i in range(steps):  # columns
                    u, u2, mu2 = uvlt[i]
                    vert[xp] = u * mv2
                    vert[yp] = v * mu2
                    vert[zp] = radius * sqrt(u2 * tv2mh - hv2 + 1.)

                    if (side == 5) or (i < arcdiv and j < arcdiv and (
                            side < 4 or (i and j or odd_aligned))):

                        vert[0] = (vert[0] + copysign(ex, vert[0])) * dir[0]
                        vert[1] = (vert[1] + copysign(ey, vert[1])) * dir[1]
                        vert[2] = (vert[2] + copysign(ez, vert[2])) * dir[2]
                        rv = tuple(vert)

                        if exr and i == hemi:
                            rx = vert[xp]  # save rotated x
                            vert[xp] = rxi = (-exr - half_chord) * dir[xp]
                            if axis_aligned:
                                svitc[ri].append(len(verts))
                                verts.append(tuple(vert))
                            if subdiv:
                                offsetx = dssxyz[xp] * dir[xp]
                                for k in range(dxyz[xp]):
                                    vert[xp] += offsetx
                                    svitc[ri].append(len(verts))
                                    verts.append(tuple(vert))
                            if eyr and j == hemi and axis_aligned:
                                vert[xp] = rxi
                                vert[yp] = -eyr * dir[yp]
                                svitc[hemi].append(len(verts))
                                verts.append(tuple(vert))
                                if subdiv:
                                    offsety = dssxyz[yp] * dir[yp]
                                    ry = vert[yp]
                                    for k in range(dxyz[yp]):
                                        vert[yp] += offsety
                                        svitc[hemi + axis_aligned + k].append(len(verts))
                                        verts.append(tuple(vert))
                                    vert[yp] = ry
                                    for k in range(dxyz[xp]):
                                        vert[xp] += offsetx
                                        svitc[hemi].append(len(verts))
                                        verts.append(tuple(vert))
                                        if subdiv & ALL:
                                            for l in range(dxyz[yp]):
                                                vert[yp] += offsety
                                                svitc[hemi + axis_aligned + l].append(len(verts))
                                                verts.append(tuple(vert))
                                            vert[yp] = ry
                            vert[xp] = rx  # restore

                        if eyr and j == hemi:
                            vert[yp] = (-eyr - half_chord) * dir[yp]
                            if axis_aligned:
                                svitc[hemi].append(len(verts))
                                verts.append(tuple(vert))
                            if subdiv:
                                offsety = dssxyz[yp] * dir[yp]
                                for k in range(dxyz[yp]):
                                    vert[yp] += offsety
                                    if exr and i == hemi and not axis_aligned and subdiv & ALL:
                                        vert[xp] = rxi
                                        for l in range(dxyz[xp]):
                                            vert[xp] += offsetx
                                            svitc[hemi + k].append(len(verts))
                                            verts.append(tuple(vert))
                                        vert[xp] = rx
                                    svitc[hemi + axis_aligned + k].append(len(verts))
                                    verts.append(tuple(vert))

                        svitc[ri].append(len(verts))
                        verts.append(rv)
                ri += 1

    # Complete svit edges (shared vertices)
    # Sides' right edge
    for side, rows in enumerate(svit[:4]):
        for j, row in enumerate(rows[:-1]):
            svit[3 if not side else side - 1][j].append(row[0])
    # Sides' top edge
    for j, row in enumerate(svit[5]):
        if not j:
            for col in row:
                svit[0][-1].append(col)
        if j == len(svit[5]) - 1:
            for col in reversed(row):
                svit[2][-1].append(col)
        svit[3][-1].insert(0, row[0])
        svit[1][-1].append(row[-1])
    if odd_aligned:
        for side in svit[:4]:
            side[-1].append(-1)
    # Bottom edges
    if odd_aligned:
        svit[4].insert(0, [-1] + svit[2][0][-2::-1] + [-1])
        for i, col in enumerate(svit[3][0][:-1]):
            svit[4][i + 1].insert(0, col)
            svit[4][i + 1].append(svit[1][0][-i - 2])
        svit[4].append([-1] + svit[0][0][:-1] + [-1])
    else:
        svit[4][0].extend(svit[2][0][::-1])
        for i, col in enumerate(svit[3][0][1:-1]):
            svit[4][i + 1].insert(0, col)
            svit[4][i + 1].append(svit[1][0][-i - 2])
        svit[4][-1].extend(svit[0][0])

    # Build faces
    faces = []
    if not axis_aligned:
        hemi -= 1
    for side, rows in enumerate(svit):
        xp, yp = sides[side][:2]
        oa4 = odd_aligned and side == 4
        if oa4:  # special case
            hemi += 1
        for j, row in enumerate(rows[:-1]):
            tri = odd_aligned and (oa4 and not j or rows[j+1][-1] < 0)
            for i, vi in enumerate(row[:-1]):
                # odd_aligned triangle corners
                if vi < 0:
                    if not j and not i:
                        faces.append((row[i+1], rows[j+1][i+1], rows[j+1][i]))
                elif oa4 and not i and j == len(rows) - 2:
                    faces.append((vi, row[i+1], rows[j+1][i+1]))
                elif tri and i == len(row) - 2:
                    if j:
                        faces.append((vi, row[i+1], rows[j+1][i]))
                    else:
                        if oa4 or arcdiv > 1:
                            faces.append((vi, rows[j+1][i+1], rows[j+1][i]))
                        else:
                            faces.append((vi, row[i+1], rows[j+1][i]))
                # subdiv = EDGES (not ALL)
                elif subdiv and len(rows[j + 1]) < len(row) and (i >= hemi):

                    if (i == hemi):
                        faces.append((
                            vi,
                            row[i+1+dxyz[xp]],
                            rows[j+1+dxyz[yp]][i+1+dxyz[xp]],
                            rows[j+1+dxyz[yp]][i]))
                    elif i > hemi + dxyz[xp]:
                        faces.append((
                            vi, row[i+1],
                            rows[j+1][i+1-dxyz[xp]],
                            rows[j+1][i-dxyz[xp]]))

                elif subdiv and len(rows[j + 1]) > len(row) and (i >= hemi):
                    if (i > hemi):
                        faces.append((vi, row[i+1], rows[j+1][i+1+dxyz[xp]], rows[j+1][i+dxyz[xp]]))

                elif subdiv and len(row) < len(rows[0]) and i == hemi:
                    pass

                else:
                    # Most faces...
                    faces.append((vi, row[i+1], rows[j+1][i+1], rows[j+1][i]))
        if oa4:
            hemi -= 1

    return verts, faces


class SvBoxRoundedNode(bpy.types.Node, SverchCustomTreeNode):
    ''' Box rounded'''
    bl_idname = 'SvBoxRoundedNode'
    bl_label = 'Rounded Box'
    bl_icon = 'MESH_CAPSULE'

    radius = FloatProperty(
        name='radius', description='fillet radius',
        default=1.0, update=updateNode, min=0.0, step=0.2)

    arcdiv = IntProperty(
        name='arcdiv', description='number of divisions in fillet',
        min=0, max=10, default=4, update=updateNode)

    lindiv = FloatProperty(
        name='lindiv', description='rate of linear division per surface',
        default=0., min=0.0, step=0.2, precision=1, update=updateNode)

    div_type = IntProperty(
        name='div_type', description='CORNERS, EDGES, ALL',
        min=0, max=2, default=0, update=updateNode)

    odd_axis_align = IntProperty(
        name='odd_axis_align', description='uhh',
        default=0, min=0, max=1, update=updateNode)

    vector_vsize = FloatVectorProperty(size=3, default=(1,1,1), name='vector size', update=updateNode)

    def sv_init(self, context):
        new = self.inputs.new
        new('StringsSocket', "radius").prop_name = 'radius'
        new('StringsSocket', "arcdiv").prop_name = 'arcdiv'
        new('StringsSocket', "lindiv").prop_name = 'lindiv'
        new('VerticesSocket', "vector_size").prop_name = 'vector_vsize'
        new('StringsSocket', "div_type").prop_name = 'div_type'
        new('StringsSocket', "odd_axis_align").prop_name = 'odd_axis_align'

        self.outputs.new('VerticesSocket', "Vers")
        self.outputs.new('StringsSocket', "Pols")

    def draw_buttons(self, context, layout):
        pass

    def process(self):
        inputs = self.inputs
        outputs = self.outputs
        sizes = inputs['vector_size'].sv_get()[0]

        # sizes determines FullLength
        num_boxes = len(sizes)

        radii = inputs['radius'].sv_get()[0]
        arc_divs = inputs['arcdiv'].sv_get()[0]
        lin_divs = inputs['lindiv'].sv_get()[0]
        div_types = inputs['div_type'].sv_get()[0]
        axis_aligns = inputs['odd_axis_align'].sv_get()[0]

        fullList(radii, num_boxes)
        fullList(arc_divs, num_boxes)
        fullList(lin_divs, num_boxes)
        fullList(div_types, num_boxes)
        fullList(axis_aligns, num_boxes)

        multi_dict = []
        for i, args in enumerate(sizes):
            multi_dict.append({
                'radius': radii[i],
                'arcdiv': arc_divs[i],
                'lindiv': lin_divs[i],
                'size': args,
                'div_type': div_types[i],
                'odd_axis_align': axis_aligns[i]
                })
            # print(multi_dict[i])

        out = list(zip(*[round_cube(**kwargs) for kwargs in multi_dict]))
        outputs['Vers'].sv_set(out[0])
        outputs['Pols'].sv_set(out[1])


def register():
    bpy.utils.register_class(SvBoxRoundedNode)


def unregister():
    bpy.utils.unregister_class(SvBoxRoundedNode)
