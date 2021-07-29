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

#  (c) 2016 meta-androcto, parts based on work by Saidenka, lijenstina
#  Materials Utils: by MichaleW, lijenstina,
#       (some code thanks to: CoDEmanX, SynaGl0w, ideasman42)
#  Materials Conversion: Silvio Falcinelli, johnzero7#,
#        fixes by angavrilov and others
#  Link to base names: Sybren, Texture renamer: Yadoob

bl_info = {
    "name": "Materials Utils Specials",
    "author": "Community",
    "version": (1, 0, 3),
    "blender": (2, 77, 0),
    "location": "Materials Properties Specials > Shift Q",
    "description": "Materials Utils and Convertors",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/3D_interaction/Materials_Utils",
    "category": "Material"
    }

if "bpy" in locals():
    import importlib
    importlib.reload(material_converter)
    importlib.reload(materials_cycles_converter)
    importlib.reload(texture_rename)
    importlib.reload(warning_messages_utils)
else:
    from . import material_converter
    from . import materials_cycles_converter
    from . import texture_rename
    from . import warning_messages_utils

import bpy
import os
from os import (
        path as os_path,
        access as os_access,
        remove as os_remove,
        )
from bpy.props import (
        StringProperty,
        BoolProperty,
        EnumProperty,
        PointerProperty,
        )
from bpy.types import (
        Menu,
        Operator,
        Panel,
        AddonPreferences,
        PropertyGroup,
        )
from .warning_messages_utils import (
        warning_messages,
        c_data_has_materials,
        )


# Functions

def fake_user_set(fake_user='ON', materials='UNUSED', operator=None):
    warn_mesg, w_mesg = '', ""
    if materials == 'ALL':
        mats = (mat for mat in bpy.data.materials if mat.library is None)
        w_mesg = "(All Materials in this .blend file)"
    elif materials == 'UNUSED':
        mats = (mat for mat in bpy.data.materials if mat.library is None and mat.users == 0)
        w_mesg = "(Unused Materials - Active/Selected Objects)"
    else:
        mats = []
        if materials == 'ACTIVE':
            objs = [bpy.context.active_object]
            w_mesg = "(All Materials on Active Object)"
        elif materials == 'SELECTED':
            objs = bpy.context.selected_objects
            w_mesg = "(All Materials on Selected Objects)"
        elif materials == 'SCENE':
            objs = bpy.context.scene.objects
            w_mesg = "(All Scene Objects)"
        else:
            # used materials
            objs = bpy.data.objects
            w_mesg = "(All Used Materials)"

        mats = (mat for ob in objs if hasattr(ob.data, "materials") for
                mat in ob.data.materials if mat.library is None)

    # collect mat names for warning_messages
    matnames = []

    warn_mesg = ('FAKE_SET_ON' if fake_user == 'ON' else 'FAKE_SET_OFF')

    for mat in mats:
        mat.use_fake_user = (fake_user == 'ON')
        matnames.append(getattr(mat, "name", "NO NAME"))

    if operator:
        if matnames:
            warning_messages(operator, warn_mesg, matnames, 'MAT', w_mesg)
        else:
            warning_messages(operator, 'FAKE_NO_MAT')

    for area in bpy.context.screen.areas:
        if area.type in ('PROPERTIES', 'NODE_EDITOR', 'OUTLINER'):
            area.tag_redraw()


def replace_material(m1, m2, all_objects=False, update_selection=False, operator=None):
    # replace material named m1 with material named m2
    # m1 is the name of original material
    # m2 is the name of the material to replace it with
    # 'all' will replace throughout the blend file

    matorg = bpy.data.materials.get(m1)
    matrep = bpy.data.materials.get(m2)

    if matorg != matrep and None not in (matorg, matrep):
        # store active object
        if all_objects:
            objs = bpy.data.objects
        else:
            objs = bpy.context.selected_editable_objects

        for ob in objs:
            if ob.type == 'MESH':
                match = False
                for m in ob.material_slots:
                    if m.material == matorg:
                        m.material = matrep
                        # don't break the loop as the material can be
                        # ref'd more than once

                        # Indicate which objects were affected
                        if update_selection:
                            ob.select = True
                            match = True

                if update_selection and not match:
                    ob.select = False
    else:
        if operator:
            warning_messages(operator, "REP_MAT_NONE")


def select_material_by_name(find_mat_name):
    # in object mode selects all objects with material find_mat_name
    # in edit mode selects all polygons with material find_mat_name

    find_mat = bpy.data.materials.get(find_mat_name)

    if find_mat is None:
        return

    # check for editmode
    editmode = False

    scn = bpy.context.scene

    # set selection mode to polygons
    scn.tool_settings.mesh_select_mode = False, False, True

    actob = bpy.context.active_object
    if actob.mode == 'EDIT':
        editmode = True
        bpy.ops.object.mode_set()

    if not editmode:
        objs = bpy.data.objects
        for ob in objs:
            if included_object_types(ob.type):
                ms = ob.material_slots
                for m in ms:
                    if m.material == find_mat:
                        ob.select = True
                        # the active object may not have the mat!
                        # set it to one that does!
                        scn.objects.active = ob
                        break
                    else:
                        ob.select = False
            # deselect non-meshes
            else:
                ob.select = False
    else:
        # it's editmode, so select the polygons
        ob = actob
        ms = ob.material_slots

        # same material can be on multiple slots
        slot_indeces = []
        i = 0

        for m in ms:
            if m.material == find_mat:
                slot_indeces.append(i)
            i += 1
        me = ob.data

        for f in me.polygons:
            if f.material_index in slot_indeces:
                f.select = True
            else:
                f.select = False
        me.update()

    if editmode:
        bpy.ops.object.mode_set(mode='EDIT')


def mat_to_texface(operator=None):
    # assigns the first image in each material to the polygons in the active
    # uvlayer for all selected objects

    # check for editmode
    editmode = False

    actob = bpy.context.active_object
    if actob.mode == 'EDIT':
        editmode = True
        bpy.ops.object.mode_set()

    # collect object names for warning messages
    message_a = []
    # Flag if there are non MESH objects selected
    mixed_obj = False

    for ob in bpy.context.selected_editable_objects:
        if ob.type == 'MESH':
            # get the materials from slots
            ms = ob.material_slots

            # build a list of images, one per material
            images = []
            # get the textures from the mats
            for m in ms:
                if m.material is None:
                    continue
                gotimage = False
                textures = zip(m.material.texture_slots, m.material.use_textures)
                for t, enabled in textures:
                    if enabled and t is not None:
                        tex = t.texture
                        if tex.type == 'IMAGE':
                            img = tex.image
                            images.append(img)
                            gotimage = True
                            break

                if not gotimage:
                    images.append(None)

            # check materials for warning messages
            mats = ob.material_slots.keys()
            if operator and not mats and mixed_obj is False:
                message_a.append(ob.name)

            # now we have the images, apply them to the uvlayer
            me = ob.data

            # got uvs?
            if not me.uv_textures:
                scn = bpy.context.scene
                scn.objects.active = ob
                bpy.ops.mesh.uv_texture_add()
                scn.objects.active = actob

            # get active uvlayer
            for t in me.uv_textures:
                if t.active:
                    uvtex = t.data
                    for f in me.polygons:
                        # check that material had an image!
                        if images and images[f.material_index] is not None:
                            uvtex[f.index].image = images[f.material_index]
                        else:
                            uvtex[f.index].image = None
            me.update()
        else:
            message_a.append(ob.name)
            mixed_obj = True

    if editmode:
        bpy.ops.object.mode_set(mode='EDIT')

    if operator and message_a:
        warn_mess = ('MAT_TEX_NO_MESH' if mixed_obj is True else 'MAT_TEX_NO_MAT')
        warning_messages(operator, warn_mess, message_a)


def assignmatslots(ob, matlist):
    # given an object and a list of material names
    # removes all material slots from the object
    # adds new ones for each material in matlist
    # adds the materials to the slots as well.

    scn = bpy.context.scene
    ob_active = bpy.context.active_object
    scn.objects.active = ob

    for s in ob.material_slots:
        bpy.ops.object.material_slot_remove()

    # re-add them and assign material
    if matlist:
        for m in matlist:
            try:
                mat = bpy.data.materials[m]
                ob.data.materials.append(mat)
            except:
                # there is no material with that name in data
                # or an empty mat is for some reason assigned
                # to face indices, mat tries to get an '' as mat index
                pass

    # restore active object:
    scn.objects.active = ob_active


def cleanmatslots(operator=None):
    # check for edit mode
    editmode = False
    actob = bpy.context.active_object

    # active object?
    if actob:
        if actob.mode == 'EDIT':
            editmode = True
            bpy.ops.object.mode_set()

        # is active object selected ?
        selected = bool(actob.select)
        actob.select = True

    objs = bpy.context.selected_editable_objects
    # collect all object names for warning_messages
    message_a = []
    # Flag if there are non MESH objects selected
    mixed_obj = False

    for ob in objs:
        if ob.type == 'MESH':
            mats = ob.material_slots.keys()

            # if mats is empty then mats[faceindex] will be out of range
            if mats:
                # check the polygons on the mesh to build a list of used materials
                usedMatIndex = []  # we'll store used materials indices here
                faceMats = []
                me = ob.data
                for f in me.polygons:
                    # get the material index for this face...
                    faceindex = f.material_index

                    # indices will be lost: Store face mat use by name
                    currentfacemat = mats[faceindex]
                    faceMats.append(currentfacemat)

                    # check if index is already listed as used or not
                    found = False
                    for m in usedMatIndex:
                        if m == faceindex:
                            found = True
                            # break

                    if found is False:
                        # add this index to the list
                        usedMatIndex.append(faceindex)

                # re-assign the used mats to the mesh and leave out the unused
                ml = []
                mnames = []
                for u in usedMatIndex:
                    ml.append(mats[u])
                    # we'll need a list of names to get the face indices...
                    mnames.append(mats[u])

                assignmatslots(ob, ml)

                # restore face indices:
                i = 0
                for f in me.polygons:
                    matindex = mnames.index(faceMats[i])
                    f.material_index = matindex
                    i += 1
            else:
                message_a.append(getattr(ob, "name", "NO NAME"))
                continue
        else:
            message_a.append(getattr(ob, "name", "NO NAME"))
            if mixed_obj is False:
                mixed_obj = True
            continue

    if message_a and operator:
        warn_mess = ('C_OB_MIX_NO_MAT' if mixed_obj is True else 'C_OB_NO_MAT')
        warning_messages(operator, warn_mess, message_a)

    if actob:
        # restore selection state
        actob.select = selected

        if editmode:
            bpy.ops.object.mode_set(mode='EDIT')


# separate edit mode mesh function (faster than iterating through all faces)

def assign_mat_mesh_edit(matname="Default", operator=None):
    actob = bpy.context.active_object
    found = False
    for m in bpy.data.materials:
        if m.name == matname:
            target = m
            found = True
            break
    if not found:
        target = bpy.data.materials.new(matname)

    if (actob.type in {'MESH'} and actob.mode in {'EDIT'}):
        # check material slots for matname material
        found = False
        i = 0
        mats = actob.material_slots
        for m in mats:
            if m.name == matname:
                found = True
                # make slot active
                actob.active_material_index = i
                break
            i += 1

        if not found:
            # the material is not attached to the object
            actob.data.materials.append(target)

        # is selected ?
        selected = bool(actob.select)
        # select active object
        actob.select = True

        # activate the chosen material
        actob.active_material_index = i

        # assign the material to the object
        bpy.ops.object.material_slot_assign()
        actob.data.update()

        # restore selection state
        actob.select = selected

        if operator:
            mat_names = ("A New Untitled" if matname in ("", None) else matname)
            warning_messages(operator, 'A_MAT_NAME_EDIT', mat_names, 'MAT')


def assign_mat(matname="Default", operator=None):
    # get active object so we can restore it later
    actob = bpy.context.active_object

    # is active object selected ?
    selected = bool(actob.select)
    actob.select = True

    # check if material exists, if it doesn't then create it
    found = False
    for m in bpy.data.materials:
        if m.name == matname:
            target = m
            found = True
            break

    if not found:
        target = bpy.data.materials.new(matname)

    # if objectmode then set all polygons
    editmode = False
    allpolygons = True
    if actob.mode == 'EDIT':
        editmode = True
        allpolygons = False
        bpy.ops.object.mode_set()

    objs = bpy.context.selected_editable_objects

    # collect non mesh object names
    message_a = []

    for ob in objs:
        # skip the objects that can't have mats
        if not included_object_types(ob.type):
            message_a.append(ob.name)
            continue
        else:
            # set the active object to our object
            scn = bpy.context.scene
            scn.objects.active = ob

            if ob.type in {'CURVE', 'SURFACE', 'FONT', 'META'}:
                found = False
                i = 0
                for m in bpy.data.materials:
                    if m.name == matname:
                        found = True
                        index = i
                        break
                    i += 1
                    if not found:
                        index = i - 1
                targetlist = [index]
                assignmatslots(ob, targetlist)
            elif ob.type == 'MESH':
                # check material slots for matname material
                found = False
                i = 0
                mats = ob.material_slots
                for m in mats:
                    if m.name == matname:
                        found = True
                        index = i
                        # make slot active
                        ob.active_material_index = i
                        break
                    i += 1

                if not found:
                    index = i
                    # the material is not attached to the object
                    ob.data.materials.append(target)

                # now assign the material:
                me = ob.data
                if allpolygons:
                    for f in me.polygons:
                        f.material_index = index
                elif allpolygons is False:
                    for f in me.polygons:
                        if f.select:
                            f.material_index = index
                me.update()

    # restore the active object
    bpy.context.scene.objects.active = actob

    # restore selection state
    actob.select = selected

    if editmode:
        bpy.ops.object.mode_set(mode='EDIT')

    if operator and message_a:
        warning_messages(operator, 'A_OB_MIX_NO_MAT', message_a)


def check_texture(img, mat):
    # finds a texture from an image
    # makes a texture if needed
    # adds it to the material if it isn't there already

    tex = bpy.data.textures.get(img.name)

    if tex is None:
        tex = bpy.data.textures.new(name=img.name, type='IMAGE')

    tex.image = img

    # see if the material already uses this tex
    # add it if needed
    found = False
    for m in mat.texture_slots:
        if m and m.texture == tex:
            found = True
            break
    if not found and mat:
        mtex = mat.texture_slots.add()
        mtex.texture = tex
        mtex.texture_coords = 'UV'
        mtex.use_map_color_diffuse = True


def texface_to_mat(operator=None):
    # editmode check here!
    editmode = False
    ob = bpy.context.object
    if ob.mode == 'EDIT':
        editmode = True
        bpy.ops.object.mode_set()

    for ob in bpy.context.selected_editable_objects:

        faceindex = []
        unique_images = []
        # collect object names for warning messages
        message_a = []

        # check if object has UV and texture data and active image in Editor
        if check_texface_to_mat(ob):
            # get the texface images and store indices
            for f in ob.data.uv_textures.active.data:
                if f.image:
                    img = f.image
                    # build list of unique images
                    if img not in unique_images:
                        unique_images.append(img)
                    faceindex.append(unique_images.index(img))
                else:
                    img = None
                    faceindex.append(None)
        else:
            message_a.append(ob.name)
            continue

        # check materials for images exist; create if needed
        matlist = []

        for i in unique_images:
            if i:
                try:
                    m = bpy.data.materials[i.name]
                except:
                    m = bpy.data.materials.new(name=i.name)
                    continue

                finally:
                    matlist.append(m.name)
                    # add textures if needed
                    check_texture(i, m)

        # set up the object material slots
        assignmatslots(ob, matlist)

        # set texface indices to material slot indices..
        me = ob.data

        i = 0
        for f in faceindex:
            if f is not None:
                me.polygons[i].material_index = f
            i += 1
    if editmode:
        bpy.ops.object.mode_set(mode='EDIT')

    if operator and message_a:
        warning_messages(operator, "TEX_MAT_NO_CRT", message_a)


def remove_materials(operator=None, setting="SLOT"):
    # Remove material slots from active object
    # SLOT - removes the object's active material
    # ALL - removes the all the object's materials
    actob = bpy.context.active_object
    actob_name = getattr(actob, "name", "NO NAME")

    if actob:
        if not included_object_types(actob.type):
            if operator:
                warning_messages(operator, 'OB_CANT_MAT', actob_name)
        else:
            if (hasattr(actob.data, "materials") and
               len(actob.data.materials) > 0):
                if setting == "SLOT":
                    bpy.ops.object.material_slot_remove()
                elif setting == "ALL":
                    for mat in actob.data.materials:
                        try:
                            bpy.ops.object.material_slot_remove()
                        except:
                            pass

                if operator:
                    warn_mess = ('R_ACT_MAT_ALL' if setting == "ALL" else 'R_ACT_MAT')
                    warning_messages(operator, warn_mess, actob_name)
            elif operator:
                warning_messages(operator, 'R_OB_NO_MAT', actob_name)


def remove_materials_all(operator=None):
    # Remove material slots from all selected objects
    # counter for material slots warning messages, collect errors
    mat_count, collect_mess = False, []

    for ob in bpy.context.selected_editable_objects:
        if not included_object_types(ob.type):
            continue
        else:
            # code from blender stackexchange (by CoDEmanX)
            ob.active_material_index = 0

            if (hasattr(ob.data, "materials") and
               len(ob.material_slots) >= 1):
                mat_count = True

            # Ctx - copy the context for operator override
            Ctx = bpy.context.copy()
            # for this operator it needs only the active object replaced
            Ctx['object'] = ob

            for i in range(len(ob.material_slots)):
                try:
                    bpy.ops.object.material_slot_remove(Ctx)
                except:
                    ob_name = getattr(ob, "name", "NO NAME")
                    collect_mess.append(ob_name)
                    pass

    if operator:
        warn_msg = ('R_ALL_NO_MAT' if mat_count is False else 'R_ALL_SL_MAT')
        if not collect_mess:
            warning_messages(operator, warn_msg)
        else:
            warning_messages(operator, 'R_OB_FAIL_MAT', collect_mess)


# Operator Classes #

class VIEW3D_OT_show_mat_preview(Operator):
    bl_label = "Preview Active Material"
    bl_idname = "view3d.show_mat_preview"
    bl_description = "Show the preview of Active Material and context related settings"
    bl_options = {'REGISTER', 'UNDO'}

    is_not_undo = False     # prevent drawing props on undo

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                context.object.active_material is not None and
                included_object_types(context.object.type))

    def invoke(self, context, event):
        self.is_not_undo = True
        return context.window_manager.invoke_props_dialog(self, width=200)

    def draw(self, context):
        layout = self.layout
        ob = context.active_object
        prw_size = size_preview()

        if self.is_not_undo is True:
            if ob and hasattr(ob, "active_material"):
                mat = ob.active_material
                is_opaque = (True if (ob and hasattr(ob, "show_transparent") and
                             ob.show_transparent is True)
                             else False)
                is_opaque_bi = (True if (mat and hasattr(mat, "use_transparency") and
                                mat.use_transparency is True)
                                else False)
                is_mesh = (True if ob.type == 'MESH' else False)

                if size_type_is_preview():
                    layout.template_ID_preview(ob, "active_material", new="material.new",
                                               rows=prw_size['Width'], cols=prw_size['Height'])
                else:
                    layout.template_ID(ob, "active_material", new="material.new")
                layout.separator()

                if c_render_engine("Both"):
                    layout.prop(mat, "use_nodes", icon='NODETREE')

                if c_need_of_viewport_colors():
                    color_txt = ("Viewport Color:" if c_render_engine("Cycles") else "Diffuse")
                    spec_txt = ("Viewport Specular:" if c_render_engine("Cycles") else "Specular")
                    col = layout.column(align=True)
                    col.label(color_txt)
                    col.prop(mat, "diffuse_color", text="")
                    if c_render_engine("BI"):
                        # Blender Render
                        col.prop(mat, "diffuse_intensity", text="Intensity")
                    col.separator()

                    col.label(spec_txt)
                    col.prop(mat, "specular_color", text="")
                    col.prop(mat, "specular_hardness")

                    if (c_render_engine("BI") and not c_context_use_nodes()):
                        # Blender Render
                        col.separator()
                        col.prop(mat, "use_transparency")
                        col.separator()
                        if is_opaque_bi:
                            col.prop(mat, "transparency_method", text="")
                            col.separator()
                            col.prop(mat, "alpha")
                    elif (c_render_engine("Cycles") and is_mesh):
                        # Cycles
                        col.separator()
                        col.prop(ob, "show_transparent", text="Transparency")
                        if is_opaque:
                            col.separator()
                            col.prop(mat, "alpha")
                            col.separator()
                            col.label("Viewport Alpha:")
                            col.prop(mat.game_settings, "alpha_blend", text="")
                    layout.separator()
                else:
                    other_render = ("*Unavailable with this Renderer*" if not c_render_engine("Both")
                                    else "*Unavailable in this Context*")
                    no_col_label = ("*Only available in Solid Shading*" if c_render_engine("Cycles")
                                    else other_render)
                    layout.label(no_col_label, icon="INFO")
        else:
            layout.label(text="**Only Undo is available**", icon="INFO")

    def check(self, context):
        return self.is_not_undo

    def execute(self, context):
        self.is_not_undo = False
        return {'FINISHED'}


class VIEW3D_OT_copy_material_to_selected(Operator):
    bl_idname = "view3d.copy_material_to_selected"
    bl_label = "Copy Materials to others"
    bl_description = ("Copy Material From Active to Selected objects \n"
                      "Works on Object's Data linked Materials")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (c_data_has_materials() and
                context.active_object is not None and
                included_object_types(context.active_object.type) and
                context.object.active_material is not None and
                context.selected_editable_objects)

    def execute(self, context):
        warn_mess = "DEFAULT"
        if (len(context.selected_editable_objects) < 2):
            warn_mess = 'CPY_MAT_ONE_OB'
        else:
            if check_is_excluded_obj_types(context):
                warn_mess = 'CPY_MAT_MIX_OB'
            try:
                bpy.ops.object.material_slot_copy()
                warn_mess = 'CPY_MAT_DONE'
            except:
                warning_messages(self, 'CPY_MAT_FAIL')
                return {'CANCELLED'}

        warning_messages(self, warn_mess)
        return {'FINISHED'}


class VIEW3D_OT_texface_to_material(Operator):
    bl_idname = "view3d.texface_to_material"
    bl_label = "Texface Images to Material/Texture"
    bl_description = ("Create texture materials for images assigned in UV editor \n"
                      "Needs an UV Unwrapped Mesh and an image active in the  \n"
                      "UV/Image Editor for each Selected Object")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        if context.selected_editable_objects:
            texface_to_mat(self)
            return {'FINISHED'}
        else:
            warning_messages(self, 'TEX_MAT_NO_SL')
            return {'CANCELLED'}


class VIEW3D_OT_set_new_material_name(Operator):
    bl_idname = "view3d.set_new_material_name"
    bl_label = "New Material Settings"
    bl_description = ("Set the Base name of the new Material\n"
                      "and tweaking after the new Material creation")
    bl_options = {'REGISTER'}

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def draw(self, context):
        layout = self.layout
        scene = context.scene.mat_specials

        box = layout.box()
        box.label("Base name:")
        box.prop(scene, "set_material_name", text="", icon="SYNTAX_ON")
        layout.separator()
        layout.prop(scene, "use_tweak")

    def execute(self, context):
        return {'FINISHED'}


class VIEW3D_OT_assign_material(Operator):
    bl_idname = "view3d.assign_material"
    bl_label = "Assign Material"
    bl_description = "Assign a material to the selection"
    bl_options = {'REGISTER', 'UNDO'}

    is_existing = BoolProperty(
        name="Is it a new Material",
        options={'HIDDEN'},
        default=True,
        )
    matname = StringProperty(
        name="Material Name",
        description="Name of the Material to Assign",
        options={'HIDDEN'},
        default="Material_New",
        maxlen=128,
        )

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def invoke(self, context, event):
        return self.execute(context)

    def execute(self, context):
        actob = context.active_object
        mn = self.matname
        scene = context.scene.mat_specials
        tweak = scene.use_tweak

        if not self.is_existing:
            new_name = check_mat_name_unique(scene.set_material_name)
            mn = new_name

        if (actob.type in {'MESH'} and actob.mode in {'EDIT'}):
            assign_mat_mesh_edit(mn, self)
        else:
            assign_mat(mn, self)

        if use_cleanmat_slots():
            cleanmatslots()

        mat_to_texface()
        self.is_not_undo = False

        if tweak and not self.is_existing:
            try:
                bpy.ops.view3d.show_mat_preview('INVOKE_DEFAULT')
            except:
                self.report({'INFO'}, "Preview Active Material could not be opened")

        return {'FINISHED'}


class VIEW3D_OT_clean_material_slots(Operator):
    bl_idname = "view3d.clean_material_slots"
    bl_label = "Clean Material Slots"
    bl_description = ("Removes any unused material slots \n"
                      "from selected objects in Object mode")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    # materials can't be removed in Edit mode
    def poll(cls, context):
        return (c_data_has_materials() and
                context.active_object is not None and
                not context.object.mode == 'EDIT')

    def execute(self, context):
        cleanmatslots(self)
        return {'FINISHED'}


class VIEW3D_OT_material_to_texface(Operator):
    bl_idname = "view3d.material_to_texface"
    bl_label = "Material Images to Texface"
    bl_description = ("Transfer material assignments to UV editor \n"
                      "Works on a Mesh Object with a Material and Texture\n"
                      "assigned. Used primarily with MultiTexture Shading")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (c_data_has_materials() and
                context.active_object is not None)

    def execute(self, context):
        if context.selected_editable_objects:
            mat_to_texface(self)
            return {'FINISHED'}
        else:
            warning_messages(self, "MAT_TEX_NO_SL")
            return {'CANCELLED'}


class VIEW3D_OT_material_remove_slot(Operator):
    bl_idname = "view3d.material_remove_slot"
    bl_label = "Remove Active Slot (Active Object)"
    bl_description = ("Remove active material slot from active object\n"
                      "Can't be used in Edit Mode")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        # materials can't be removed in Edit mode
        return (c_data_has_materials() and
                context.active_object is not None and
                not context.object.mode == 'EDIT')

    def execute(self, context):
        if context.selected_editable_objects:
            remove_materials(self, "SLOT")
            return {'FINISHED'}
        else:
            warning_messages(self, 'R_NO_SL_MAT')
            return {'CANCELLED'}


class VIEW3D_OT_material_remove_object(Operator):
    bl_idname = "view3d.material_remove_object"
    bl_label = "Remove all Slots (Active Object)"
    bl_description = ("Remove all material slots from active object\n"
                      "Can't be used in Edit Mode")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        # materials can't be removed in Edit mode
        return (c_data_has_materials() and
                context.active_object is not None and
                not context.object.mode == 'EDIT')

    def execute(self, context):
        if context.selected_editable_objects:
            remove_materials(self, "ALL")
            return {'FINISHED'}
        else:
            warning_messages(self, 'R_NO_SL_MAT')
            return {'CANCELLED'}


class VIEW3D_OT_material_remove_all(Operator):
    bl_idname = "view3d.material_remove_all"
    bl_label = "Remove All Material Slots"
    bl_description = ("Remove all material slots from all selected objects \n"
                      "Can't be used in Edit Mode")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        # materials can't be removed in Edit mode
        return (c_data_has_materials() and
                context.active_object is not None and
                not context.object.mode == 'EDIT')

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        if context.selected_editable_objects:
            remove_materials_all(self)
            return {'FINISHED'}
        else:
            warning_messages(self, 'R_NO_SL_MAT')
            return {'CANCELLED'}


class VIEW3D_OT_select_material_by_name(Operator):
    bl_idname = "view3d.select_material_by_name"
    bl_label = "Select Material By Name"
    bl_description = "Select geometry with this material assigned to it"
    bl_options = {'REGISTER', 'UNDO'}

    matname = StringProperty(
            name='Material Name',
            description='Name of Material to Select',
            maxlen=63,
            )

    @classmethod
    def poll(cls, context):
        return (c_data_has_materials() and
                context.active_object is not None)

    def execute(self, context):
        mn = self.matname
        select_material_by_name(mn)
        warning_messages(self, 'SL_MAT_BY_NAME', mn)
        return {'FINISHED'}


class VIEW3D_OT_replace_material(Operator):
    bl_idname = "view3d.replace_material"
    bl_label = "Replace Material"
    bl_description = "Replace a material by name"
    bl_options = {'REGISTER', 'UNDO'}

    matorg = StringProperty(
            name="Original",
            description="Material to replace",
            maxlen=63,
            )
    matrep = StringProperty(
            name="Replacement",
            description="Replacement material",
            maxlen=63,
            )
    all_objects = BoolProperty(
            name="All objects",
            description="Replace for all objects in this blend file",
            default=True,
            )
    update_selection = BoolProperty(
            name="Update Selection",
            description="Select affected objects and deselect unaffected",
            default=True,
            )

    @classmethod
    def poll(cls, context):
        return c_data_has_materials()

    def draw(self, context):
        layout = self.layout
        layout.prop_search(self, "matorg", bpy.data, "materials")
        layout.prop_search(self, "matrep", bpy.data, "materials")
        layout.prop(self, "all_objects")
        layout.prop(self, "update_selection")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        replace_material(self.matorg, self.matrep, self.all_objects,
                         self.update_selection, self)
        self.matorg, self.matrep = "", ""
        return {'FINISHED'}


class VIEW3D_OT_fake_user_set(Operator):
    bl_idname = "view3d.fake_user_set"
    bl_label = "Set Fake User"
    bl_description = "Enable/disable fake user for materials"
    bl_options = {'REGISTER', 'UNDO'}

    fake_user = EnumProperty(
            name="Fake User",
            description="Turn fake user on or off",
            items=(('ON', "On", "Enable fake user"), ('OFF', "Off", "Disable fake user")),
            default='ON',
            )
    materials = EnumProperty(
            name="Materials",
            description="Chose what objects and materials to affect",
            items=(('ACTIVE', "Active object", "Materials of active object only"),
                   ('SELECTED', "Selected objects", "Materials of selected objects"),
                   ('SCENE', "Scene objects", "Materials of objects in current scene"),
                   ('USED', "Used", "All materials used by objects"),
                   ('UNUSED', "Unused", "Currently unused materials"),
                   ('ALL', "All", "All materials in this blend file")),
            default='UNUSED',
            )

    @classmethod
    def poll(cls, context):
        return c_data_has_materials()

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "fake_user", expand=True)
        layout.prop(self, "materials")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        fake_user_set(self.fake_user, self.materials, self)
        return {'FINISHED'}


class MATERIAL_OT_set_transparent_back_side(Operator):
    bl_idname = "material.set_transparent_back_side"
    bl_label = "Transparent back (BI)"
    bl_description = ("Creates BI nodes with Alpha output connected to Front/Back\n"
                      "Geometry node on Object's Active Material Slot")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        if (not obj):
            return False
        mat = obj.active_material
        if (not mat):
            return False
        if (mat.node_tree):
            if (len(mat.node_tree.nodes) == 0):
                return True
        if (not mat.use_nodes):
            return True
        return False

    def execute(self, context):
        obj = context.active_object
        mat = obj.active_material
        try:
            mat.use_nodes = True
            if (mat.node_tree):
                for node in mat.node_tree.nodes:
                    if (node):
                        mat.node_tree.nodes.remove(node)

            mat.use_transparency = True
            node_mat = mat.node_tree.nodes.new('ShaderNodeMaterial')
            node_out = mat.node_tree.nodes.new('ShaderNodeOutput')
            node_geo = mat.node_tree.nodes.new('ShaderNodeGeometry')
            node_mat.material = mat
            node_out.location = [node_out.location[0] + 500, node_out.location[1]]
            node_geo.location = [node_geo.location[0] + 150, node_geo.location[1] - 150]
            mat.node_tree.links.new(node_mat.outputs[0], node_out.inputs[0])
            mat.node_tree.links.new(node_geo.outputs[8], node_out.inputs[1])
        except:
            warning_messages(self, 'E_MAT_TRNSP_BACK')
            return {'CANCELLED'}

        if hasattr(mat, "name"):
            warning_messages(self, 'MAT_TRNSP_BACK', mat.name, 'MAT')

        return {'FINISHED'}


class MATERIAL_OT_move_slot_top(Operator):
    bl_idname = "material.move_material_slot_top"
    bl_label = "Slot to the top"
    bl_description = "Move the active material slot on top"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        if (not obj):
            return False
        if (len(obj.material_slots) <= 2):
            return False
        if (obj.active_material_index <= 0):
            return False
        return True

    def execute(self, context):
        activeObj = context.active_object

        for i in range(activeObj.active_material_index):
            bpy.ops.object.material_slot_move(direction='UP')

        active_mat = context.object.active_material
        if active_mat and hasattr(active_mat, "name"):
            warning_messages(self, 'MOVE_SLOT_UP', active_mat.name, 'MAT')

        return {'FINISHED'}


class MATERIAL_OT_move_slot_bottom(Operator):
    bl_idname = "material.move_material_slot_bottom"
    bl_label = "Slots to the bottom"
    bl_description = "Move the active material slot to the bottom"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        if (not obj):
            return False
        if (len(obj.material_slots) <= 2):
            return False
        if (len(obj.material_slots) - 1 <= obj.active_material_index):
            return False
        return True

    def execute(self, context):
        activeObj = context.active_object
        lastSlotIndex = len(activeObj.material_slots) - 1

        for i in range(lastSlotIndex - activeObj.active_material_index):
            bpy.ops.object.material_slot_move(direction='DOWN')

        active_mat = context.object.active_material
        if active_mat and hasattr(active_mat, "name"):
            warning_messages(self, 'MOVE_SLOT_DOWN', active_mat.name, 'MAT')

        return {'FINISHED'}


class MATERIAL_OT_link_to_base_names(Operator):
    bl_idname = "material.link_to_base_names"
    bl_label = "Merge Base Names"
    bl_description = ("Replace .001, .002 slots with Original \n"
                      "Material/Name on All Materials/Objects")
    bl_options = {'REGISTER', 'UNDO'}

    mat_keep = StringProperty(
                name="Material to keep",
                default="",
                )
    is_auto = BoolProperty(
                name="Auto Rename/Replace",
                description=("Automatically Replace names "
                             "by stripping numerical suffix"),
                default=False,
               )
    mat_error = []          # collect mat for warning messages
    is_not_undo = False     # prevent drawing props on undo
    check_no_name = True    # check if no name is passed

    @classmethod
    def poll(cls, context):
        return (c_data_has_materials() and context.active_object is not None)

    def draw(self, context):
        layout = self.layout
        if self.is_not_undo is True:
            boxee = layout.box()
            boxee.prop_search(self, "mat_keep", bpy.data, "materials")
            boxee.enabled = not self.is_auto
            layout.separator()

            boxs = layout.box()
            boxs.prop(self, "is_auto", text="Auto Rename/Replace", icon="SYNTAX_ON")
        else:
            layout.label(text="**Only Undo is available**", icon="INFO")

    def invoke(self, context, event):
        self.is_not_undo = True
        return context.window_manager.invoke_props_dialog(self)

    def replace_name(self):
        # use the chosen material as a base one, check if there is a name
        self.check_no_name = (False if self.mat_keep in {""} else True)

        if self.check_no_name is True:
            for mat in bpy.data.materials:
                name = mat.name
                if name == self.mat_keep:
                    try:
                        base, suffix = name.rsplit('.', 1)
                        # trigger the exception
                        num = int(suffix, 10)
                        self.mat_keep = base
                        mat.name = self.mat_keep
                        return
                    except ValueError:
                        if name not in self.mat_error:
                            self.mat_error.append(name)
                        return
        return

    def split_name(self, material):
        name = material.name

        if '.' not in name:
            return name, None

        base, suffix = name.rsplit('.', 1)

        try:
            # trigger the exception
            num = int(suffix, 10)
        except ValueError:
            # Not a numeric suffix
            if name not in self.mat_error:
                self.mat_error.append(name)
            return name, None

        if self.is_auto is False:
            if base == self.mat_keep:
                return base, suffix
            else:
                return name, None

        return base, suffix

    def fixup_slot(self, slot):
        if not slot.material:
            return

        base, suffix = self.split_name(slot.material)
        if suffix is None:
            return

        try:
            base_mat = bpy.data.materials[base]
        except KeyError:
            print("\n[Materials Utils Specials]\nLink to base names\nError:"
                  "Base material %r not found\n" % base)
            return

        slot.material = base_mat

    def check(self, context):
        return self.is_not_undo

    def main_loop(self, context):
        for ob in context.scene.objects:
            for slot in ob.material_slots:
                self.fixup_slot(slot)

    def execute(self, context):
        if self.is_auto is False:
            self.replace_name()
            if self.check_no_name is True:
                self.main_loop(context)
            else:
                warning_messages(self, 'MAT_LINK_NO_NAME')
                self.is_not_undo = False
                return {'CANCELLED'}

        self.main_loop(context)

        if use_cleanmat_slots():
            cleanmatslots()

        if self.mat_error:
            warning_messages(self, 'MAT_LINK_ERROR', self.mat_error, 'MAT')

        self.is_not_undo = False
        return {'FINISHED'}


class MATERIAL_OT_check_converter_path(Operator):
    bl_idname = "material.check_converter_path"
    bl_label = "Check Converters images/data save path"
    bl_description = "Check if the given path is writeable (has OS writing privileges)"
    bl_options = {'REGISTER', 'INTERNAL'}

    def check_valid_path(self, context):
        sc = context.scene
        paths = bpy.path.abspath(sc.mat_specials.conv_path)

        if bpy.data.filepath == "":
            warning_messages(self, "DIR_PATH_EMPTY", override=True)
            return False

        if os_path.exists(paths):
            if os_access(paths, os.W_OK | os.X_OK):
                try:
                    path_test = os_path.join(paths, "XYfoobartestXY.txt")
                    with open(path_test, 'w') as f:
                        f.closed
                    os_remove(path_test)
                    return True
                except (OSError, IOError):
                    warning_messages(self, 'DIR_PATH_W_ERROR', override=True)
                    return False
            else:
                warning_messages(self, 'DIR_PATH_A_ERROR', override=True)
                return False
        else:
            warning_messages(self, 'DIR_PATH_N_ERROR', override=True)
            return False

        return True

    def execute(self, context):
        if not self.check_valid_path(context):
            return {'CANCELLED'}

        warning_messages(self, 'DIR_PATH_W_OK', override=True)

        return {'FINISHED'}


# Menu classes

class VIEW3D_MT_assign_material(Menu):
    bl_label = "Assign Material"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        if c_data_has_materials():
            # no materials
            for material_name in bpy.data.materials.keys():
                mats = layout.operator("view3d.assign_material",
                                text=material_name,
                                icon='MATERIAL_DATA')
                mats.matname = material_name
                mats.is_existing = True
            use_separator(self, context)

        if (not context.active_object):
            # info why the add material is innactive
            layout.label(text="*No active Object in the Scene*", icon="INFO")
            use_separator(self, context)
        mat_prop_name = context.scene.mat_specials.set_material_name
        add_new = layout.operator("view3d.assign_material",
                                  text="Add New", icon='ZOOMIN')
        add_new.matname = mat_prop_name
        add_new.is_existing = False


class VIEW3D_MT_select_material(Menu):
    bl_label = "Select by Material"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        ob = context.object

        if (not c_data_has_materials()):
            layout.label(text="*No Materials in the data*", icon="INFO")
        elif (not ob):
            layout.label(text="*No Objects to select*", icon="INFO")
        else:
            if ob.mode == 'OBJECT':
                # show all used materials in entire blend file
                for material_name, material in bpy.data.materials.items():
                    if (material.users > 0):
                        layout.operator("view3d.select_material_by_name",
                                        text=material_name,
                                        icon='MATERIAL_DATA',
                                        ).matname = material_name
            elif ob.mode == 'EDIT':
                # show only the materials on this object
                mats = ob.material_slots.keys()
                for m in mats:
                    layout.operator("view3d.select_material_by_name",
                                    text=m,
                                    icon='MATERIAL_DATA').matname = m


class VIEW3D_MT_remove_material(Menu):
    bl_label = "Clean Slots"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        layout.operator("view3d.clean_material_slots",
                        text="Clean Material Slots",
                        icon='COLOR_BLUE')
        use_separator(self, context)

        if not c_render_engine("Lux"):
            layout.operator("view3d.material_remove_slot", icon='COLOR_GREEN')
            layout.operator("view3d.material_remove_object", icon='COLOR_RED')

            if use_remove_mat_all():
                use_separator(self, context)
                layout.operator("view3d.material_remove_all",
                                text="Remove Material Slots "
                                "(All Selected Objects)",
                                icon='CANCEL')
        else:
            layout.label(text="Sorry, other Menu functions are", icon="INFO")
            layout.label(text="unvailable with Lux Renderer")


class VIEW3D_MT_master_material(Menu):
    bl_label = "Material Specials Menu"

    def draw(self, context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'

        if use_mat_preview() is True:
            layout.operator("view3d.show_mat_preview", icon="VISIBLE_IPO_ON")
            use_separator(self, context)

        layout.menu("VIEW3D_MT_assign_material", icon='ZOOMIN')
        layout.menu("VIEW3D_MT_select_material", icon='HAND')
        use_separator(self, context)

        layout.operator("view3d.copy_material_to_selected", icon="COPY_ID")
        use_separator(self, context)

        layout.menu("VIEW3D_MT_remove_material", icon="COLORSET_10_VEC")
        use_separator(self, context)

        layout.operator("view3d.replace_material",
                        text='Replace Material',
                        icon='ARROW_LEFTRIGHT')
        layout.operator("view3d.fake_user_set",
                        text='Set Fake User',
                        icon='UNPINNED')
        use_separator(self, context)

        layout.menu("VIEW3D_MT_mat_special", icon="SOLO_ON")


class VIEW3D_MT_mat_special(Menu):
    bl_label = "Specials"

    def draw(self, context):
        layout = self.layout

        layout.operator("view3d.set_new_material_name", icon="SETTINGS")

        if c_render_engine("Cycles"):
            if (enable_converters() is True and converter_type('BI_CONV')):
                ml_restore_1 = layout.operator("ml.restore",
                                               text='To BI Nodes Off',
                                               icon="BLENDER")
                ml_restore_1.switcher = False
                ml_restore_1.renderer = "BI"

                ml_restore_2 = layout.operator("ml.restore",
                                               text='To BI Nodes On',
                                               icon="APPEND_BLEND")
                ml_restore_2.switcher = True
                ml_restore_2.renderer = "BI"
                use_separator(self, context)

        elif c_render_engine("BI"):
            if (enable_converters() is True and converter_type('CYC_CONV')):
                layout.operator("ml.refresh_active",
                                text='Convert Active to Cycles',
                                icon='NODE_INSERT_OFF')
                layout.operator("ml.refresh",
                                text='Convert All to Cycles',
                                icon='NODE_INSERT_ON')
                use_separator(self, context)
                ml_restore_1 = layout.operator("ml.restore",
                                               text='To Cycles Nodes Off',
                                               icon="SOLID")
                ml_restore_1.switcher = False
                ml_restore_1.renderer = "CYCLES"

                ml_restore_2 = layout.operator("ml.restore",
                                               text='To Cycles Nodes On',
                                               icon="IMGDISPLAY")
                ml_restore_2.switcher = True
                ml_restore_2.renderer = "CYCLES"
                use_separator(self, context)

            layout.operator("material.set_transparent_back_side",
                            icon='IMAGE_RGB_ALPHA',
                            text="Transparent back (BI)")
            layout.operator("view3d.material_to_texface",
                            text="Material to Texface",
                            icon='MATERIAL_DATA')
            layout.operator("view3d.texface_to_material",
                            text="Texface to Material",
                            icon='TEXTURE_SHADED')
            use_separator(self, context)

        layout.operator("material.link_to_base_names", icon="KEYTYPE_BREAKDOWN_VEC")
        use_separator(self, context)
        layout.operator("texture.patern_rename",
                        text='Rename Image As Texture',
                        icon='TEXTURE')


# Specials Menu's #

def menu_func(self, context):
    layout = self.layout
    layout.operator_context = 'INVOKE_REGION_WIN'

    use_separator(self, context)
    layout.menu("VIEW3D_MT_assign_material", icon='ZOOMIN')
    layout.menu("VIEW3D_MT_select_material", icon='HAND')
    layout.operator("view3d.replace_material",
                    text='Replace Material',
                    icon='ARROW_LEFTRIGHT')
    use_separator(self, context)

    layout.menu("VIEW3D_MT_remove_material", icon="COLORSET_10_VEC")
    use_separator(self, context)

    layout.operator("view3d.fake_user_set",
                    text='Set Fake User',
                    icon='UNPINNED')
    use_separator(self, context)

    layout.menu("VIEW3D_MT_mat_special", icon="SOLO_ON")


def menu_move(self, context):
    layout = self.layout
    layout.operator_context = 'INVOKE_REGION_WIN'

    layout.operator("material.move_material_slot_top",
                    icon='TRIA_UP', text="Slot to top")
    layout.operator("material.move_material_slot_bottom",
                    icon='TRIA_DOWN', text="Slot to bottom")
    use_separator(self, context)


# Converters Menu's #

class MATERIAL_MT_scenemassive_opt(Menu):
    bl_idname = "scenemassive.opt"
    bl_description = "Additional Options for Convert BI to Cycles"
    bl_label = "Options"
    bl_options = {'REGISTER'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene.mat_specials

        layout.prop(scene, "EXTRACT_ALPHA",
                    text="Extract Alpha Textures (slow)")
        use_separator(self, context)
        layout.prop(scene, "EXTRACT_PTEX",
                    text="Extract Procedural Textures (slow)")
        use_separator(self, context)
        layout.prop(scene, "EXTRACT_OW", text="Re-extract Textures")
        use_separator(self, context)
        layout.prop(scene, "SET_FAKE_USER", text="Set Fake User on unused images")
        use_separator(self, context)
        layout.prop(scene, "SCULPT_PAINT", text="Sculpt/Texture paint mode")
        use_separator(self, context)
        layout.prop(scene, "UV_UNWRAP", text="Set Auto UV Unwrap (Active Object)")
        use_separator(self, context)
        layout.prop(scene, "enable_report", text="Enable Report in the UI")
        use_separator(self, context)

        layout.label("Set the Bake Resolution")
        res = str(scene.img_bake_size)
        layout.label("Current Setting is : %s" % (res + "x" + res), icon='INFO')
        use_separator(self, context)
        layout.prop(scene, "img_bake_size", icon='NODE_SEL', expand=True)


class MATERIAL_PT_scenemassive(Panel):
    bl_label = "Convert BI Materials to Cycles"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (enable_converters() is True and converter_type('BI_CONV'))

    def draw(self, context):
        layout = self.layout
        sc = context.scene
        row = layout.row()
        box = row.box()

        split = box.box().split(0.5)
        split.operator("ml.refresh",
                       text="Convert All to Cycles", icon='MATERIAL')
        split.operator("ml.refresh_active",
                       text="Convert Active to Cycles", icon='MATERIAL')
        row = box.row()
        ml_restore = row.operator("ml.restore", text="To BI Nodes Off",
                                  icon='MATERIAL')
        ml_restore.switcher = False
        ml_restore.renderer = "BI"

        box = layout.box()
        row = box.row()
        row.menu("scenemassive.opt", text="Advanced Options", icon='SCRIPTWIN')
        row.menu("help.biconvert",
                 text="Usage Information Guide", icon="INFO")

        box = layout.box()
        box.label("Save Directory")
        split = box.split(0.85)
        split.prop(sc.mat_specials, "conv_path", text="", icon="RENDER_RESULT")
        split.operator("material.check_converter_path",
                       text="", icon="EXTERNAL_DATA")


class MATERIAL_PT_xps_convert(Panel):
    bl_label = "Convert to BI and Cycles Nodes"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return (enable_converters() is True and converter_type('CYC_CONV'))

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        box = row.box()

        box.label(text="Multi Image Support (Imports)")
        split = box.box().split(0.5)
        split.operator("xps_tools.convert_to_cycles_all",
                       text="Convert All to Nodes", icon="TEXTURE")
        split.operator("xps_tools.convert_to_cycles_selected",
                       text="Convert Selected to Nodes", icon="TEXTURE")

        box = layout.box()
        row = box.row()
        ml_restore = row.operator("ml.restore", text="To BI Nodes ON",
                                  icon='MATERIAL')
        ml_restore.switcher = True
        ml_restore.renderer = "BI"

        row.menu("help.nodeconvert",
                 text="Usage Information Guide", icon="INFO")


# Converters Help #

class MATERIAL_MT_biconv_help(Menu):
    bl_idname = "help.biconvert"
    bl_description = "Read Instructions & Current Limitations"
    bl_label = "Usage Information Guide"
    bl_options = {'REGISTER'}

    def draw(self, context):
        layout = self.layout
        layout.label(text="If possible, avoid multiple conversions in a row")
        layout.label(text="Save Your Work Often", icon="ERROR")
        use_separator(self, context)
        layout.label(text="Try to link them manually using Mix Color nodes")
        layout.label(text="Only the last Image in the stack gets linked to Shader")
        layout.label(text="Current limitation:", icon="MOD_EXPLODE")
        use_separator(self, context)
        layout.label(text="Select the texture loaded in the image node")
        layout.label(text="Press Ctrl/T to create the image nodes")
        layout.label(text="In the Node Editor, Select the Diffuse Node")
        layout.label(text="Enable Node Wrangler addon", icon="NODETREE")
        layout.label(text="If Unconnected or No Image Node Error:", icon="MOD_EXPLODE")
        use_separator(self, context)
        layout.label(text="Extract Alpha: the images have to have alpha channel")
        layout.label(text="The default path is the folder where the current .blend is")
        layout.label(text="During Baking, the script will check writting privileges")
        layout.label(text="Set the save path for extracting images with full access")
        layout.label(text="May Require Run As Administrator on Windows OS", icon="ERROR")
        layout.label(text="Converts Bi Textures to Image Files:", icon="MOD_EXPLODE")
        use_separator(self, context)
        layout.label(text="The Converter report can point out to some failures")
        layout.label(text="Some material combinations are unsupported")
        layout.label(text="Single BI Texture/Image per convert is only supported")
        layout.label(text="Converts Basic BI non node materials to Cycles")
        use_separator(self, context)
        layout.label(text="Convert Bi Materials to Cycles Nodes:", icon="INFO")


class MATERIAL_MT_nodeconv_help(Menu):
    bl_idname = "help.nodeconvert"
    bl_description = "Read Instructions & Current Limitations"
    bl_label = "Usage Information Guide"
    bl_options = {'REGISTER'}

    def draw(self, context):
        layout = self.layout
        layout.label(text="If possible, avoid multiple conversions in a row")
        layout.label(text="Save Your Work Often", icon="ERROR")
        use_separator(self, context)
        layout.label(text="Relinking and removing some not needed nodes")
        layout.label(text="The result Node tree will need some cleaning up")
        use_separator(self, context)
        layout.label(text="Select the texture loaded in the image node")
        layout.label(text="Press Ctrl/T to create the image nodes")
        layout.label(text="In the Node Editor, Select the Diffuse Node")
        layout.label(text="Enable Node Wrangler addon", icon="NODETREE")
        layout.label(text="If Unconnected or No Image Node Error:", icon="MOD_EXPLODE")
        use_separator(self, context)
        layout.label(text="For Specular Nodes, Image color influence has to be enabled")
        layout.label(text="Generated images (i.e. Noise and others) are not converted")
        layout.label(text="The Converter report can point out to some failures")
        layout.label(text="Not all Files will produce good results", icon="ERROR")
        layout.label(text="fbx, .dae, .obj, .3ds, .xna and more")
        layout.label(text="**Supports Imported Files**:", icon="IMPORT")
        use_separator(self, context)
        layout.label(text="For some file types")
        layout.label(text="Supports Alpha, Normals, Specular and Diffuse")
        layout.label(text="Then Converts BI Nodes to Cycles Nodes")
        layout.label(text="Converts BI non node materials to BI Nodes")
        use_separator(self, context)
        layout.label(text="Convert Materials/Image Textures from Imports:", icon="INFO")


# Make Report
class material_converter_report(Operator):
    bl_idname = "mat_converter.reports"
    bl_label = "Material Converter Report"
    bl_description = "Report about done Material Conversions"
    bl_options = {'REGISTER', 'INTERNAL'}

    message = StringProperty(maxlen=8192)

    def draw(self, context):
        layout = self.layout
        layout.label(text="Information:", icon='INFO')

        if self.message and type(self.message) is str:
            list_string = self.message.split("*")
            for line in range(len(list_string)):
                layout.label(text=str(list_string[line]))

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width=500)

    def execute(self, context):
        return {'FINISHED'}


# Scene Properties
class material_specials_scene_props(PropertyGroup):
    conv_path = StringProperty(
            name="Save Directory",
            description=("Path to save images during conversion \n"
                         "Default is the location of the blend file"),
            default="//",
            subtype='DIR_PATH',
            )
    EXTRACT_ALPHA = BoolProperty(
            attr="EXTRACT_ALPHA",
            default=False,
            description=("Extract Alpha channel from non-procedural images \n"
                         "Don't use this option if the image doesn't have Alpha"),
            )
    SET_FAKE_USER = BoolProperty(
            attr="SET_FAKE_USER",
            default=False,
            description="Set fake user on unused images, so they can be kept in the .blend",
            )
    EXTRACT_PTEX = BoolProperty(
            attr="EXTRACT_PTEX",
            default=False,
            description="Extract procedural images and bake them to jpeg",
            )
    EXTRACT_OW = BoolProperty(
            attr="Overwrite",
            default=False,
            description="Extract textures again instead of re-using priorly extracted textures",
            )
    SCULPT_PAINT = BoolProperty(
            attr="SCULPT_PAINT",
            default=False,
            description=("Conversion geared towards sculpting and painting.\n"
                         "Creates a diffuse, glossy mixed with layer weight. \n"
                         "Image nodes are not connected"),
            )
    UV_UNWRAP = BoolProperty(
            attr="UV_UNWRAP",
            default=False,
            description=("Use automatical Angle based UV Unwrap of the active Object"),
            )
    enable_report = BoolProperty(
            attr="enable_report",
            default=False,
            description=("Enable Converter Report in the UI"),
            )
    img_bake_size = EnumProperty(
            name="Bake Image Size",
            description="Set the resolution size of baked images \n",
            items=(('512', "Set : 512 x 512", "Bake Resolution 512 x 512"),
                   ('1024', "Set : 1024 x 1024", "Bake Resolution 1024 x 1024"),
                   ('2048', "Set : 2048 x 2048", "Bake Resolution 2048 x 2048")),
            default='1024',
            )
    set_material_name = StringProperty(
            name="New Material name",
            description="What Base name pattern to use for a new created Material\n"
                        "It is appended by an automatic numeric pattern depending\n"
                        "on the number of Scene's materials containing the Base",
            default="Material_New",
            maxlen=128,
            )
    use_tweak = BoolProperty(
        name="Tweak Settings",
        description="Open Preview Active Material after new Material creation",
        default=False,
        )


# Addon Preferences
class VIEW3D_MT_material_utils_pref(AddonPreferences):
    bl_idname = __name__

    show_warnings = BoolProperty(
            name="Enable Warning messages",
            default=False,
            description="Show warning messages \n"
                        "when an action is executed or failed.\n \n"
                        "Advisable if you don't know how the tool works",
            )
    show_remove_mat = BoolProperty(
            name="Enable Remove all Materials",
            default=False,
            description="Enable Remove all Materials for all Selected Objects\n\n"
                        "Use with care - if you want to keep materials after\n"
                        "closing or reloading Blender, Set Fake User for them",
            )
    show_mat_preview = BoolProperty(
            name="Enable Material Preview",
            default=True,
            description="Material Preview of the Active Object \n"
                        "Contains the preview of the active Material, \n"
                        "Use nodes, Color, Specular and Transparency \n"
                        "settings depending on the Context and Preferences",
            )
    set_cleanmatslots = BoolProperty(
            name="Enable Auto Clean",
            default=True,
            description="Enable Automatic Removal of unused Material Slots \n"
                        "called together with the Assign Material menu option. \n \n"
                        "Apart from preference and the cases when it affects \n"
                        "adding materials, enabling it can have some \n"
                        "performance impact on very dense meshes",
            )
    show_separators = BoolProperty(
            name="Use Separators in the menus",
            default=True,
            description="Use separators in the menus, a trade-off between \n"
                        "readability vs. using more space for displaying items",
            )
    show_converters = BoolProperty(
            name="Enable Converters",
            default=False,
            description="Enable Material Converters",
            )
    set_preview_size = EnumProperty(
            name="Preview Menu Size",
            description="Set the preview menu size \n"
                        "depending on the number of materials \n"
                        "in the scene (width and height)",
            items=(('2x2', "Size 2x2", "Width 2 Height 2"),
                   ('2x3', "Size 2x3", "Width 3 Height 2"),
                   ('3x3', "Size 3x3", "Width 3 Height 3"),
                   ('3x4', "Size 3x4", "Width 4 Height 3"),
                   ('4x4', "Size 4x4", "Width 4 Height 4"),
                   ('5x5', "Size 5x5", "Width 5 Height 5"),
                   ('6x6', "Size 6x6", "Width 6 Height 6"),
                   ('0x0', "List", "Display as a List")),
            default='3x3',
            )
    set_preview_type = EnumProperty(
            name="Preview Menu Type",
            description="Set the the Preview menu type",
            items=(('LIST', "Classic",
                    "Display as a Classic List like in Blender Propreties.\n"
                    "Preview of Active Material is not available"),
                   ('PREVIEW', "Preview Display",
                    "Display as a preview of Thumbnails\n"
                    "It can have some performance issues with scenes containing a lot of materials\n"
                    "Preview of Active Material is available")),
            default='PREVIEW',
            )
    set_experimental_type = EnumProperty(
            name="Experimental Features",
            description="Set the Type of converters enabled",
            items=(('ALL', "All Converters",
                    "Enable all Converters"),
                   ('CYC_CONV', "BI and Cycles Nodes",
                    "Enable Cycles related Convert"),
                   ('BI_CONV', "BI To Cycles",
                    "Enable Blender Internal related Converters")),
            default='ALL',
            )

    def draw(self, context):
        layout = self.layout
        sc = context.scene

        box = layout.box()
        box.label("Save Directory")
        split = box.split(0.85)
        split.prop(sc.mat_specials, "conv_path", text="", icon="RENDER_RESULT")
        split.operator("material.check_converter_path",
                       text="", icon="EXTERNAL_DATA")

        box = layout.box()
        split = box.split(align=True)

        col = split.column()
        col.prop(self, "show_warnings")
        col.prop(self, "show_remove_mat")

        col = split.column()
        col.alignment = 'RIGHT'
        col.prop(self, "set_cleanmatslots")
        col.prop(self, "show_separators")

        boxie = box.box()
        split = boxie.split(percentage=0.3)
        split.prop(self, "show_mat_preview")
        if self.show_mat_preview:
            rowsy = split.row(align=True)
            rowsy.enabled = True if self.show_mat_preview else False
            rowsy.prop(self, "set_preview_type", expand=True)
            rowsa = boxie.row(align=True)
            rowsa.enabled = True if self.set_preview_type in {'PREVIEW'} else False
            rowsa.prop(self, "set_preview_size", expand=True)

        boxif = box.box()
        split = boxif.split(percentage=0.3)
        split.prop(self, "show_converters")
        if self.show_converters:
            rowe = split.row(align=True)
            rowe.prop(self, "set_experimental_type", expand=True)


# utility functions:

def check_mat_name_unique(name_id="Material_new"):
    # check if the new name pattern is in materials' data
    name_list = []
    suffix = 1
    try:
        if c_data_has_materials():
            name_list = [mat.name for mat in bpy.data.materials if name_id in mat.name]
            new_name = "{}_{}".format(name_id, len(name_list) + suffix)
            if new_name in name_list:
                # KISS failed - numbering is not sequential
                # try harvesting numbers in material names, find the rightmost ones
                test_num = []
                from re import findall
                for words in name_list:
                    test_num.append(findall("\d+", words))

                suffix += max([int(l[-1]) for l in test_num])
                new_name = "{}_{}".format(name_id, suffix)
            return new_name
    except Exception as e:
        print("\n[Materials Utils Specials]\nfunction: check_mat_name_unique\nError: %s \n" % e)
        pass
    return name_id


def included_object_types(objects):
    # Pass the bpy.data.objects.type to avoid needless assigning/removing
    # included - type that can have materials
    included = ['MESH', 'CURVE', 'SURFACE', 'FONT', 'META']
    obj = objects
    return bool(obj and obj in included)


def check_is_excluded_obj_types(contxt):
    # pass the context to check if selected objects have excluded types
    if contxt and contxt.selected_editable_objects:
        for obj in contxt.selected_editable_objects:
            if not included_object_types(obj.type):
                return True
    return False


def check_texface_to_mat(obj):
    # check for UV data presence
    if obj:
        if hasattr(obj.data, "uv_textures"):
            if hasattr(obj.data.uv_textures, "active"):
                if hasattr(obj.data.uv_textures.active, "data"):
                    return True
    return False


def c_context_mat_preview():
    # returns the type of viewport shading
    # needed for using the optional UI elements (the context gets lost)

    # code from BA user SynaGl0w
    # if there are multiple 3d views return the biggest screen area one
    views_3d = [area for area in bpy.context.screen.areas if
                area.type == 'VIEW_3D' and area.spaces.active]

    if views_3d:
        main_view_3d = max(views_3d, key=lambda area: area.width * area.height)
        return main_view_3d.spaces.active.viewport_shade
    return "NONE"


def c_context_use_nodes():
    # checks if Use Nodes is ticked on
    actob = bpy.context.active_object
    u_node = (actob.active_material.use_nodes if
              hasattr(actob, "active_material") else False)

    return bool(u_node)


def c_render_engine(cyc=None):
    # valid cyc inputs "Cycles", "BI", "Both", "Lux"
    scene = bpy.context.scene
    render_engine = scene.render.engine

    r_engines = {"Cycles": 'CYCLES',
                 "BI": 'BLENDER_RENDER',
                 "Both": ('CYCLES', 'BLENDER_RENDER'),
                 "Lux": 'LUXRENDER_RENDER'}
    if cyc:
        return (True if cyc in r_engines and render_engine in r_engines[cyc] else False)
    return render_engine


def c_need_of_viewport_colors():
    # check the context where using Viewport color and friends are needed
    # Cycles and BI are supported
    if c_render_engine("Cycles"):
        if c_context_use_nodes() and c_context_mat_preview() == 'SOLID':
            return True
        elif c_context_mat_preview() in ('SOLID', 'TEXTURED', 'MATERIAL'):
            return True
    elif (c_render_engine("BI") and not c_context_use_nodes()):
        return True

    return False


# Draw Separator
def use_separator(operator, context):
    # pass the preferences show_separators bool to enable/disable them
    pref = return_preferences()
    useSep = pref.show_separators
    if useSep:
        operator.layout.separator()


# preferences utilities

def return_preferences():
    return bpy.context.user_preferences.addons[__name__].preferences


def use_remove_mat_all():
    pref = return_preferences()
    show_rmv_mat = pref.show_remove_mat

    return bool(show_rmv_mat)


def use_mat_preview():
    pref = return_preferences()
    show_mat_prw = pref.show_mat_preview

    return bool(show_mat_prw)


def use_cleanmat_slots():
    pref = return_preferences()
    use_mat_clean = pref.set_cleanmatslots

    return bool(use_mat_clean)


def size_preview():
    pref = return_preferences()
    set_size_prw = pref.set_preview_size

    cell_w = int(set_size_prw[0])
    cell_h = int(set_size_prw[-1])
    cell_tbl = {'Width': cell_w, 'Height': cell_h}

    return cell_tbl


def size_type_is_preview():
    pref = return_preferences()
    set_prw_type = pref.set_preview_type

    return bool(set_prw_type in {'PREVIEW'})


def enable_converters():
    pref = return_preferences()
    shw_conv = pref.show_converters

    return shw_conv


def converter_type(types='ALL'):
    # checks the type of the preferences 'ALL', 'CYC_CONV', 'BI_CONV'
    pref = return_preferences()
    set_exp_type = pref.set_experimental_type

    return bool(set_exp_type in {'ALL'} or types == set_exp_type)


def register():
    bpy.utils.register_module(__name__)

    warning_messages_utils.MAT_SPEC_NAME = __name__

    # Register Scene Properties
    bpy.types.Scene.mat_specials = PointerProperty(
                                        type=material_specials_scene_props
                                        )

    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps.new(name="3D View", space_type="VIEW_3D")
        kmi = km.keymap_items.new('wm.call_menu', 'Q', 'PRESS', shift=True)
        kmi.properties.name = "VIEW3D_MT_master_material"

    bpy.types.MATERIAL_MT_specials.prepend(menu_move)
    bpy.types.MATERIAL_MT_specials.append(menu_func)


def unregister():
    kc = bpy.context.window_manager.keyconfigs.addon
    if kc:
        km = kc.keymaps["3D View"]
        for kmi in km.keymap_items:
            if kmi.idname == 'wm.call_menu':
                if kmi.properties.name == "VIEW3D_MT_master_material":
                    km.keymap_items.remove(kmi)
                    break

    bpy.types.MATERIAL_MT_specials.remove(menu_move)
    bpy.types.MATERIAL_MT_specials.remove(menu_func)

    del bpy.types.Scene.mat_specials

    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
