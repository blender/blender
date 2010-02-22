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

__author__ = ["Anthony D'Agostino (Scorpius)", "Aurel Wildfellner"]
__version__ = '0.2'
__bpydoc__ = """\
This script imports Raw Triangle File format files to Blender.

The raw triangle format is very simple; it has no verts or faces lists.
It's just a simple ascii text file with the vertices of each triangle
listed on each line. In addition, a line with 12 values will be 
imported as a quad. This may be in conflict with some other 
applications, which use a raw format, but this is how it was 
implemented back in blender 2.42.

Usage:<br>
    Execute this script from the "File->Import" menu and choose a Raw file to
open.

Notes:<br>
    Generates the standard verts and faces lists, but without duplicate
verts. Only *exact* duplicates are removed, there is no way to specify a
tolerance.
"""



import bpy

# move those to a utility modul
from import_scene_obj import unpack_face_list, unpack_list # TODO, make generic


def readMesh(filename, objName):
    file = open(filename, "rb")

    def line_to_face(line):
        # Each triplet is an xyz float
        line_split = []
        try:
            line_split = list(map(float, line.split()))
        except:
            return None

        if len(line_split) == 9: # Tri
            f1, f2, f3, f4, f5, f6, f7, f8, f9 = line_split
            return [(f1, f2, f3), (f4, f5, f6), (f7, f8, f9)]
        elif len(line_split) == 12: # Quad
            f1, f2, f3, f4, f5, f6, f7, f8, f9, A, B, C = line_split
            return [(f1, f2, f3), (f4, f5, f6), (f7, f8, f9), (A, B, C)]
        else:
            return None
        
    
    faces = []
    for line in file.readlines():
        face = line_to_face(line)
        if face:
            faces.append(face)

    file.close()

    # Generate verts and faces lists, without duplicates
    verts = []
    coords = {}
    index = 0

    for f in faces:
        for i, v in enumerate(f):
            try:
                f[i] = coords[v]
            except:
                f[i] = coords[v] = index
                index += 1
                verts.append(v)

    mesh = bpy.data.meshes.new(objName)
    mesh.add_geometry(int(len(verts)), 0, int(len(faces)))
    mesh.verts.foreach_set("co", unpack_list(verts))
    mesh.faces.foreach_set("verts_raw", unpack_face_list(faces))
    mesh.faces.foreach_set("smooth", [False] * len(mesh.faces))

    return mesh


def addMeshObj(mesh, objName):
    scn = bpy.context.scene
    
    for o in scn.objects:
        o.selected = False

    mesh.update()
    nobj = bpy.data.objects.new(objName, mesh)
    scn.objects.link(nobj)
    nobj.selected = True

    if scn.objects.active == None or scn.objects.active.mode == 'OBJECT':
        scn.objects.active = nobj


from bpy.props import *

class RawImporter(bpy.types.Operator):
    '''Load Raw triangle mesh data'''
    bl_idname = "import_mesh.raw"
    bl_label = "Import RAW"

    path = StringProperty(name="File Path", description="File path used for importing the RAW file", maxlen=1024, default="")
    filename = StringProperty(name="File Name", description="Name of the file.")
    directory = StringProperty(name="Directory", description="Directory of the file.")

    def execute(self, context):

        #convert the filename to an object name
        objName = bpy.utils.display_name(self.properties.filename)

        mesh = readMesh(self.properties.path, objName)
        addMeshObj(mesh, objName)

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}

# package manages registering
