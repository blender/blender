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
import math

# spline function modifed from
# from looptools 4.5.2 done by Bart Crouch


# kept as it is used in older nodes
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

    return splines


# kept as it is used in older nodes
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

# not used currently
def sv_interpolate(v, t_in, mode='SPL'):
    '''
    input
        v       : list, vectors to interpolate
        t_in    : list, interpolation points [0.0 <= t_in <= 1.0]
        modes   : string,
                ('SPL', 'Cubic', "Cubic Spline"),
                ('LIN', 'Linear', "Linear Interpolation")
    output
        _       : list, interpolated coordinates
    '''

    pts = np.array(v).T
    tmp = np.apply_along_axis(np.linalg.norm, 0, pts[:, :-1]-pts[:, 1:])
    t = np.insert(tmp, 0, 0).cumsum()
    t = t/t[-1]
    t_corr = [min(1, max(t_c, 0)) for t_c in t_in]
    # this should also be numpy
    if mode == 'LIN':
        out = [np.interp(t_corr, t, pts[i]) for i in range(3)]
        return list(zip(*out))
    else:  # SPL
        spl = cubic_spline(v, t)
        return eval_spline(spl, t, t_corr)

