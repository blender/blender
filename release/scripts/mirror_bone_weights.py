#!BPY
"""
Name: 'Mirror Bone Weights'
Blender: 239
Group: 'Mesh'
Submenu: '-x to +x' nxtopx
Submenu: '+x to -x' pxtonx
Tooltip: 'Mirror vertex group influences of a model'
"""

__author__ = "Thomas Oppl"
__version__ = "5.12"
__url__ = "elysiun"
__email__ = "scripts"
__bpydoc__ = """\
Description:

This script copies vertex group influences from one half of a model to the
other half of it.

Usage:

- Select the model<br>
- Start the script (Object -> Scripts -> Mirror Bone Weights)<br>
- Use the "-x to +x" or the "+x to -x" submenu depending on which side should
  be the source and which the destination.<br>

Notes:

- The model has to be in the center of the world along the x-axis.<br>
- The model has to be symmetrical along the x-axis.<br>
- You have to use the ".R" and ".L" suffix naming scheme for your vertex groups.<br>

"""

#------------------------------------------------------------
# Mirror Bone Weights - (c) 2005 thomas oppl - toppl@fh-sbg.ac.at
#------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
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
#
# ***** END GPL LICENCE BLOCK *****
#------------------------------------------------------------





from Blender import NMesh, Object, Draw, sys, Types, Window





threshold = 0.001

################################################################################
def mirror(mode):
    print
    print "mirror bone weights: %s" % (mode)
    print "threshold:           %.6f" % (threshold)

    objects = Object.GetSelected()
    if not objects:
        Draw.PupMenu("Error: no object selected!")
        print "no object selected!"
        return
    mesh = objects[0].getData()
    if type(mesh) != Types.NMeshType:
        Draw.PupMenu("Error: object must be a mesh!")
        print "object is no mesh!"
        return
    
    # nmesh.getvertexinfluences function seems to be broken so i create a dictionary instead
    Window.WaitCursor(1)
    time = sys.time()
    in_editmode = Window.EditMode()
    if in_editmode: Window.EditMode(0)
    vertexinfluences = {}
    for i in range(len(mesh.verts)):
        vertexinfluences[i] = []
    for groupname in mesh.getVertGroupNames():
        for vertex in mesh.getVertsFromGroup(groupname, 1):
            index, weight = vertex[0], vertex[1]
            vertexinfluences[index].append((groupname, weight))
    influencestime = sys.time() - time
    print "influence dictionary generated in %.6f seconds!" % (influencestime)

    # generate binary tree to speed up looking for opposite vertex
    time = sys.time()
    tree = c_tree(mesh.verts)
    treetime = sys.time() - time
    print "binary tree generated in %.6f seconds!" % (treetime)

    # mirror vertex group influences
    time = sys.time()
    if mode == "-x to +x":
        verticeshalf = [v for v in mesh.verts if v.co[0] < 0]
    else:
        verticeshalf = [v for v in mesh.verts if v.co[0] > 0]
    i = 0
    for vertex in verticeshalf:
        oppositeposition = (-vertex.co[0], vertex.co[1], vertex.co[2])
        foundvertex = []
        tree.findvertex(oppositeposition, foundvertex, threshold)
        if foundvertex:
            oppositevertex = foundvertex[0]
            # remove all influences from opposite vertex
            for influence in vertexinfluences[oppositevertex.index]:
                mesh.removeVertsFromGroup(influence[0], [oppositevertex.index])
            # copy influences to opposite vertex
            for influence in vertexinfluences[vertex.index]:
                name = influence[0]
                if name[-2:] == ".R":
                    name = name[:-2] + ".L"
                elif name[-2:] == ".L":
                    name = name[:-2] + ".R"
                if name not in mesh.getVertGroupNames(): # create opposite group if it doesn't exist
                    mesh.addVertGroup(name)            
                mesh.assignVertsToGroup(name, [oppositevertex.index], influence[1], "add")
            i += 1
    mirrortime = sys.time() - time
    print "%d vertices mirrored in %.6f seconds!" % (i, mirrortime)

    # done!
    print "done in %.6f seconds total!" % (influencestime + treetime + mirrortime)
    if in_editmode: Window.EditMode(1)
    Window.WaitCursor(0)




################################################################################
NODE_VERTEX_LIMIT = 50

class c_boundingbox:
    def __init__(self, vertices):
        self.min_x = self.max_x = vertices[0].co[0]
        self.min_y = self.max_y = vertices[0].co[1]
        self.min_z = self.max_z = vertices[0].co[2]
        for vertex in vertices:
            self.min_x = min(self.min_x, vertex.co[0])
            self.min_y = min(self.min_y, vertex.co[1])
            self.min_z = min(self.min_z, vertex.co[2])
            self.max_x = max(self.max_x, vertex.co[0])
            self.max_y = max(self.max_y, vertex.co[1])
            self.max_z = max(self.max_z, vertex.co[2])
        self.dim_x = self.max_x - self.min_x
        self.dim_y = self.max_y - self.min_y
        self.dim_z = self.max_z - self.min_z
        self.splitaxis = [self.dim_x, self.dim_y, self.dim_z].index(max(self.dim_x, self.dim_y, self.dim_z))
        self.center_x = self.max_x - (self.dim_x / 2.0)
        self.center_y = self.max_y - (self.dim_y / 2.0)
        self.center_z = self.max_z - (self.dim_z / 2.0)
        self.splitcenter = [self.center_x, self.center_y, self.center_z][self.splitaxis]
    def __str__(self):
        return "min: %.3f %.3f %.3f max: %.3f %.3f %.3f dim: %.3f %.3f %.3f" %\
               (self.min_x, self.min_y, self.min_z,
                self.max_x, self.max_y, self.max_z,
                self.dim_x, self.dim_y, self.dim_z)
    def isinside(self, position, threshold):
        return (position[0] <= self.max_x + threshold and position[1] <= self.max_y + threshold and \
                position[2] <= self.max_z + threshold and position[0] >= self.min_x - threshold and \
                position[1] >= self.min_y - threshold and position[2] >= self.min_z - threshold)

class c_tree:
    def __init__(self, vertices, level = 0):
        self.level = level
        self.children = []
        self.vertices = []
        self.boundingbox = c_boundingbox(vertices)
        splitaxis = self.boundingbox.splitaxis
        splitcenter = self.boundingbox.splitcenter
        if len(vertices) > NODE_VERTEX_LIMIT:
            self.children.append(c_tree(
                [v for v in vertices if v.co[splitaxis] > splitcenter], self.level + 1))
            self.children.append(c_tree(
                [v for v in vertices if v.co[splitaxis] <= splitcenter], self.level + 1))
        else: # leaf node
            self.vertices = vertices
    def __str__(self):
        s = "  " * self.level + "-node %d\n" % (len(self.vertices))
        for child in self.children:
            s += str(child)
        return s
    def findvertex(self, position, foundvertex, threshold):
        if self.boundingbox.isinside(position, threshold):
            if self.children:
                for child in self.children:
                    child.findvertex(position, foundvertex, threshold)
            else: # node found
                for vertex in self.vertices:
                    v, p, t = vertex.co, position, threshold
                    if abs(v[0] - p[0]) < t and abs(v[1] - p[1]) < t and abs(v[2] - p[2]) < t: # vertex found
                        foundvertex.append(vertex)





################################################################################
if __script__["arg"] == "nxtopx":
    mirror("-x to +x")
if __script__["arg"] == "pxtonx":
    mirror("+x to -x")
