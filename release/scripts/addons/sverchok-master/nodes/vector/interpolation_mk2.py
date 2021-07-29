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

import bisect
import numpy as np

import bpy
from bpy.props import EnumProperty, FloatProperty, BoolProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import updateNode, dataCorrect, repeat_last

# spline function modifed from
# from looptools 4.5.2 done by Bart Crouch


# calculates natural cubic splines through all given knots
def cubic_spline(locs, tknots):
    knots = list(range(len(locs)))

    n = len(knots)
    if n < 2:
        return False
    x = tknots[:]
    result = []
    for j in range(3):
        a = []
        for i in locs:
            a.append(i[j])
        h = []
        for i in range(n-1):
            if x[i+1] - x[i] == 0:
                h.append(1e-8)
            else:
                h.append(x[i+1] - x[i])
        q = [False]
        for i in range(1, n-1):
            q.append(3/h[i]*(a[i+1]-a[i]) - 3/h[i-1]*(a[i]-a[i-1]))
        l = [1.0]
        u = [0.0]
        z = [0.0]
        for i in range(1, n-1):
            l.append(2*(x[i+1]-x[i-1]) - h[i-1]*u[i-1])
            if l[i] == 0:
                l[i] = 1e-8
            u.append(h[i] / l[i])
            z.append((q[i] - h[i-1] * z[i-1]) / l[i])
        l.append(1.0)
        z.append(0.0)
        b = [False for i in range(n-1)]
        c = [False for i in range(n)]
        d = [False for i in range(n-1)]
        c[n-1] = 0.0
        for i in range(n-2, -1, -1):
            c[i] = z[i] - u[i]*c[i+1]
            b[i] = (a[i+1]-a[i])/h[i] - h[i]*(c[i+1]+2*c[i])/3
            d[i] = (c[i+1]-c[i]) / (3*h[i])
        for i in range(n-1):
            result.append([a[i], b[i], c[i], d[i], x[i]])
    splines = []
    for i in range(len(knots)-1):
        splines.append([result[i], result[i+n-1], result[i+(n-1)*2]])
    return(splines)


def eval_spline(splines, tknots, t_in):
    out = []
    for t in t_in:
        n = bisect.bisect(tknots, t, lo=0, hi=len(tknots))-1
        if n > len(splines)-1:
            n = len(splines)-1
        if n < 0:
            n = 0
        pt = []
        for i in range(3):
            ax, bx, cx, dx, tx = splines[n][i]
            x = ax + bx*(t-tx) + cx*(t-tx)**2 + dx*(t-tx)**3
            pt.append(x)
        out.append(pt)
    return out


class SvInterpolationNodeMK2(bpy.types.Node, SverchCustomTreeNode):
    '''Vector Interpolate'''
    bl_idname = 'SvInterpolationNodeMK2'
    bl_label = 'Vector Interpolation mk2'
    bl_icon = 'OUTLINER_OB_EMPTY'


    t_in_x = FloatProperty(name="tU",
                        default=.5, min=0, max=1, precision=5,
                        update=updateNode)
    t_in_y = FloatProperty(name="tV",
                        default=.5, min=0, max=1, precision=5,
                        update=updateNode)
    defgrid = BoolProperty(name='default_grid', default=True,
                        update=updateNode)
    regimes = [('P', 'Pattern', "Pattern", 0),
               ('G', 'Grid', "Grid", 1)]
    regime = EnumProperty(name='regime',
                        default='G', items=regimes,
                        update=updateNode)
    directions = [('UV', 'UV', "Two directions", 0),
             ('U', 'U', "One direction", 1)]
    direction = EnumProperty(name='Direction',
                        default='U', items=directions,
                        update=updateNode)
    modes = [('SPL', 'Cubic', "Cubic Spline", 0),
             ('LIN', 'Linear', "Linear Interpolation", 1)]
    mode = EnumProperty(name='Mode',
                        default="SPL", items=modes,
                        update=updateNode)

    def sv_init(self, context):
        self.inputs.new('VerticesSocket', 'Vertices')
        self.inputs.new('StringsSocket', 'IntervalX').prop_name = 't_in_x'
        self.inputs.new('StringsSocket', 'IntervalY').prop_name = 't_in_y'
        self.outputs.new('VerticesSocket', 'Vertices')

    def draw_buttons(self, context, layout):
        #pass
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, 'mode', expand=True)
        row = col.row(align=True)
        row.prop(self, 'regime', expand=True)
        if self.regime == 'G':
            row = col.row(align=True)
            row.prop(self, 'direction', expand=True)
        col.prop(self, 'defgrid')


    def interpol(self, verts, t_ins):
        verts_out = []
        for v, t_in in zip(verts, repeat_last(t_ins)):
            pts = np.array(v).T
            tmp = np.apply_along_axis(np.linalg.norm, 0, pts[:, :-1]-pts[:, 1:])
            t = np.insert(tmp, 0, 0).cumsum()
            t = t/t[-1]
            t_corr = [min(1, max(t_c, 0)) for t_c in t_in]
            # this should also be numpy
            if self.mode == 'LIN':
                out = [np.interp(t_corr, t, pts[i]) for i in range(3)]
                verts_out.append(list(zip(*out)))
            else:  # SPL
                spl = cubic_spline(v, t)
                out = eval_spline(spl, t, t_corr)
                verts_out.append(out)
        return verts_out



    def process(self):
        if not any(s.is_linked for s in self.outputs):
            return

        if self.inputs['Vertices'].is_linked:
            verts = self.inputs['Vertices'].sv_get()
            verts = dataCorrect(verts)
            t_ins_x = self.inputs['IntervalX'].sv_get()
            t_ins_y = self.inputs['IntervalY'].sv_get()

            if self.regime == 'P' and self.direction == 'U':
                self.direction = 'UV'
            if self.defgrid:
                t_ins_x = [[i/10 for i in range(11)]]
                t_ins_y = [[i/10 for i in range(11)]]
            if self.regime == 'G':
                vertsX = self.interpol(verts, t_ins_x)
                if self.direction == 'UV':
                    verts_T = np.swapaxes(np.array(vertsX),0,1).tolist()
                    verts_out = self.interpol(verts_T, t_ins_y)

                else:
                    verts_out = vertsX
            else:
                verts_out_ = []
                for x,y in zip(t_ins_x[0],t_ins_y[0]):
                    vertsX = self.interpol(verts, [[x]])
                    verts_T = np.swapaxes(np.array(vertsX),0,1).tolist()
                    vertsY = self.interpol(verts_T, [[y]])
                    verts_out_.extend(vertsY)

                verts_out = [[i[0] for i in verts_out_]]
            self.outputs['Vertices'].sv_set(verts_out)


def register():
    bpy.utils.register_class(SvInterpolationNodeMK2)


def unregister():
    bpy.utils.unregister_class(SvInterpolationNodeMK2)
