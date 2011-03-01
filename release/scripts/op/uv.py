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

import bpy


def write_svg(fw, mesh, image_width, image_height, opacity, face_iter):
    # for making an XML compatible string
    from xml.sax.saxutils import escape
    from os.path import basename

    fw('<?xml version="1.0" standalone="no"?>\n')
    fw('<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" \n')
    fw('  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">\n')
    fw('<svg width="%dpx" height="%dpx" viewBox="0px 0px %dpx %dpx"\n' % (image_width, image_height, image_width, image_height))
    fw('     xmlns="http://www.w3.org/2000/svg" version="1.1">\n')
    desc = "%r, %s, (Blender %s)" % (basename(bpy.data.filepath), mesh.name, bpy.app.version_string)
    fw('<desc>%s</desc>\n' % escape(desc))

    # svg colors
    fill_settings = []
    fill_default = 'fill="grey"'
    for mat in mesh.materials if mesh.materials else [None]:
        if mat:
            fill_settings.append('fill="rgb(%d, %d, %d)"' % tuple(int(c * 255) for c in mat.diffuse_color))
        else:
            fill_settings.append(fill_default)

    faces = mesh.faces
    for i, uvs in face_iter:
        try:  # rare cases material index is invalid.
            fill = fill_settings[faces[i].material_index]
        except IndexError:
            fill = fill_default

        fw('<polygon stroke="black" stroke-width="1px"')
        if opacity > 0.0:
            fw(' %s fill-opacity="%.2g"' % (fill, opacity))

        fw(' points="')

        for j, uv in enumerate(uvs):
            x, y = uv[0], 1.0 - uv[1]
            fw('%.3f,%.3f ' % (x * image_width, y * image_height))
        fw('" />\n')
    fw('\n')
    fw('</svg>\n')


def write_eps(fw, mesh, image_width, image_height, opacity, face_iter):
    fw("%!PS-Adobe-3.0 EPSF-3.0\n")
    fw("%%%%Creator: Blender %s\n" % bpy.app.version_string)
    fw("%%Pages: 1\n")
    fw("%%Orientation: Portrait\n")
    fw("%%%%BoundingBox: 0 0 %d %d\n" % (image_width, image_height))
    fw("%%%%HiResBoundingBox: 0.0 0.0 %.4f %.4f\n" % (image_width, image_height))
    fw("%%EndComments\n")
    fw("%%Page: 1 1\n")
    fw("0 0 translate\n")
    fw("1.0 1.0 scale\n")
    fw("0 0 0 setrgbcolor\n")
    fw("[] 0 setdash\n")
    fw("1 setlinewidth\n")
    fw("1 setlinejoin\n")
    fw("1 setlinecap\n")

    faces = mesh.faces

    if opacity > 0.0:
        # since we need to loop over twice
        face_iter = [(i, uvs) for i, uvs in face_iter]

        for i, mat in enumerate(mesh.materials if mesh.materials else [None]):
            fw("/DRAW_%d {" % i)
            fw("gsave\n")
            if mat:
                color = tuple((1.0 - ((1.0 - c) * opacity)) for c in mat.diffuse_color)
            else:
                color = 1.0, 1.0, 1.0
            fw("%.3g %.3g %.3g setrgbcolor\n" % color)
            fw("fill\n")
            fw("grestore\n")
            fw("0 setgray\n")
            fw("} def\n")

        # fill
        for i, uvs in face_iter:
            fw("newpath\n")
            for j, uv in enumerate(uvs):
                uv_scale = (uv[0] * image_width, uv[1] * image_height)
                if j == 0:
                    fw("%.5f %.5f moveto\n" % uv_scale)
                else:
                    fw("%.5f %.5f lineto\n" % uv_scale)

            fw("closepath\n")
            fw("DRAW_%d\n" % faces[i].material_index)

    # stroke only
    for i, uvs in face_iter:
        fw("newpath\n")
        for j, uv in enumerate(uvs):
            uv_scale = (uv[0] * image_width, uv[1] * image_height)
            if j == 0:
                fw("%.5f %.5f moveto\n" % uv_scale)
            else:
                fw("%.5f %.5f lineto\n" % uv_scale)

        fw("closepath\n")
        fw("stroke\n")

    fw("showpage\n")
    fw("%%EOF\n")


def write_png(fw, mesh_source, image_width, image_height, opacity, face_iter):
    filepath = fw.__self__.name
    fw.__self__.close()

    material_solids = [bpy.data.materials.new("uv_temp_solid") for i in range(max(1, len(mesh_source.materials)))]
    material_wire = bpy.data.materials.new("uv_temp_wire")

    scene = bpy.data.scenes.new("uv_temp")
    mesh = bpy.data.meshes.new("uv_temp")
    for mat_solid in material_solids:
        mesh.materials.append(mat_solid)

    tot_verts = 0
    face_lens = []
    for f in mesh_source.faces:
        tot_verts += len(f.vertices)

    faces_source = mesh_source.faces

    # get unique UV's incase there are many overlapping which slow down filling.
    face_hash_3 = set()
    face_hash_4 = set()
    for i, uv in face_iter:
        material_index = faces_source[i].material_index
        if len(uv) == 3:
            face_hash_3.add((uv[0][0], uv[0][1], uv[1][0], uv[1][1], uv[2][0], uv[2][1], material_index))
        else:
            face_hash_4.add((uv[0][0], uv[0][1], uv[1][0], uv[1][1], uv[2][0], uv[2][1], uv[3][0], uv[3][1], material_index))

    # now set the faces coords and locations
    # build mesh data
    mesh_new_vertices = []
    mesh_new_materials = []
    mesh_new_face_vertices = []

    current_vert = 0

    for face_data in face_hash_3:
        mesh_new_vertices.extend([face_data[0], face_data[1], 0.0, face_data[2], face_data[3], 0.0, face_data[4], face_data[5], 0.0])
        mesh_new_face_vertices.extend([current_vert, current_vert + 1, current_vert + 2, 0])
        mesh_new_materials.append(face_data[6])
        current_vert += 3
    for face_data in face_hash_4:
        mesh_new_vertices.extend([face_data[0], face_data[1], 0.0, face_data[2], face_data[3], 0.0, face_data[4], face_data[5], 0.0, face_data[6], face_data[7], 0.0])
        mesh_new_face_vertices.extend([current_vert, current_vert + 1, current_vert + 2, current_vert + 3])
        mesh_new_materials.append(face_data[8])
        current_vert += 4

    mesh.vertices.add(len(mesh_new_vertices) // 3)
    mesh.faces.add(len(mesh_new_face_vertices) // 4)

    mesh.vertices.foreach_set("co", mesh_new_vertices)
    mesh.faces.foreach_set("vertices_raw", mesh_new_face_vertices)
    mesh.faces.foreach_set("material_index", mesh_new_materials)

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

    # scene render settings
    scene.render.use_raytrace = False
    scene.render.alpha_mode = 'STRAIGHT'
    scene.render.color_mode = 'RGBA'

    scene.render.resolution_x = image_width
    scene.render.resolution_y = image_height
    scene.render.resolution_percentage = 100

    if image_width > image_height:
        scene.render.pixel_aspect_y = image_width / image_height
    elif image_width < image_height:
        scene.render.pixel_aspect_x = image_height / image_width

    scene.frame_start = 1
    scene.frame_end = 1

    scene.render.file_format = 'PNG'
    scene.render.filepath = filepath

    data_context = {"blend_data": bpy.context.blend_data, "scene": scene}
    bpy.ops.render.render(data_context, write_still=True)

    # cleanup
    bpy.data.scenes.remove(scene)
    bpy.data.objects.remove(obj_cam)
    bpy.data.objects.remove(obj_solid)
    bpy.data.objects.remove(obj_wire)

    bpy.data.cameras.remove(cam)
    bpy.data.meshes.remove(mesh)

    bpy.data.materials.remove(material_wire)
    for mat_solid in material_solids:
        bpy.data.materials.remove(mat_solid)


from bpy.props import StringProperty, BoolProperty, EnumProperty, IntVectorProperty, FloatProperty


class ExportUVLayout(bpy.types.Operator):
    """Export UV layout to file"""

    bl_idname = "uv.export_layout"
    bl_label = "Export UV Layout"
    bl_options = {'REGISTER', 'UNDO'}

    filepath = StringProperty(name="File Path", description="File path used for exporting the SVG file", maxlen=1024, default="", subtype='FILE_PATH')
    check_existing = BoolProperty(name="Check Existing", description="Check and warn on overwriting existing files", default=True, options={'HIDDEN'})
    export_all = BoolProperty(name="All UV's", description="Export all UVs in this mesh (not just the visible ones)", default=False)
    mode = EnumProperty(items=(
                        ('SVG', "Scalable Vector Graphic (.svg)", "Export the UV layout to a vector SVG file"),
                        ('EPS', "Encapsulate PostScript (.eps)", "Export the UV layout to a vector EPS file"),
                        ('PNG', "PNG Image (.png)", "Export the UV layout a bitmap image")),
                name="Format",
                description="File format to export the UV layout to",
                default='PNG')
    size = IntVectorProperty(size=2, default=(1024, 1024), min=8, max=32768, description="Dimensions of the exported file")
    opacity = FloatProperty(name="Fill Opacity", min=0.0, max=1.0, default=0.25)

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and obj.data.uv_textures)

    def _space_image(self, context):
        space_data = context.space_data
        if isinstance(space_data, bpy.types.SpaceImageEditor):
            return space_data
        else:
            return None

    def _image_size(self, context, default_width=1024, default_height=1024):
        # fallback if not in image context.
        image_width, image_height = default_width, default_height

        space_data = self._space_image(context)
        if space_data:
            image = space_data.image
            if image:
                width, height = tuple(context.space_data.image.size)
                # incase no data is found.
                if width and height:
                    image_width, image_height = width, height

        return image_width, image_height

    def _face_uv_iter(self, context):
        obj = context.active_object
        mesh = obj.data
        uv_layer = mesh.uv_textures.active.data
        uv_layer_len = len(uv_layer)

        if not self.export_all:

            local_image = Ellipsis

            if context.tool_settings.show_uv_local_view:
                space_data = self._space_image(context)
                if space_data:
                    local_image = space_data.image

            faces = mesh.faces

            for i in range(uv_layer_len):
                uv_elem = uv_layer[i]
                # context checks
                if faces[i].select and (local_image is Ellipsis or local_image == uv_elem.image):
                    #~ uv = uv_elem.uv
                    #~ if False not in uv_elem.select_uv[:len(uv)]:
                    #~     yield (i, uv)

                    # just write what we see.
                    yield (i, uv_layer[i].uv)
        else:
            # all, simple
            for i in range(uv_layer_len):
                yield (i, uv_layer[i].uv)

    def execute(self, context):

        obj = context.active_object
        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        mesh = obj.data

        mode = self.mode

        filepath = self.filepath
        filepath = bpy.path.ensure_ext(filepath, "." + mode.lower())
        file = open(filepath, "w")
        fw = file.write

        if mode == 'SVG':
            func = write_svg
        elif mode == 'EPS':
            func = write_eps
        elif mode == 'PNG':
            func = write_png

        func(fw, mesh, self.size[0], self.size[1], self.opacity, self._face_uv_iter(context))

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

        file.close()

        return {'FINISHED'}

    def check(self, context):
        filepath = bpy.path.ensure_ext(self.filepath, "." + self.mode.lower())
        if filepath != self.filepath:
            self.filepath = filepath
            return True
        else:
            return False

    def invoke(self, context, event):
        import os
        self.size = self._image_size(context)
        self.filepath = os.path.splitext(bpy.data.filepath)[0]
        wm = context.window_manager
        wm.fileselect_add(self)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    self.layout.operator(ExportUVLayout.bl_idname)


def register():
    bpy.utils.register_module(__name__)
    bpy.types.IMAGE_MT_uvs.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.IMAGE_MT_uvs.remove(menu_func)

if __name__ == "__main__":
    register()
