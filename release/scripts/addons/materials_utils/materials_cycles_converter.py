# gpl: author Silvio Falcinelli. Fixes by angavrilov and others.
# special thanks to user blenderartists.org cmomoney
# -*- coding: utf-8 -*-

import bpy
from os import path as os_path
from bpy.types import Operator
from math import (
        log2, ceil,
        )
from bpy.props import (
        BoolProperty,
        EnumProperty,
        )
from .warning_messages_utils import (
        warning_messages,
        c_is_cycles_addon_enabled,
        c_data_has_materials,
        collect_report,
        )

# -----------------------------------------------------------------------------
# Globals

# switch for operator's function called after AutoNodeInitiate
CHECK_AUTONODE = False

# set the node color for baked images (default greenish)
NODE_COLOR = (0.32, 0.75, 0.32)
# set the node color for the paint base images (default reddish)
NODE_COLOR_PAINT = (0.6, 0.0, 0.0)
# set the mix node color (default blueish)
NODE_COLOR_MIX = (0.1, 0.7, 0.8)

# color for sculpt/texture painting setting (default clay the last entry is Roughness)
PAINT_SC_COLOR = (0.80, 0.75, 0.54, 0.9)
CLAY_GLOSSY = (0.38, 0.032, 0.023, 1)

# -----------------------------------------------------------------------------
# Functions


def AutoNodeSwitch(renderer="CYCLES", switch="OFF", operator=None):
    mats = bpy.data.materials
    use_nodes = (True if switch in ("ON") else False)
    warn_message = ('BI_SW_NODES_ON' if switch in ("ON") else
                    'BI_SW_NODES_OFF')
    warn_message_2 = ('CYC_SW_NODES_ON' if switch in ("ON") else
                      'CYC_SW_NODES_OFF')
    for cmat in mats:
        cmat.use_nodes = use_nodes
    renders = ('CYCLES' if renderer and renderer == "CYCLES" else
               'BLENDER_RENDER')
    bpy.context.scene.render.engine = renders
    if operator:
        warning_messages(operator, (warn_message_2 if renders in ('CYCLES') else
                                    warn_message))


def SetFakeUserTex():
    images = bpy.data.images
    for image in images:
        has_user = getattr(image, "users", -1)
        image_name = getattr(image, "name", "NONAME")

        if has_user == 0:
            image.use_fake_user = True
            collect_report("INFO: Set fake user for unused image: " + image_name)


def BakingText(tex, mode, tex_type=None):
    collect_report("INFO: start bake texture named: " + tex.name)
    saved_img_path = None

    bpy.ops.object.mode_set(mode='OBJECT')
    sc = bpy.context.scene
    tmat = ''
    img = ''
    Robj = bpy.context.active_object
    for n in bpy.data.materials:
        if n.name == 'TMP_BAKING':
            tmat = n
    if not tmat:
        tmat = bpy.data.materials.new('TMP_BAKING')
        tmat.name = "TMP_BAKING"

    bpy.ops.mesh.primitive_plane_add()
    tm = bpy.context.active_object
    tm.name = "TMP_BAKING"
    tm.data.name = "TMP_BAKING"
    bpy.ops.object.select_pattern(extend=False, pattern="TMP_BAKING",
                                  case_sensitive=False)
    sc.objects.active = tm
    bpy.context.scene.render.engine = 'BLENDER_RENDER'
    tm.data.materials.append(tmat)
    if len(tmat.texture_slots.items()) == 0:
        tmat.texture_slots.add()
    tmat.texture_slots[0].texture_coords = 'UV'
    tmat.texture_slots[0].use_map_alpha = True
    tmat.texture_slots[0].texture = tex.texture
    tmat.texture_slots[0].use_map_alpha = True
    tmat.texture_slots[0].use_map_color_diffuse = False
    tmat.use_transparency = True
    tmat.alpha = 0
    tmat.use_nodes = False
    tmat.diffuse_color = 1, 1, 1
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.uv.unwrap()

    # clean up temporary baking images if any
    for n in bpy.data.images:
        if n.name == 'TMP_BAKING':
            n.user_clear()
            bpy.data.images.remove(n)

    if mode == "ALPHA" and tex.texture.type == 'IMAGE':
        sizeX = tex.texture.image.size[0]
        sizeY = tex.texture.image.size[1]
    else:
        bake_size = (int(sc.mat_specials.img_bake_size) if
                     sc.mat_specials.img_bake_size else 1024)
        sizeX = bake_size
        sizeY = bake_size

    bpy.ops.image.new(name="TMP_BAKING", width=sizeX, height=sizeY,
                      color=(0.0, 0.0, 0.0, 1.0), alpha=True, float=False)

    sc.render.engine = 'BLENDER_RENDER'
    img = bpy.data.images["TMP_BAKING"]
    img = bpy.data.images.get("TMP_BAKING")
    img.file_format = ("JPEG" if not mode == "ALPHA" else "PNG")

    # switch temporarly to 'IMAGE EDITOR', other approaches are not reliable
    check_area = False
    store_area = bpy.context.area.type
    collect_report("INFO: Temporarly switching context to Image Editor")
    try:
        bpy.context.area.type = 'IMAGE_EDITOR'
        bpy.context.area.spaces[0].image = bpy.data.images["TMP_BAKING"]
        check_area = True
    except:
        collect_report("ERROR: Setting to Image Editor failed, Baking aborted")
        check_area = False

    if check_area:
        paths = bpy.path.abspath(sc.mat_specials.conv_path)
        tex_name = getattr(getattr(tex.texture, "image", None), "name", None)
        texture_name = (tex_name.rpartition(".")[0] if tex_name else tex.texture.name)
        new_tex_name = "baked"
        name_append = ("_BAKING" if mode == "ALPHA" and
                       tex.texture.type == 'IMAGE' else "_PTEXT")
        new_appendix = (".jpg" if not mode == "ALPHA" else ".png")

        if name_append in texture_name:
            new_tex_name = texture_name
        elif tex_type:
            new_tex_name = tex_type + name_append
        else:
            new_tex_name = texture_name + name_append

        img.filepath_raw = paths + new_tex_name + new_appendix
        saved_img_path = img.filepath_raw

        sc.render.bake_type = 'ALPHA'
        sc.render.use_bake_selected_to_active = True
        sc.render.use_bake_clear = True

        # try to bake if it fails give report
        try:
            bpy.ops.object.bake_image()
            img.save()
        except:
            # no return value, so the image loading is skipped
            saved_img_path = None
            collect_report("ERROR: Baking could not be completed. "
                           "Check System Console for info")
    if store_area:
        bpy.context.area.type = store_area

    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.delete()
    bpy.ops.object.select_pattern(extend=False, pattern=Robj.name, case_sensitive=False)
    sc.objects.active = Robj
    img.user_clear()
    bpy.data.images.remove(img)

    if tmat.users == 0:
        bpy.data.materials.remove(tmat)

    if saved_img_path:
        collect_report("------- Baking finished -------")
        return saved_img_path


def AutoNodeInitiate(active=False, operator=None):
    # Checks with bpy.ops.material.check_converter_path
    # if it's possible to write in the output path
    # if it passes procedes with calling AutoNode

    # if CheckImagePath(operator):
    check_path = bpy.ops.material.check_converter_path()

    global CHECK_AUTONODE

    if 'FINISHED' in check_path:
        sc = bpy.context.scene
        CHECK_AUTONODE = True
        collect_report("_______________________", True, False)
        AutoNode(active, operator)
        if sc.mat_specials.SET_FAKE_USER:
            SetFakeUserTex()
    else:
        warning_messages(operator, 'DIR_PATH_CONVERT', override=True)


def AutoNode(active=False, operator=None):
    global CHECK_AUTONODE
    sc = bpy.context.scene
    if active:
        # fix for empty slots by angavrilov
        mats = [slot.material for slot in bpy.context.active_object.material_slots if
                slot.material]
    else:
        mats = bpy.data.materials

    # No Materials for the chosen action - abort
    if not mats:
        CHECK_AUTONODE = False
        if operator:
            if active:
                act_obj = bpy.context.active_object
                warning_messages(operator, 'CONV_NO_OBJ_MAT', act_obj.name)
            else:
                warning_messages(operator, 'CONV_NO_SC_MAT')
        return

    for cmat in mats:
        # check for empty material (it will fall through the first check)
        test_empty = getattr(cmat, "name", None)
        if test_empty is None:
            collect_report("An empty material was hit, skipping")
            continue

        cmat.use_nodes = True
        TreeNodes = cmat.node_tree
        links = TreeNodes.links

        # Don't alter nodes of locked materials
        locked = False
        for n in TreeNodes.nodes:
            if n.type == 'ShaderNodeOutputMaterial':
                if n.label == 'Locked':
                    locked = True
                    break

        if not locked:
            # Convert this material from non-nodes to Cycles nodes
            shader = ''
            shtsl = ''
            Add_Emission = ''
            Add_Translucent = ''
            Mix_Alpha = ''
            sT = False
            # check if some link creation failed
            link_fail = False

            for n in TreeNodes.nodes:
                TreeNodes.nodes.remove(n)

            # Starting point is diffuse BSDF and output material
            # and a Color Ramp node
            shader = TreeNodes.nodes.new('ShaderNodeBsdfDiffuse')
            shader.location = 10, 10
            shader_val = TreeNodes.nodes.new('ShaderNodeValToRGB')
            shader_val.location = 0, -200
            shout = TreeNodes.nodes.new('ShaderNodeOutputMaterial')
            shout.location = 200, 10
            try:
                links.new(shader.outputs[0], shout.inputs[0])
                links.new(shader.inputs[0], shader_val.outputs[0])
            except:
                link_fail = True

            # Create other shader types only sculpt/texture paint mode is False
            sculpt_paint = sc.mat_specials.SCULPT_PAINT
            if sculpt_paint is False:

                cmat_is_transp = cmat.use_transparency and cmat.alpha < 1

                if not cmat.raytrace_mirror.use and not cmat_is_transp:
                    if not shader.type == 'ShaderNodeBsdfDiffuse':
                        collect_report("INFO: Make DIFFUSE shader node for: " + cmat.name)
                        TreeNodes.nodes.remove(shader)
                        shader = TreeNodes.nodes.new('ShaderNodeBsdfDiffuse')
                        shader.location = 10, 10
                        try:
                            links.new(shader.outputs[0], shout.inputs[0])
                        except:
                            link_fail = True

                if cmat.raytrace_mirror.use and cmat.raytrace_mirror.reflect_factor > 0.001 and cmat_is_transp:
                    if not shader.type == 'ShaderNodeBsdfGlass':
                        collect_report("INFO: Make GLASS shader node for: " + cmat.name)
                        TreeNodes.nodes.remove(shader)
                        shader = TreeNodes.nodes.new('ShaderNodeBsdfGlass')
                        shader.location = 0, 100
                        try:
                            links.new(shader.outputs[0], shout.inputs[0])
                        except:
                            link_fail = True

                if cmat.raytrace_mirror.use and not cmat_is_transp and cmat.raytrace_mirror.reflect_factor > 0.001:
                    if not shader.type == 'ShaderNodeBsdfGlossy':
                        collect_report("INFO: Make MIRROR shader node for: " + cmat.name)
                        TreeNodes.nodes.remove(shader)
                        shader = TreeNodes.nodes.new('ShaderNodeBsdfGlossy')
                        shader.location = 0, 10
                        try:
                            links.new(shader.outputs[0], shout.inputs[0])
                        except:
                            link_fail = True

                if cmat.emit > 0.001:
                    if (not shader.type == 'ShaderNodeEmission' and not
                       cmat.raytrace_mirror.reflect_factor > 0.001 and not cmat_is_transp):

                        collect_report("INFO: Mix EMISSION shader node for: " + cmat.name)
                        TreeNodes.nodes.remove(shader)
                        shader = TreeNodes.nodes.new('ShaderNodeEmission')
                        shader.location = 0, 200
                        try:
                            links.new(shader.outputs[0], shout.inputs[0])
                        except:
                            link_fail = True
                    else:
                        if not Add_Emission:
                            collect_report("INFO: Add EMISSION shader node for: " + cmat.name)
                            shout.location = 600, 100
                            Add_Emission = TreeNodes.nodes.new('ShaderNodeAddShader')
                            Add_Emission.location = 370, 100

                            shem = TreeNodes.nodes.new('ShaderNodeEmission')
                            shem.location = 0, 200
                            try:
                                links.new(Add_Emission.outputs[0], shout.inputs[0])
                                links.new(shem.outputs[0], Add_Emission.inputs[1])
                                links.new(shader.outputs[0], Add_Emission.inputs[0])
                            except:
                                link_fail = True

                            shem.inputs['Color'].default_value = (cmat.diffuse_color.r,
                                                                  cmat.diffuse_color.g,
                                                                  cmat.diffuse_color.b, 1)
                            shem.inputs['Strength'].default_value = cmat.emit

                if cmat.translucency > 0.001:
                    collect_report("INFO: Add BSDF_TRANSLUCENT shader node for: " + cmat.name)
                    shout.location = 770, 330
                    Add_Translucent = TreeNodes.nodes.new('ShaderNodeAddShader')
                    Add_Translucent.location = 580, 490

                    shtsl = TreeNodes.nodes.new('ShaderNodeBsdfTranslucent')
                    shtsl.location = 400, 350
                    try:
                        links.new(Add_Translucent.outputs[0], shout.inputs[0])
                        links.new(shtsl.outputs[0], Add_Translucent.inputs[1])

                        if Add_Emission:
                            links.new(Add_Emission.outputs[0], Add_Translucent.inputs[0])
                        else:
                            links.new(shader.outputs[0], Add_Translucent.inputs[0])
                    except:
                        link_fail = True

                    shtsl.inputs['Color'].default_value = (cmat.translucency,
                                                           cmat.translucency,
                                                           cmat.translucency, 1)
            if sculpt_paint is False:
                shader.inputs['Color'].default_value = (cmat.diffuse_color.r,
                                                        cmat.diffuse_color.g,
                                                        cmat.diffuse_color.b, 1)
            else:
                # Create Clay Material (Diffuse, Glossy, Layer Weight)
                shader.inputs['Color'].default_value = PAINT_SC_COLOR
                shader.inputs['Roughness'].default_value = 0.9

                # remove Color Ramp and links from the default shader and reroute
                try:
                    shout.location = 400, 0
                    for link in links:
                        links.remove(link)

                    clay_frame = TreeNodes.nodes.new('NodeFrame')
                    clay_frame.name = 'Clay Material'
                    clay_frame.label = 'Clay Material'

                    sh_glossy = TreeNodes.nodes.new('ShaderNodeBsdfGlossy')
                    sh_glossy.location = 0, 200
                    sh_glossy.inputs['Color'].default_value = CLAY_GLOSSY
                    sh_mix = TreeNodes.nodes.new('ShaderNodeMixShader')
                    sh_mix.location = 200, 0
                    sh_weight = TreeNodes.nodes.new('ShaderNodeLayerWeight')
                    sh_weight.location = 0, 350
                    links.new(sh_mix.outputs[0], shout.inputs[0])
                    links.new(sh_weight.outputs[1], sh_mix.inputs[0])
                    links.new(shader.outputs[0], sh_mix.inputs[1])
                    links.new(sh_glossy.outputs[0], sh_mix.inputs[2])
                    # set frame as parent to everything
                    for clay_node in (shader, sh_glossy, sh_mix, sh_weight):
                        clay_node.parent = clay_frame
                except:
                    collect_report("ERROR: Failure to create Clay Material")

            if not sculpt_paint:
                if shader.type == 'ShaderNodeBsdfDiffuse':
                    shader.inputs['Roughness'].default_value = cmat.specular_intensity

                if shader.type == 'ShaderNodeBsdfGlossy':
                    shader.inputs['Roughness'].default_value = 1 - cmat.raytrace_mirror.gloss_factor

                if shader.type == 'ShaderNodeBsdfGlass':
                    shader.inputs['Roughness'].default_value = 1 - cmat.raytrace_mirror.gloss_factor
                    shader.inputs['IOR'].default_value = cmat.raytrace_transparency.ior

                if shader.type == 'ShaderNodeEmission':
                    shader.inputs['Strength'].default_value = cmat.emit

            # texture presence check
            is_textures = False

            for tex in cmat.texture_slots:
                if tex:
                    if not is_textures:
                        is_textures = True
                        break

            if is_textures:
                # collect the texture nodes created
                # for spreading a bit the texture nodes
                tex_node_collect = []

                sM = True

                for tex in cmat.texture_slots:
                    sT = False
                    tex_use = getattr(tex, "use", None)
                    baked_path = None
                    if tex_use:
                        tex_node_loc = -200, 450
                        ma_alpha = getattr(tex, "use_map_alpha", None)
                        sM = (False if ma_alpha else True)
                        img = None

                        if tex.texture.type == 'IMAGE':
                            if sc.mat_specials.EXTRACT_ALPHA and tex.texture.use_alpha:
                                if (not
                                   os_path.exists(bpy.path.abspath(tex.texture.image.filepath + "_BAKING.png")) or
                                   sc.mat_specials.EXTRACT_OW):
                                    baked_path = BakingText(tex, 'ALPHA')

                                    if baked_path:
                                        try:
                                            img = bpy.data.images.load(baked_path)
                                            collect_report("INFO: Loading Baked texture path:")
                                            collect_report(baked_path)
                                        except:
                                            collect_report("ERROR: Baked image could not be loaded")
                            else:
                                has_image = getattr(tex.texture, "image", None)
                                if has_image:
                                    img = has_image

                            if img:
                                img_name = (img.name if hasattr(img, "name") else "NO NAME")
                                shtext = TreeNodes.nodes.new('ShaderNodeTexImage')
                                shtext.location = tex_node_loc
                                shtext.hide = True
                                shtext.width_hidden = 150
                                shtext.image = img
                                shtext.name = img_name
                                shtext.label = "Image " + img_name
                                if baked_path:
                                    shtext.use_custom_color = True
                                    shtext.color = NODE_COLOR
                                collect_report("INFO: Creating Image Node for image: " + img_name)
                                tex_node_collect.append(shtext)
                                sT = True
                            else:
                                collect_report("ERROR: A problem occured with loading an image for {} "
                                               "(possibly missing)".format(tex.texture.name))
                        else:
                            if sc.mat_specials.EXTRACT_PTEX or (sc.mat_specials.EXTRACT_ALPHA and ma_alpha):
                                if (not os_path.exists(bpy.path.abspath(tex.texture.name + "_PTEXT.jpg")) or
                                   sc.mat_specials.EXTRACT_OW):
                                    tex_type = tex.texture.type.lower()
                                    collect_report("Attempting to Extract Procedural Texture type: " + tex_type)
                                    baked_path = BakingText(tex, 'PTEX', tex_type)

                                if baked_path:
                                    try:
                                        img = bpy.data.images.load(baked_path)
                                        collect_report("Loading Baked texture path:")
                                        collect_report(baked_path)
                                        img_name = (img.name if hasattr(img, "name") else "NO NAME")
                                        shtext = TreeNodes.nodes.new('ShaderNodeTexImage')
                                        shtext.location = tex_node_loc
                                        shtext.hide = True
                                        shtext.width_hidden = 150
                                        shtext.image = img
                                        shtext.name = img_name
                                        shtext.label = "Baked Image " + img_name
                                        shtext.use_custom_color = True
                                        shtext.color = NODE_COLOR
                                        collect_report("Creating Image Node for baked image: " + img_name)
                                        tex_node_collect.append(shtext)
                                        sT = True
                                    except:
                                        collect_report("ERROR: Failure to load baked image: " + img_name)
                                else:
                                    collect_report("ERROR: Failure during baking, no images loaded")

                    if sculpt_paint is False:
                        if (cmat_is_transp and cmat.raytrace_transparency.ior == 1 and
                           not cmat.raytrace_mirror.use and sM):

                            if not shader.type == 'ShaderNodeBsdfTransparent':
                                collect_report("INFO: Make TRANSPARENT shader node for: " + cmat.name)
                                TreeNodes.nodes.remove(shader)
                                shader = TreeNodes.nodes.new('ShaderNodeBsdfTransparent')
                                shader.location = 0, 470
                                try:
                                    links.new(shader.outputs[0], shout.inputs[0])
                                except:
                                    link_fail = True

                                shader.inputs['Color'].default_value = (cmat.diffuse_color.r,
                                                                        cmat.diffuse_color.g,
                                                                        cmat.diffuse_color.b, 1)

                    if sT and sculpt_paint is False:
                        if tex.use_map_color_diffuse:
                            try:
                                links.new(shtext.outputs[0], shader.inputs[0])
                            except:
                                pass
                        if tex.use_map_emit:
                            if not Add_Emission:
                                collect_report("INFO: Mix EMISSION + Texture shader node for: " + cmat.name)
                                intensity = 0.5 + (tex.emit_factor / 2)

                                shout.location = 550, 330
                                Add_Emission = TreeNodes.nodes.new('ShaderNodeAddShader')
                                Add_Emission.name = "Add_Emission"
                                Add_Emission.location = 370, 490

                                shem = TreeNodes.nodes.new('ShaderNodeEmission')
                                shem.location = 180, 380

                                try:
                                    links.new(Add_Emission.outputs[0], shout.inputs[0])
                                    links.new(shem.outputs[0], Add_Emission.inputs[1])
                                    links.new(shader.outputs[0], Add_Emission.inputs[0])
                                    links.new(shtext.outputs[0], shem.inputs[0])
                                except:
                                    link_fail = True

                                shem.inputs['Color'].default_value = (cmat.diffuse_color.r,
                                                                      cmat.diffuse_color.g,
                                                                      cmat.diffuse_color.b, 1)
                                shem.inputs['Strength'].default_value = intensity * 2

                        if tex.use_map_mirror:
                            try:
                                links.new(shtext.outputs[0], shader.inputs[0])
                            except:
                                link_fail = True

                        if tex.use_map_translucency:
                            if not Add_Translucent:
                                collect_report("INFO: Add Translucency + Texture shader node for: " + cmat.name)

                                intensity = 0.5 + (tex.emit_factor / 2)
                                shout.location = 550, 330
                                Add_Translucent = TreeNodes.nodes.new('ShaderNodeAddShader')
                                Add_Translucent.name = "Add_Translucent"
                                Add_Translucent.location = 370, 290

                                shtsl = TreeNodes.nodes.new('ShaderNodeBsdfTranslucent')
                                shtsl.location = 180, 240
                                try:
                                    links.new(shtsl.outputs[0], Add_Translucent.inputs[1])

                                    if Add_Emission:
                                        links.new(Add_Translucent.outputs[0], shout.inputs[0])
                                        links.new(Add_Emission.outputs[0], Add_Translucent.inputs[0])
                                        pass
                                    else:
                                        links.new(Add_Translucent.outputs[0], shout.inputs[0])
                                        links.new(shader.outputs[0], Add_Translucent.inputs[0])

                                    links.new(shtext.outputs[0], shtsl.inputs[0])
                                except:
                                    link_fail = True

                        if tex.use_map_alpha:
                            if not Mix_Alpha:
                                collect_report("INFO: Mix Alpha + Texture shader node for: " + cmat.name)

                                shout.location = 750, 330
                                Mix_Alpha = TreeNodes.nodes.new('ShaderNodeMixShader')
                                Mix_Alpha.name = "Add_Alpha"
                                Mix_Alpha.location = 570, 290
                                sMask = TreeNodes.nodes.new('ShaderNodeBsdfTransparent')
                                sMask.location = 250, 180
                                tMask, imask = None, None

                                # search if the texture node already exists, if not create
                                nodes = getattr(cmat.node_tree, "nodes", None)
                                img_name = getattr(img, "name", "NO NAME")
                                for node in nodes:
                                    if type(node) == bpy.types.ShaderNodeTexImage:
                                        node_name = getattr(node, "name")
                                        if img_name in node_name:
                                            tMask = node
                                            collect_report("INFO: Using existing Texture Node for Mask: " + node_name)
                                            break

                                if tMask is None:
                                    tMask = TreeNodes.nodes.new('ShaderNodeTexImage')
                                    tMask.location = tex_node_loc
                                    tex_node_collect.append(tMask)

                                    try:
                                        file_path = getattr(img, "filepath", None)
                                        if file_path:
                                            imask = bpy.data.images.load(file_path)
                                        else:
                                            imask = bpy.data.images.get(img_name)
                                        collect_report("INFO: Attempting to load image for Mask: " + img_name)
                                    except:
                                        collect_report("ERROR: Failure to load image for Mask: " + img_name)

                                    if imask:
                                        tMask.image = imask

                                if tMask:
                                    try:
                                        links.new(Mix_Alpha.inputs[0], tMask.outputs[1])
                                        links.new(shout.inputs[0], Mix_Alpha.outputs[0])
                                        links.new(sMask.outputs[0], Mix_Alpha.inputs[1])

                                        if not Add_Translucent:
                                            if Add_Emission:
                                                links.new(Mix_Alpha.inputs[2], Add_Emission.outputs[0])
                                            else:
                                                links.new(Mix_Alpha.inputs[2], shader.outputs[0])
                                        else:
                                            links.new(Mix_Alpha.inputs[2], Add_Translucent.outputs[0])
                                    except:
                                        link_fail = True
                                else:
                                    collect_report("ERROR: Mix Alpha could not be created "
                                                   "(mask image could not be loaded)")

                        if tex.use_map_normal:
                            t = TreeNodes.nodes.new('ShaderNodeRGBToBW')
                            t.location = -0, 300
                            try:
                                links.new(t.outputs[0], shout.inputs[2])
                                links.new(shtext.outputs[1], t.inputs[0])
                            except:
                                link_fail = True

                if sculpt_paint:
                    try:
                        # create a new image for texture painting and make it active
                        img_size = (int(sc.mat_specials.img_bake_size) if
                                    sc.mat_specials.img_bake_size else 1024)
                        paint_mat_name = getattr(cmat, "name", "NO NAME")
                        paint_img_name = "Paint Base Image {}".format(paint_mat_name)
                        bpy.ops.image.new(name=paint_img_name, width=img_size, height=img_size,
                                          color=(1.0, 1.0, 1.0, 1.0), alpha=True, float=False)

                        img = bpy.data.images.get(paint_img_name)
                        img_name = (img.name if hasattr(img, "name") else "NO NAME")
                        shtext = TreeNodes.nodes.new('ShaderNodeTexImage')
                        shtext.hide = True
                        shtext.width_hidden = 150
                        shtext.location = tex_node_loc
                        shtext.image = img
                        shtext.name = img_name
                        shtext.label = "Paint: " + img_name
                        shtext.use_custom_color = True
                        shtext.color = NODE_COLOR_PAINT
                        shtext.select = True
                        collect_report("INFO: Creating Image Node for Painting: " + img_name)
                        collect_report("WARNING: Don't forget to save it on Disk")
                        tex_node_collect.append(shtext)
                    except:
                        collect_report("ERROR: Failed to create image and node for Texture Painting")

                # spread the texture nodes, create node frames if necessary
                # create texture coordinate and mapping too
                row_node = -1
                tex_node_collect_size = len(tex_node_collect)
                median_point = ((tex_node_collect_size / 2) * 100)
                check_frame = bool(tex_node_collect_size > 1)

                node_frame, tex_map = None, None
                node_f_coord, node_f_mix = None, None
                tex_map_collection, tex_map_coord = [], None
                tree_size, tree_tex_start = 0, 0

                if check_frame:
                    node_frame = TreeNodes.nodes.new('NodeFrame')
                    node_frame.name = 'Converter Textures'
                    node_frame.label = 'Converter Textures'

                    node_f_coord = TreeNodes.nodes.new('NodeFrame')
                    node_f_coord.name = "Coordinates"
                    node_f_coord.label = "Coordinates"

                    node_f_mix = TreeNodes.nodes.new('NodeFrame')
                    node_f_mix.name = "Mix"
                    node_f_mix.label = "Mix"

                if tex_node_collect:
                    tex_map_coord = TreeNodes.nodes.new('ShaderNodeTexCoord')
                    tex_map_coord.location = -900, 575

                    # precalculate the depth of the inverted tree
                    tree_size = int(ceil(log2(tex_node_collect_size)))
                    # offset the start of the mix nodes by the depth of the tree
                    tree_tex_start = ((tree_size + 1) * 150)

                for node_tex in tex_node_collect:
                    row_node += 1
                    col_node_start = (median_point - (-(row_node * 50) + median_point))
                    tex_node_row = tree_tex_start + 300
                    mix_node_row = tree_tex_start + 620
                    tex_node_loc = (-(tex_node_row), col_node_start)

                    try:
                        node_tex.location = tex_node_loc
                        if check_frame:
                            node_tex.parent = node_frame
                        else:
                            node_tex.hide = False

                        tex_node_name = getattr(node_tex, "name", "NO NAME")
                        tex_map_name = "Mapping: {}".format(tex_node_name)
                        tex_map = TreeNodes.nodes.new('ShaderNodeMapping')
                        tex_map.location = (-(mix_node_row), col_node_start)
                        tex_map.width = 240
                        tex_map.hide = True
                        tex_map.width_hidden = 150
                        tex_map.name = tex_map_name
                        tex_map.label = tex_map_name
                        tex_map_collection.append(tex_map)
                        links.new(tex_map.outputs[0], node_tex.inputs[0])
                    except:
                        link_fail = True
                        continue

                if tex_map_collection:
                    tex_mix_start = len(tex_map_collection) / 2
                    row_map_start = -(tree_tex_start + 850)

                    if tex_map_coord:
                        tex_map_coord.location = (row_map_start,
                                                  (median_point - (tex_mix_start * 50)))

                    for maps in tex_map_collection:
                        try:
                            if node_f_coord:
                                maps.parent = node_f_coord
                            else:
                                maps.hide = False

                            links.new(maps.inputs[0], tex_map_coord.outputs['UV'])
                        except:
                            link_fail = True
                            continue

                # create mix nodes to connect texture nodes to the shader input
                # sculpt mode doesn't need them
                if check_frame and not sculpt_paint:
                    mix_node_pairs = loop_node_from_list(TreeNodes, links, tex_node_collect,
                                                         0, tree_tex_start, median_point, node_f_mix)

                    for n in range(1, tree_size):
                        mix_node_pairs = loop_node_from_list(TreeNodes, links, mix_node_pairs,
                                                             n, tree_tex_start, median_point, node_f_mix)
                    try:
                        for node in mix_node_pairs:
                            links.new(node.outputs[0], shader.inputs[0])
                    except:
                        link_fail = True

                    mix_node_pairs = []

                tex_node_collect, tex_map_collection = [], []

                if link_fail:
                    collect_report("ERROR: Some of the node links failed to connect")

            else:
                collect_report("No textures in the Scene, no Image Nodes to add")

    bpy.context.scene.render.engine = 'CYCLES'


def loop_node_from_list(TreeNodes, links, node_list, loc, start, median_point, frame):
    row = 1
    mix_nodes = []
    node_list_size = len(node_list)
    tuplify = [tuple(node_list[s:s + 2]) for s in range(0, node_list_size, 2)]
    for nodes in tuplify:
        row += 1
        create_mix = create_mix_node(TreeNodes, links, nodes, loc, start,
                                     median_point, row, frame)
        if create_mix:
            mix_nodes.append(create_mix)
    return mix_nodes


def create_mix_node(TreeNodes, links, nodes, loc, start, median_point, row, frame):
    mix_node = TreeNodes.nodes.new('ShaderNodeMixRGB')
    mix_node.name = "MIX level: " + str(loc)
    mix_node.label = "MIX level: " + str(loc)
    mix_node.use_custom_color = True
    mix_node.color = NODE_COLOR_MIX
    mix_node.hide = True
    mix_node.width_hidden = 75

    if frame:
        mix_node.parent = frame
    mix_node.location = -(start - loc * 175), ((median_point / 4) + (row * 50))

    try:
        if len(nodes) > 1:
            links.new(nodes[0].outputs[0], mix_node.inputs["Color2"])
            links.new(nodes[1].outputs[0], mix_node.inputs["Color1"])
        elif len(nodes) == 1:
            links.new(nodes[0].outputs[0], mix_node.inputs["Color1"])
    except:
        collect_report("ERROR: Link failed for mix node {}".format(mix_node.label))
    return mix_node


# -----------------------------------------------------------------------------
# Operator Classes

class mllock(Operator):
    bl_idname = "ml.lock"
    bl_label = "Lock"
    bl_description = "Lock/unlock this material against modification by conversions"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (c_is_cycles_addon_enabled() and c_data_has_materials())

    def execute(self, context):
        cmat = bpy.context.selected_objects[0].active_material
        TreeNodes = cmat.node_tree
        for n in TreeNodes.nodes:
            if n.type == 'ShaderNodeOutputMaterial':
                if n.label == 'Locked':
                    n.label = ''
                else:
                    n.label = 'Locked'
        return {'FINISHED'}


class mlrefresh(Operator):
    bl_idname = "ml.refresh"
    bl_label = "Convert All Materials"
    bl_description = ("Convert All Materials in the scene from non-nodes to Cycles\n"
                      "Needs saving the .blend file first")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (bpy.data.filepath != ""and c_is_cycles_addon_enabled() and
                c_data_has_materials())

    def execute(self, context):
        AutoNodeInitiate(False, self)

        if CHECK_AUTONODE is True:
            enable_unwrap = bpy.context.scene.mat_specials.UV_UNWRAP
            if enable_unwrap:
                obj_name = getattr(context.active_object, "name", "UNNAMED OBJECT")
                try:
                    # it's possible to the active object would fail UV Unwrap
                    bpy.ops.object.editmode_toggle()
                    bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=0.001)
                    bpy.ops.object.editmode_toggle()
                    collect_report("INFO: UV Unwrapping active object "
                                   "{}".format(obj_name))
                except:
                    collect_report("ERROR: UV Unwrapping failed for "
                                   "active object {}".format(obj_name))

        collect_report("Conversion finished !", False, True)
        return {'FINISHED'}


class mlrefresh_active(Operator):
    bl_idname = "ml.refresh_active"
    bl_label = "Convert All Materials From Active Object"
    bl_description = ("Convert all Active Object's Materials from non-nodes to Cycles\n"
                      "Needs saving the .blend file first")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (bpy.data.filepath != "" and c_is_cycles_addon_enabled() and
                c_data_has_materials() and context.active_object is not None)

    def execute(self, context):
        AutoNodeInitiate(True, self)
        if CHECK_AUTONODE is True:
            obj_name = getattr(context.active_object, "name", "UNNAMED OBJECT")
            enable_unwrap = bpy.context.scene.mat_specials.UV_UNWRAP
            if enable_unwrap:
                try:
                    # you can already guess it, what could happen here
                    bpy.ops.object.editmode_toggle()
                    bpy.ops.uv.unwrap(method='ANGLE_BASED', margin=0.001)
                    bpy.ops.object.editmode_toggle()
                    collect_report("INFO: UV Unwrapping object {}".format(obj_name))
                except:
                    collect_report("ERROR: UV Unwrapping failed for "
                                   "object {}".format(obj_name))

        collect_report("Conversion finished !", False, True)
        return {'FINISHED'}


class mlrestore(Operator):
    bl_idname = "ml.restore"
    bl_label = "Switch Between Renderers"
    bl_description = ("Switch between Renderers \n"
                      "(Doesn't create new nor converts existing materials)")
    bl_options = {'REGISTER', 'UNDO'}

    switcher = BoolProperty(
            name="Use Nodes",
            description="When restoring, switch Use Nodes On/Off",
            default=True
            )
    renderer = EnumProperty(
            name="Renderer",
            description="Choose Cycles or Blender Internal",
            items=(('CYCLES', "Cycles", "Switch to Cycles"),
                   ('BI', "Blender Internal", "Switch to Blender Internal")),
            default='CYCLES',
            )

    @classmethod
    def poll(cls, context):
        return c_is_cycles_addon_enabled()

    def execute(self, context):
        if self.switcher:
            AutoNodeSwitch(self.renderer, "ON", self)
        else:
            AutoNodeSwitch(self.renderer, "OFF", self)
        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)
    pass


def unregister():
    bpy.utils.unregister_module(__name__)
    pass


if __name__ == "__main__":
    register()
