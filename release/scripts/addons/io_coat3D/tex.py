# ***** BEGIN GPL LICENSE BLOCK *****
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****

import bpy
import os

def find_index(objekti):
    luku = 0
    for tex in objekti.active_material.texture_slots:
        if(not(hasattr(tex,'texture'))):
            break
        luku = luku +1
    return luku

def gettex(mat_list, objekti, scene,export):

    coat3D = bpy.context.scene.coat3D
    coa = objekti.coat3D

    if(bpy.context.scene.render.engine == 'VRAY_RENDER' or bpy.context.scene.render.engine == 'VRAY_RENDER_PREVIEW'):
        vray = True
    else:
        vray = False

    take_color = 0
    take_spec = 0
    take_normal = 0
    take_disp = 0

    bring_color = 1
    bring_spec = 1
    bring_normal = 1
    bring_disp = 1

    texcoat = {}
    texcoat['color'] = []
    texcoat['specular'] = []
    texcoat['nmap'] = []
    texcoat['disp'] = []
    texu = []

    if(export):
        objekti.coat3D.objpath = export
        nimi = os.path.split(export)[1]
        osoite = os.path.dirname(export) + os.sep #pitaa ehka muuttaa
        for mate in objekti.material_slots:
            for tex_slot in mate.material.texture_slots:
                if(hasattr(tex_slot,'texture')):
                    if(tex_slot.texture.type == 'IMAGE'):
                        if tex_slot.texture.image is not None:
                            tex_slot.texture.image.reload()
    else:
        if(os.sys.platform == 'win32'):
            osoite = os.path.expanduser("~") + os.sep + 'Documents' + os.sep + '3DC2Blender' + os.sep + 'Textures' + os.sep
        else:
            osoite = os.path.expanduser("~") + os.sep + '3DC2Blender' + os.sep + 'Textures' + os.sep
    ki = os.path.split(coa.applink_name)[1]
    ko = os.path.splitext(ki)[0]
    just_nimi = ko + '_'
    just_nimi_len = len(just_nimi)
    print('terve:' + coa.applink_name)

    if(len(objekti.material_slots) != 0):
        for obj_tex in objekti.active_material.texture_slots:
            if(hasattr(obj_tex,'texture')):
                if(obj_tex.texture.type == 'IMAGE'):
                    if(obj_tex.use_map_color_diffuse):
                        bring_color = 0;
                    if(obj_tex.use_map_specular):
                        bring_spec = 0;
                    if(obj_tex.use_map_normal):
                        bring_normal = 0;
                    if(obj_tex.use_map_displacement):
                        bring_disp = 0;

    files = os.listdir(osoite)
    for i in files:
        tui = i[:just_nimi_len]
        if(tui == just_nimi):
            texu.append(i)

    for yy in texu:
        minimi = (yy.rfind('_'))+1
        maksimi = (yy.rfind('.'))
        tex_name = yy[minimi:maksimi]
        koko = ''
        koko += osoite
        koko += yy
        texcoat[tex_name].append(koko)

    if((texcoat['color'] or texcoat['nmap'] or texcoat['disp'] or texcoat['specular']) and (len(objekti.material_slots)) == 0):
        materials_old = bpy.data.materials.keys()
        bpy.ops.material.new()
        materials_new = bpy.data.materials.keys()
        new_ma = list(set(materials_new).difference(set(materials_old)))
        new_mat = new_ma[0]
        ki = bpy.data.materials[new_mat]
        objekti.data.materials.append(ki)

    if(bring_color == 1 and texcoat['color']):
        index = find_index(objekti)
        tex = bpy.ops.Texture
        objekti.active_material.texture_slots.create(index)
        total_mat = len(objekti.active_material.texture_slots.items())
        useold = ''

        for seekco in bpy.data.textures:
            if((seekco.name[:5] == 'Color') and (seekco.users_material == ())):
                useold = seekco


        if(useold == ''):

            textures_old = bpy.data.textures.keys()
            bpy.data.textures.new('Color',type='IMAGE')
            textures_new = bpy.data.textures.keys()
            name_te = list(set(textures_new).difference(set(textures_old)))
            name_tex = name_te[0]

            bpy.ops.image.new(name=name_tex)
            bpy.data.images[name_tex].filepath = texcoat['color'][0]
            bpy.data.images[name_tex].source = 'FILE'

            objekti.active_material.texture_slots[index].texture = bpy.data.textures[name_tex]
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[name_tex]

            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name

            objekti.active_material.texture_slots[index].texture.image.reload()


        elif(useold != ''):

            objekti.active_material.texture_slots[index].texture = useold
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[useold.name]
            objekti.active_material.texture_slots[index].texture.image.filepath = texcoat['color'][0]
            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name


    if(bring_normal == 1 and texcoat['nmap']):
        index = find_index(objekti)
        tex = bpy.ops.Texture
        objekti.active_material.texture_slots.create(index)
        total_mat = len(objekti.active_material.texture_slots.items())
        useold = ''

        for seekco in bpy.data.textures:
            if((seekco.name[:6] == 'Normal') and (seekco.users_material == ())):
                useold = seekco

        if(useold == ''):

            textures_old = bpy.data.textures.keys()
            bpy.data.textures.new('Normal',type='IMAGE')
            textures_new = bpy.data.textures.keys()
            name_te = list(set(textures_new).difference(set(textures_old)))
            name_tex = name_te[0]

            bpy.ops.image.new(name=name_tex)
            bpy.data.images[name_tex].filepath = texcoat['nmap'][0]
            bpy.data.images[name_tex].source = 'FILE'

            objekti.active_material.texture_slots[index].texture = bpy.data.textures[name_tex]
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[name_tex]

            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name

            objekti.active_material.texture_slots[index].use_map_color_diffuse = False
            objekti.active_material.texture_slots[index].use_map_normal = True

            objekti.active_material.texture_slots[index].texture.image.reload()
            if(vray):
                bpy.data.textures[name_tex].vray_slot.BRDFBump.map_type = 'TANGENT'

            else:
                bpy.data.textures[name_tex].use_normal_map = True
                objekti.active_material.texture_slots[index].normal_map_space = 'TANGENT'
                objekti.active_material.texture_slots[index].normal_factor = 1



        elif(useold != ''):

            objekti.active_material.texture_slots[index].texture = useold
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[useold.name]
            objekti.active_material.texture_slots[index].texture.image.filepath = texcoat['nmap'][0]
            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name
            objekti.active_material.texture_slots[index].use_map_color_diffuse = False
            objekti.active_material.texture_slots[index].use_map_normal = True
            objekti.active_material.texture_slots[index].normal_factor = 1


    if(bring_spec == 1 and texcoat['specular']):

        index = find_index(objekti)

        objekti.active_material.texture_slots.create(index)
        useold = ''

        for seekco in bpy.data.textures:
            if((seekco.name[:8] == 'Specular') and (seekco.users_material == ())):
                useold = seekco

        if(useold == ''):

            textures_old = bpy.data.textures.keys()
            bpy.data.textures.new('Specular',type='IMAGE')
            textures_new = bpy.data.textures.keys()
            name_te = list(set(textures_new).difference(set(textures_old)))
            name_tex = name_te[0]

            bpy.ops.image.new(name=name_tex)
            bpy.data.images[name_tex].filepath = texcoat['specular'][0]
            bpy.data.images[name_tex].source = 'FILE'

            objekti.active_material.texture_slots[index].texture = bpy.data.textures[name_tex]
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[name_tex]

            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name

            objekti.active_material.texture_slots[index].use_map_color_diffuse = False
            objekti.active_material.texture_slots[index].use_map_specular = True

            objekti.active_material.texture_slots[index].texture.image.reload()


        elif(useold != ''):

            objekti.active_material.texture_slots[index].texture = useold
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[useold.name]
            objekti.active_material.texture_slots[index].texture.image.filepath = texcoat['specular'][0]
            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name
            objekti.active_material.texture_slots[index].use_map_color_diffuse = False
            objekti.active_material.texture_slots[index].use_map_specular = True

    if(bring_disp == 1 and texcoat['disp']):

        index = find_index(objekti)


        objekti.active_material.texture_slots.create(index)
        useold = ''

        for seekco in bpy.data.textures:
            if((seekco.name[:12] == 'Displacement') and (seekco.users_material == ())):
                useold = seekco

        if useold == "":

            textures_old = bpy.data.textures.keys()
            bpy.data.textures.new('Displacement',type='IMAGE')
            textures_new = bpy.data.textures.keys()
            name_te = list(set(textures_new).difference(set(textures_old)))
            name_tex = name_te[0]

            bpy.ops.image.new(name=name_tex)
            bpy.data.images[name_tex].filepath = texcoat['disp'][0]
            bpy.data.images[name_tex].source = 'FILE'

            objekti.active_material.texture_slots[index].texture = bpy.data.textures[name_tex]
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[name_tex]

            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name

            objekti.active_material.texture_slots[index].use_map_color_diffuse = False
            objekti.active_material.texture_slots[index].use_map_displacement = True

            objekti.active_material.texture_slots[index].texture.image.reload()


        elif(useold != ''):

            objekti.active_material.texture_slots[index].texture = useold
            objekti.active_material.texture_slots[index].texture.image = bpy.data.images[useold.name]
            objekti.active_material.texture_slots[index].texture.image.filepath = texcoat['disp'][0]
            if(objekti.data.uv_textures.active):
                objekti.active_material.texture_slots[index].texture_coords = 'UV'
                objekti.active_material.texture_slots[index].uv_layer = objekti.data.uv_textures.active.name
            objekti.active_material.texture_slots[index].use_map_color_diffuse = False
            objekti.active_material.texture_slots[index].use_map_displacement = True

        if(vray):
            objekti.active_material.texture_slots[index].texture.use_interpolation = False
            objekti.active_material.texture_slots[index].displacement_factor = 0.05


        else:
            disp_modi = ''
            for seek_modi in objekti.modifiers:
                if(seek_modi.type == 'DISPLACE'):
                    disp_modi = seek_modi
                    break
            if(disp_modi):
                disp_modi.texture = objekti.active_material.texture_slots[index].texture
                if(objekti.data.uv_textures.active):
                    disp_modi.texture_coords = 'UV'
                    disp_modi.uv_layer = objekti.data.uv_textures.active.name
            else:
                objekti.modifiers.new('Displace',type='DISPLACE')
                objekti.modifiers['Displace'].texture = objekti.active_material.texture_slots[index].texture
                if(objekti.data.uv_textures.active):
                    objekti.modifiers['Displace'].texture_coords = 'UV'
                    objekti.modifiers['Displace'].uv_layer = objekti.data.uv_textures.active.name

    return('FINISHED')
