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

# <pep8 compliant>

# Copyright (C) 2004, 2005: Bruce Merry, bmerry@cs.uct.ac.za
# Contributors: Bruce Merry, Campbell Barton

"""
This script exports Stanford PLY files from Blender. It supports normals,
colours, and texture coordinates per face or per vertex.
Only one mesh can be exported at a time.
"""

import bpy
import os


def save(operator, context, filepath="", use_modifiers=True, use_normals=True, use_uv_coords=True, use_colors=True):
    
    def rvec3d(v):
        return round(v[0], 6), round(v[1], 6), round(v[2], 6)


    def rvec2d(v):
        return round(v[0], 6), round(v[1], 6)
    
    scene = context.scene
    obj = context.object

    if not obj:
        raise Exception("Error, Select 1 active object")

    file = open(filepath, 'w')

    if scene.objects.active:
        bpy.ops.object.mode_set(mode='OBJECT')

    if use_modifiers:
        mesh = obj.create_mesh(scene, True, 'PREVIEW')
    else:
        mesh = obj.data

    if not mesh:
        raise Exception("Error, could not get mesh data from active object")

    # mesh.transform(obj.matrix_world) # XXX

    faceUV = (len(mesh.uv_textures) > 0)
    vertexUV = (len(mesh.sticky) > 0)
    vertexColors = len(mesh.vertex_colors) > 0

    if (not faceUV) and (not vertexUV):
        use_uv_coords = False
    if not vertexColors:
        use_colors = False

    if not use_uv_coords:
        faceUV = vertexUV = False
    if not use_colors:
        vertexColors = False

    if faceUV:
        active_uv_layer = mesh.uv_textures.active
        if not active_uv_layer:
            use_uv_coords = False
            faceUV = None
        else:
            active_uv_layer = active_uv_layer.data

    if vertexColors:
        active_col_layer = mesh.vertex_colors.active
        if not active_col_layer:
            use_colors = False
            vertexColors = None
        else:
            active_col_layer = active_col_layer.data

    # incase
    color = uvcoord = uvcoord_key = normal = normal_key = None

    mesh_verts = mesh.vertices # save a lookup
    ply_verts = [] # list of dictionaries
    # vdict = {} # (index, normal, uv) -> new index
    vdict = [{} for i in range(len(mesh_verts))]
    ply_faces = [[] for f in range(len(mesh.faces))]
    vert_count = 0
    for i, f in enumerate(mesh.faces):


        smooth = f.use_smooth
        if not smooth:
            normal = tuple(f.normal)
            normal_key = rvec3d(normal)

        if faceUV:
            uv = active_uv_layer[i]
            uv = uv.uv1, uv.uv2, uv.uv3, uv.uv4 # XXX - crufty :/
        if vertexColors:
            col = active_col_layer[i]
            col = col.color1, col.color2, col.color3, col.color4

        f_verts = f.vertices

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

            if pf_vidx is None: # same as vdict_local.has_key(key)
                pf_vidx = vdict_local[key] = vert_count
                ply_verts.append((vidx, normal, uvcoord, color))
                vert_count += 1

            pf.append(pf_vidx)

    file.write('ply\n')
    file.write('format ascii 1.0\n')
    file.write('comment Created by Blender %s - www.blender.org, source file: %r\n' % (bpy.app.version_string, os.path.basename(bpy.data.filepath)))

    file.write('element vertex %d\n' % len(ply_verts))

    file.write('property float x\n')
    file.write('property float y\n')
    file.write('property float z\n')

    if use_normals:
        file.write('property float nx\n')
        file.write('property float ny\n')
        file.write('property float nz\n')
    if use_uv_coords:
        file.write('property float s\n')
        file.write('property float t\n')
    if use_colors:
        file.write('property uchar red\n')
        file.write('property uchar green\n')
        file.write('property uchar blue\n')

    file.write('element face %d\n' % len(mesh.faces))
    file.write('property list uchar uint vertex_indices\n')
    file.write('end_header\n')

    for i, v in enumerate(ply_verts):
        file.write('%.6f %.6f %.6f ' % tuple(mesh_verts[v[0]].co)) # co
        if use_normals:
            file.write('%.6f %.6f %.6f ' % v[1]) # no
        if use_uv_coords:
            file.write('%.6f %.6f ' % v[2]) # uv
        if use_colors:
            file.write('%u %u %u' % v[3]) # col
        file.write('\n')

    for pf in ply_faces:
        if len(pf) == 3:
            file.write('3 %d %d %d\n' % tuple(pf))
        else:
            file.write('4 %d %d %d %d\n' % tuple(pf))

    file.close()
    print("writing %r done" % filepath)

    if use_modifiers:
        bpy.data.meshes.remove(mesh)

    # XXX
    """
    if is_editmode:
        Blender.Window.EditMode(1, '', 0)
    """
    
    return {'FINISHED'}
