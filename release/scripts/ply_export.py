#!BPY

"""
Name: 'PLY...'
Blender: 237
Group: 'Export'
Tooltip: 'Export to Stanford PLY format'
"""

import Blender
import meshtools
import math

__author__ = "Bruce Merry"
__version__ = "0.9"
__bpydoc__ = """\
This script exports Stanford PLY files from Blender. It supports per-vertex
normals and per-face colours and texture coordinates.
"""

# Copyright (C) 2004, 2005: Bruce Merry, bmerry@cs.uct.ac.za
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

def file_callback(filename):
        if filename.find('.ply', -4) < 0: filename += '.ply'
        file = open(filename, "wb")
        objects = Blender.Object.GetSelected()
        obj = objects[0]
        mesh = objects[0].data

        have_uv = mesh.hasFaceUV()
        have_col = meshtools.has_vertex_colors(mesh)
        verts = [] # list of dictionaries
        vdict = {} # (index, normal, uv) -> new index
        for (i, f) in enumerate(mesh.faces):
                for (j, v) in enumerate(f.v):
                        index = v.index
                        key = index, tuple(v.no)
                        vdata = {'position': v.co, 'normal': v.no}
                        if have_uv:
                                vdata['uv'] = f.uv[j]
                                key = key + (tuple(f.uv[j]), )
                        if have_col:
                                vdata['col'] = f.col[j]
                                key = key + ((f.col[j].r, f.col[j].g, f.col[j].b, f.col[j].a), )
                        if not vdict.has_key(key):
                                vdict[key] = len(verts);
                                verts.append(vdata)
                if not i % 100 and meshtools.show_progress:
                        Blender.Window.DrawProgressBar(float(i) / len(mesh.faces), "Organising vertices")

        print >> file, "ply"
        print >> file, "format ascii 1.0"
        print >> file, "comment created by ply_export.py from Blender"
        print >> file, "element vertex %d" % len(verts)
        print >> file, "property float32 x"
        print >> file, "property float32 y"
        print >> file, "property float32 z"
        print >> file, "property float32 nx"
        print >> file, "property float32 ny"
        print >> file, "property float32 nz"
        if have_uv:
                print >> file, "property float32 s"
                print >> file, "property float32 t"
        if have_col:
                print >> file, "property uint8 red"
                print >> file, "property uint8 green"
                print >> file, "property uint8 blue"
        print >> file, "element face %d" % len(mesh.faces)
        print >> file, "property list uint8 int32 vertex_indices"
        print >> file, "end_header"

        for (i, v) in enumerate(verts):
                print >> file, "%f %f %f %f %f %f" % (tuple(v['position']) + tuple(v['normal'])),
                if have_uv: print >> file, "%f %f" % tuple(v['uv']),
                if have_col: print >> file, "%u %u %u" % (v['col'].r, v['col'].g, v['col'].b),
                print >> file
                if not i % 100 and meshtools.show_progress:
                        Blender.Window.DrawProgressBar(float(i) / len(verts), "Writing vertices")
        for (i, f) in enumerate(mesh.faces):
                print >> file, "%d" % len(f.v),
                for j in range(len(f.v)):
                        v = f.v[j]
                        index = v.index
                        key = index, tuple(v.no)
                        if have_uv:
                                key = key + (tuple(f.uv[j]), )
                        if have_col:
                                key = key + ((f.col[j].r, f.col[j].g, f.col[j].b, f.col[j].a), )
                        print >> file, "%d" % vdict[key],
                print >> file
                if not i % 100 and meshtools.show_progress:
                        Blender.Window.DrawProgressBar(float(i) / len(mesh.faces), "Writing faces")

	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	message = "Successfully exported " + Blender.sys.basename(filename)
	meshtools.print_boxed(message)

Blender.Window.FileSelector(file_callback, "PLY Export")
