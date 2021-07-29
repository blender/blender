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
from bpy.types import (
        Operator,
        Panel,
        )
import os
from math import (
        degrees, tan,
        radians,
        )
from mathutils import Vector

__bpydoc__ = """
Light Field Tools

This script helps setting up rendering of lightfields. It
also supports the projection of lightfields with textured
spotlights.

Usage:
A simple interface can be accessed in the tool shelf panel
in 3D View ([T] Key).

A base mesh has to be provided, which will normaly be a
subdivided plane. The script will then create a camera rig
and a light rig with adjustable properties. A sample camera
and a spotlight will be created on each vertex of the
basemesh object axis (maybe vertex normal in future
versions).

    Vertex order:
        The user has to provide the number of cameras or
        lights in one row in an unevenly spaced grid, the
        basemesh. Then the right vertex order can be
        computed as shown here.
         6-7-8
         | | |
       ^ 3-4-5
       | | | |
       y 0-1-2
         x->

There is also a tool to create a basemesh, which is an
evenly spaced grid. The row length parameter is taken to
construct such a NxN grid. Someone would start out by adding
a rectengular plane as the slice plane of the frustrum of
the most middle camera of the light field rig. The spacing
parameter then places the other cameras in a way, so they
have an offset of n pixels from the other camera on this
plane.


Version history:
v0.3.0 - Make compatible with 2.64
v0.2.1 - Empty handler, multiple camera grid, r34843
v0.2.0 - To be included in contrib, r34456
v0.1.4 - To work with r34261
v0.1.3 - Fixed base mesh creation for r29998
v0.1.2 - Minor fixes, working with r29994
v0.1.1 - Basemesh from focal plane.
v0.1.0 - API updates, draft done.
v0.0.4 - Texturing.
v0.0.3 - Creates an array of non textured spotlights.
v0.0.2 - Renders lightfields.
v0.0.1 - Initial version.

TODO:
* Restore view after primary camera is changed.
* Apply object matrix to normals.
* Allign to normals, somehow,....
* StringProperties with PATH tag, for proper ui.
"""


class OBJECT_OT_create_lightfield_rig(Operator):
    bl_idname = "object.create_lightfield_rig"
    bl_label = "Create a light field rig"
    bl_description = "Create a lightfield rig based on the active object/mesh"
    bl_options = {'REGISTER'}

    layer0 = [True] + [False] * 19

    numSamples = 0
    baseObject = None
    handler = None
    verts = []
    imagePaths = []

    def arrangeVerts(self):
        """Sorts the vertices as described in the usage part of the doc."""
        # FIXME get mesh with applied modifer stack
        scene = bpy.context.scene
        mesh = self.baseObject.data
        verts = []
        row_length = scene.lightfield.row_length
        matrix = self.baseObject.matrix_local.copy()
        for vert in mesh.vertices:
            # world/parent origin
            # ???, normal and co are in different spaces, sure you want this?
            co = matrix * vert.co
            normal = vert.normal
            verts.append([co, normal])

        def key_x(v):
            return v[0][0]

        def key_y(v):
            return v[0][1]

        verts.sort(key=key_y)
        sorted_verts = []
        for i in range(0, len(verts), row_length):
            row = verts[i: i + row_length]
            row.sort(key=key_x)
            sorted_verts.extend(row)

        return sorted_verts

    def createCameraAnimated(self):
        scene = bpy.context.scene

        bpy.ops.object.camera_add(view_align=False)
        cam = bpy.context.active_object
        cam.name = "light_field_camera"

        # set props
        cam.data.angle = scene.lightfield.angle

        # display options of the camera
        cam.data.lens_unit = 'FOV'

        # handler parent
        if scene.lightfield.create_handler:
            cam.parent = self.handler

        # set as primary camera
        scene.camera = cam

        # animate
        scene.frame_current = 0

        for frame, vert in enumerate(self.verts):
            scene.frame_current = frame
            # translate
            cam.location = vert[0]
            # rotation
            cam.rotation_euler = self.baseObject.rotation_euler
            # insert LocRot keyframes
            cam.keyframe_insert('location')

        # set anim render props
        scene.frame_current = 0
        scene.frame_start = 0
        scene.frame_end = self.numSamples - 1

    def createCameraMultiple(self):
        scene = bpy.context.scene

        for cam_idx, vert in enumerate(self.verts):
            # add and name camera
            bpy.ops.object.camera_add(view_align=False)
            cam = bpy.context.active_object
            cam.name = "light_field_cam_" + str(cam_idx)

            # translate
            cam.location = vert[0]
            # rotation
            cam.rotation_euler = self.baseObject.rotation_euler

            # set camera props
            cam.data.angle = scene.lightfield.angle

            # display options of the camera
            cam.data.draw_size = 0.15
            cam.data.lens_unit = 'FOV'

            # handler parent
            if scene.lightfield.create_handler:
                cam.parent = self.handler

    def createCamera(self):
        if bpy.context.scene.lightfield.animate_camera:
            self.createCameraAnimated()
        else:
            self.createCameraMultiple()

    def getImagePaths(self):
        path = bpy.context.scene.lightfield.texture_path
        if not os.path.isdir(path):
            return False
        files = os.listdir(path)
        if not len(files) == self.numSamples:
            return False
        files.sort()
        self.imagePaths = list(map(lambda f: os.path.join(path, f), files))
        return True

    def createTexture(self, index):
        name = "light_field_spot_tex_" + str(index)
        tex = bpy.data.textures.new(name, type='IMAGE')

        # load and set the image
        # FIXME width, height. not necessary to set in the past.
        img = bpy.data.images.new("lfe_str_" + str(index), width=5, height=5)
        img.filepath = self.imagePaths[index]
        img.source = 'FILE'
        tex.image = img

        return tex

    def createSpot(self, index, textured=False):
        scene = bpy.context.scene
        bpy.ops.object.lamp_add(
                type='SPOT')
        spot = bpy.context.active_object

        # set object props
        spot.name = "light_field_spot_" + str(index)

        # set constants
        spot.data.use_square = True
        spot.data.shadow_method = "RAY_SHADOW"
        # FIXME
        spot.data.distance = 10

        # set spot props
        spot.data.energy = scene.lightfield.light_intensity / self.numSamples
        spot.data.spot_size = scene.lightfield.angle
        spot.data.spot_blend = scene.lightfield.spot_blend

        # add texture
        if textured:
            spot.data.active_texture = self.createTexture(index)
            # texture mapping
            spot.data.texture_slots[0].texture_coords = 'VIEW'

        # handler parent
        if scene.lightfield.create_handler:
            spot.parent = self.handler

        return spot

    def createLightfieldEmitter(self, textured=False):
        for i, vert in enumerate(self.verts):
            spot = self.createSpot(i, textured)
            spot.location = vert[0]
            spot.rotation_euler = self.baseObject.rotation_euler

    def execute(self, context):
        scene = context.scene

        obj = self.baseObject = context.active_object
        if not obj or obj.type != 'MESH':
            self.report({'ERROR'}, "No selected mesh object!")
            return {'CANCELLED'}

        self.verts = self.arrangeVerts()
        self.numSamples = len(self.verts)

        if scene.lightfield.create_handler:
            # create an empty
            bpy.ops.object.add(type='EMPTY')
            empty = bpy.context.active_object
            empty.location = self.baseObject.location
            empty.name = "light_field_handler"
            empty.rotation_euler = self.baseObject.rotation_euler
            self.handler = empty

        if scene.lightfield.do_camera:
            self.createCamera()

        if scene.lightfield.do_projection:
            if self.getImagePaths():
                self.createLightfieldEmitter(textured=True)
            else:
                self.createLightfieldEmitter(textured=False)

        return {'FINISHED'}


class OBJECT_OT_create_lightfield_basemesh(Operator):
    bl_idname = "object.create_lightfield_basemesh"
    bl_label = "Create a basemesh from the selected focal plane"
    bl_description = "Creates a basemesh from the selected focal plane"
    bl_options = {'REGISTER'}

    objName = "lf_basemesh"

    def getWidth(self, obj):
        mat = obj.matrix_local
        mesh = obj.data
        v0 = mat * mesh.vertices[mesh.edges[0].vertices[0]].co
        v1 = mat * mesh.vertices[mesh.edges[0].vertices[1]].co
        return (v0 - v1).length

    def getCamVec(self, obj, angle):
        width = self.getWidth(obj)
        itmat = obj.matrix_local.inverted().transposed()
        normal = itmat * obj.data.polygons[0].normal.normalized()
        vl = (width / 2) * (1 / tan(radians(angle / 2)))
        return normal * vl

    def addMeshObj(self, mesh):
        scene = bpy.context.scene

        for o in scene.objects:
            o.select = False

        mesh.update()
        nobj = bpy.data.objects.new(self.objName, mesh)
        scene.objects.link(nobj)
        nobj.select = True

        if scene.objects.active is None or scene.objects.active.mode == 'OBJECT':
            scene.objects.active = nobj

    def execute(self, context):
        scene = context.scene
        obj = context.active_object
        # check if active object is a mesh object
        if not obj or obj.type != 'MESH':
            self.report({'ERROR'}, "No selected mesh object!")
            return {'CANCELLED'}

        # check if it has one single face
        if len(obj.data.polygons) != 1:
            self.report({'ERROR'}, "The selected mesh object has to have exactly one quad!")
            return {'CANCELLED'}

        rl = scene.lightfield.row_length
        # use a degree angle here
        angle = degrees(scene.lightfield.angle)
        spacing = scene.lightfield.spacing
        # resolution of final renderings
        res = round(scene.render.resolution_x * (scene.render.resolution_percentage / 100.))
        width = self.getWidth(obj)

        # the offset between n pixels on the focal plane
        fplane_offset = (width / res) * spacing

        # vertices for the basemesh
        verts = []
        # the offset vector
        vec = self.getCamVec(obj, angle)
        # lower left coordinates of the grid
        sx = obj.location[0] - fplane_offset * int(rl / 2)
        sy = obj.location[1] - fplane_offset * int(rl / 2)
        z = obj.location[2]
        # position on the focal plane
        fplane_pos = Vector()
        for x in [sx + fplane_offset * i for i in range(rl)]:
            for y in [sy + fplane_offset * i for i in range(rl)]:
                fplane_pos.x = x
                fplane_pos.y = y
                fplane_pos.z = z
                # position of a vertex in a basemesh
                pos = fplane_pos + vec
                # pack coordinates flat into the vert list
                verts.append((pos.x, pos.y, pos.z))

        # setup the basemesh and add verts
        mesh = bpy.data.meshes.new(self.objName)
        mesh.from_pydata(verts, [], [])
        self.addMeshObj(mesh)

        return {'FINISHED'}


class VIEW3D_OT_lightfield_tools(Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_context = "objectmode"
    bl_label = "Light Field Tools"
    bl_category = "Tools"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        col = layout.column()
        col.prop(scene.lightfield, "row_length")
        col.prop(scene.lightfield, "angle")

        col.prop(scene.lightfield, "create_handler")

        col.prop(scene.lightfield, "do_camera")
        col.prop(scene.lightfield, "animate_camera")
        col.prop(scene.lightfield, "do_projection")

        col = layout.column(align=True)
        col.enabled = scene.lightfield.do_projection
        col.prop(scene.lightfield, "texture_path")
        col.prop(scene.lightfield, "light_intensity")
        col.prop(scene.lightfield, "spot_blend")

        # create a basemesh
        col = layout.column(align=True)
        col.operator("object.create_lightfield_basemesh", text="Create Base Grid")
        col.prop(scene.lightfield, "spacing")

        layout.operator("object.create_lightfield_rig", text="Create Rig")
