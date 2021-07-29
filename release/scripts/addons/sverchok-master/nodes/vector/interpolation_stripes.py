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
from bpy.props import EnumProperty, FloatProperty, BoolProperty, StringProperty

from sverchok.node_tree import SverchCustomTreeNode
from sverchok.data_structure import (updateNode, dataCorrect, repeat_last,
                                     match_long_repeat)
from mathutils import Vector

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

vector_out = {
    #func(dists_np,maxidist,t_ins_y,factor)
    "SIMPLE":       (lambda dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum: \
                            (dists_np/(maxidist*len(t_ins_y))).clip(min=minimum,max=maximum)),
    "MULT":         (lambda dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum: \
                            (dists_np*scale*factor/(maxidist*len(t_ins_y))).clip(min=minimum,max=maximum)),
    "SIN":          (lambda dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum: \
                            (np.sin(dists_np*scale)*factor/(maxidist*len(t_ins_y))).clip(min=minimum,max=maximum)),
    "COS":          (lambda dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum: \
                            (np.cos(dists_np*scale)*factor/(maxidist*len(t_ins_y))).clip(min=minimum,max=maximum)),
    "POW":          (lambda dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum: \
                            (np.power((dists_np*scale*factor),2)/(maxidist*len(t_ins_y))).clip(min=minimum,max=maximum)),
    "SQRT":         (lambda dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum: \
                            (np.sqrt(dists_np*scale)*factor/(maxidist*len(t_ins_y))).clip(min=minimum,max=maximum)),
}



class SvInterpolationStripesNode(bpy.types.Node, SverchCustomTreeNode):
    '''Vector Interpolate Stripes'''
    bl_idname = 'SvInterpolationStripesNode'
    bl_label = 'Vector Stripes'
    bl_icon = 'OUTLINER_OB_EMPTY'


    # vector math functions
    mode_items = [
        ("SIMPLE",       "Simple",      "", 0),
        ("MULT",         "Mult",        "", 1),
        ("SIN",          "Sin",         "", 2),
        ("COS",          "Cos",         "", 3),
        ("POW",         "POW",        "", 4),
        ("SQRT",         "Sqrt",        "", 5),
    ]

    current_op = StringProperty(default="SIMPLE")

    def mode_change(self, context):

        if not (self.operations == self.current_op):
            self.label = 'Stripes ' + self.operations
            self.current_op = self.operations
            updateNode(self, context)

    operations = EnumProperty(
        items=mode_items,
        name="Function",
        description="Function choice",
        default="SIMPLE",
        update=mode_change)

    factor = FloatProperty(name="factor",
                        default=1.0, precision=5,
                        update=updateNode)
    minimum = FloatProperty(name="minimum",
                        default=0.0, min=0.0, max=0.5, precision=5,
                        update=updateNode)
    maximum = FloatProperty(name="maximum",
                        default=1.0, min=0.5, max=1.0, precision=5,
                        update=updateNode)
    scale = FloatProperty(name="scale",
                        default=1.0, precision=5,
                        update=updateNode)
    t_in_x = FloatProperty(name="tU",
                        default=.5, min=0, max=1, precision=5,
                        update=updateNode)
    t_in_y = FloatProperty(name="tV",
                        default=.5, min=0, max=1, precision=5,
                        update=updateNode)

    def sv_init(self, context):
        s = self.inputs.new('VerticesSocket', 'Vertices')
        s.use_prop = True
        self.inputs.new('StringsSocket', 'IntervalX')
        self.inputs.new('StringsSocket', 'IntervalY')
        a = self.inputs.new('VerticesSocket', 'Attractor')
        a.use_prop = True
        s.prop = (0, 0, 1)
        self.outputs.new('VerticesSocket', 'vStripesOut')
        self.outputs.new('VerticesSocket', 'vStripesIn')
        self.outputs.new('VerticesSocket', 'vShape')
        self.outputs.new('StringsSocket',  'sCoefs')

    def draw_buttons(self, context, layout):
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, 'factor')
        row = col.row(align=True)
        row.prop(self, 'scale')
        row = col.row(align=True)
        row.prop(self, 'operations')

    def draw_buttons_ext(self, context, layout):
        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(self, 'minimum')
        row = col.row(align=True)
        row.prop(self, 'maximum')

    def interpol(self, verts, t_ins):
        verts_out = []
        for v, t_in in zip(verts, repeat_last(t_ins)):
            pts = np.array(v).T
            tmp = np.apply_along_axis(np.linalg.norm, 0, pts[:, :-1]-pts[:, 1:])
            t = np.insert(tmp, 0, 0).cumsum()
            t = t/t[-1]
            t_corr = [min(1, max(t_c, 0)) for t_c in t_in]
            spl = cubic_spline(v, t)
            out = eval_spline(spl, t, t_corr)
            verts_out.append(out)
        return verts_out

    def distance(self, x, y):
        vec = Vector((x[0]-y[0], x[1]-y[1], x[2]-y[2]))
        return vec.length

    def process(self):
        if not any(s.is_linked for s in self.outputs):
            return

        if self.inputs['Vertices'].is_linked:
            verts = self.inputs['Vertices'].sv_get()
            verts = dataCorrect(verts)
            attrs = self.inputs['Attractor'].sv_get()
            attrs = dataCorrect(attrs)
            if not self.inputs['IntervalX'].is_linked:
                t_ins_x = [[i/10 for i in range(0,11)]]
            else:
                t_ins_x = self.inputs['IntervalX'].sv_get()
            if not self.inputs['IntervalY'].is_linked:
                t_ins_y = [[i/10 for i in range(0,11)]]
            else:
                t_ins_y = self.inputs['IntervalY'].sv_get()
            factor = self.factor
            scale = self.scale
            minimum = self.minimum
            maximum = self.maximum
            operations = self.operations
            func = vector_out[operations]

            # initial interpolation
            vertsX = self.interpol(verts, t_ins_x)
            verts_T = np.swapaxes(np.array(vertsX),0,1).tolist()
            verts_int = self.interpol(verts_T, t_ins_y)

            # calculating distances with maximum one
            dists = []
            verts_int, attrs = match_long_repeat([verts_int, attrs])
            for overts, oattrs in zip(verts_int,attrs):
                overts, oattrs = match_long_repeat([overts, oattrs])
                dists_ = []
                for v, a in zip(overts,oattrs):
                    dists_.append(self.distance(v,a))
                dists.append(dists_)
            dists_np = np.array(dists)
            maxidist = dists_np.max()

            # normalize distances to coefficients for every vertex
            # can be extended with formula evaluation... next step
            #factor = eval(self.factor)
            # vector-output

            try:
                #dists_normalized = dists_np/(maxidist*len(t_ins_y))
                dists_normalized = func(dists_np,maxidist,t_ins_y,factor,scale,minimum,maximum)
                #print(dists_normalized)
            except ZeroDivisionError:
                print ("division by zero!")
                return
            #except:
            #    print('stripes cannot calc function')
            #    return

            # calculate vertex moving coefficient
            # simmetrically mirrored
            t_ins_y_np = np.array(t_ins_y).repeat(len(dists_normalized),0)
            a = np.roll(t_ins_y_np,1,1)
            b = np.roll(t_ins_y_np,-1,1)
            c = t_ins_y_np-(t_ins_y_np-a)*dists_normalized/2
            d = t_ins_y_np+(b-t_ins_y_np)*dists_normalized/2

            # replacing first-last for both mirrors
            c[:,0] = t_ins_y_np[:,0]
            d[:,-1] = t_ins_y_np[:,-1]
            t_ins_y_mins = c.tolist()
            t_ins_y_plus = d.tolist()

            # secondary interpolation
            # processing sliced stripes
            vertsY_mins = self.interpol(verts_int, t_ins_y_mins)
            vertsY_plus = self.interpol(verts_int, t_ins_y_plus)
            verts_T_mins = np.swapaxes(np.array(vertsY_mins),0,1).tolist()
            verts_T_plus = np.swapaxes(np.array(vertsY_plus),0,1).tolist()
            verts_X_mins = self.interpol(verts_T_mins, t_ins_x)
            verts_X_plus = self.interpol(verts_T_plus, t_ins_x)

            # zipping for UVconnect node to "eat" this
            # mirrors on left and right side from initial interpolation
            verts_out = [[M,P] for M,P in zip(verts_X_mins,verts_X_plus)]
            vm,vp = verts_X_mins[1:],verts_X_plus[:-1]
            #print('mnis----------',verts_X_mins[0])
            verts_inner_out = [[M,P] for M,P in zip(vm,vp)]

            if self.outputs['vStripesOut'].is_linked:
                self.outputs['vStripesOut'].sv_set(verts_out)
            if self.outputs['vStripesIn'].is_linked:
                self.outputs['vStripesIn'].sv_set(verts_inner_out)
            if self.outputs['vShape'].is_linked:
                self.outputs['vShape'].sv_set(verts_int)
            if self.outputs['sCoefs'].is_linked:
                self.outputs['sCoefs'].sv_set(dists_normalized.tolist())


def register():
    bpy.utils.register_class(SvInterpolationStripesNode)


def unregister():
    bpy.utils.unregister_class(SvInterpolationStripesNode)

if __name__ == '__main__':
    register()
