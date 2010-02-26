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

__author__ = ["Aurel Wildfellner"]
__version__ = '0.2'
__bpydoc__ = """\
This script exports a Mesh to a RAW triangle format file.

The raw triangle format is very simple; it has no verts or faces lists.
It's just a simple ascii text file with the vertices of each triangle
listed on each line. In addition, also quads can be exported as a line
of 12 values (this was the default before blender 2.5). Now default
settings will triangulate the mesh.

Usage:<br>
    Execute this script from the "File->Export" menu. You can select
whether modifiers should be applied and if the mesh is triangulated.

"""

import bpy


def faceToTriangles(face):
    triangles = []
    if (len(face) == 4): #quad
        triangles.append( [ face[0], face[1], face[2] ] )
        triangles.append( [ face[2], face[3], face[0] ] )
    else:
        triangles.append(face)

    return triangles


def faceValues(face, mesh, matrix):
    fv = []
    for verti in face.verts_raw:
        fv.append(matrix * mesh.verts[verti].co)
    return fv


def faceToLine(face):
    line = ""
    for v in face:
        line += str(v[0]) + " " + str(v[1]) + " " + str(v[2]) + " "
    return line[:-1] + "\n"


def export_raw(path, applyMods, triangulate):
    faces = []
    for obj in bpy.context.selected_objects:
        if obj.type == 'MESH':
            matrix = obj.matrix

            if (applyMods):
                me = obj.create_mesh(True, "PREVIEW")
            else:
                me = obj.data

            for face in me.faces:
                fv = faceValues(face, me, matrix)
                if triangulate:
                    faces.extend(faceToTriangles(fv))
                else:
                    faces.append(fv)

    # write the faces to a file
    file = open(path, "w")
    for face in faces:
        file.write(faceToLine(face))
    file.close()


from bpy.props import *


class RawExporter(bpy.types.Operator):
    '''Save Raw triangle mesh data'''
    bl_idname = "export_mesh.raw"
    bl_label = "Export RAW"

    path = StringProperty(name="File Path", description="File path used for exporting the RAW file", maxlen= 1024, default= "")
    filename = StringProperty(name="File Name", description="Name of the file.")
    directory = StringProperty(name="Directory", description="Directory of the file.")
    check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, options={'HIDDEN'})

    apply_modifiers = BoolProperty(name="Apply Modifiers", description="Use transformed mesh data from each object", default=True)
    triangulate = BoolProperty(name="Triangulate", description="Triangulate quads.", default=True)

    def execute(self, context):
        export_raw(self.properties.path, self.properties.apply_modifiers, self.properties.triangulate)
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}

# package manages registering
