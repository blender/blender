# ##### BEGIN GPL LICENSE BLOCK #####
#
#  The Blender Rock Creation tool is for rapid generation of mesh rocks.
#  Copyright (C) 2011  Paul Marshall
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


# Converts a formated string to a float tuple:
#   IN - '(0.5, 0.2)' -> CONVERT -> OUT - (0.5, 0.2)
def toTuple(stringIn):
    sTemp = str(stringIn)[1:len(str(stringIn)) - 1].split(', ')
    fTemp = []
    for i in sTemp:
        fTemp.append(float(i))
    return tuple(fTemp)


# Converts a formated string to a float tuple:
#   IN - '[0.5, 0.2]' -> CONVERT -> OUT - [0.5, 0.2]
def toList(stringIn):
    sTemp = str(stringIn)[1:len(str(stringIn)) - 1].split(', ')
    fTemp = []
    for i in sTemp:
        fTemp.append(float(i))
    return fTemp


# Converts each item of a list into a float:
def toFloats(inList):
    outList = []
    for i in inList:
        outList.append(float(i))
    return outList


# Converts each item of a list into an integer:
def toInts(inList):
    outList = []
    for i in inList:
        outList.append(int(i))
    return outList


# Sets all faces smooth.  Done this way since I can't
# find a simple way without using bpy.ops:
def smooth(mesh):
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(mesh)
    for f in bm.faces:
        f.smooth = True
    bm.to_mesh(mesh)
    return mesh
