# gpl author: Pistiwique

bl_info = {
    "name": "Mesh Check BGL edition",
    "description": "Display the triangles and ngons of the mesh",
    "author": "Pistiwique",
    "version": (1, 0, 1),
    "blender": (2, 75, 0),
    "location": "3D View(s) > Properties > Shading",
    "category": "3D View"
    }

import bpy
import bmesh
from bgl import (
        glBegin,
        glLineWidth,
        glColor4f,
        glVertex3f,
        glEnd,
        GL_LINES,
        glEnable,
        glDisable,
        GL_DEPTH_TEST,
        GL_BLEND,
        GL_POLYGON
        )
from mathutils.geometry import tessellate_polygon as tessellate
from bpy.types import (
        Operator,
        PropertyGroup,
        )
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        FloatVectorProperty,
        PointerProperty,
        )

# -- Globals -- #
mesh_check_handle = []
draw_enabled = [False]
edge_width = [1.0]
face_opacity = [0.2]
edges_tri_color = [(1.0, 1.0, 0.0, 1)]
faces_tri_color = [(1.0, 1.0, 0.0, face_opacity[0])]
edges_ngons_color = [(1.0, 0.0, 0.0, 1.0)]
faces_ngons_color = [(1.0, 0.0, 0.0, face_opacity[0])]
bm_old = [None]
finer_lines = [False]


def draw_poly(points):
    for i in range(len(points)):
        glVertex3f(points[i][0], points[i][1], points[i][2])


def mesh_check_draw_callback():
    obj = bpy.context.object
    if obj and obj.type == 'MESH':
        if draw_enabled[0]:
            mesh = obj.data
            matrix_world = obj.matrix_world

            glLineWidth(edge_width[0])

            if bpy.context.mode == 'EDIT_MESH':
                use_occlude = True

                if bm_old[0] is None or not bm_old[0].is_valid:
                    bm = bm_old[0] = bmesh.from_edit_mesh(mesh)
                else:
                    bm = bm_old[0]

                no_depth = not bpy.context.space_data.use_occlude_geometry

                if no_depth:
                    glDisable(GL_DEPTH_TEST)

                    use_occlude = False

                    if finer_lines[0]:
                        glLineWidth(edge_width[0] / 4.0)
                        use_occlude = True

                    for face in bm.faces:
                        if len([verts for verts in face.verts]) == 3:
                            faces = [matrix_world * vert.co for vert in face.verts]
                            glColor4f(*faces_tri_color[0])
                            glEnable(GL_BLEND)
                            glBegin(GL_POLYGON)
                            draw_poly(faces)
                            glEnd()

                            for edge in face.edges:
                                if edge.is_valid:
                                    edges = [matrix_world * vert.co for vert in edge.verts]
                                    glColor4f(*edges_tri_color[0])
                                    glBegin(GL_LINES)
                                    draw_poly(edges)
                                    glEnd()

                        elif len([verts for verts in face.verts]) > 4:
                            new_faces = []
                            faces = []
                            coords = [v.co for v in face.verts]
                            indices = [v.index for v in face.verts]
                            for pol in tessellate([coords]):
                                new_faces.append([indices[i] for i in pol])

                            for f in new_faces:
                                faces.append(
                                        [((matrix_world * bm.verts[i].co)[0] + face.normal.x * 0.001,
                                        (matrix_world * bm.verts[i].co)[1] + face.normal.y * 0.001,
                                        (matrix_world * bm.verts[i].co)[2] + face.normal.z * 0.001)
                                        for i in f]
                                        )

                            for f in faces:
                                glColor4f(*faces_ngons_color[0])
                                glEnable(GL_BLEND)
                                glBegin(GL_POLYGON)
                                draw_poly(f)
                                glEnd()

                            for edge in face.edges:
                                if edge.is_valid:
                                    edges = [matrix_world * vert.co for vert in edge.verts]
                                    glColor4f(*edges_ngons_color[0])
                                    glBegin(GL_LINES)
                                    draw_poly(edges)
                                    glEnd()

                    glDisable(GL_BLEND)
                    glColor4f(0.0, 0.0, 0.0, 1.0)
                    glLineWidth(edge_width[0])
                    glEnable(GL_DEPTH_TEST)

                if use_occlude:

                    for face in bm.faces:
                        if len([verts for verts in face.verts]) == 3:
                            faces = []
                            for vert in face.verts:
                                vert_face = matrix_world * vert.co
                                faces.append(
                                        (vert_face[0] + face.normal.x * 0.001,
                                        vert_face[1] + face.normal.y * 0.001,
                                        vert_face[2] + face.normal.z * 0.001)
                                        )

                            glColor4f(*faces_tri_color[0])
                            glEnable(GL_BLEND)
                            glBegin(GL_POLYGON)
                            draw_poly(faces)
                            glEnd()

                            for edge in face.edges:
                                if edge.is_valid:
                                    edges = []
                                    for vert in edge.verts:
                                        vert_edge = matrix_world * vert.co
                                        edges.append(
                                            (vert_edge[0] + face.normal.x * 0.001,
                                            vert_edge[1] + face.normal.y * 0.001,
                                            vert_edge[2] + face.normal.z * 0.001)
                                            )
                                    glColor4f(*edges_tri_color[0])
                                    glBegin(GL_LINES)
                                    draw_poly(edges)
                                    glEnd()

                        elif len([verts for verts in face.verts]) > 4:
                            new_faces = []
                            faces = []
                            coords = [v.co for v in face.verts]
                            indices = [v.index for v in face.verts]
                            for pol in tessellate([coords]):
                                new_faces.append([indices[i] for i in pol])

                            for f in new_faces:
                                faces.append([
                                        ((matrix_world * bm.verts[i].co)[0] + face.normal.x * 0.001,
                                         (matrix_world * bm.verts[i].co)[1] + face.normal.y * 0.001,
                                         (matrix_world * bm.verts[i].co)[2] + face.normal.z * 0.001)
                                        for i in f]
                                        )

                            for f in faces:
                                glColor4f(*faces_ngons_color[0])
                                glEnable(GL_BLEND)
                                glBegin(GL_POLYGON)
                                draw_poly(f)
                                glEnd()

                            for edge in face.edges:
                                if edge.is_valid:
                                    edges = []
                                    for vert in edge.verts:
                                        vert_edge = matrix_world * vert.co
                                        edges.append(
                                                (vert_edge[0] + face.normal.x * 0.001,
                                                vert_edge[1] + face.normal.y * 0.001,
                                                vert_edge[2] + face.normal.z * 0.001)
                                                )
                                    glColor4f(*edges_ngons_color[0])
                                    glBegin(GL_LINES)
                                    draw_poly(edges)
                                    glEnd()

                    glDisable(GL_BLEND)
                    glColor4f(0.0, 0.0, 0.0, 1.0)


def updateBGLData(self, context):
    if self.mesh_check_use and self.display_faces:
        bpy.ops.object.mode_set(mode='EDIT')
        draw_enabled[0] = True
        edge_width[0] = self.edge_width
        finer_lines[0] = self.finer_lines_behind_use
        face_opacity[0] = self.face_opacity
        edges_tri_color[0] = (
                self.custom_tri_color[0],
                self.custom_tri_color[1],
                self.custom_tri_color[2],
                1)
        faces_tri_color[0] = (
                self.custom_tri_color[0],
                self.custom_tri_color[1],
                self.custom_tri_color[2],
                self.face_opacity
                )
        edges_ngons_color[0] = (
                self.custom_ngons_color[0],
                self.custom_ngons_color[1],
                self.custom_ngons_color[2],
                1)
        faces_ngons_color[0] = (
                self.custom_ngons_color[0],
                self.custom_ngons_color[1],
                self.custom_ngons_color[2],
                self.face_opacity
                )
        return

    draw_enabled[0] = False


class FaceTypeSelect(Operator):
    bl_idname = "object.face_type_select"
    bl_label = "Face type select"
    bl_description = "Select Triangles and / or Ngons on the Active Object"
    bl_options = {'REGISTER', 'UNDO'}

    face_type = EnumProperty(
            name="Face Type",
            items=(('tris', "Tris", "Colorize Triangles in the Mesh"),
                   ('ngons', "Ngons", "Colorize Ngons in the Mesh")),
            default='ngons'
            )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None and context.active_object.type == 'MESH'

    def execute(self, context):
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='DESELECT')
        context.tool_settings.mesh_select_mode = (False, False, True)

        if self.face_type == "tris":
            bpy.ops.mesh.select_face_by_sides(number=3, type='EQUAL')
        else:
            bpy.ops.mesh.select_face_by_sides(number=4, type='GREATER')

        return {'FINISHED'}


class MeshCheckCollectionGroup(PropertyGroup):
    mesh_check_use = BoolProperty(
            name="Mesh Check",
            description="Display Mesh Check options",
            default=False,
            update=updateBGLData
            )
    display_faces = BoolProperty(
            name="Display Faces",
            description="Use BGL to display Ngons and Tris of the mesh",
            default=False,
            update=updateBGLData
            )
    edge_width = FloatProperty(
            name="Width",
            description="Drawn Edges width in pixels",
            min=1.0,
            max=10.0,
            default=3.0,
            subtype='PIXEL',
            update=updateBGLData
            )
    finer_lines_behind_use = BoolProperty(
            name="Finer Lines behind",
            description="Display partially hidden edges finer in non-occlude mode",
            default=True,
            update=updateBGLData
            )
    custom_tri_color = FloatVectorProperty(
            name="Tri Color",
            description="Custom color for the Triangles",
            min=0.0,
            max=1.0,
            default=(1.0, 1.0, 0.0),
            size=3,
            subtype='COLOR',
            update=updateBGLData
            )
    custom_ngons_color = FloatVectorProperty(
            name="Ngons Color",
            description="Custom color for the Ngons",
            min=0.0,
            max=1.0,
            default=(1.0, 0.0, 0.0),
            size=3,
            subtype='COLOR',
            update=updateBGLData
            )
    face_opacity = FloatProperty(
            name="Face Opacity",
            description="Opacity of the color for the face",
            min=0.0,
            max=1.0,
            default=0.2,
            subtype='FACTOR',
            update=updateBGLData
            )


# Register
classes = (
    FaceTypeSelect,
    MeshCheckCollectionGroup,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.WindowManager.mesh_check = PointerProperty(
                                                type=MeshCheckCollectionGroup
                                                )
    if mesh_check_handle:
        bpy.types.SpaceView3D.draw_handler_remove(mesh_check_handle[0], 'WINDOW')
    mesh_check_handle[:] = [bpy.types.SpaceView3D.draw_handler_add(mesh_check_draw_callback,
                            (), 'WINDOW', 'POST_VIEW')]


def unregister():
    del bpy.types.WindowManager.mesh_check
    if mesh_check_handle:
        bpy.types.SpaceView3D.draw_handler_remove(mesh_check_handle[0], 'WINDOW')
        mesh_check_handle[:] = []

    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
