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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy

__author__ = "Bruce Merry"
__version__ = "0.93"
__bpydoc__ = """\
This script exports Stanford PLY files from Blender. It supports normals,
colours, and texture coordinates per face or per vertex.
Only one mesh can be exported at a time.
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
# Vector rounding se we can use as keys
#
# Updated on Aug 11, 2008 by Campbell Barton
#    - added 'comment' prefix to comments - Needed to comply with the PLY spec.
#
# Updated on Jan 1, 2007 by Gabe Ghearing
#    - fixed normals so they are correctly smooth/flat
#    - fixed crash when the model doesn't have uv coords or vertex colors
#    - fixed crash when the model has vertex colors but doesn't have uv coords
#    - changed float32 to float and uint8 to uchar for compatibility
# Errata/Notes as of Jan 1, 2007
#    - script exports texture coords if they exist even if TexFace isn't selected (not a big deal to me)
#    - ST(R) should probably be renamed UV(T) like in most PLY files (importer needs to be updated to take either)
#
# Updated on Jan 3, 2007 by Gabe Ghearing
#    - fixed "sticky" vertex UV exporting
#    - added pupmenu to enable/disable exporting normals, uv coords, and colors
# Errata/Notes as of Jan 3, 2007
#    - ST(R) coords should probably be renamed UV(T) like in most PLY files (importer needs to be updated to take either)
#    - edges should be exported since PLY files support them
#    - code is getting spaghettish, it should be refactored...
#


def rvec3d(v):
    return round(v[0], 6), round(v[1], 6), round(v[2], 6)


def rvec2d(v):
    return round(v[0], 6), round(v[1], 6)


def write(filename, scene, ob, \
        EXPORT_APPLY_MODIFIERS=True,\
        EXPORT_NORMALS=True,\
        EXPORT_UV=True,\
        EXPORT_COLORS=True):

    if not filename.lower().endswith('.ply'):
        filename += '.ply'

    if not ob:
        raise Exception("Error, Select 1 active object")
        return

    file = open(filename, 'w')


    #EXPORT_EDGES = Draw.Create(0)
    """
    is_editmode = Blender.Window.EditMode()
    if is_editmode:
        Blender.Window.EditMode(0, '', 0)

    Window.WaitCursor(1)
    """

    #mesh = BPyMesh.getMeshFromObject(ob, None, EXPORT_APPLY_MODIFIERS, False, scn) # XXX
    if EXPORT_APPLY_MODIFIERS:
        mesh = ob.create_mesh(True, 'PREVIEW')
    else:
        mesh = ob.data

    if not mesh:
        raise ("Error, could not get mesh data from active object")
        return

    # mesh.transform(ob.matrixWorld) # XXX

    faceUV = (len(mesh.uv_textures) > 0)
    vertexUV = (len(mesh.sticky) > 0)
    vertexColors = len(mesh.vertex_colors) > 0

    if (not faceUV) and (not vertexUV):
        EXPORT_UV = False
    if not vertexColors:
        EXPORT_COLORS = False

    if not EXPORT_UV:
        faceUV = vertexUV = False
    if not EXPORT_COLORS:
        vertexColors = False

    if faceUV:
        active_uv_layer = None
        for lay in mesh.uv_textures:
            if lay.active:
                active_uv_layer = lay.data
                break
        if not active_uv_layer:
            EXPORT_UV = False
            faceUV = None

    if vertexColors:
        active_col_layer = None
        for lay in mesh.vertex_colors:
            if lay.active:
                active_col_layer = lay.data
        if not active_col_layer:
            EXPORT_COLORS = False
            vertexColors = None

    # incase
    color = uvcoord = uvcoord_key = normal = normal_key = None

    mesh_verts = mesh.verts # save a lookup
    ply_verts = [] # list of dictionaries
    # vdict = {} # (index, normal, uv) -> new index
    vdict = [{} for i in range(len(mesh_verts))]
    ply_faces = [[] for f in range(len(mesh.faces))]
    vert_count = 0
    for i, f in enumerate(mesh.faces):


        smooth = f.smooth
        if not smooth:
            normal = tuple(f.normal)
            normal_key = rvec3d(normal)

        if faceUV:
            uv = active_uv_layer[i]
            uv = uv.uv1, uv.uv2, uv.uv3, uv.uv4 # XXX - crufty :/
        if vertexColors:
            col = active_col_layer[i]
            col = col.color1, col.color2, col.color3, col.color4

        f_verts = f.verts

        pf = ply_faces[i]
        for j, vidx in enumerate(f_verts):
            v = mesh_verts[vidx]

            if smooth:
                normal = tuple(v.normal)
                normal_key = rvec3d(normal)

            if faceUV:
                uvcoord = uv[j][0], 1.0 - uv[j][1]
                uvcoord_key = rvec2d(uvcoord)
            elif vertexUV:
                uvcoord = v.uvco[0], 1.0 - v.uvco[1]
                uvcoord_key = rvec2d(uvcoord)

            if vertexColors:
                color = col[j]
                color = int(color[0] * 255.0), int(color[1] * 255.0), int(color[2] * 255.0)


            key = normal_key, uvcoord_key, color

            vdict_local = vdict[vidx]
            pf_vidx = vdict_local.get(key) # Will be None initially

            if pf_vidx == None: # same as vdict_local.has_key(key)
                pf_vidx = vdict_local[key] = vert_count
                ply_verts.append((vidx, normal, uvcoord, color))
                vert_count += 1

            pf.append(pf_vidx)

    file.write('ply\n')
    file.write('format ascii 1.0\n')
    file.write('comment Created by Blender3D %s - www.blender.org, source file: %s\n' % (bpy.app.version_string, bpy.data.filename.split('/')[-1].split('\\')[-1]))

    file.write('element vertex %d\n' % len(ply_verts))

    file.write('property float x\n')
    file.write('property float y\n')
    file.write('property float z\n')

    # XXX
    """
    if EXPORT_NORMALS:
        file.write('property float nx\n')
        file.write('property float ny\n')
        file.write('property float nz\n')
    """
    if EXPORT_UV:
        file.write('property float s\n')
        file.write('property float t\n')
    if EXPORT_COLORS:
        file.write('property uchar red\n')
        file.write('property uchar green\n')
        file.write('property uchar blue\n')

    file.write('element face %d\n' % len(mesh.faces))
    file.write('property list uchar uint vertex_indices\n')
    file.write('end_header\n')

    for i, v in enumerate(ply_verts):
        file.write('%.6f %.6f %.6f ' % tuple(mesh_verts[v[0]].co)) # co
        """
        if EXPORT_NORMALS:
            file.write('%.6f %.6f %.6f ' % v[1]) # no
        """
        if EXPORT_UV:
            file.write('%.6f %.6f ' % v[2]) # uv
        if EXPORT_COLORS:
            file.write('%u %u %u' % v[3]) # col
        file.write('\n')

    for pf in ply_faces:
        if len(pf) == 3:
            file.write('3 %d %d %d\n' % tuple(pf))
        else:
            file.write('4 %d %d %d %d\n' % tuple(pf))

    file.close()
    print("writing", filename, "done")

    if EXPORT_APPLY_MODIFIERS:
        bpy.data.meshes.remove(mesh)

    # XXX
    """
    if is_editmode:
        Blender.Window.EditMode(1, '', 0)
    """

from bpy.props import *


class ExportPLY(bpy.types.Operator):
    '''Export a single object as a stanford PLY with normals, colours and texture coordinates.'''
    bl_idname = "export.ply"
    bl_label = "Export PLY"

    # List of operator properties, the attributes will be assigned
    # to the class instance from the operator settings before calling.


    path = StringProperty(name="File Path", description="File path used for exporting the PLY file", maxlen=1024, default="")
    check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, options={'HIDDEN'})
    use_modifiers = BoolProperty(name="Apply Modifiers", description="Apply Modifiers to the exported mesh", default=True)
    use_normals = BoolProperty(name="Normals", description="Export Normals for smooth and hard shaded faces", default=True)
    use_uvs = BoolProperty(name="UVs", description="Exort the active UV layer", default=True)
    use_colors = BoolProperty(name="Vertex Colors", description="Exort the active vertex color layer", default=True)

    def poll(self, context):
        return context.active_object != None

    def execute(self, context):
        # print("Selected: " + context.active_object.name)

        if not self.properties.path:
            raise Exception("filename not set")

        write(self.properties.path, context.scene, context.active_object,\
            EXPORT_APPLY_MODIFIERS=self.properties.use_modifiers,
            EXPORT_NORMALS=self.properties.use_normals,
            EXPORT_UV=self.properties.use_uvs,
            EXPORT_COLORS=self.properties.use_colors,
        )

        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.manager
        wm.add_fileselect(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        layout = self.layout
        props = self.properties

        row = layout.row()
        row.prop(props, "use_modifiers")
        row.prop(props, "use_normals")
        row = layout.row()
        row.prop(props, "use_uvs")
        row.prop(props, "use_colors")


bpy.types.register(ExportPLY)


def menu_func(self, context):
    default_path = bpy.data.filename.replace(".blend", ".ply")
    self.layout.operator(ExportPLY.bl_idname, text="Stanford (.ply)...").path = default_path

bpy.types.INFO_MT_file_export.append(menu_func)

if __name__ == "__main__":
    bpy.ops.export.ply(path="/tmp/test.ply")
