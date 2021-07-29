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

# ----------------------------------------------------------
# support routines and general functions
# Author: Antonio Vazquez (antonioya)
#
# ----------------------------------------------------------
# noinspection PyUnresolvedReferences
import bpy
from os import path


# --------------------------------------------------------------------
# Get length Blender units
# --------------------------------------------------------------------
def get_blendunits(units):
    if bpy.context.scene.unit_settings.system == "IMPERIAL":
        return units * 0.3048
    else:
        return units


# --------------------------------------------------------------------
# Set normals
# True= faces to inside
# False= faces to outside
# --------------------------------------------------------------------
def set_normals(myobject, direction=False):
    bpy.context.scene.objects.active = myobject
    # go edit mode
    bpy.ops.object.mode_set(mode='EDIT')
    # select all faces
    bpy.ops.mesh.select_all(action='SELECT')
    # recalculate outside normals
    bpy.ops.mesh.normals_make_consistent(inside=direction)
    # go object mode again
    bpy.ops.object.editmode_toggle()


# --------------------------------------------------------------------
# Remove doubles
# --------------------------------------------------------------------
def remove_doubles(myobject):
    bpy.context.scene.objects.active = myobject
    # go edit mode
    bpy.ops.object.mode_set(mode='EDIT')
    # select all faces
    bpy.ops.mesh.select_all(action='SELECT')
    # remove
    bpy.ops.mesh.remove_doubles()
    # go object mode again
    bpy.ops.object.editmode_toggle()


# --------------------------------------------------------------------
# Set shade smooth
# --------------------------------------------------------------------
def set_smooth(myobject):
    # deactivate others
    for o in bpy.data.objects:
        if o.select is True:
            o.select = False

    myobject.select = True
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.shade_smooth()


# --------------------------------------------------------------------
# Add modifier (subdivision)
# --------------------------------------------------------------------
def set_modifier_subsurf(myobject):
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.modifier_add(type='SUBSURF')
        for mod in myobject.modifiers:
            if mod.type == 'SUBSURF':
                mod.levels = 2


# --------------------------------------------------------------------
# Add modifier (mirror)
# --------------------------------------------------------------------
def set_modifier_mirror(myobject, axis="Y"):
    bpy.ops.object.select_all(False)
    myobject.select = True
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.modifier_add(type='MIRROR')
        for mod in myobject.modifiers:
            if mod.type == 'MIRROR':
                if axis == "X":
                    mod.use_x = True
                else:
                    mod.use_x = False

                if axis == "Y":
                    mod.use_y = True
                else:
                    mod.use_y = False

                if axis == "Z":
                    mod.use_z = True
                else:
                    mod.use_z = False

                mod.use_clip = True


# --------------------------------------------------------------------
# Add modifier (array)
# --------------------------------------------------------------------
def set_modifier_array(myobject, axis, move, repeat, fix=False, fixmove=0, zmove=0):
    bpy.ops.object.select_all(False)
    myobject.select = True
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.modifier_add(type='ARRAY')
        for mod in myobject.modifiers:
            if mod.type == 'ARRAY':
                if mod.name == "Array":
                    mod.name = "Array_" + axis
                    mod.count = repeat
                    mod.use_constant_offset = fix
                    if axis == "X":
                        mod.relative_offset_displace[0] = move
                        mod.constant_offset_displace[0] = fixmove
                        mod.relative_offset_displace[1] = 0.0
                        mod.constant_offset_displace[1] = 0.0
                        mod.relative_offset_displace[2] = 0.0
                        mod.constant_offset_displace[2] = zmove

                    if axis == "Y":
                        mod.relative_offset_displace[0] = 0.0
                        mod.constant_offset_displace[0] = 0.0
                        mod.relative_offset_displace[1] = move
                        mod.constant_offset_displace[1] = fixmove
                        mod.relative_offset_displace[2] = 0.0
                        mod.constant_offset_displace[2] = 0.0


# --------------------------------------------------------------------
# Add modifier (curve)
# --------------------------------------------------------------------
def set_modifier_curve(myobject, mycurve):
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.modifier_add(type='CURVE')
        for mod in myobject.modifiers:
            if mod.type == 'CURVE':
                mod.deform_axis = 'POS_X'
                mod.object = mycurve


# --------------------------------------------------------------------
# Add modifier (solidify)
# --------------------------------------------------------------------
def set_modifier_solidify(myobject, width):
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.modifier_add(type='SOLIDIFY')
        for mod in myobject.modifiers:
            if mod.type == 'SOLIDIFY':
                mod.thickness = width
                mod.use_even_offset = True
                mod.use_quality_normals = True
                break


# --------------------------------------------------------------------
# Add modifier (boolean)
# --------------------------------------------------------------------
def set_modifier_boolean(myobject, bolobject):
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.modifier_add(type='BOOLEAN')
        mod = myobject.modifiers[len(myobject.modifiers) - 1]
        mod.operation = 'DIFFERENCE'
        mod.object = bolobject


# --------------------------------------------------------------------
# Set material to object
# --------------------------------------------------------------------
def set_material(myobject, mymaterial):
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        myobject.data.materials.append(mymaterial)


# --------------------------------------------------------------------
# Set material to selected faces
# --------------------------------------------------------------------
def set_material_faces(myobject, idx):
    bpy.context.scene.objects.active = myobject
    myobject.select = True
    bpy.context.object.active_material_index = idx
    if bpy.context.scene.objects.active.name == myobject.name:
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.object.material_slot_assign()
        # Deselect
        bpy.ops.mesh.select_all(action='DESELECT')
        bpy.ops.object.mode_set(mode='OBJECT')


# --------------------------------------------------------------------
# Select faces
# --------------------------------------------------------------------
def select_faces(myobject, selface, clear):
    myobject.select = True
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        # deselect everything
        if clear:
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='DESELECT')

        # reselect the originally selected face
        bpy.ops.object.mode_set(mode='OBJECT')
        myobject.data.polygons[selface].select = True


# --------------------------------------------------------------------
# Select vertices
# --------------------------------------------------------------------
def select_vertices(myobject, selvertices, clear=True):
    myobject.select = True
    bpy.context.scene.objects.active = myobject
    if bpy.context.scene.objects.active.name == myobject.name:
        # deselect everything
        if clear:
            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='DESELECT')

        # Select Vertices
        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        sel_mode = bpy.context.tool_settings.mesh_select_mode

        bpy.context.tool_settings.mesh_select_mode = [True, False, False]
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        for i in selvertices:
            myobject.data.vertices[i].select = True

        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        bpy.context.tool_settings.mesh_select_mode = sel_mode
        bpy.ops.object.mode_set(mode='OBJECT')


# --------------------------------------------------------------------
# Mark Seam
# --------------------------------------------------------------------
def mark_seam(myobject):
    # noinspection PyBroadException
    try:
        myobject.select = True
        bpy.context.scene.objects.active = myobject
        if bpy.context.scene.objects.active.name == myobject.name:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)
            bpy.ops.mesh.mark_seam()
            bpy.ops.object.mode_set(mode='OBJECT')
    except:
        bpy.ops.object.mode_set(mode='OBJECT')


# --------------------------------------------------------------------
# Unwrap mesh
# --------------------------------------------------------------------
def unwrap_mesh(myobject, allfaces=True):
    # noinspection PyBroadException
    try:
        myobject.select = True
        bpy.context.scene.objects.active = myobject
        if bpy.context.scene.objects.active.name == myobject.name:
            # Unwrap
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)
            if allfaces is True:
                bpy.ops.mesh.select_all(action='DESELECT')
                bpy.ops.mesh.select_all()
            bpy.ops.uv.unwrap()
            bpy.ops.object.mode_set(mode='OBJECT')
    except:
        bpy.ops.object.mode_set(mode='OBJECT')


# --------------------------------------------------------------------
# Get Node Index(multilanguage support)
# --------------------------------------------------------------------
def get_node_index(nodes, datatype):
    idx = 0
    for m in nodes:
        if m.type == datatype:
            return idx
        idx += 1

    # by default
    return 1


# --------------------------------------------------------------------
# Create cycles diffuse material
# --------------------------------------------------------------------
def create_diffuse_material(matname, replace, r, g, b, rv=0.8, gv=0.8, bv=0.8, mix=0.1, twosides=False):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.diffuse_color = (rv, gv, bv)  # viewport color
    mat.use_nodes = True
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    node.name = 'Diffuse BSDF'
    node.label = 'Diffuse BSDF'

    node.inputs[0].default_value = [r, g, b, 1]
    node.location = 200, 320

    node = nodes.new('ShaderNodeBsdfGlossy')
    node.name = 'Glossy_0'
    node.location = 200, 0

    node = nodes.new('ShaderNodeMixShader')
    node.name = 'Mix_0'
    node.inputs[0].default_value = mix
    node.location = 500, 160

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 1100, 160

    # Connect nodes
    outn = nodes['Diffuse BSDF'].outputs[0]
    inn = nodes['Mix_0'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Glossy_0'].outputs[0]
    inn = nodes['Mix_0'].inputs[2]
    mat.node_tree.links.new(outn, inn)

    if twosides is False:
        outn = nodes['Mix_0'].outputs[0]
        inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
        mat.node_tree.links.new(outn, inn)

    if twosides is True:
        node = nodes.new('ShaderNodeNewGeometry')
        node.name = 'Input_1'
        node.location = -80, -70

        node = nodes.new('ShaderNodeBsdfDiffuse')
        node.name = 'Diffuse_1'
        node.inputs[0].default_value = [0.30, 0.30, 0.30, 1]
        node.location = 200, -280

        node = nodes.new('ShaderNodeMixShader')
        node.name = 'Mix_1'
        node.inputs[0].default_value = mix
        node.location = 800, -70

        outn = nodes['Input_1'].outputs[6]
        inn = nodes['Mix_1'].inputs[0]
        mat.node_tree.links.new(outn, inn)

        outn = nodes['Diffuse_1'].outputs[0]
        inn = nodes['Mix_1'].inputs[2]
        mat.node_tree.links.new(outn, inn)

        outn = nodes['Mix_0'].outputs[0]
        inn = nodes['Mix_1'].inputs[1]
        mat.node_tree.links.new(outn, inn)

        outn = nodes['Mix_1'].outputs[0]
        inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
        mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles translucent material
# --------------------------------------------------------------------
def create_translucent_material(matname, replace, r, g, b, rv=0.8, gv=0.8, bv=0.8, mix=0.1):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.diffuse_color = (rv, gv, bv)  # viewport color
    mat.use_nodes = True
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    node.name = 'Diffuse BSDF'
    node.label = 'Diffuse BSDF'

    node.inputs[0].default_value = [r, g, b, 1]
    node.location = 200, 320

    node = nodes.new('ShaderNodeBsdfTranslucent')
    node.name = 'Translucent_0'
    node.location = 200, 0

    node = nodes.new('ShaderNodeMixShader')
    node.name = 'Mix_0'
    node.inputs[0].default_value = mix
    node.location = 500, 160

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 1100, 160

    # Connect nodes
    outn = nodes['Diffuse BSDF'].outputs[0]
    inn = nodes['Mix_0'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Translucent_0'].outputs[0]
    inn = nodes['Mix_0'].inputs[2]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Mix_0'].outputs[0]
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles glass material
# --------------------------------------------------------------------
def create_glass_material(matname, replace, rv=0.333, gv=0.342, bv=0.9):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    mat.diffuse_color = (rv, gv, bv)
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    mat.node_tree.nodes.remove(node)  # remove not used

    node = nodes.new('ShaderNodeLightPath')
    node.name = 'Light_0'
    node.location = 10, 160

    node = nodes.new('ShaderNodeBsdfRefraction')
    node.name = 'Refraction_0'
    node.inputs[2].default_value = 1  # IOR 1.0
    node.location = 250, 400

    node = nodes.new('ShaderNodeBsdfGlossy')
    node.name = 'Glossy_0'
    node.distribution = 'SHARP'
    node.location = 250, 100

    node = nodes.new('ShaderNodeBsdfTransparent')
    node.name = 'Transparent_0'
    node.location = 500, 10

    node = nodes.new('ShaderNodeMixShader')
    node.name = 'Mix_0'
    node.inputs[0].default_value = 0.035
    node.location = 500, 160

    node = nodes.new('ShaderNodeMixShader')
    node.name = 'Mix_1'
    node.inputs[0].default_value = 0.1
    node.location = 690, 290

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 920, 290

    # Connect nodes
    outn = nodes['Light_0'].outputs[1]
    inn = nodes['Mix_1'].inputs[0]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Refraction_0'].outputs[0]
    inn = nodes['Mix_0'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Glossy_0'].outputs[0]
    inn = nodes['Mix_0'].inputs[2]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Mix_0'].outputs[0]
    inn = nodes['Mix_1'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Transparent_0'].outputs[0]
    inn = nodes['Mix_1'].inputs[2]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Mix_1'].outputs[0]
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# ---------------------------------------------
# Create cycles transparents material
# --------------------------------------------------------------------
def create_transparent_material(matname, replace, r=1, g=1, b=1, alpha=0):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    mat.diffuse_color = (r, g, b)
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    mat.node_tree.nodes.remove(node)  # remove not used

    node = nodes.new('ShaderNodeBsdfTransparent')
    node.name = 'Transparent_0'
    node.location = 250, 160
    node.inputs[0].default_value = [r, g, b, alpha]

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 700, 160

    # Connect nodes
    outn = nodes['Transparent_0'].outputs[0]
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles glossy material
# --------------------------------------------------------------------
def create_glossy_material(matname, replace, r, g, b, rv=0.578, gv=0.555, bv=0.736, rvalue=0.2):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    mat.diffuse_color = (rv, gv, bv)
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    mat.node_tree.nodes.remove(node)  # remove not used

    node = nodes.new('ShaderNodeBsdfGlossy')
    node.name = 'Glossy_0'
    node.inputs[0].default_value = [r, g, b, 1]
    node.inputs[1].default_value = rvalue
    node.location = 200, 160

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 700, 160

    # Connect nodes
    outn = nodes['Glossy_0'].outputs[0]
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles emission material
# --------------------------------------------------------------------
def create_emission_material(matname, replace, r, g, b, energy):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    mat.node_tree.nodes.remove(node)  # remove not used

    node = nodes.new('ShaderNodeEmission')
    node.name = 'Emission_0'
    node.inputs[0].default_value = [r, g, b, 1]
    node.inputs[1].default_value = energy
    node.location = 200, 160

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 700, 160

    # Connect nodes
    outn = nodes['Emission_0'].outputs[0]
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles glass material
# --------------------------------------------------------------------
def create_old_glass_material(matname, replace, rv=0.352716, gv=0.760852, bv=0.9):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    mat.diffuse_color = (rv, gv, bv)
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    mat.node_tree.nodes.remove(node)  # remove not used

    node = nodes.new('ShaderNodeLightPath')
    node.name = 'Light_0'
    node.location = 10, 160

    node = nodes.new('ShaderNodeBsdfGlass')
    node.name = 'Glass_0'
    node.location = 250, 300

    node = nodes.new('ShaderNodeBsdfTransparent')
    node.name = 'Transparent_0'
    node.location = 250, 0

    node = nodes.new('ShaderNodeMixShader')
    node.name = 'Mix_0'
    node.inputs[0].default_value = 0.1
    node.location = 500, 160

    node = nodes.new('ShaderNodeMixShader')
    node.name = 'Mix_1'
    node.inputs[0].default_value = 0.1
    node.location = 690, 290

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 920, 290

    # Connect nodes
    outn = nodes['Light_0'].outputs[1]
    inn = nodes['Mix_0'].inputs[0]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Light_0'].outputs[2]
    inn = nodes['Mix_1'].inputs[0]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Glass_0'].outputs[0]
    inn = nodes['Mix_0'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Transparent_0'].outputs[0]
    inn = nodes['Mix_0'].inputs[2]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Mix_0'].outputs[0]
    inn = nodes['Mix_1'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Mix_1'].outputs[0]
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles brick texture material
# --------------------------------------------------------------------
def create_brick_material(matname, replace, r, g, b, rv=0.8, gv=0.636, bv=0.315):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    mat.diffuse_color = (rv, gv, bv)
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    node.name = 'Diffuse BSDF'
    node.label = 'Diffuse BSDF'

    node.inputs[0].default_value = [r, g, b, 1]
    node.location = 500, 160

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 700, 160

    node = nodes.new('ShaderNodeTexBrick')
    node.name = 'Brick_0'
    node.inputs[3].default_value = [0.407, 0.411, 0.394, 1]  # mortar color
    node.inputs[4].default_value = 3  # scale
    node.inputs[5].default_value = 0.001  # mortar
    node.inputs[7].default_value = 0.60  # size_w
    node.inputs[8].default_value = 0.30  # size_h
    node.location = 300, 160

    node = nodes.new('ShaderNodeRGB')
    node.name = 'RGB_0'
    node.outputs[0].default_value = [r, g, b, 1]
    node.location = 70, 160

    # Connect nodes
    outn = nodes['RGB_0'].outputs['Color']
    inn = nodes['Brick_0'].inputs['Color1']
    mat.node_tree.links.new(outn, inn)

    inn = nodes['Brick_0'].inputs['Color2']
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Brick_0'].outputs['Color']
    inn = nodes['Diffuse BSDF'].inputs['Color']
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Create cycles fabric texture material
# --------------------------------------------------------------------
def create_fabric_material(matname, replace, r, g, b, rv=0.8, gv=0.636, bv=0.315):
    # Avoid duplicate materials
    if replace is False:
        matlist = bpy.data.materials
        for m in matlist:
            if m.name == matname:
                return m
    # Create material
    mat = bpy.data.materials.new(matname)
    mat.use_nodes = True
    mat.diffuse_color = (rv, gv, bv)
    nodes = mat.node_tree.nodes

    # support for multilanguage
    node = nodes[get_node_index(nodes, 'BSDF_DIFFUSE')]
    node.name = 'Diffuse BSDF'
    node.label = 'Diffuse BSDF'

    node.inputs[0].default_value = [r, g, b, 1]
    node.location = 810, 270

    node = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')]
    node.location = 1210, 320

    node = nodes.new('ShaderNodeTexCoord')
    node.name = 'UVCoordinates'
    node.location = 26, 395

    node = nodes.new('ShaderNodeMapping')
    node.name = 'UVMapping'
    node.location = 266, 380
    node.scale[0] = 1000
    node.scale[1] = 1000
    node.scale[2] = 1000

    # ===========================================================================
    # Image texture
    # ===========================================================================
    # Load image file.

    realpath = path.join(path.dirname(__file__), "images", "fabric_diffuse.png")
    print("Loading: " + realpath)
    try:
        img = bpy.data.images.load(realpath)
    except:
        raise NameError("Cannot load image %s" % realpath)

    # Create image texture from image
    ctex = bpy.data.textures.new('ColorTex', type='IMAGE')
    ctex.image = img

    node = nodes.new('ShaderNodeTexImage')
    node.name = 'Image1'
    node.image = ctex.image
    node.location = 615, 350

    node = nodes.new('ShaderNodeBsdfTransparent')
    node.name = 'Transparent1'
    node.location = 810, 395
    node.inputs[0].default_value = [r, g, b, 1]

    node = nodes.new('ShaderNodeAddShader')
    node.name = 'Add1'
    node.location = 1040, 356

    # Connect nodes
    outn = nodes['UVCoordinates'].outputs['UV']
    inn = nodes['UVMapping'].inputs['Vector']
    mat.node_tree.links.new(outn, inn)

    outn = nodes['UVMapping'].outputs['Vector']
    inn = nodes['Image1'].inputs['Vector']
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Image1'].outputs['Color']
    inn = nodes['Diffuse BSDF'].inputs['Color']
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Transparent1'].outputs['BSDF']
    inn = nodes['Add1'].inputs[0]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Diffuse BSDF'].outputs['BSDF']
    inn = nodes['Add1'].inputs[1]
    mat.node_tree.links.new(outn, inn)

    outn = nodes['Add1'].outputs['Shader']
    inn = nodes[get_node_index(nodes, 'OUTPUT_MATERIAL')].inputs[0]
    mat.node_tree.links.new(outn, inn)

    return mat


# --------------------------------------------------------------------
# Copy bin file
# --------------------------------------------------------------------
def copy_binfile(fromfile, tofile):
    with open(fromfile, 'rb') as f1:
        with open(tofile, 'wb') as f2:
            while True:
                mybytes = f1.read(1024)
                if mybytes:
                    f2.write(mybytes)
                else:
                    break


# --------------------------------------------------------------------
# Parent object (keep positions)
# --------------------------------------------------------------------
def parentobject(parentobj, childobj):
    # noinspection PyBroadException
    try:
        bpy.ops.object.select_all(action='DESELECT')
        bpy.context.scene.objects.active = parentobj
        parentobj.select = True
        childobj.select = True
        bpy.ops.object.parent_set(type='OBJECT', keep_transform=False)
        return True
    except:
        return False


# ------------------------------------------------------------------------------
# Create control box
#
# objName: Object name
# x: size x axis
# y: size y axis
# z: size z axis
# tube: True create a tube, False only sides
# ------------------------------------------------------------------------------
def create_control_box(objname, x, y, z, tube=True):
    myvertex = [(-x / 2, 0, 0.0),
                (-x / 2, y, 0.0),
                (x / 2, y, 0.0),
                (x / 2, 0, 0.0),
                (-x / 2, 0, z),
                (-x / 2, y, z),
                (x / 2, y, z),
                (x / 2, 0, z)]

    if tube is True:
        myfaces = [(0, 1, 2, 3), (0, 4, 5, 1), (1, 5, 6, 2), (3, 7, 4, 0), (2, 6, 7, 3), (5, 4, 7, 6)]
    else:
        myfaces = [(0, 4, 5, 1), (2, 6, 7, 3)]

    mesh = bpy.data.meshes.new(objname)
    myobject = bpy.data.objects.new(objname, mesh)

    myobject.location = bpy.context.scene.cursor_location
    bpy.context.scene.objects.link(myobject)

    mesh.from_pydata(myvertex, [], myfaces)
    mesh.update(calc_edges=True)

    return myobject


# ------------------------------------------------------------------------------
# Remove all children objects
# ------------------------------------------------------------------------------
def remove_children(myobject):
    # Remove children
    for child in myobject.children:
        # noinspection PyBroadException
        try:
            # noinspection PyBroadException
            try:
                # remove child relationship
                for grandchild in child.children:
                    grandchild.parent = None
                # remove modifiers
                for mod in child.modifiers:
                    bpy.ops.object.modifier_remove(name=mod.name)
            except:
                pass
            # clear child data
            if child.type == 'MESH':
                old = child.data
                child.select = True
                bpy.ops.object.delete()
                bpy.data.meshes.remove(old)
            if child.type == 'CURVE':
                child.select = True
                bpy.ops.object.delete()
        except:
            pass


# --------------------------------------------------------------------
# Get all parents
# --------------------------------------------------------------------
def get_allparents(myobj):
    obj = myobj
    mylist = []
    while obj.parent is not None:
        mylist.append(obj)
        objp = obj.parent
        obj = objp

    mylist.append(obj)

    return mylist


# --------------------------------------------------------------------
# Verify all faces are in vertice group to avoid Blander crash
#
# Review the faces array and remove any vertex out of the range
# this avoid any bug that can appear avoiding Blender crash
# --------------------------------------------------------------------
def check_mesh_errors(myvertices, myfaces):
    vmax = len(myvertices)

    f = 0
    for face in myfaces:
        for v in face:
            if v < 0 or v > vmax:
                print("Face=" + str(f) + "->removed vertex=" + str(v))
                myfaces[f].remove(v)
        f += 1

    return myfaces
