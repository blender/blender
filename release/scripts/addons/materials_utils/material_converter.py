# -*- coding: utf-8 -*-

import bpy
from mathutils import Vector
from bpy.types import Operator
from .warning_messages_utils import (
        warning_messages,
        c_is_cycles_addon_enabled,
        c_data_has_materials,
        collect_report,
        )

# -----------------------------------------------------------------------------
# Globals

nodesDictionary = None

NODE_FRAME = 'NodeFrame'
BI_MATERIAL_NODE = 'ShaderNodeMaterial'
BI_OUTPUT_NODE = 'ShaderNodeOutput'
TEXTURE_IMAGE_NODE = 'ShaderNodeTexImage'
OUTPUT_NODE = 'ShaderNodeOutputMaterial'
RGB_MIX_NODE = 'ShaderNodeMixRGB'
MAPPING_NODE = 'ShaderNodeMapping'
NORMAL_MAP_NODE = 'ShaderNodeNormalMap'
SHADER_MIX_NODE = 'ShaderNodeMixShader'
SHADER_ADD_NODE = 'ShaderNodeAddShader'
COORD_NODE = 'ShaderNodeTexCoord'
RGB_TO_BW_NODE = 'ShaderNodeRGBToBW'
BSDF_DIFFUSE_NODE = 'ShaderNodeBsdfDiffuse'
BSDF_EMISSION_NODE = 'ShaderNodeEmission'
BSDF_TRANSPARENT_NODE = 'ShaderNodeBsdfTransparent'
BSDF_GLOSSY_NODE = 'ShaderNodeBsdfGlossy'
BSDF_GLASS_NODE = 'ShaderNodeBsdfGlass'

textureNodeSizeX = 150
textureNodeSizeY = 350


# -----------------------------------------------------------------------------
# Functions

def makeTextureNodeDict(cmat):
    global nodesDictionary
    nodesDictionary = {}
    textures = {textureSlot.texture for textureSlot in cmat.texture_slots if textureSlot}

    for tex in textures:
        texNode = None
        if tex.type == 'IMAGE':
            texNode = makeNodeUsingImage1(cmat, tex)
        if texNode:
            nodesDictionary[tex] = texNode
    return nodesDictionary


def getTexNodeDic(texture):
    return nodesDictionary.get(texture)


def clearNodes(TreeNodes):
    TreeNodes.nodes.clear()


def clearCycleMaterial(cmat):
    TreeNodes = cmat.node_tree
    clearNodes(TreeNodes)


def copyMapping(textureSlot, textureMapping):
    textureMapping.scale.x = textureSlot.scale.x
    textureMapping.scale.y = textureSlot.scale.y
    textureMapping.scale.z = textureSlot.scale.z


def addRGBMixNode(TreeNodes, textureSlot, mixRgbNode, prevTexNode, newTexNode, nodeType, textureIdx):
    try:
        links = TreeNodes.links
        mixRgbNode.name = '{} Mix {:d}'.format(nodeType, textureIdx)
        mixRgbNode.blend_type = textureSlot.blend_type
        mixRgbNode.inputs['Fac'].default_value = textureSlot.diffuse_color_factor
        links.new(prevTexNode.outputs['Color'], mixRgbNode.inputs['Color2'])
        links.new(newTexNode.outputs['Color'], mixRgbNode.inputs['Color1'])
    except:
        collect_report("ERROR: Failure to find link with a Mix node")


def makeBiNodes(cmat):
    # Create Blender Internal Material Nodes
    TreeNodes = cmat.node_tree
    links = TreeNodes.links

    BIFrame = TreeNodes.nodes.new(NODE_FRAME)
    BIFrame.name = 'BI Frame'
    BIFrame.label = 'BI Material'

    biShaderNodeMaterial = TreeNodes.nodes.new(BI_MATERIAL_NODE)
    biShaderNodeMaterial.parent = BIFrame
    biShaderNodeMaterial.name = 'BI Material'
    biShaderNodeMaterial.material = cmat
    biShaderNodeMaterial.location = 0, 600

    biShaderNodeOutput = TreeNodes.nodes.new(BI_OUTPUT_NODE)
    biShaderNodeOutput.parent = BIFrame
    biShaderNodeOutput.name = 'BI Output'
    biShaderNodeOutput.location = 200, 600
    try:
        links.new(biShaderNodeMaterial.outputs['Color'], biShaderNodeOutput.inputs['Color'])
        links.new(biShaderNodeMaterial.outputs['Alpha'], biShaderNodeOutput.inputs['Alpha'])
    except:
        collect_report("ERROR: Failure to find links with the BI Shader Material")


def placeNode(node, posX, posY, deltaX, deltaY, countX, countY):
    nodeX = posX - (deltaX * countX)
    nodeY = posY - (deltaY * countY)
    node.location = nodeX, nodeY


def makeImageTextureNode(TreeNodes, img):
    texNode = TreeNodes.nodes.new(TEXTURE_IMAGE_NODE)
    texNode.image = img
    return texNode


def makeNodeUsingImage1(cmat, texture):
    TreeNodes = cmat.node_tree
    img = texture.image
    texNode = makeImageTextureNode(TreeNodes, img)
    return texNode


def makeMainShader(TreeNodes):
    mainShader = TreeNodes.nodes.new(BSDF_DIFFUSE_NODE)
    mainShader.name = 'Diffuse BSDF'
    mainShader.location = 0, 0
    return mainShader


def makeEmissionShader(TreeNodes):
    mainShader = TreeNodes.nodes.new(BSDF_EMISSION_NODE)
    mainShader.name = 'Emmission'
    mainShader.location = 0, 0
    return mainShader


def makeMaterialOutput(TreeNodes):
    shout = TreeNodes.nodes.new(OUTPUT_NODE)
    shout.location = 200, 0
    return shout


def replaceNode(oldNode, newNode):
    newNode.location = oldNode.location
    try:
        for link in oldNode.outputs['BSDF'].links:
            link.new(newNode.outputs['BSDF'], link.to_socket)
        for link in oldNode.inputs['Color'].links:
            link.new(newNode.inputs['Color'], link.from_socket)
        for link in oldNode.inputs['Normal'].links:
            link.new(newNode.inputs['Normal'], link.from_socket)
    except:
        collect_report("ERROR: Failure to replace node")


def BIToCycleTexCoord(links, textureSlot, texCoordNode, textureMappingNode):
    # Texture Coordinates
    linkOutput = None
    if textureSlot.texture_coords in {'TANGENT', 'STRESS', 'STRAND'}:
        linkOutput = None
    elif textureSlot.texture_coords == 'REFLECTION':
        linkOutput = 'Reflection'
    elif textureSlot.texture_coords == 'NORMAL':
        linkOutput = 'Normal'
    elif textureSlot.texture_coords == 'WINDOW':
        linkOutput = 'Window'
    elif textureSlot.texture_coords == 'UV':
        linkOutput = 'UV'
    elif textureSlot.texture_coords == 'ORCO':
        linkOutput = 'Generated'
    elif textureSlot.texture_coords == 'OBJECT':
        linkOutput = 'Object'
    elif textureSlot.texture_coords == 'GLOBAL':
        linkOutput = 'Camera'

    if linkOutput:
        links.new(texCoordNode.outputs[linkOutput], textureMappingNode.inputs['Vector'])


def createDiffuseNodes(cmat, texCoordNode, mainShader, materialOutput):
    TreeNodes = cmat.node_tree
    links = TreeNodes.links
    texCount = len([node for node in TreeNodes.nodes if node.type == 'MAPPING'])
    currPosY = -textureNodeSizeY * texCount

    textureSlots = [textureSlot for textureSlot in cmat.texture_slots if
                    (textureSlot and textureSlot.use_map_color_diffuse)]

    texCount = len(textureSlots)
    texNode = None
    latestNode = None
    groupName = 'Diffuse'

    if any(textureSlots):
        diffuseFrame = TreeNodes.nodes.new(NODE_FRAME)
        diffuseFrame.name = '{} Frame'.format(groupName)
        diffuseFrame.label = '{}'.format(groupName)

    for textureIdx, textureSlot in enumerate(textureSlots):
        texNode = getTexNodeDic(textureSlot.texture)
        if texNode:
            tex_node_name = getattr(texNode.image, "name", "")
            collect_report("INFO: Generating {} Nodes for: ".format(groupName) + tex_node_name)
            texNode.parent = diffuseFrame
            placeNode(texNode, -500 - ((texCount - 1) * 200),
                      currPosY, textureNodeSizeX, textureNodeSizeY, 0, textureIdx)

            # Add mapping node
            textureMapping = TreeNodes.nodes.new(MAPPING_NODE)
            textureMapping.parent = diffuseFrame
            renameNode(textureMapping, '{} Mapping'.format(groupName), texCount, textureIdx)
            textureMapping.location = texNode.location + Vector((-400, 0))
            copyMapping(textureSlot, textureMapping)

            # Texture Coordinates
            BIToCycleTexCoord(links, textureSlot, texCoordNode, textureMapping)

            # Place the texture node
            renameNode(texNode, '{} Texture'.format(groupName), texCount, textureIdx)
            links.new(textureMapping.outputs['Vector'], texNode.inputs['Vector'])

            # Add multiply node
            colorMult = TreeNodes.nodes.new(RGB_MIX_NODE)
            colorMult.parent = diffuseFrame
            renameNode(colorMult, 'Color Mult', texCount, textureIdx)
            colorMult.blend_type = 'MIX'
            colorMult.inputs['Fac'].default_value = 1
            colorMult.inputs['Color1'].default_value = (1, 1, 1, 1)

            colorMult.location = texNode.location + Vector((200, 0))
            links.new(texNode.outputs['Color'], colorMult.inputs['Color2'])

            texNode = colorMult
            if textureSlot.use and textureIdx == 0:
                latestNode = texNode

            if textureSlot.use and textureIdx > 0:
                try:
                    # Create a node to mix multiple texture nodes
                    mixRgbNode = TreeNodes.nodes.new(RGB_MIX_NODE)
                    mixRgbNode.parent = diffuseFrame
                    addRGBMixNode(TreeNodes, textureSlot, mixRgbNode, texNode, latestNode,
                                  '{}'.format(groupName), textureIdx)
                    mixRgbNode.location = Vector(
                                    (max(texNode.location.x, latestNode.location.x),
                                    (texNode.location.y + latestNode.location.y) / 2)) + Vector((200, 0)
                                    )
                    latestNode = mixRgbNode
                except:
                    continue

    if latestNode:
        links.new(latestNode.outputs['Color'], mainShader.inputs['Color'])

    # Y Position next texture node
    currPosY = currPosY - (textureNodeSizeY * (texCount))

    # BI Material to Cycles - Alpha Transparency
    textureSlots = [textureSlot for textureSlot in cmat.texture_slots if
                    (textureSlot and textureSlot.use_map_alpha)]
    texCount = len(textureSlots)
    texNode = None
    latestNode = None
    for textureIdx, textureSlot in enumerate(textureSlots):
        texNode = getTexNodeDic(textureSlot.texture)
        if texNode:
            tex_node_name = getattr(texNode.image, "name", "")
            collect_report("INFO: Generating Transparency Nodes for: " + tex_node_name)
            if textureSlot.use and textureIdx == 0:
                latestNode = texNode

            if textureSlot.use and textureIdx > 0:
                try:
                    # Create a node to mix multiple texture nodes
                    mixAlphaNode = TreeNodes.nodes.new(RGB_MIX_NODE)
                    mixAlphaNode.name = 'Alpha Mix {:d}'.format(textureIdx)
                    mixAlphaNode.blend_type = textureSlot.blend_type
                    mixAlphaNode.inputs['Fac'].default_value = textureSlot.diffuse_color_factor
                    placeNode(mixAlphaNode, -200 - ((texCount - textureIdx - 1) * 200), 400 - 240,
                              textureNodeSizeX, textureNodeSizeY, 0, 0)
                    links.new(texNode.outputs['Alpha'], mixAlphaNode.inputs['Color2'])
                    links.new(latestNode.outputs['Alpha'], mixAlphaNode.inputs['Color1'])
                    latestNode = mixAlphaNode
                except:
                    continue
    if latestNode:
        alphaMixShader = TreeNodes.nodes.get('Alpha Mix Shader')
        if alphaMixShader:
            if latestNode.type == 'TEX_IMAGE':
                outputLink = 'Alpha'
            else:
                outputLink = 'Color'
            links.new(latestNode.outputs[outputLink], alphaMixShader.inputs['Fac'])


def createNormalNodes(cmat, texCoordNode, mainShader, materialOutput):
    TreeNodes = cmat.node_tree
    links = TreeNodes.links
    texCount = len([node for node in TreeNodes.nodes if node.type == 'MAPPING'])
    currPosY = -textureNodeSizeY * texCount

    textureSlots = [textureSlot for textureSlot in cmat.texture_slots if
                    (textureSlot and textureSlot.use_map_normal)]
    texCount = len(textureSlots)
    texNode = None
    latestNode = None
    groupName = 'Normal'
    if any(textureSlots):
        normalFrame = TreeNodes.nodes.new(NODE_FRAME)
        normalFrame.name = '{} Frame'.format(groupName)
        normalFrame.label = '{}'.format(groupName)

    for textureIdx, textureSlot in enumerate(textureSlots):
        texNode = getTexNodeDic(textureSlot.texture)
        if texNode:
            tex_node_name = getattr(texNode.image, "name", "")
            collect_report("INFO: Generating Normal Nodes for: " + tex_node_name)
            texNode.parent = normalFrame
            placeNode(texNode, -500 - ((texCount) * 200), currPosY,
                      textureNodeSizeX, textureNodeSizeY, 0, textureIdx)

            # Add mapping node
            normalMapping = TreeNodes.nodes.new(MAPPING_NODE)
            normalMapping.parent = normalFrame
            renameNode(normalMapping, '{} Mapping'.format(groupName), texCount, textureIdx)
            normalMapping.location = texNode.location + Vector((-400, 0))
            copyMapping(textureSlot, normalMapping)

            # Texture Coordinates
            BIToCycleTexCoord(links, textureSlot, texCoordNode, normalMapping)

            # Place the texture node
            renameNode(texNode, '{} Texture'.format(groupName), texCount, textureIdx)
            texNode.color_space = 'NONE'
            links.new(normalMapping.outputs['Vector'], texNode.inputs['Vector'])

            # Add multiply node
            normalMult = TreeNodes.nodes.new(RGB_MIX_NODE)
            normalMult.parent = normalFrame
            renameNode(normalMult, 'Normal Mult', texCount, textureIdx)
            normalMult.blend_type = 'MIX'
            normalMult.inputs['Fac'].default_value = 1
            normalMult.inputs['Color1'].default_value = (.5, .5, 1, 1)

            normalMult.location = texNode.location + Vector((200, 0))
            links.new(texNode.outputs['Color'], normalMult.inputs['Color2'])

            texNode = normalMult
            if textureSlot.use and textureIdx == 0:
                latestNode = texNode

            if textureSlot.use and textureIdx > 0:
                try:
                    # Create a node to mix multiple texture nodes
                    mixRgbNode = TreeNodes.nodes.new(RGB_MIX_NODE)
                    mixRgbNode.parent = normalFrame
                    addRGBMixNode(TreeNodes, textureSlot, mixRgbNode, texNode, latestNode,
                                  '{}'.format(groupName), textureIdx)
                    mixRgbNode.location = Vector(
                                            (max(texNode.location.x, latestNode.location.x),
                                            (texNode.location.y + latestNode.location.y) / 2)) + Vector((200, 0)
                                            )
                    latestNode = mixRgbNode
                except:
                    continue

    if latestNode:
        normalMapNode = TreeNodes.nodes.new(NORMAL_MAP_NODE)
        normalMapNode.parent = normalFrame
        normalMapNode.location = latestNode.location + Vector((200, 0))
        links.new(latestNode.outputs['Color'], normalMapNode.inputs['Color'])
        links.new(normalMapNode.outputs['Normal'], mainShader.inputs['Normal'])


def createSpecularNodes(cmat, texCoordNode, mainShader, mainDiffuse, materialOutput):
    TreeNodes = cmat.node_tree
    links = TreeNodes.links
    texCount = len([node for node in TreeNodes.nodes if node.type == 'MAPPING'])
    currPosY = -textureNodeSizeY * texCount

    textureSlots = [textureSlot for textureSlot in cmat.texture_slots if
                    (textureSlot and textureSlot.use_map_color_spec)]
    texCount = len(textureSlots)
    texNode = None
    latestNode = None
    groupName = 'Specular'
    if any(textureSlots):
        specularFrame = TreeNodes.nodes.new(NODE_FRAME)
        specularFrame.name = '{} Frame'.format(groupName)
        specularFrame.label = '{}'.format(groupName)

    for textureIdx, textureSlot in enumerate(textureSlots):
        texNode = getTexNodeDic(textureSlot.texture)
        if texNode:
            tex_node_name = getattr(texNode.image, "name", "")
            collect_report("INFO: Generating {} Nodes for: ".format(groupName) + tex_node_name)
            texNode.parent = specularFrame
            placeNode(texNode, -500 - ((texCount) * 200),
                      currPosY, textureNodeSizeX, textureNodeSizeY, 0, textureIdx)

            # Add mapping node
            specularMapping = TreeNodes.nodes.new(MAPPING_NODE)
            specularMapping.parent = specularFrame
            renameNode(specularMapping, '{} Mapping'.format(groupName), texCount, textureIdx)
            specularMapping.location = texNode.location + Vector((-400, 0))
            copyMapping(textureSlot, specularMapping)

            # Texture Coordinates
            BIToCycleTexCoord(links, textureSlot, texCoordNode, specularMapping)

            # Place the texture node
            renameNode(texNode, '{} Texture'.format(groupName), texCount, textureIdx)
            links.new(specularMapping.outputs['Vector'], texNode.inputs['Vector'])

            # Add multiply node
            specularMult = TreeNodes.nodes.new(RGB_MIX_NODE)
            specularMult.parent = specularFrame
            renameNode(specularMult, 'Specular Mult', texCount, textureIdx)
            specularMult.blend_type = 'MULTIPLY'
            specularMult.inputs['Fac'].default_value = 1
            specularMult.inputs['Color1'].default_value = (1, 1, 1, 1)

            specularMult.location = texNode.location + Vector((200, 0))
            links.new(texNode.outputs['Color'], specularMult.inputs['Color2'])

            texNode = specularMult
            if textureSlot.use and textureIdx == 0:
                latestNode = texNode

            if textureSlot.use and textureIdx > 0:
                try:
                    # Create a node to mix multiple texture nodes
                    mixRgbNode = TreeNodes.nodes.new(RGB_MIX_NODE)
                    mixRgbNode.parent = specularFrame
                    addRGBMixNode(TreeNodes, textureSlot, mixRgbNode, texNode, latestNode,
                                  '{}'.format(groupName), textureIdx)
                    mixRgbNode.location = Vector(
                                            (max(texNode.location.x, latestNode.location.x),
                                            (texNode.location.y + latestNode.location.y) / 2)) + Vector((200, 0)
                                            )
                    latestNode = mixRgbNode
                except:
                    continue

    if latestNode:
        try:
            glossShader = TreeNodes.nodes.new(BSDF_GLOSSY_NODE)
            RGBToBW = TreeNodes.nodes.new(RGB_TO_BW_NODE)
            RGBToBW.location = Vector((0, latestNode.location.y)) + Vector((0, 0))
            glossShader.location = Vector((0, latestNode.location.y)) + Vector((0, -80))

            links.new(latestNode.outputs['Color'], glossShader.inputs['Color'])
            links.new(latestNode.outputs['Color'], RGBToBW.inputs['Color'])

            outputNode = TreeNodes.nodes.get('Material Output')
            spec_mixer_1 = TreeNodes.nodes.new(SHADER_MIX_NODE)
            spec_mixer_1.location = outputNode.location
            spec_mixer_2 = TreeNodes.nodes.new(SHADER_MIX_NODE)
            spec_mixer_2.inputs['Fac'].default_value = .4
            spec_mixer_2.location = outputNode.location + Vector((180, 0))
            links.new(spec_mixer_1.outputs['Shader'], spec_mixer_2.inputs[2])
            links.new(spec_mixer_2.outputs['Shader'], outputNode.inputs['Surface'])
            links.new(RGBToBW.outputs['Val'], spec_mixer_1.inputs['Fac'])

            links.new(glossShader.outputs['BSDF'], spec_mixer_1.inputs[2])

            outputNode.location += Vector((360, 0))
            normalMapNode = TreeNodes.nodes.get('Normal Map')
            links.new(normalMapNode.outputs['Normal'], glossShader.inputs['Normal'])

            if mainDiffuse.type == 'BSDF_DIFFUSE':
                outputLink = 'BSDF'
            else:
                outputLink = 'Shader'

            links.new(mainDiffuse.outputs[outputLink], spec_mixer_1.inputs[1])
            links.new(mainDiffuse.outputs[outputLink], spec_mixer_2.inputs[1])
        except:
            return


def createEmissionNodes(cmat, texCoordNode, mainShader, materialOutput):
    TreeNodes = cmat.node_tree
    links = TreeNodes.links
    texCount = len([node for node in TreeNodes.nodes if node.type == 'MAPPING'])
    currPosY = -textureNodeSizeY * texCount

    textureSlots = [textureSlot for textureSlot in cmat.texture_slots if
                    (textureSlot and textureSlot.use_map_emit)]
    texCount = len(textureSlots)
    texNode = None
    latestNode = None
    groupName = 'Emission'
    if any(textureSlots):
        emissionFrame = TreeNodes.nodes.new(NODE_FRAME)
        emissionFrame.name = '{} Frame'.format(groupName)
        emissionFrame.label = '{}'.format(groupName)

    for textureIdx, textureSlot in enumerate(textureSlots):
        texNode = getTexNodeDic(textureSlot.texture)
        if texNode:
            tex_node_name = getattr(texNode.image, "name", "")
            collect_report("INFO: Generating {} Nodes for: ".format(groupName) + tex_node_name)
            texNode.parent = emissionFrame
            placeNode(texNode, -500 - ((texCount) * 200), currPosY,
                      textureNodeSizeX, textureNodeSizeY, 0, textureIdx)

            # Add mapping node
            emissionMapping = TreeNodes.nodes.new(MAPPING_NODE)
            emissionMapping.parent = emissionFrame
            renameNode(emissionMapping, '{} Mapping'.format(groupName), texCount, textureIdx)
            emissionMapping.location = texNode.location + Vector((-400, 0))
            copyMapping(textureSlot, emissionMapping)

            # Texture Coordinates
            BIToCycleTexCoord(links, textureSlot, texCoordNode, emissionMapping)

            # Place the texture node
            renameNode(texNode, '{} Texture'.format(groupName), texCount, textureIdx)
            texNode.color_space = 'NONE'
            links.new(emissionMapping.outputs['Vector'], texNode.inputs['Vector'])

            # Add multiply node
            emissionMult = TreeNodes.nodes.new(RGB_MIX_NODE)
            emissionMult.parent = emissionFrame
            renameNode(emissionMult, 'Emission Mult', texCount, textureIdx)
            emissionMult.blend_type = 'MIX'
            emissionMult.inputs['Fac'].default_value = 1
            emissionMult.inputs['Color1'].default_value = (0, 0, 0, 1)

            emissionMult.location = texNode.location + Vector((200, 0))
            links.new(texNode.outputs['Color'], emissionMult.inputs['Color2'])

            texNode = emissionMult
            if textureSlot.use and textureIdx == 0:
                latestNode = texNode

            if textureSlot.use and textureIdx > 0:
                try:
                    # Create a node to mix multiple texture nodes
                    mixRgbNode = TreeNodes.nodes.new(RGB_MIX_NODE)
                    mixRgbNode.parent = emissionFrame
                    addRGBMixNode(TreeNodes, textureSlot, mixRgbNode, texNode, latestNode,
                                  '{}'.format(groupName), textureIdx)
                    mixRgbNode.location = Vector(
                                            (max(texNode.location.x, latestNode.location.x),
                                            (texNode.location.y + latestNode.location.y) / 2)) + Vector((200, 0)
                                            )
                    latestNode = mixRgbNode
                except:
                    continue

    if latestNode:
        try:
            emissionNode = TreeNodes.nodes.new(BSDF_EMISSION_NODE)
            emissionNode.inputs['Strength'].default_value = 1
            addShaderNode = TreeNodes.nodes.new(SHADER_ADD_NODE)
            addShaderNode.location = materialOutput.location + Vector((0, -100))
            xPos = mainShader.location.x
            yPos = latestNode.location.y

            emissionNode.location = Vector((xPos, yPos))
            materialOutput.location += Vector((400, 0))

            node = materialOutput.inputs[0].links[0].from_node
            node.location += Vector((400, 0))

            links.new(latestNode.outputs['Color'], emissionNode.inputs['Color'])
            links.new(emissionNode.outputs['Emission'], addShaderNode.inputs[1])
            links.new(mainShader.outputs['BSDF'], addShaderNode.inputs[0])
            links.new(addShaderNode.outputs['Shader'], node.inputs[2])
        except:
            return


def renameNode(node, baseName, nodesCount, nodeIndex):
    if nodesCount == 1:
        node.name = baseName
    else:
        node.name = '{} {:d}'.format(baseName, nodeIndex + 1)


def hasAlphaTex(cmat):
    tex_is_transp = False
    for textureSlot in cmat.texture_slots:
        if textureSlot:
            if textureSlot.use:
                if textureSlot.use_map_alpha:
                    tex_is_transp = tex_is_transp or True
    return tex_is_transp


def AutoNode(active=False, operator=None):
    collect_report("________________________________________", True, False)
    collect_report("START CYCLES CONVERSION")

    if active:
        materials = [mat for obj in bpy.context.selected_objects if
                     obj.type == 'MESH' for mat in obj.data.materials]
    else:
        materials = bpy.data.materials

    # No Materials for the chosen action - abort
    if not materials:
        if operator:
            if active:
                warning_messages(operator, 'CONV_NO_SEL_MAT', override=True)
            else:
                warning_messages(operator, 'CONV_NO_SC_MAT', override=True)
        return

    for cmat in materials:
        # check for empty material (it will fall through the first check)
        test_empty = getattr(cmat, "name", None)
        if test_empty is None:
            collect_report("INFO: An empty material was hit, skipping")
            continue
        else:
            cmat.use_nodes = True
            clearCycleMaterial(cmat)
            makeBiNodes(cmat)
            makeCyclesFromBI(cmat)

    collect_report("Conversion finished !", False, True)

    bpy.context.scene.render.engine = 'CYCLES'


def makeCyclesFromBI(cmat):
    mat_name = getattr(cmat, "name", "NO NAME")
    collect_report("Converting Material: " + mat_name)

    global nodesDictionary
    TreeNodes = cmat.node_tree
    links = TreeNodes.links

    # Convert this material from non-nodes to Cycles nodes
    mainShader = None
    mainDiffuse = None
    Mix_Alpha = None

    tex_is_transp = hasAlphaTex(cmat)

    cmat_use_transp = cmat.use_transparency and cmat.alpha < 1
    cmat_trans_method = cmat.transparency_method
    cmat_ior = cmat.raytrace_transparency.ior
    cmat_transp_z = cmat_use_transp and cmat_trans_method == 'Z_TRANSPARENCY'
    cmat_transp_ray = cmat_use_transp and cmat_trans_method == 'RAYTRACE' and cmat_ior == 1
    cmat_mirror = cmat.raytrace_mirror.use
    cmat_mirror_fac = cmat.raytrace_mirror.reflect_factor

    # Material Shaders
    # Diffuse nodes
    # --------------------------------------

    # Make Diffuse and Output nodes
    mainShader = makeMainShader(TreeNodes)
    mainShader.inputs['Roughness'].default_value = cmat.specular_intensity
    mainDiffuse = mainShader
    materialOutput = makeMaterialOutput(TreeNodes)
    links.new(mainShader.outputs['BSDF'], materialOutput.inputs['Surface'])

    texCoordNode = TreeNodes.nodes.new(COORD_NODE)
    texCoordNode.name = 'Texture Coordinate'

    # Material Transparent
    if not cmat_mirror and cmat_use_transp and tex_is_transp and (cmat_transp_z or cmat_transp_ray):
        collect_report("INFO:  Make TRANSPARENT material nodes: " + cmat.name)
        Mix_Alpha = TreeNodes.nodes.new(SHADER_MIX_NODE)
        Mix_Alpha.name = 'Alpha Mix Shader'
        Mix_Alpha.location = materialOutput.location
        materialOutput.location += Vector((180, 0))
        Mix_Alpha.inputs['Fac'].default_value = cmat.alpha
        transparentShader = TreeNodes.nodes.new(BSDF_TRANSPARENT_NODE)
        transparentShader.location = mainShader.location
        mainShader.location += Vector((0, -100))
        links.new(transparentShader.outputs['BSDF'], Mix_Alpha.inputs[1])
        links.new(mainShader.outputs['BSDF'], Mix_Alpha.inputs[2])
        links.new(Mix_Alpha.outputs['Shader'], materialOutput.inputs['Surface'])
        mainDiffuse = Mix_Alpha

    if cmat_mirror and cmat_mirror_fac > 0.001:
        if cmat_use_transp:
            # Material Glass
            collect_report("INFO:  Make GLASS shader node: " + cmat.name)
            newShader = TreeNodes.nodes.new(BSDF_GLASS_NODE)
            shader = newShader
            replaceNode(shader, newShader)
            TreeNodes.nodes.remove(shader)
        else:
            # Material Mirror
            collect_report("INFO:  Make MIRROR shader node: " + cmat.name)
            newShader = TreeNodes.nodes.new(BSDF_GLOSSY_NODE)
            shader = newShader
            replaceNode(shader, newShader)
            TreeNodes.nodes.remove(shader)

    nodesDictionary = makeTextureNodeDict(cmat)

    # --------------------------------------
    # Texture nodes

    # BI Material to Cycles - Diffuse Textures
    createDiffuseNodes(cmat, texCoordNode, mainShader, materialOutput)

    # BI Material to Cycles - Normal map
    createNormalNodes(cmat, texCoordNode, mainShader, materialOutput)

    # BI Material to Cycles - Specular map
    createSpecularNodes(cmat, texCoordNode, mainShader, mainDiffuse, materialOutput)

    # BI Material to Cycles - Emission map
    createEmissionNodes(cmat, texCoordNode, mainShader, materialOutput)

    # Texture coordinates
    # list all nodes conected to outputs
    mappingNodes = [link.to_node for output in texCoordNode.outputs for link in output.links]
    mappingNodesCount = len(mappingNodes)

    if mappingNodes:
        xList = [node.location.x for node in mappingNodes]
        yList = [node.location.y for node in mappingNodes]
        minPosX = min(xList) - 400
        avgPosY = sum(yList) / mappingNodesCount
        texCoordNode.location = Vector((minPosX, avgPosY))


# -----------------------------------------------------------------------------
# Operator Classes

class material_convert_all(Operator):
    bl_idname = "xps_tools.convert_to_cycles_all"
    bl_label = "Convert All Materials"
    bl_description = ("Convert All Materials to BI and Cycles Nodes\n"
                      "Needs saving the .blend file first")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (bpy.data.filepath != "" and c_is_cycles_addon_enabled() and
                c_data_has_materials())

    def execute(self, context):
        AutoNode(False, self)
        return {'FINISHED'}


class material_convert_selected(Operator):
    bl_idname = "xps_tools.convert_to_cycles_selected"
    bl_label = "Convert All Materials From Selected Objects"
    bl_description = ("Convert All Materials on Selected Objects to BI and Cycles Nodes\n"
                      "Needs saving the .blend file first")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (bpy.data.filepath != "" and c_data_has_materials() and
                c_is_cycles_addon_enabled() and
                bool(next((obj for obj in context.selected_objects if obj.type == 'MESH'),
                         None)
                    )
                )

    def execute(self, context):
        AutoNode(True, self)
        return {'FINISHED'}


def register():
    bpy.utils.register_module(__name__)
    pass


def unregister():
    bpy.utils.unregister_module(__name__)
    pass


if __name__ == "__main__":
    register()
