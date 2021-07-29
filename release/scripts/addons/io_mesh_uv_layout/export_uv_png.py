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

# <pep8-80 compliant>

import bpy


def write(fw, mesh_source, image_width, image_height, opacity, face_iter_func):
    filepath = fw.__self__.name
    fw.__self__.close()

    material_solids = [bpy.data.materials.new("uv_temp_solid")
                       for i in range(max(1, len(mesh_source.materials)))]

    material_wire = bpy.data.materials.new("uv_temp_wire")

    scene = bpy.data.scenes.new("uv_temp")
    mesh = bpy.data.meshes.new("uv_temp")
    for mat_solid in material_solids:
        mesh.materials.append(mat_solid)

    polys_source = mesh_source.polygons

    # get unique UV's in case there are many overlapping
    # which slow down filling.
    face_hash = {(uvs, polys_source[i].material_index)
                 for i, uvs in face_iter_func()}

    # now set the faces coords and locations
    # build mesh data
    mesh_new_vertices = []
    mesh_new_materials = []
    mesh_new_polys_startloop = []
    mesh_new_polys_totloop = []
    mesh_new_loops_vertices = []

    current_vert = 0

    for uvs, mat_idx in face_hash:
        num_verts = len(uvs)
        dummy = (0.0,) * num_verts
        for uv in uvs:
            mesh_new_vertices += (uv[0], uv[1], 0.0)
        mesh_new_polys_startloop.append(current_vert)
        mesh_new_polys_totloop.append(num_verts)
        mesh_new_loops_vertices += range(current_vert,
                                         current_vert + num_verts)
        mesh_new_materials.append(mat_idx)
        current_vert += num_verts

    mesh.vertices.add(current_vert)
    mesh.loops.add(current_vert)
    mesh.polygons.add(len(mesh_new_polys_startloop))

    mesh.vertices.foreach_set("co", mesh_new_vertices)
    mesh.loops.foreach_set("vertex_index", mesh_new_loops_vertices)
    mesh.polygons.foreach_set("loop_start", mesh_new_polys_startloop)
    mesh.polygons.foreach_set("loop_total", mesh_new_polys_totloop)
    mesh.polygons.foreach_set("material_index", mesh_new_materials)

    mesh.update(calc_edges=True)

    obj_solid = bpy.data.objects.new("uv_temp_solid", mesh)
    obj_wire = bpy.data.objects.new("uv_temp_wire", mesh)
    base_solid = scene.objects.link(obj_solid)
    base_wire = scene.objects.link(obj_wire)
    base_solid.layers[0] = True
    base_wire.layers[0] = True

    # place behind the wire
    obj_solid.location = 0, 0, -1

    obj_wire.material_slots[0].link = 'OBJECT'
    obj_wire.material_slots[0].material = material_wire

    # setup the camera
    cam = bpy.data.cameras.new("uv_temp")
    cam.type = 'ORTHO'
    cam.ortho_scale = 1.0
    obj_cam = bpy.data.objects.new("uv_temp_cam", cam)
    obj_cam.location = 0.5, 0.5, 1.0
    scene.objects.link(obj_cam)
    scene.camera = obj_cam

    # setup materials
    for i, mat_solid in enumerate(material_solids):
        if mesh_source.materials and mesh_source.materials[i]:
            mat_solid.diffuse_color = mesh_source.materials[i].diffuse_color

        mat_solid.use_shadeless = True
        mat_solid.use_transparency = True
        mat_solid.alpha = opacity

    material_wire.type = 'WIRE'
    material_wire.use_shadeless = True
    material_wire.diffuse_color = 0, 0, 0
    material_wire.use_transparency = True

    # scene render settings
    scene.render.use_raytrace = False
    scene.render.alpha_mode = 'TRANSPARENT'
    scene.render.image_settings.color_mode = 'RGBA'

    scene.render.resolution_x = image_width
    scene.render.resolution_y = image_height
    scene.render.resolution_percentage = 100

    if image_width > image_height:
        scene.render.pixel_aspect_y = image_width / image_height
    elif image_width < image_height:
        scene.render.pixel_aspect_x = image_height / image_width

    scene.frame_start = 1
    scene.frame_end = 1

    scene.render.image_settings.file_format = 'PNG'
    scene.render.filepath = filepath

    scene.update()

    data_context = {"blend_data": bpy.context.blend_data, "scene": scene}
    bpy.ops.render.render(data_context, write_still=True)

    # cleanup
    bpy.data.scenes.remove(scene, do_unlink=True)
    bpy.data.objects.remove(obj_cam, do_unlink=True)
    bpy.data.objects.remove(obj_solid, do_unlink=True)
    bpy.data.objects.remove(obj_wire, do_unlink=True)

    bpy.data.cameras.remove(cam, do_unlink=True)
    bpy.data.meshes.remove(mesh, do_unlink=True)

    bpy.data.materials.remove(material_wire, do_unlink=True)
    for mat_solid in material_solids:
        bpy.data.materials.remove(mat_solid, do_unlink=True)
