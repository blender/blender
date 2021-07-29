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

import bpy
from bpy.props import StringProperty

import sverchok
from sverchok.data_structure import levelsOflist, dataSpoil, \
                        match_long_cycle, match_long_repeat

def sv_recursive_transformations(*args):
    ''' main function takes 5 args
    function, verts1, verts2, multiplyer, separate
    resulting levels arg to recursion'''
    # print('arguments recursive',len(args))
    if len(args) == 5:
        f,v1,v2,m,s = args
        m,v1,v2 = match_long_repeat([m,v1,v2])
        level1 = levelsOflist(v1)
        level2 = levelsOflist(v2)
        level3 = levelsOflist(m)
        lev = max(level1,level2,level3)-1
        #print('recursion-dataspoiled',m,level3)
        v1 = dataSpoil(v1, lev)
        v2 = dataSpoil(v2, lev)
        m = dataSpoil(m, lev)
        #print('recursion-dataspoiled',m,lev)
        return sv_recursion(f,v1,v2,m,s,lev)

def sv_recursion(function, verts1, verts2, multiplyer, separate, level):
    ''' recursion itself
    function defined at node
    func - definition
    verts1,2 - operating verts
    multiplyer - as named
    separate - boolean to group vertices
    level - depth of verts1,2 '''
    if not separate:
        if level:
            multiplyer,verts1,verts2 = match_long_cycle([multiplyer,verts1,verts2])
            out = []
            outa = out.append
            oute = out.extend
            for v1,v2,m in zip(verts1, verts2, multiplyer):
                out_ = sv_recursion(function, v1,v2,m,separate, level-1)
                if level > 1:
                    outa(out_)
                elif level == 1:
                    oute(out_)
            return out
        else:
            return function(verts1,verts2,multiplyer)
    else:
        if level:
            multiplyer,verts2 = match_long_cycle([multiplyer,verts2])
            out = []
            outa = out.append
            oute = out.extend
            if level > 1:
                for v1,v2,m in zip(verts1, verts2, multiplyer):
                    out_ = sv_recursion(function, v1,v2,m,separate, level-1)
                    outa(out_)
            elif level == 1:
                for v2,m in zip(verts2,multiplyer):
                    out_ = []
                    for v1 in verts1:
                        out_.extend(function(v1,v2,m))
                    oute([out_])
            return out