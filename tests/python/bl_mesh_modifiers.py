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

# Currently this script only generates images from different modifier
# combinations and does not validate they work correctly,
# this is because we don't get 1:1 match with bmesh.
#
# Later, we may have a way to check the results are valid.


# ./blender.bin --factory-startup --python tests/python/bl_mesh_modifiers.py
#

import math

USE_QUICK_RENDER = False
IS_BMESH = hasattr(__import__("bpy").types, "LoopColors")

# -----------------------------------------------------------------------------
# utility functions


def render_gl(context, filepath, shade):

    def ctx_viewport_shade(context, shade):
        for area in context.window.screen.areas:
            if area.type == 'VIEW_3D':
                space = area.spaces.active
                # rv3d = space.region_3d
                space.viewport_shade = shade

    import bpy
    scene = context.scene
    render = scene.render
    render.filepath = filepath
    render.image_settings.file_format = 'PNG'
    render.image_settings.color_mode = 'RGB'
    render.use_file_extension = True
    render.use_antialiasing = False

    # render size
    render.resolution_percentage = 100
    render.resolution_x = 512
    render.resolution_y = 512

    ctx_viewport_shade(context, shade)

    #~ # stop to inspect!
    #~ if filepath == "test_cube_shell_solidify_subsurf_wp_wire":
        #~ assert(0)
    #~ else:
        #~ return

    bpy.ops.render.opengl(write_still=True,
                          view_context=True)


def render_gl_all_modes(context, obj, filepath=""):

    assert(obj is not None)
    assert(filepath != "")

    scene = context.scene

    # avoid drawing outline/center dot
    bpy.ops.object.select_all(action='DESELECT')
    scene.objects.active = None

    # editmode
    scene.tool_settings.mesh_select_mode = False, True, False

    # render
    render_gl(context, filepath + "_ob_solid", shade='SOLID')

    if USE_QUICK_RENDER:
        return

    render_gl(context, filepath + "_ob_wire", shade='WIREFRAME')
    render_gl(context, filepath + "_ob_textured", shade='TEXTURED')

    # -------------------------------------------------------------------------
    # not just draw modes, but object modes!
    scene.objects.active = obj

    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    bpy.ops.mesh.select_all(action='DESELECT')
    render_gl(context, filepath + "_edit_wire", shade='WIREFRAME')
    render_gl(context, filepath + "_edit_solid", shade='SOLID')
    render_gl(context, filepath + "_edit_textured", shade='TEXTURED')
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    bpy.ops.object.mode_set(mode='WEIGHT_PAINT', toggle=False)

    render_gl(context, filepath + "_wp_wire", shade='WIREFRAME')

    assert(1)

    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    scene.objects.active = None


def ctx_clear_scene():  # copied from batch_import.py
    import bpy
    unique_obs = set()
    for scene in bpy.data.scenes:
        for obj in scene.objects[:]:
            scene.objects.unlink(obj)
            unique_obs.add(obj)

    # remove obdata, for now only worry about the startup scene
    for bpy_data_iter in (bpy.data.objects,
                          bpy.data.meshes,
                          bpy.data.lamps,
                          bpy.data.cameras,
                          ):

        for id_data in bpy_data_iter:
            bpy_data_iter.remove(id_data)


def ctx_viewport_camera(context):
    # because gl render without view_context has no shading option.
    for area in context.window.screen.areas:
        if area.type == 'VIEW_3D':
            space = area.spaces.active
            space.region_3d.view_perspective = 'CAMERA'


def ctx_camera_setup(context,
                     location=(0.0, 0.0, 0.0),
                     lookat=(0.0, 0.0, 0.0),
                     # most likely the following vars can be left as defaults
                     up=(0.0, 0.0, 1.0),
                     lookat_axis='-Z',
                     up_axis='Y',
                     ):

    camera = bpy.data.cameras.new(whoami())
    obj = bpy.data.objects.new(whoami(), camera)

    scene = context.scene
    scene.objects.link(obj)
    scene.camera = obj

    from mathutils import Vector, Matrix

    # setup transform
    view_vec = Vector(lookat) - Vector(location)
    rot_mat = view_vec.to_track_quat(lookat_axis, up_axis).to_matrix().to_4x4()
    tra_mat = Matrix.Translation(location)

    obj.matrix_world = tra_mat * rot_mat

    ctx_viewport_camera(context)

    return obj


# -----------------------------------------------------------------------------
# inspect functions

import inspect


# functions

def whoami():
    return inspect.stack()[1][3]


def whosdaddy():
    return inspect.stack()[2][3]


# -----------------------------------------------------------------------------
# models (defaults)

def defaults_object(obj):
    obj.show_wire = True

    if obj.type == 'MESH':
        mesh = obj.data
        mesh.show_all_edges = True

        mesh.show_normal_vertex = True

        # lame!
        if IS_BMESH:
            for poly in mesh.polygons:
                poly.use_smooth = True
        else:
            for face in mesh.faces:
                face.use_smooth = True


def defaults_modifier(mod):
    mod.show_in_editmode = True
    mod.show_on_cage = True


# -----------------------------------------------------------------------------
# models (utils)


if IS_BMESH:
    def mesh_bmesh_poly_elems(poly, elems):
        vert_start = poly.loop_start
        vert_total = poly.loop_total
        return elems[vert_start:vert_start + vert_total]

    def mesh_bmesh_poly_vertices(poly):
        return [loop.vertex_index
                for loop in mesh_bmesh_poly_elems(poly, poly.id_data.loops)]


def mesh_bounds(mesh):
    xmin = ymin = zmin = +100000000.0
    xmax = ymax = zmax = -100000000.0

    for v in mesh.vertices:
        x, y, z = v.co
        xmax = max(x, xmax)
        ymax = max(y, ymax)
        zmax = max(z, zmax)

        xmin = min(x, xmin)
        ymin = min(y, ymin)
        zmin = min(z, zmin)

    return (xmin, ymin, zmin), (xmax, ymax, zmax)


def mesh_uv_add(obj):

    uvs = ((0.0, 0.0),
           (0.0, 1.0),
           (1.0, 1.0),
           (1.0, 0.0))

    uv_lay = obj.data.uv_textures.new()

    if IS_BMESH:
        # XXX, odd that we need to do this. until UV's and texface
        # are separated we will need to keep it
        uv_loops = obj.data.uv_layers[-1]
        uv_list = uv_loops.data[:]
        for poly in obj.data.polygons:
            poly_uvs = mesh_bmesh_poly_elems(poly, uv_list)
            for i, c in enumerate(poly_uvs):
                c.uv = uvs[i % 4]
    else:
        for uv in uv_lay.data:
            uv.uv1 = uvs[0]
            uv.uv2 = uvs[1]
            uv.uv3 = uvs[2]
            uv.uv4 = uvs[3]

    return uv_lay


def mesh_vcol_add(obj, mode=0):

    colors = ((0.0, 0.0, 0.0),  # black
              (1.0, 0.0, 0.0),  # red
              (0.0, 1.0, 0.0),  # green
              (0.0, 0.0, 1.0),  # blue
              (1.0, 1.0, 0.0),  # yellow
              (0.0, 1.0, 1.0),  # cyan
              (1.0, 0.0, 1.0),  # magenta
              (1.0, 1.0, 1.0),  # white
              )

    def colors_get(i):
        return colors[i % len(colors)]

    vcol_lay = obj.data.vertex_colors.new()

    mesh = obj.data

    if IS_BMESH:
        col_list = vcol_lay.data[:]
        for poly in mesh.polygons:
            face_verts = mesh_bmesh_poly_vertices(poly)
            poly_cols = mesh_bmesh_poly_elems(poly, col_list)
            for i, c in enumerate(poly_cols):
                c.color = colors_get(face_verts[i])
    else:
        for i, col in enumerate(vcol_lay.data):
            face_verts = mesh.faces[i].vertices
            col.color1 = colors_get(face_verts[0])
            col.color2 = colors_get(face_verts[1])
            col.color3 = colors_get(face_verts[2])
            if len(face_verts) == 4:
                col.color4 = colors_get(face_verts[3])

    return vcol_lay


def mesh_vgroup_add(obj, name="Group", axis=0, invert=False, mode=0):
    mesh = obj.data
    vgroup = obj.vertex_groups.new(name=name)
    vgroup.add(list(range(len(mesh.vertices))), 1.0, 'REPLACE')
    group_index = len(obj.vertex_groups) - 1

    min_bb, max_bb = mesh_bounds(mesh)

    range_axis = max_bb[axis] - min_bb[axis]

    # gradient
    for v in mesh.vertices:
        for vg in v.groups:
            if vg.group == group_index:
                f = (v.co[axis] - min_bb[axis]) / range_axis
                vg.weight = 1.0 - f if invert else f

    return vgroup


def mesh_shape_add(obj, mode=0):
    pass


def mesh_armature_add(obj, mode=0):
    pass


# -----------------------------------------------------------------------------
# modifiers

def modifier_subsurf_add(scene, obj, levels=2):
    mod = obj.modifiers.new(name=whoami(), type='SUBSURF')
    defaults_modifier(mod)

    mod.levels = levels
    mod.render_levels = levels
    return mod


def modifier_armature_add(scene, obj):
    mod = obj.modifiers.new(name=whoami(), type='ARMATURE')
    defaults_modifier(mod)

    arm_data = bpy.data.armatures.new(whoami())
    obj_arm = bpy.data.objects.new(whoami(), arm_data)

    scene.objects.link(obj_arm)

    obj_arm.select = True
    scene.objects.active = obj_arm

    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
    bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    # XXX, annoying, remove bone.
    while arm_data.edit_bones:
        obj_arm.edit_bones.remove(arm_data.edit_bones[-1])

    bone_a = arm_data.edit_bones.new("Bone.A")
    bone_b = arm_data.edit_bones.new("Bone.B")
    bone_b.parent = bone_a

    bone_a.head = -1, 0, 0
    bone_a.tail = 0, 0, 0
    bone_b.head = 0, 0, 0
    bone_b.tail = 1, 0, 0

    # Get armature animation data
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    # 45d armature
    obj_arm.pose.bones["Bone.B"].rotation_quaternion = 1, -0.5, 0, 0

    # set back to the original
    scene.objects.active = obj

    # display options
    obj_arm.show_x_ray = True
    arm_data.draw_type = 'STICK'

    # apply to modifier
    mod.object = obj_arm

    mesh_vgroup_add(obj, name="Bone.A", axis=0, invert=True)
    mesh_vgroup_add(obj, name="Bone.B", axis=0, invert=False)

    return mod


def modifier_mirror_add(scene, obj):
    mod = obj.modifiers.new(name=whoami(), type='MIRROR')
    defaults_modifier(mod)

    return mod


def modifier_solidify_add(scene, obj, thickness=0.25):
    mod = obj.modifiers.new(name=whoami(), type='SOLIDIFY')
    defaults_modifier(mod)

    mod.thickness = thickness

    return mod


def modifier_hook_add(scene, obj, use_vgroup=True):
    scene.objects.active = obj

    # no nice way to add hooks from py api yet
    # assume object mode, hook first face!
    mesh = obj.data

    if use_vgroup:
        for v in mesh.vertices:
            v.select = True
    else:
        for v in mesh.vertices:
            v.select = False

        for i in mesh.faces[0].vertices:
            mesh.vertices[i].select = True

    bpy.ops.object.mode_set(mode='EDIT', toggle=False)
    bpy.ops.object.hook_add_newob()
    bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

    # mod = obj.modifiers.new(name=whoami(), type='HOOK')
    mod = obj.modifiers[-1]
    defaults_modifier(mod)

    obj_hook = mod.object
    obj_hook.rotation_euler = 0, math.radians(45), 0
    obj_hook.show_x_ray = True

    if use_vgroup:
        mod.vertex_group = obj.vertex_groups[0].name

    return mod


def modifier_decimate_add(scene, obj):
    mod = obj.modifiers.new(name=whoami(), type='DECIMATE')
    defaults_modifier(mod)

    mod.ratio = 1 / 3

    return mod


def modifier_build_add(scene, obj):
    mod = obj.modifiers.new(name=whoami(), type='BUILD')
    defaults_modifier(mod)

    # ensure we display some faces
    if IS_BMESH:
        totface = len(obj.data.polygons)
    else:
        totface = len(obj.data.faces)

    mod.frame_start = totface // 2
    mod.frame_duration = totface

    return mod


def modifier_mask_add(scene, obj):
    mod = obj.modifiers.new(name=whoami(), type='MASK')
    defaults_modifier(mod)

    mod.vertex_group = obj.vertex_groups[0].name

    return mod


# -----------------------------------------------------------------------------
# models

# useful since its solid boxy shape but simple enough to debug errors
cube_like_vertices = (
    (1, 1, -1),
    (1, -1, -1),
    (-1, -1, -1),
    (-1, 1, -1),
    (1, 1, 1),
    (1, -1, 1),
    (-1, -1, 1),
    (-1, 1, 1),
    (0, -1, -1),
    (1, 0, -1),
    (0, 1, -1),
    (-1, 0, -1),
    (1, 0, 1),
    (0, -1, 1),
    (-1, 0, 1),
    (0, 1, 1),
    (1, -1, 0),
    (1, 1, 0),
    (-1, -1, 0),
    (-1, 1, 0),
    (0, 0, -1),
    (0, 0, 1),
    (1, 0, 0),
    (0, -1, 0),
    (-1, 0, 0),
    (2, 0, 0),
    (2, 0, -1),
    (2, 1, 0),
    (2, 1, -1),
    (0, 1, 2),
    (0, 0, 2),
    (-1, 0, 2),
    (-1, 1, 2),
    (-1, 0, 3),
    (-1, 1, 3),
    (0, 1, 3),
    (0, 0, 3),
    )


cube_like_faces = (
    (0, 9, 20, 10),
    (0, 10, 17),
    (0, 17, 27, 28),
    (1, 16, 23, 8),
    (2, 18, 24, 11),
    (3, 19, 10),
    (4, 15, 21, 12),
    (4, 17, 15),
    (7, 14, 31, 32),
    (7, 15, 19),
    (8, 23, 18, 2),
    (9, 0, 28, 26),
    (9, 1, 8, 20),
    (9, 22, 16, 1),
    (10, 20, 11, 3),
    (11, 24, 19, 3),
    (12, 21, 13, 5),
    (13, 6, 18),
    (14, 21, 30, 31),
    (15, 7, 32, 29),
    (15, 17, 10, 19),
    (16, 5, 13, 23),
    (17, 4, 12, 22),
    (17, 22, 25, 27),
    (18, 6, 14, 24),
    (20, 8, 2, 11),
    (21, 14, 6, 13),
    (21, 15, 29, 30),
    (22, 9, 26, 25),
    (22, 12, 5, 16),
    (23, 13, 18),
    (24, 14, 7, 19),
    (28, 27, 25, 26),
    (29, 32, 34, 35),
    (30, 29, 35, 36),
    (31, 30, 36, 33),
    (32, 31, 33, 34),
    (35, 34, 33, 36),
    )


# useful since its a shell for solidify and it can be mirrored
cube_shell_vertices = (
    (0, 0, 1),
    (0, 1, 1),
    (-1, 1, 1),
    (-1, 0, 1),
    (0, 0, 0),
    (0, 1, 0),
    (-1, 1, 0),
    (-1, 0, 0),
    (-1, -1, 0),
    (0, -1, 0),
    (0, 0, -1),
    (0, 1, -1),
    )


cube_shell_face = (
    (0, 1, 2, 3),
    (0, 3, 8, 9),
    (1, 5, 6, 2),
    (2, 6, 7, 3),
    (3, 7, 8),
    (4, 7, 10),
    (6, 5, 11),
    (7, 4, 9, 8),
    (10, 7, 6, 11),
    )


def make_cube(scene):
    bpy.ops.mesh.primitive_cube_add(view_align=False,
                                    enter_editmode=False,
                                    location=(0, 0, 0),
                                    rotation=(0, 0, 0),
                                    )

    obj = scene.objects.active

    defaults_object(obj)
    return obj


def make_cube_extra(scene):
    obj = make_cube(scene)

    # extra data layers
    mesh_uv_add(obj)
    mesh_vcol_add(obj)
    mesh_vgroup_add(obj)

    return obj


def make_cube_like(scene):
    mesh = bpy.data.meshes.new(whoami())

    mesh.from_pydata(cube_like_vertices, (), cube_like_faces)
    mesh.update()  # add edges
    obj = bpy.data.objects.new(whoami(), mesh)
    scene.objects.link(obj)

    defaults_object(obj)
    return obj


def make_cube_like_extra(scene):
    obj = make_cube_like(scene)

    # extra data layers
    mesh_uv_add(obj)
    mesh_vcol_add(obj)
    mesh_vgroup_add(obj)

    return obj


def make_cube_shell(scene):
    mesh = bpy.data.meshes.new(whoami())

    mesh.from_pydata(cube_shell_vertices, (), cube_shell_face)
    mesh.update()  # add edges
    obj = bpy.data.objects.new(whoami(), mesh)
    scene.objects.link(obj)

    defaults_object(obj)
    return obj


def make_cube_shell_extra(scene):
    obj = make_cube_shell(scene)

    # extra data layers
    mesh_uv_add(obj)
    mesh_vcol_add(obj)
    mesh_vgroup_add(obj)

    return obj


def make_monkey(scene):
    bpy.ops.mesh.primitive_monkey_add(view_align=False,
                                      enter_editmode=False,
                                      location=(0, 0, 0),
                                      rotation=(0, 0, 0),
                                      )
    obj = scene.objects.active

    defaults_object(obj)
    return obj


def make_monkey_extra(scene):
    obj = make_monkey(scene)

    # extra data layers
    mesh_uv_add(obj)
    mesh_vcol_add(obj)
    mesh_vgroup_add(obj)

    return obj


# -----------------------------------------------------------------------------
# tests (utils)

global_tests = []

global_tests.append(("none",
    (),
    ))

# single
global_tests.append(("subsurf_single",
    ((modifier_subsurf_add, dict(levels=2)), ),
    ))


global_tests.append(("armature_single",
    ((modifier_armature_add, dict()), ),
    ))


global_tests.append(("mirror_single",
    ((modifier_mirror_add, dict()), ),
    ))

global_tests.append(("hook_single",
    ((modifier_hook_add, dict()), ),
    ))

global_tests.append(("decimate_single",
    ((modifier_decimate_add, dict()), ),
    ))

global_tests.append(("build_single",
    ((modifier_build_add, dict()), ),
    ))

global_tests.append(("mask_single",
    ((modifier_mask_add, dict()), ),
    ))


# combinations
global_tests.append(("mirror_subsurf",
    ((modifier_mirror_add, dict()),
     (modifier_subsurf_add, dict(levels=2))),
    ))

global_tests.append(("solidify_subsurf",
    ((modifier_solidify_add, dict()),
     (modifier_subsurf_add, dict(levels=2))),
    ))


def apply_test(test, scene, obj,
               render_func=None,
               render_args=None,
               render_kwargs=None,
               ):

    test_name, test_funcs = test

    for cb, kwargs in test_funcs:
        cb(scene, obj, **kwargs)

    render_kwargs_copy = render_kwargs.copy()

    # add test name in filepath
    render_kwargs_copy["filepath"] += "_%s" % test_name

    render_func(*render_args, **render_kwargs_copy)


# -----------------------------------------------------------------------------
# tests themselves!
# having the 'test_' prefix automatically means these functions are called
# for testing


def test_cube(context, test):
    scene = context.scene
    obj = make_cube_extra(scene)
    ctx_camera_setup(context, location=(3, 3, 3))

    apply_test(test, scene, obj,
               render_func=render_gl_all_modes,
               render_args=(context, obj),
               render_kwargs=dict(filepath=whoami()))


def test_cube_like(context, test):
    scene = context.scene
    obj = make_cube_like_extra(scene)
    ctx_camera_setup(context, location=(5, 5, 5))

    apply_test(test, scene, obj,
               render_func=render_gl_all_modes,
               render_args=(context, obj),
               render_kwargs=dict(filepath=whoami()))


def test_cube_shell(context, test):
    scene = context.scene
    obj = make_cube_shell_extra(scene)
    ctx_camera_setup(context, location=(4, 4, 4))

    apply_test(test, scene, obj,
               render_func=render_gl_all_modes,
               render_args=(context, obj),
               render_kwargs=dict(filepath=whoami()))


# -----------------------------------------------------------------------------
# call all tests

def main():
    print("Calling main!")
    #render_gl(bpy.context, "/testme")
    #ctx_clear_scene()

    context = bpy.context

    ctx_clear_scene()

    # run all tests
    for key, val in sorted(globals().items()):
        if key.startswith("test_") and hasattr(val, "__call__"):
            print("calling:", key)
            for t in global_tests:
                val(context, test=t)
                ctx_clear_scene()


# -----------------------------------------------------------------------------
# annoying workaround for theme initialization

if __name__ == "__main__":
    import bpy
    from bpy.app.handlers import persistent

    @persistent
    def load_handler(dummy):
        print("Load Handler:", bpy.data.filepath)
        if load_handler.first is False:
            bpy.app.handlers.scene_update_post.remove(load_handler)
            try:
                main()
                import sys
                sys.exit(0)
            except:
                import traceback
                traceback.print_exc()

                # import sys
                # sys.exit(1)  # comment to debug

        else:
            load_handler.first = False

    load_handler.first = True
    bpy.app.handlers.scene_update_post.append(load_handler)
