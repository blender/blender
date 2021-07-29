# gpl: author Nobuyuki Hirakata

import bpy

import bmesh
from random import (
        gauss,
        seed,
        )
from math import radians
from mathutils import Euler


# Allow changing the original material names from the .blend file
# by replacing them with the UI Names from the EnumProperty
def get_ui_mat_name(mat_name):
    mat_ui_name = "CrackIt Material"
    try:
        # access the Scene type directly to get the name from the enum
        mat_items = bpy.types.Scene.crackit[1]["type"].bl_rna.material_preset[1]["items"]
        for mat_id, mat_list in enumerate(mat_items):
            if mat_name in mat_list:
                mat_ui_name = mat_items[mat_id][1]
                break
        del mat_items
    except Exception as e:
        error_handlers(
                False, "get_ui_mat_name", e,
                "Retrieving the EnumProperty key UI Name could not be completed", True
                )
        pass

    return mat_ui_name


def error_handlers(self, op_name, error, reports="ERROR", func=False):
    if self and reports:
        self.report({'WARNING'}, reports + " (See Console for more info)")

    is_func = "Function" if func else "Operator"
    print("\n[Cell Fracture Crack It]\n{}: {}\nError: "
          "{}\nReport: {}\n".format(is_func, op_name, error, reports))


# -------------------- Crack -------------------
# Cell fracture and post-process:
def makeFracture(child_verts=False, division=100, noise=0.00,
                scaleX=1.00, scaleY=1.00, scaleZ=1.00, recursion=0, margin=0.001):

    # Get active object name and active layer
    active_name = bpy.context.scene.objects.active.name
    active_layer = bpy.context.scene.active_layer

    # source method of whether use child verts
    if child_verts is True:
        crack_source = 'VERT_CHILD'
    else:
        crack_source = 'PARTICLE_OWN'

    bpy.ops.object.add_fracture_cell_objects(
            source={crack_source}, source_limit=division, source_noise=noise,
            cell_scale=(scaleX, scaleY, scaleZ), recursion=recursion,
            recursion_source_limit=8, recursion_clamp=250, recursion_chance=0.25,
            recursion_chance_select='SIZE_MIN', use_smooth_faces=False,
            use_sharp_edges=False, use_sharp_edges_apply=True, use_data_match=True,
            use_island_split=True, margin=margin, material_index=0,
            use_interior_vgroup=False, mass_mode='VOLUME', mass=1, use_recenter=True,
            use_remove_original=True, use_layer_index=0, use_layer_next=False,
            group_name="", use_debug_points=False, use_debug_redraw=True, use_debug_bool=False
            )

    _makeJoin(active_name, active_layer)


# Join fractures into an object
def _makeJoin(active_name, active_layer):
    # Get object by name
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.object.select_pattern(pattern=active_name + '_cell*')
    fractures = bpy.context.selected_objects

    if fractures:
        # Execute join
        bpy.context.scene.objects.active = fractures[0]
        fractures[0].select = True
        bpy.ops.object.join()
    else:
        error_handlers(
            False, "_makeJoin", "if fractures condition has not passed",
            "Warning: No objects could be joined", True
            )

    # Change name
    bpy.context.scene.objects.active.name = active_name + '_crack'

    # Change origin
    bpy.ops.object.origin_set(type='GEOMETRY_ORIGIN')


# Add modifier and setting
def addModifiers():
    bpy.ops.object.modifier_add(type='DECIMATE')
    decimate = bpy.context.object.modifiers[-1]
    decimate.name = 'DECIMATE_crackit'
    decimate.ratio = 0.4

    bpy.ops.object.modifier_add(type='SUBSURF')
    subsurf = bpy.context.object.modifiers[-1]
    subsurf.name = 'SUBSURF_crackit'

    bpy.ops.object.modifier_add(type='SMOOTH')
    smooth = bpy.context.object.modifiers[-1]
    smooth.name = 'SMOOTH_crackit'


# -------------- multi extrude --------------------
# var1=random offset, var2=random rotation, var3=random scale
def multiExtrude(off=0.1, rotx=0, roty=0, rotz=0, sca=1.0,
                var1=0.01, var2=0.3, var3=0.3, num=1, ran=0):

    obj = bpy.context.object
    bpy.context.tool_settings.mesh_select_mode = [False, False, True]

    # bmesh operations
    bpy.ops.object.mode_set()
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    sel = [f for f in bm.faces if f.select]

    # faces loop
    for i, of in enumerate(sel):
        rot = _vrot(r=i, ran=ran, rotx=rotx, var2=var2, roty=roty, rotz=rotz)
        off = _vloc(r=i, ran=ran, off=off, var1=var1)
        of.normal_update()

        # extrusion loop
        for r in range(num):
            nf = of.copy()
            nf.normal_update()
            no = nf.normal.copy()
            ce = nf.calc_center_bounds()
            s = _vsca(r=i + r, ran=ran, var3=var3, sca=sca)

            for v in nf.verts:
                v.co -= ce
                v.co.rotate(rot)
                v.co += ce + no * off
                v.co = v.co.lerp(ce, 1 - s)

            # extrude code from TrumanBlending
            for a, b in zip(of.loops, nf.loops):
                sf = bm.faces.new((a.vert, a.link_loop_next.vert,
                                   b.link_loop_next.vert, b.vert))
                sf.normal_update()

            bm.faces.remove(of)
            of = nf

    for v in bm.verts:
        v.select = False

    for e in bm.edges:
        e.select = False

    bm.to_mesh(obj.data)
    obj.data.update()


def _vloc(r, ran, off, var1):
    seed(ran + r)
    return off * (1 + gauss(0, var1 / 3))


def _vrot(r, ran, rotx, var2, roty, rotz):
    seed(ran + r)
    return Euler((radians(rotx) + gauss(0, var2 / 3),
                radians(roty) + gauss(0, var2 / 3),
                radians(rotz) + gauss(0, var2 / 3)), 'XYZ')


def _vsca(r, ran, sca, var3):
    seed(ran + r)
    return sca * (1 + gauss(0, var3 / 3))


# Centroid of a selection of vertices
def _centro(ver):
    vvv = [v for v in ver if v.select]
    if not vvv or len(vvv) == len(ver):
        return ('error')

    x = sum([round(v.co[0], 4) for v in vvv]) / len(vvv)
    y = sum([round(v.co[1], 4) for v in vvv]) / len(vvv)
    z = sum([round(v.co[2], 4) for v in vvv]) / len(vvv)

    return (x, y, z)


# Retrieve the original state of the object
def _volver(obj, copia, om, msm, msv):
    for i in copia:
        obj.data.vertices[i].select = True
    bpy.context.tool_settings.mesh_select_mode = msm

    for i in range(len(msv)):
        obj.modifiers[i].show_viewport = msv[i]


# -------------- Material preset --------------------------
def appendMaterial(addon_path, material_name, mat_ui_names="Nameless Material"):
    # Load material from the addon directory
    file_path = _makeFilePath(addon_path=addon_path)
    bpy.ops.wm.append(filename=material_name, directory=file_path)

    # If material is loaded some times, select the last-loaded material
    last_material = _getAppendedMaterial(material_name)

    if last_material:
        mat = bpy.data.materials[last_material]
        # skip renaming if the prop is True
        if not bpy.context.scene.crackit.material_lib_name:
            mat.name = mat_ui_names

        # Apply Only one material in the material slot
        for m in bpy.context.object.data.materials:
            bpy.ops.object.material_slot_remove()

        bpy.context.object.data.materials.append(mat)

        return True

    return False


# Make file path of addon
def _makeFilePath(addon_path):
    material_folder = "/materials"
    blend_file = "/materials1.blend"
    category = "\\Material\\"

    file_path = addon_path + material_folder + blend_file + category
    return file_path


# Get last-loaded material, such as ~.002
def _getAppendedMaterial(material_name):
    # Get material name list
    material_names = [m.name for m in bpy.data.materials if material_name in m.name]
    if material_names:
        # Return last material in the sorted order
        material_names.sort()

        return material_names[-1]

    return None
