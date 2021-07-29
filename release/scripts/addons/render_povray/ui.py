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
import sys #really import here and in render.py?
import os #really import here and in render.py?
from os.path import isfile

# Use some of the existing buttons.
from bl_ui import properties_render
properties_render.RENDER_PT_render.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_render.RENDER_PT_dimensions.COMPAT_ENGINES.add('POVRAY_RENDER')
# properties_render.RENDER_PT_antialiasing.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_render.RENDER_PT_shading.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_render.RENDER_PT_output.COMPAT_ENGINES.add('POVRAY_RENDER')
del properties_render


# Use only a subset of the world panels
from bl_ui import properties_world
properties_world.WORLD_PT_preview.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_world.WORLD_PT_context_world.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_world.WORLD_PT_world.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_world.WORLD_PT_mist.COMPAT_ENGINES.add('POVRAY_RENDER')
del properties_world


# Example of wrapping every class 'as is'
from bl_ui import properties_texture
from bl_ui.properties_texture import context_tex_datablock
for member in dir(properties_texture):
    subclass = getattr(properties_texture, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_texture

# Physics Main wrapping every class 'as is'
from bl_ui import properties_physics_common
for member in dir(properties_physics_common):
    subclass = getattr(properties_physics_common, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_common

# Physics Rigid Bodies wrapping every class 'as is'
from bl_ui import properties_physics_rigidbody
for member in dir(properties_physics_rigidbody):
    subclass = getattr(properties_physics_rigidbody, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_rigidbody

# Physics Rigid Body Constraint wrapping every class 'as is'
from bl_ui import properties_physics_rigidbody_constraint
for member in dir(properties_physics_rigidbody_constraint):
    subclass = getattr(properties_physics_rigidbody_constraint, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_rigidbody_constraint

# Physics Smoke wrapping every class 'as is'
from bl_ui import properties_physics_smoke
for member in dir(properties_physics_smoke):
    subclass = getattr(properties_physics_smoke, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_smoke

# Physics softbody wrapping every class 'as is'
from bl_ui import properties_physics_softbody
for member in dir(properties_physics_softbody):
    subclass = getattr(properties_physics_softbody, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_softbody

# Physics Fluid wrapping every class 'as is'
from bl_ui import properties_physics_fluid
for member in dir(properties_physics_fluid):
    subclass = getattr(properties_physics_fluid, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_fluid

# Physics Field wrapping every class 'as is'
from bl_ui import properties_physics_field
for member in dir(properties_physics_field):
    subclass = getattr(properties_physics_field, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_field

# Physics Cloth wrapping every class 'as is'
from bl_ui import properties_physics_cloth
for member in dir(properties_physics_cloth):
    subclass = getattr(properties_physics_cloth, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_cloth

# Physics Dynamic Paint wrapping every class 'as is'
from bl_ui import properties_physics_dynamicpaint
for member in dir(properties_physics_dynamicpaint):
    subclass = getattr(properties_physics_dynamicpaint, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_physics_dynamicpaint


# Example of wrapping every class 'as is'
from bl_ui import properties_data_modifier
for member in dir(properties_data_modifier):
    subclass = getattr(properties_data_modifier, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_data_modifier

# Example of wrapping every class 'as is' except some
from bl_ui import properties_material
for member in dir(properties_material):
    subclass = getattr(properties_material, member)
    if subclass not in (properties_material.MATERIAL_PT_transp_game,
                        properties_material.MATERIAL_PT_game_settings,
                        properties_material.MATERIAL_PT_physics):
        try:
            #mat=context.material
            #if mat and mat.type == "SURFACE" and (engine in cls.COMPAT_ENGINES) and not (mat.pov.material_use_nodes or mat.use_nodes):
            subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
        except:
            pass
del properties_material


from bl_ui import properties_data_camera
for member in dir(properties_data_camera):
    subclass = getattr(properties_data_camera, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_data_camera




from bl_ui import properties_particle as properties_particle
for member in dir(properties_particle):  # add all "particle" panels from blender
    subclass = getattr(properties_particle, member)
    try:
        subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    except:
        pass
del properties_particle

def check_add_mesh_extra_objects():
    if "add_mesh_extra_objects" in bpy.context.user_preferences.addons.keys():
        return True
    return False

def locate_docpath():
    addon_prefs = bpy.context.user_preferences.addons[__package__].preferences
    # Use the system preference if its set.
    pov_documents = addon_prefs.docpath_povray
    if pov_documents:
        if os.path.exists(pov_documents):
            return pov_documents
        else:
            print("User Preferences path to povray documents %r NOT FOUND, checking $PATH" % pov_documents)

    # Windows Only
    if sys.platform[:3] == "win":
        import winreg
        try:
            win_reg_key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                "Software\\POV-Ray\\v3.7\\Windows")
            win_docpath = winreg.QueryValueEx(win_reg_key, "DocPath")[0]
            pov_documents = os.path.join(win_docpath, "Insert Menu")
            if os.path.exists(pov_documents):
                return pov_documents
        except FileNotFoundError:
            return""
    # search the path all os's
    pov_documents_default = "include"

    os_path_ls = os.getenv("PATH").split(':') + [""]

    for dir_name in os_path_ls:
        pov_documents = os.path.join(dir_name, pov_documents_default)
        if os.path.exists(pov_documents):
            return pov_documents
    return ""

class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

class ModifierButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "modifier"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        mods = context.object.modifiers
        rd = context.scene.render
        return mods and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

class MaterialButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        mat = context.material
        rd = context.scene.render
        return mat and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)


class TextureButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "texture"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        tex = context.texture
        rd = context.scene.render
        return tex and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

# class TextureTypePanel(TextureButtonsPanel):

    # @classmethod
    # def poll(cls, context):
        # tex = context.texture
        # engine = context.scene.render.engine
        # return tex and ((tex.type == cls.tex_type and not tex.use_nodes) and (engine in cls.COMPAT_ENGINES))


class ObjectButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        obj = context.object
        rd = context.scene.render
        return obj and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

class CameraDataButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        cam = context.camera
        rd = context.scene.render
        return cam and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

class WorldButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        wld = context.world
        rd = context.scene.render
        return wld and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

class TextButtonsPanel():
    bl_space_type = 'TEXT_EDITOR'
    bl_region_type = 'UI'
    bl_label = "POV-Ray"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        text = context.space_data
        rd = context.scene.render
        return text and (rd.use_game_engine is False) and (rd.engine in cls.COMPAT_ENGINES)

from bl_ui import properties_data_mesh
# These panels are kept
properties_data_mesh.DATA_PT_custom_props_mesh.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_data_mesh.DATA_PT_context_mesh.COMPAT_ENGINES.add('POVRAY_RENDER')

## make some native panels contextual to some object variable
## by recreating custom panels inheriting their properties

class PovDataButtonsPanel(properties_data_mesh.MeshButtonsPanel):
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    POV_OBJECT_TYPES = {'PLANE', 'BOX', 'SPHERE', 'CYLINDER', 'CONE', 'TORUS', 'BLOB',
                        'ISOSURFACE', 'SUPERELLIPSOID', 'SUPERTORUS', 'HEIGHT_FIELD',
                        'PARAMETRIC', 'POLYCIRCLE'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        # We use our parent class poll func too, avoids to re-define too much things...
        return (super(PovDataButtonsPanel, cls).poll(context) and
                obj and obj.pov.object_as not in cls.POV_OBJECT_TYPES)


# We cannot inherit from RNA classes (like e.g. properties_data_mesh.DATA_PT_vertex_groups).
# Complex py/bpy/rna interactions (with metaclass and all) simply do not allow it to work.
# So we simply have to explicitly copy here the interesting bits. ;)
class DATA_PT_POV_normals(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_normals.bl_label

    draw = properties_data_mesh.DATA_PT_normals.draw


class DATA_PT_POV_texture_space(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_texture_space.bl_label
    bl_options = properties_data_mesh.DATA_PT_texture_space.bl_options

    draw = properties_data_mesh.DATA_PT_texture_space.draw


class DATA_PT_POV_vertex_groups(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_vertex_groups.bl_label

    draw = properties_data_mesh.DATA_PT_vertex_groups.draw


class DATA_PT_POV_shape_keys(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_shape_keys.bl_label

    draw = properties_data_mesh.DATA_PT_shape_keys.draw


class DATA_PT_POV_uv_texture(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_uv_texture.bl_label

    draw = properties_data_mesh.DATA_PT_uv_texture.draw


class DATA_PT_POV_vertex_colors(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_vertex_colors.bl_label

    draw = properties_data_mesh.DATA_PT_vertex_colors.draw


class DATA_PT_POV_customdata(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_mesh.DATA_PT_customdata.bl_label
    bl_options = properties_data_mesh.DATA_PT_customdata.bl_options
    draw = properties_data_mesh.DATA_PT_customdata.draw



del properties_data_mesh


################################################################################
# from bl_ui import properties_data_lamp
# for member in dir(properties_data_lamp):
    # subclass = getattr(properties_data_lamp, member)
    # try:
        # subclass.COMPAT_ENGINES.add('POVRAY_RENDER')
    # except:
        # pass
# del properties_data_lamp
#########################LAMPS################################

from bl_ui import properties_data_lamp

# These panels are kept
properties_data_lamp.DATA_PT_custom_props_lamp.COMPAT_ENGINES.add('POVRAY_RENDER')
properties_data_lamp.DATA_PT_context_lamp.COMPAT_ENGINES.add('POVRAY_RENDER')

## make some native panels contextual to some object variable
## by recreating custom panels inheriting their properties
class PovLampButtonsPanel(properties_data_lamp.DataButtonsPanel):
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    POV_OBJECT_TYPES = {'RAINBOW'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        # We use our parent class poll func too, avoids to re-define too much things...
        return (super(PovLampButtonsPanel, cls).poll(context) and
                obj and obj.pov.object_as not in cls.POV_OBJECT_TYPES)


# We cannot inherit from RNA classes (like e.g. properties_data_mesh.DATA_PT_vertex_groups).
# Complex py/bpy/rna interactions (with metaclass and all) simply do not allow it to work.
# So we simply have to explicitly copy here the interesting bits. ;)

class LAMP_PT_POV_preview(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_preview.bl_label

    draw = properties_data_lamp.DATA_PT_preview.draw

class LAMP_PT_POV_lamp(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_lamp.bl_label

    draw = properties_data_lamp.DATA_PT_lamp.draw

class LAMP_PT_POV_sunsky(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_sunsky.bl_label

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'SUN') and (engine in cls.COMPAT_ENGINES)

    draw = properties_data_lamp.DATA_PT_sunsky.draw

class LAMP_PT_POV_shadow(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_shadow.bl_label

    draw = properties_data_lamp.DATA_PT_shadow.draw

class LAMP_PT_POV_area(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_area.bl_label

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'AREA') and (engine in cls.COMPAT_ENGINES)

    draw = properties_data_lamp.DATA_PT_area.draw

class LAMP_PT_POV_spot(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_spot.bl_label

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine
        return (lamp and lamp.type == 'SPOT') and (engine in cls.COMPAT_ENGINES)
    draw = properties_data_lamp.DATA_PT_spot.draw

class LAMP_PT_POV_falloff_curve(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = properties_data_lamp.DATA_PT_falloff_curve.bl_label
    bl_options = properties_data_lamp.DATA_PT_falloff_curve.bl_options

    @classmethod
    def poll(cls, context):
        lamp = context.lamp
        engine = context.scene.render.engine

        return (lamp and lamp.type in {'POINT', 'SPOT'} and lamp.falloff_type == 'CUSTOM_CURVE') and (engine in cls.COMPAT_ENGINES)
    draw = properties_data_lamp.DATA_PT_falloff_curve.draw

class OBJECT_PT_povray_obj_rainbow(PovLampButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Rainbow"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'RAINBOW' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'RAINBOW':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Rainbow projection angle: " + str(obj.data.spot_size))
                col.label(text="Rainbow width: " + str(obj.data.spot_blend))
                col.label(text="Rainbow distance: " + str(obj.data.shadow_buffer_clip_start))
                col.label(text="Rainbow arc angle: " + str(obj.pov.arc_angle))
                col.label(text="Rainbow falloff angle: " + str(obj.pov.falloff_angle))

            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.cone_update", text="Update",icon="MESH_CONE")

                #col.label(text="Parameters:")
                col.prop(obj.data, "spot_size", text="Rainbow Projection Angle")
                col.prop(obj.data, "spot_blend", text="Rainbow width")
                col.prop(obj.data, "shadow_buffer_clip_start", text="Visibility distance")
                col.prop(obj.pov, "arc_angle")
                col.prop(obj.pov, "falloff_angle")

del properties_data_lamp
###############################################################################

class RENDER_PT_povray_export_settings(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "INI Options"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}


    def draw_header(self, context):
        scene = context.scene
        if scene.pov.tempfiles_enable:
            self.layout.prop(scene.pov, "tempfiles_enable", text="", icon='AUTO')
        else:
            self.layout.prop(scene.pov, "tempfiles_enable", text="", icon='CONSOLE')

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.active = (scene.pov.max_trace_level != 0)
        split = layout.split()

        col = split.column()
        col.label(text="Command line switches:")
        col.prop(scene.pov, "command_line_switches", text="")
        split = layout.split()

        layout.active = not scene.pov.tempfiles_enable
        #if not scene.pov.tempfiles_enable:
        split.prop(scene.pov, "deletefiles_enable", text="Delete files")
        split.prop(scene.pov, "pov_editor", text="POV Editor")

        col = layout.column()
        col.prop(scene.pov, "scene_name", text="Name")
        col.prop(scene.pov, "scene_path", text="Path to files")
        #col.prop(scene.pov, "scene_path", text="Path to POV-file")
        #col.prop(scene.pov, "renderimage_path", text="Path to image")

        split = layout.split()
        split.prop(scene.pov, "indentation_character", text="Indent")
        if scene.pov.indentation_character == 'SPACE':
            split.prop(scene.pov, "indentation_spaces", text="Spaces")

        row = layout.row()
        row.prop(scene.pov, "comments_enable", text="Comments")
        row.prop(scene.pov, "list_lf_enable", text="Line breaks in lists")


class RENDER_PT_povray_render_settings(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Render Settings"
    bl_icon = 'SETTINGS'
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw_header(self, context):
        scene = context.scene
        if scene.pov.global_settings_advanced:
            self.layout.prop(scene.pov, "global_settings_advanced", text="", icon='PREFERENCES')
        else:
            self.layout.prop(scene.pov, "global_settings_advanced", text="", icon='SETTINGS')
    def draw(self, context):
        layout = self.layout

        scene = context.scene
        #layout.active = (scene.pov.max_trace_level != 0)

        col = layout.column()

        col.label(text="Global Settings:")
        col.prop(scene.pov, "max_trace_level", text="Ray Depth")

        layout.active = scene.pov.global_settings_advanced
        layout.prop(scene.pov,"charset")
        align = True
        row = layout.row(align = align)
        row.prop(scene.pov,"adc_bailout")
        row = layout.row(align = align)
        row.prop(scene.pov,"ambient_light")
        row = layout.row(align = align)
        row.prop(scene.pov,"irid_wavelength")
        row = layout.row(align = align)
        row.prop(scene.pov,"max_intersections")
        row = layout.row(align = align)
        row.prop(scene.pov,"number_of_waves")
        row = layout.row(align = align)
        row.prop(scene.pov,"noise_generator")

class RENDER_PT_povray_photons(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Photons"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    # def draw_header(self, context):
        # self.layout.label(icon='SETTINGS')

    def draw_header(self, context):
        scene = context.scene
        if scene.pov.photon_enable:
            self.layout.prop(scene.pov, "photon_enable", text="", icon='PMARKER_ACT')
        else:
            self.layout.prop(scene.pov, "photon_enable", text="", icon='PMARKER')
    def draw(self, context):
        scene = context.scene
        layout = self.layout
        layout.active = scene.pov.photon_enable
        col = layout.column()
        #col.label(text="Global Photons:")
        col.prop(scene.pov, "photon_max_trace_level", text="Photon Depth")

        split = layout.split()

        col = split.column()
        col.prop(scene.pov, "photon_spacing", text="Spacing")
        col.prop(scene.pov, "photon_gather_min")

        col = split.column()
        col.prop(scene.pov, "photon_adc_bailout", text="Photon ADC")
        col.prop(scene.pov, "photon_gather_max")


        box = layout.box()
        box.label('Photon Map File:')
        row = box.row()
        row.prop(scene.pov, "photon_map_file_save_load",expand = True)
        if scene.pov.photon_map_file_save_load in {'save'}:
            box.prop(scene.pov, "photon_map_dir")
            box.prop(scene.pov, "photon_map_filename")
        if scene.pov.photon_map_file_save_load in {'load'}:
            box.prop(scene.pov, "photon_map_file")
        #end main photons

class RENDER_PT_povray_antialias(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Anti-Aliasing"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}


    def draw_header(self, context):
        prefs = bpy.context.user_preferences.addons[__package__].preferences
        scene = context.scene
        if prefs.branch_feature_set_povray != 'uberpov' and scene.pov.antialias_method == '2':
            self.layout.prop(scene.pov, "antialias_enable", text="", icon='ERROR')
        elif scene.pov.antialias_enable:
            self.layout.prop(scene.pov, "antialias_enable", text="", icon='ANTIALIASED')
        else:
            self.layout.prop(scene.pov, "antialias_enable", text="", icon='ALIASED')

    def draw(self, context):
        prefs = bpy.context.user_preferences.addons[__package__].preferences
        layout = self.layout
        scene = context.scene

        layout.active = scene.pov.antialias_enable

        row = layout.row()
        row.prop(scene.pov, "antialias_method", text="")

        if prefs.branch_feature_set_povray != 'uberpov' and scene.pov.antialias_method == '2':
            col = layout.column()
            col.alignment = 'CENTER'
            col.label(text="Stochastic Anti Aliasing is")
            col.label(text="Only Available with UberPOV")
            col.label(text="Feature Set in User Preferences.")
            col.label(text="Using Type 2 (recursive) instead")
        else:
            row.prop(scene.pov, "jitter_enable", text="Jitter")

            split = layout.split()
            col = split.column()
            col.prop(scene.pov, "antialias_depth", text="AA Depth")
            sub = split.column()
            sub.prop(scene.pov, "jitter_amount", text="Jitter Amount")
            if scene.pov.jitter_enable:
                sub.enabled = True
            else:
                sub.enabled = False

            row = layout.row()
            row.prop(scene.pov, "antialias_threshold", text="AA Threshold")
            row.prop(scene.pov, "antialias_gamma", text="AA Gamma")

            if prefs.branch_feature_set_povray == 'uberpov':
                row = layout.row()
                row.prop(scene.pov, "antialias_confidence", text="AA Confidence")
                if scene.pov.antialias_method == '2':
                    row.enabled = True
                else:
                    row.enabled = False



class RENDER_PT_povray_radiosity(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Radiosity"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    def draw_header(self, context):
        scene = context.scene
        if scene.pov.radio_enable:
            self.layout.prop(scene.pov, "radio_enable", text="", icon='RADIO')
        else:
            self.layout.prop(scene.pov, "radio_enable", text="", icon='RADIOBUT_OFF')

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.active = scene.pov.radio_enable

        split = layout.split()

        col = split.column()
        col.prop(scene.pov, "radio_count", text="Rays")
        col.prop(scene.pov, "radio_recursion_limit", text="Recursions")

        split.prop(scene.pov, "radio_error_bound", text="Error Bound")

        layout.prop(scene.pov, "radio_display_advanced")

        if scene.pov.radio_display_advanced:
            split = layout.split()

            col = split.column()
            col.prop(scene.pov, "radio_adc_bailout", slider=True)
            col.prop(scene.pov, "radio_minimum_reuse", text="Min Reuse")
            col.prop(scene.pov, "radio_gray_threshold", slider=True)
            col.prop(scene.pov, "radio_pretrace_start", slider=True)
            col.prop(scene.pov, "radio_low_error_factor", slider=True)

            col = split.column()
            col.prop(scene.pov, "radio_brightness")
            col.prop(scene.pov, "radio_maximum_reuse", text="Max Reuse")
            col.prop(scene.pov, "radio_nearest_count")
            col.prop(scene.pov, "radio_pretrace_end", slider=True)

            col = layout.column()
            col.label(text="Estimation Influence:")
            col.prop(scene.pov, "radio_always_sample")
            col.prop(scene.pov, "radio_normal")
            col.prop(scene.pov, "radio_media")
            col.prop(scene.pov, "radio_subsurface")



class RENDER_PT_povray_media(WorldButtonsPanel, bpy.types.Panel):
    bl_label = "Atmosphere Media"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw_header(self, context):
        scene = context.scene

        self.layout.prop(scene.pov, "media_enable", text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene

        layout.active = scene.pov.media_enable

        row = layout.row()
        row.prop(scene.pov, "media_samples", text="Samples")
        row.prop(scene.pov, "media_color", text="")

##class RENDER_PT_povray_baking(RenderButtonsPanel, bpy.types.Panel):
##    bl_label = "Baking"
##    COMPAT_ENGINES = {'POVRAY_RENDER'}
##
##    def draw_header(self, context):
##        scene = context.scene
##
##        self.layout.prop(scene.pov, "baking_enable", text="")
##
##    def draw(self, context):
##        layout = self.layout
##
##        scene = context.scene
##        rd = scene.render
##
##        layout.active = scene.pov.baking_enable
'''XXX WIP preparing for CSG
class MODIFIERS_PT_povray_modifiers(ModifierButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    #def draw_header(self, context):
        #scene = context.scene
        #self.layout.prop(scene.pov, "boolean_mod", text="")

    def draw(self, context):
        scene = context.scene
        layout = self.layout
        ob = context.object
        mod = ob.modifiers
        col = layout.column()
        # Find Boolean Modifiers for displaying CSG option
        onceCSG = 0
        for mod in ob.modifiers:
            if onceCSG == 0:
                if mod :
                    if mod.type == 'BOOLEAN':
                        col.prop(ob.pov, "boolean_mod")
                        onceCSG = 1

                    if ob.pov.boolean_mod == "POV": 
                        split = layout.split()
                        col = layout.column()
                        # Inside Vector for CSG
                        col.prop(ob.pov, "inside_vector")
'''        

class MATERIAL_PT_povray_activate_node(MaterialButtonsPanel, bpy.types.Panel):
    bl_label = "Activate Node Settings"
    bl_context = "material"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        mat=context.material
        ob = context.object
        return mat and mat.type == "SURFACE" and (engine in cls.COMPAT_ENGINES) and not (mat.pov.material_use_nodes or mat.use_nodes)

    def draw(self, context):
        layout = self.layout
        # layout.operator("pov.material_use_nodes", icon='SOUND')#'NODETREE')
        # the above replaced with a context hook below:
        layout.operator("WM_OT_context_toggle", text="Use POV-Ray Nodes", icon='NODETREE').data_path = \
                        "material.pov.material_use_nodes"

class MATERIAL_PT_povray_active_node(MaterialButtonsPanel, bpy.types.Panel):
    bl_label = "Active Node Settings"
    bl_context = "material"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        mat=context.material
        ob = context.object
        return mat and mat.type == "SURFACE" and (engine in cls.COMPAT_ENGINES) and mat.pov.material_use_nodes


    def draw(self, context):
        layout = self.layout
        mat = context.material
        node_tree = mat.node_tree
        if node_tree:
            node = node_tree.nodes.active
            if mat.use_nodes:
                if node:
                    layout.prop(mat.pov,"material_active_node")
                    if node.bl_idname=="PovrayMaterialNode":
                        layout.context_pointer_set("node", node)
                        if hasattr(node, "draw_buttons_ext"):
                            node.draw_buttons_ext(context, layout)
                        elif hasattr(node, "draw_buttons"):
                            node.draw_buttons(context, layout)
                        value_inputs = [socket for socket in node.inputs if socket.enabled and not socket.is_linked]
                        if value_inputs:
                            layout.separator()
                            layout.label("Inputs:")
                            for socket in value_inputs:
                                row = layout.row()
                                socket.draw(context, row, node, socket.name)
                    else:
                        layout.context_pointer_set("node", node)
                        if hasattr(node, "draw_buttons_ext"):
                            node.draw_buttons_ext(context, layout)
                        elif hasattr(node, "draw_buttons"):
                            node.draw_buttons(context, layout)
                        value_inputs = [socket for socket in node.inputs if socket.enabled and not socket.is_linked]
                        if value_inputs:
                            layout.separator()
                            layout.label("Inputs:")
                            for socket in value_inputs:
                                row = layout.row()
                                socket.draw(context, row, node, socket.name)
                else:
                    layout.label("No active nodes!")


class MATERIAL_PT_povray_reflection(MaterialButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Reflection"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        mat=context.material
        ob = context.object
        return mat and mat.type == "SURFACE" and (engine in cls.COMPAT_ENGINES) and not (mat.pov.material_use_nodes or mat.use_nodes)

    def draw(self, context):
        layout = self.layout
        mat = context.material
        col = layout.column()
        col.prop(mat.pov, "irid_enable")
        if mat.pov.irid_enable:
            col = layout.column()
            col.prop(mat.pov, "irid_amount", slider=True)
            col.prop(mat.pov, "irid_thickness", slider=True)
            col.prop(mat.pov, "irid_turbulence", slider=True)
        col.prop(mat.pov, "conserve_energy")
        col2=col.split().column()

        if not mat.raytrace_mirror.use:
            col2.label(text="Please Check Mirror settings :")
        col2.active = mat.raytrace_mirror.use
        col2.prop(mat.pov, "mirror_use_IOR")
        if mat.pov.mirror_use_IOR:
            col2.alignment = 'CENTER'
            col2.label(text="The current Raytrace ")
            col2.label(text="Transparency IOR is: " + str(mat.raytrace_transparency.ior))
        col2.prop(mat.pov, "mirror_metallic")


class MATERIAL_PT_povray_fade_color(MaterialButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Absorption"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        mat=context.material
        ob = context.object
        return mat and mat.type == "SURFACE" and (engine in cls.COMPAT_ENGINES) and not (mat.pov.material_use_nodes or mat.use_nodes)


    def draw_header(self, context):
        mat = context.material

        self.layout.prop(mat.pov, "interior_fade_color", text="")

    def draw(self, context):
        layout = self.layout
        mat = context.material
        # layout.active = mat.pov.interior_fade_color
        if mat.pov.interior_fade_color != (0.0, 0.0, 0.0):
            layout.label(text="Raytrace transparency")
            layout.label(text="depth max Limit needs")
            layout.label(text="to be non zero to fade")

        pass


class MATERIAL_PT_povray_caustics(MaterialButtonsPanel, bpy.types.Panel):
    bl_label = "Caustics"
    COMPAT_ENGINES = {'POVRAY_RENDER'}


    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        mat=context.material
        ob = context.object
        return mat and mat.type == "SURFACE" and (engine in cls.COMPAT_ENGINES) and not (mat.pov.material_use_nodes or mat.use_nodes)


    def draw_header(self, context):
        mat = context.material
        if mat.pov.caustics_enable:
            self.layout.prop(mat.pov, "caustics_enable", text="", icon="PMARKER_SEL" )
        else:
            self.layout.prop(mat.pov, "caustics_enable", text="", icon="PMARKER" )
    def draw(self, context):

        layout = self.layout

        mat = context.material
        layout.active = mat.pov.caustics_enable
        col = layout.column()
        if mat.pov.caustics_enable:
            col.prop(mat.pov, "refraction_caustics")
            if mat.pov.refraction_caustics:

                col.prop(mat.pov, "refraction_type", text="")

                if mat.pov.refraction_type == "1":
                    col.prop(mat.pov, "fake_caustics_power", slider=True)
                elif mat.pov.refraction_type == "2":
                    col.prop(mat.pov, "photons_dispersion", slider=True)
                    col.prop(mat.pov, "photons_dispersion_samples", slider=True)
            col.prop(mat.pov, "photons_reflection")

            if not mat.pov.refraction_caustics and not mat.pov.photons_reflection:
                col = layout.column()
                col.alignment = 'CENTER'
                col.label(text="Caustics override is on, ")
                col.label(text="but you didn't chose any !")


class MATERIAL_PT_povray_replacement_text(MaterialButtonsPanel, bpy.types.Panel):
    bl_label = "Custom POV Code"
    COMPAT_ENGINES = {'POVRAY_RENDER'}


    def draw(self, context):
        layout = self.layout

        mat = context.material

        col = layout.column()
        col.label(text="Replace properties with:")
        col.prop(mat.pov, "replacement_text", text="")

class TEXTURE_PT_povray_type(TextureButtonsPanel, bpy.types.Panel):
    bl_label = "POV-ray Textures"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        split = layout.split(percentage=0.2)
        split.label(text="POV:")
        split.prop(tex.pov, "tex_pattern_type", text="")

class TEXTURE_PT_povray_preview(TextureButtonsPanel, bpy.types.Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        if not hasattr(context, "texture_slot"):
            return False
        tex=context.texture
        mat=context.material
        return (tex and (tex.pov.tex_pattern_type != 'emulator') and (engine in cls.COMPAT_ENGINES))

    def draw(self, context):
        tex = context.texture
        slot = getattr(context, "texture_slot", None)
        idblock = context_tex_datablock(context)
        layout = self.layout
        # if idblock:
            # layout.template_preview(tex, parent=idblock, slot=slot)
        if tex.pov.tex_pattern_type != 'emulator':
            layout.operator("tex.preview_update")
        else:
            layout.template_preview(tex, slot=slot)


class TEXTURE_PT_povray_parameters(TextureButtonsPanel, bpy.types.Panel):
    bl_label = "POV-ray Pattern Options"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    def draw(self, context):
        mat = context.material
        layout = self.layout
        tex = context.texture
        align=True
        if tex is not None and tex.pov.tex_pattern_type != 'emulator':
            if tex.pov.tex_pattern_type == 'agate':
                layout.prop(tex.pov, "modifier_turbulence", text="Agate Turbulence")
            if tex.pov.tex_pattern_type in {'spiral1', 'spiral2'}:
                layout.prop(tex.pov, "modifier_numbers", text="Number of arms")
            if tex.pov.tex_pattern_type == 'tiling':
                layout.prop(tex.pov, "modifier_numbers", text="Pattern number")
            if tex.pov.tex_pattern_type == 'magnet':
                layout.prop(tex.pov, "magnet_style", text="Magnet style")
            if tex.pov.tex_pattern_type == 'quilted':
                row = layout.row(align=align)
                row.prop(tex.pov, "modifier_control0", text="Control0")
                row.prop(tex.pov, "modifier_control1", text="Control1")
            if tex.pov.tex_pattern_type == 'brick':
                col = layout.column(align=align)
                row = col.row()
                row.prop(tex.pov, "brick_size_x", text="Brick size X")
                row.prop(tex.pov, "brick_size_y", text="Brick size Y")
                row=col.row()
                row.prop(tex.pov, "brick_size_z", text="Brick size Z")
                row.prop(tex.pov, "brick_mortar", text="Brick mortar")
            if tex.pov.tex_pattern_type in {'julia','mandel','magnet'}:
                col = layout.column(align=align)
                if tex.pov.tex_pattern_type == 'julia':
                    row = col.row()
                    row.prop(tex.pov, "julia_complex_1", text="Complex 1")
                    row.prop(tex.pov, "julia_complex_2", text="Complex 2")
                if tex.pov.tex_pattern_type == 'magnet' and tex.pov.magnet_style == 'julia':
                    row = col.row()
                    row.prop(tex.pov, "julia_complex_1", text="Complex 1")
                    row.prop(tex.pov, "julia_complex_2", text="Complex 2")
                row=col.row()
                if tex.pov.tex_pattern_type in {'julia','mandel'}:
                    row.prop(tex.pov, "f_exponent", text="Exponent")
                if tex.pov.tex_pattern_type == 'magnet':
                    row.prop(tex.pov, "magnet_type", text="Type")
                row.prop(tex.pov, "f_iter", text="Iterations")
                row=col.row()
                row.prop(tex.pov, "f_ior", text="Interior")
                row.prop(tex.pov, "f_ior_fac", text="Factor I")
                row=col.row()
                row.prop(tex.pov, "f_eor", text="Exterior")
                row.prop(tex.pov, "f_eor_fac", text="Factor E")
            if tex.pov.tex_pattern_type == 'gradient':
                layout.label(text="Gradient orientation:")
                column_flow = layout.column_flow(columns=3, align=True)
                column_flow.prop(tex.pov, "grad_orient_x", text="X")
                column_flow.prop(tex.pov, "grad_orient_y", text="Y")
                column_flow.prop(tex.pov, "grad_orient_z", text="Z")
            if tex.pov.tex_pattern_type == 'pavement':
                layout.prop(tex.pov, "pave_sides", text="Pavement:number of sides")
                col = layout.column(align=align)
                column_flow = col.column_flow(columns=3, align=True)
                column_flow.prop(tex.pov, "pave_tiles", text="Tiles")
                if tex.pov.pave_sides == '4' and tex.pov.pave_tiles == 6:
                    column_flow.prop(tex.pov, "pave_pat_35", text="Pattern")
                if tex.pov.pave_sides == '6' and tex.pov.pave_tiles == 5:
                    column_flow.prop(tex.pov, "pave_pat_22", text="Pattern")
                if tex.pov.pave_sides == '4' and tex.pov.pave_tiles == 5:
                    column_flow.prop(tex.pov, "pave_pat_12", text="Pattern")
                if tex.pov.pave_sides == '3' and tex.pov.pave_tiles == 6:
                    column_flow.prop(tex.pov, "pave_pat_12", text="Pattern")
                if tex.pov.pave_sides == '6' and tex.pov.pave_tiles == 4:
                    column_flow.prop(tex.pov, "pave_pat_7", text="Pattern")
                if tex.pov.pave_sides == '4' and tex.pov.pave_tiles == 4:
                    column_flow.prop(tex.pov, "pave_pat_5", text="Pattern")
                if tex.pov.pave_sides == '3' and tex.pov.pave_tiles == 5:
                    column_flow.prop(tex.pov, "pave_pat_4", text="Pattern")
                if tex.pov.pave_sides == '6' and tex.pov.pave_tiles == 3:
                    column_flow.prop(tex.pov, "pave_pat_3", text="Pattern")
                if tex.pov.pave_sides == '3' and tex.pov.pave_tiles == 4:
                    column_flow.prop(tex.pov, "pave_pat_3", text="Pattern")
                if tex.pov.pave_sides == '4' and tex.pov.pave_tiles == 3:
                    column_flow.prop(tex.pov, "pave_pat_2", text="Pattern")
                if tex.pov.pave_sides == '6' and tex.pov.pave_tiles == 6:
                    column_flow.label(text="!!! 5 tiles!")
                column_flow.prop(tex.pov, "pave_form", text="Form")
            if tex.pov.tex_pattern_type == 'function':
                layout.prop(tex.pov, "func_list", text="Functions")
            if tex.pov.tex_pattern_type == 'function' and tex.pov.func_list != "NONE":
                func = None
                if tex.pov.func_list in {"f_noise3d", "f_ph", "f_r", "f_th"}:
                    func = 0
                if tex.pov.func_list in {"f_comma","f_crossed_trough","f_cubic_saddle",
                                         "f_cushion","f_devils_curve","f_enneper","f_glob",
                                         "f_heart","f_hex_x","f_hex_y","f_hunt_surface",
                                         "f_klein_bottle","f_kummer_surface_v1",
                                         "f_lemniscate_of_gerono","f_mitre","f_nodal_cubic",
                                         "f_noise_generator","f_odd","f_paraboloid","f_pillow",
                                         "f_piriform","f_quantum","f_quartic_paraboloid",
                                         "f_quartic_saddle","f_sphere","f_steiners_roman",
                                         "f_torus_gumdrop","f_umbrella"}:
                    func = 1
                if tex.pov.func_list in {"f_bicorn","f_bifolia","f_boy_surface","f_superellipsoid",
                                         "f_torus"}:
                    func = 2
                if tex.pov.func_list in {"f_ellipsoid","f_folium_surface","f_hyperbolic_torus",
                                         "f_kampyle_of_eudoxus","f_parabolic_torus",
                                         "f_quartic_cylinder","f_torus2"}:
                    func = 3
                if tex.pov.func_list in {"f_blob2","f_cross_ellipsoids","f_flange_cover",
                                         "f_isect_ellipsoids","f_kummer_surface_v2",
                                         "f_ovals_of_cassini","f_rounded_box","f_spikes_2d",
                                         "f_strophoid"}:
                    func = 4
                if tex.pov.func_list in {"f_algbr_cyl1","f_algbr_cyl2","f_algbr_cyl3",
                                         "f_algbr_cyl4","f_blob","f_mesh1","f_poly4","f_spikes"}:
                    func = 5
                if tex.pov.func_list in {"f_devils_curve_2d","f_dupin_cyclid",
                                         "f_folium_surface_2d","f_hetero_mf",
                                         "f_kampyle_of_eudoxus_2d","f_lemniscate_of_gerono_2d",
                                         "f_polytubes","f_ridge","f_ridged_mf","f_spiral",
                                         "f_witch_of_agnesi"}:
                    func = 6
                if tex.pov.func_list in {"f_helix1","f_helix2","f_piriform_2d","f_strophoid_2d"}:
                    func = 7
                if tex.pov.func_list == "f_helical_torus":
                    func = 8
                column_flow = layout.column_flow(columns=3, align=True)
                column_flow.label(text="X")
                column_flow.prop(tex.pov, "func_plus_x", text="")
                column_flow.prop(tex.pov, "func_x", text="Value")
                column_flow = layout.column_flow(columns=3, align=True)
                column_flow.label(text="Y")
                column_flow.prop(tex.pov, "func_plus_y", text="")
                column_flow.prop(tex.pov, "func_y", text="Value")
                column_flow = layout.column_flow(columns=3, align=True)
                column_flow.label(text="Z")
                column_flow.prop(tex.pov, "func_plus_z", text="")
                column_flow.prop(tex.pov, "func_z", text="Value")
                row=layout.row(align=align)
                if func > 0:
                    row.prop(tex.pov, "func_P0", text="P0")
                if func > 1:
                    row.prop(tex.pov, "func_P1", text="P1")
                row=layout.row(align=align)
                if func > 2:
                    row.prop(tex.pov, "func_P2", text="P2")
                if func > 3:
                    row.prop(tex.pov, "func_P3", text="P3")
                row=layout.row(align=align)
                if func > 4:
                    row.prop(tex.pov, "func_P4", text="P4")
                if func > 5:
                    row.prop(tex.pov, "func_P5", text="P5")
                row=layout.row(align=align)
                if func > 6:
                    row.prop(tex.pov, "func_P6", text="P6")
                if func > 7:
                    row.prop(tex.pov, "func_P7", text="P7")
                    row=layout.row(align=align)
                    row.prop(tex.pov, "func_P8", text="P8")
                    row.prop(tex.pov, "func_P9", text="P9")
        ###################################################End Patterns############################


            layout.prop(tex.pov, "warp_types", text="Warp types") #warp
            if tex.pov.warp_types == "TOROIDAL":
                layout.prop(tex.pov, "warp_tor_major_radius", text="Major radius")
            if tex.pov.warp_types not in {"CUBIC","NONE"}:
                layout.prop(tex.pov, "warp_orientation", text="Warp orientation")
            col = layout.column(align=align)
            row = col.row()
            row.prop(tex.pov, "warp_dist_exp", text="Distance exponent")
            row = col.row()
            row.prop(tex.pov, "modifier_frequency", text="Frequency")
            row.prop(tex.pov, "modifier_phase", text="Phase")

            row=layout.row()

            row.label(text="Offset:")
            row.label(text="Scale:")
            row.label(text="Rotate:")
            col=layout.column(align=align)
            row=col.row()
            row.prop(tex.pov, "tex_mov_x", text="X")
            row.prop(tex.pov, "tex_scale_x", text="X")
            row.prop(tex.pov, "tex_rot_x", text="X")
            row=col.row()
            row.prop(tex.pov, "tex_mov_y", text="Y")
            row.prop(tex.pov, "tex_scale_y", text="Y")
            row.prop(tex.pov, "tex_rot_y", text="Y")
            row=col.row()
            row.prop(tex.pov, "tex_mov_z", text="Z")
            row.prop(tex.pov, "tex_scale_z", text="Z")
            row.prop(tex.pov, "tex_rot_z", text="Z")
            row=layout.row()

            row.label(text="Turbulence:")
            col=layout.column(align=align)
            row=col.row()
            row.prop(tex.pov, "warp_turbulence_x", text="X")
            row.prop(tex.pov, "modifier_octaves", text="Octaves")
            row=col.row()
            row.prop(tex.pov, "warp_turbulence_y", text="Y")
            row.prop(tex.pov, "modifier_lambda", text="Lambda")
            row=col.row()
            row.prop(tex.pov, "warp_turbulence_z", text="Z")
            row.prop(tex.pov, "modifier_omega", text="Omega")

class TEXTURE_PT_povray_tex_gamma(TextureButtonsPanel, bpy.types.Panel):
    bl_label = "Image Gamma"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw_header(self, context):
        tex = context.texture

        self.layout.prop(tex.pov, "tex_gamma_enable", text="", icon='SEQ_LUMA_WAVEFORM')

    def draw(self, context):
        layout = self.layout

        tex = context.texture

        layout.active = tex.pov.tex_gamma_enable
        layout.prop(tex.pov, "tex_gamma_value", text="Gamma Value")

#commented out below UI for texture only custom code inside exported material:
# class TEXTURE_PT_povray_replacement_text(TextureButtonsPanel, bpy.types.Panel):
    # bl_label = "Custom POV Code"
    # COMPAT_ENGINES = {'POVRAY_RENDER'}

    # def draw(self, context):
        # layout = self.layout

        # tex = context.texture

        # col = layout.column()
        # col.label(text="Replace properties with:")
        # col.prop(tex.pov, "replacement_text", text="")


class OBJECT_PT_povray_obj_parameters(ObjectButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    
    @classmethod
    def poll(cls, context):

        engine = context.scene.render.engine
        return (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        obj = context.object

        split = layout.split()

        col = split.column(align=True)

        col.label(text="Radiosity:")
        col.prop(obj.pov, "importance_value", text="Importance")
        col.label(text="Photons:")
        col.prop(obj.pov, "collect_photons", text="Receive Photon Caustics")
        if obj.pov.collect_photons:
            col.prop(obj.pov, "spacing_multiplier", text="Photons Spacing Multiplier")
            
        split = layout.split()

        col = split.column()
        col.prop(obj.pov,"hollow")
        col.prop(obj.pov,"double_illuminate")

     
        if obj.type == 'META' or obj.pov.curveshape == 'lathe':
        #if obj.pov.curveshape == 'sor'
            col.prop(obj.pov,"sturm")
        col.prop(obj.pov,"no_shadow")
        col.prop(obj.pov,"no_image")
        col.prop(obj.pov,"no_reflection")
        col.prop(obj.pov,"no_radiosity")
        col.prop(obj.pov,"inverse")
        col.prop(obj.pov,"hierarchy")
        # col.prop(obj.pov,"boundorclip",text="Bound / Clip")
        # if obj.pov.boundorclip != "none":
            # col.prop_search(obj.pov,"boundorclipob",context.blend_data,"objects",text="Object")
            # text = "Clipped by"
            # if obj.pov.boundorclip == "clipped_by":
                # text = "Bounded by"
            # col.prop(obj.pov,"addboundorclip",text=text)

class OBJECT_PT_povray_obj_sphere(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Sphere"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'SPHERE' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'SPHERE':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Sphere radius: " + str(obj.pov.sphere_radius))

            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.sphere_update", text="Update",icon="SOLID")

                #col.label(text="Parameters:")
                col.prop(obj.pov, "sphere_radius", text="Radius of Sphere")


class OBJECT_PT_povray_obj_cylinder(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Cylinder"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'CYLINDER' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'CYLINDER':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Cylinder radius: " + str(obj.pov.cylinder_radius))
                col.label(text="Cylinder cap location: " + str(obj.pov.cylinder_location_cap))

            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.cylinder_update", text="Update",icon="MESH_CYLINDER")

                #col.label(text="Parameters:")
                col.prop(obj.pov, "cylinder_radius")
                col.prop(obj.pov, "cylinder_location_cap")

class OBJECT_PT_povray_obj_cone(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Cone"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'CONE' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'CONE':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Cone base radius: " + str(obj.pov.cone_base_radius))
                col.label(text="Cone cap radius: " + str(obj.pov.cone_cap_radius))
                col.label(text="Cone proxy segments: " + str(obj.pov.cone_segments))
                col.label(text="Cone height: " + str(obj.pov.cone_height))
            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.cone_update", text="Update",icon="MESH_CONE")

                #col.label(text="Parameters:")
                col.prop(obj.pov, "cone_base_radius", text="Radius of Cone Base")
                col.prop(obj.pov, "cone_cap_radius", text="Radius of Cone Cap")
                col.prop(obj.pov, "cone_segments", text="Segmentation of Cone proxy")
                col.prop(obj.pov, "cone_height", text="Height of the cone")

class OBJECT_PT_povray_obj_superellipsoid(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Superquadric ellipsoid"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'SUPERELLIPSOID' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'SUPERELLIPSOID':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Radial segmentation: " + str(obj.pov.se_u))
                col.label(text="Lateral segmentation: " + str(obj.pov.se_v))
                col.label(text="Ring shape: " + str(obj.pov.se_n1))
                col.label(text="Cross-section shape: " + str(obj.pov.se_n2))
                col.label(text="Fill up and down: " + str(obj.pov.se_edit))
            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.superellipsoid_update", text="Update",icon="MOD_SUBSURF")

                #col.label(text="Parameters:")
                col.prop(obj.pov, "se_u")
                col.prop(obj.pov, "se_v")
                col.prop(obj.pov, "se_n1")
                col.prop(obj.pov, "se_n2")
                col.prop(obj.pov, "se_edit")


class OBJECT_PT_povray_obj_torus(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Torus"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'TORUS' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'TORUS':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Torus major radius: " + str(obj.pov.torus_major_radius))
                col.label(text="Torus minor radius: " + str(obj.pov.torus_minor_radius))
                col.label(text="Torus major segments: " + str(obj.pov.torus_major_segments))
                col.label(text="Torus minor segments: " + str(obj.pov.torus_minor_segments))
            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.torus_update", text="Update",icon="MESH_TORUS")

                #col.label(text="Parameters:")
                col.prop(obj.pov, "torus_major_radius")
                col.prop(obj.pov, "torus_minor_radius")
                col.prop(obj.pov, "torus_major_segments")
                col.prop(obj.pov, "torus_minor_segments")

class OBJECT_PT_povray_obj_supertorus(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray SuperTorus"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'SUPERTORUS' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'SUPERTORUS':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="SuperTorus major radius: " + str(obj.pov.st_major_radius))
                col.label(text="SuperTorus minor radius: " + str(obj.pov.st_minor_radius))
                col.label(text="SuperTorus major segments: " + str(obj.pov.st_u))
                col.label(text="SuperTorus minor segments: " + str(obj.pov.st_v))

                col.label(text="SuperTorus Ring Manipulator: " + str(obj.pov.st_ring))
                col.label(text="SuperTorus Cross Manipulator: " + str(obj.pov.st_cross))
                col.label(text="SuperTorus Internal And External radii: " + str(obj.pov.st_ie))

                col.label(text="SuperTorus accuracy: " + str(ob.pov.st_accuracy))
                col.label(text="SuperTorus max gradient: " + str(ob.pov.st_max_gradient))


            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.supertorus_update", text="Update",icon="MESH_TORUS")

                #col.label(text="Parameters:")
                col.prop(obj.pov, "st_major_radius")
                col.prop(obj.pov, "st_minor_radius")
                col.prop(obj.pov, "st_u")
                col.prop(obj.pov, "st_v")
                col.prop(obj.pov, "st_ring")
                col.prop(obj.pov, "st_cross")
                col.prop(obj.pov, "st_ie")
                #col.prop(obj.pov, "st_edit") #?
                col.prop(obj.pov, "st_accuracy")
                col.prop(obj.pov, "st_max_gradient")

class OBJECT_PT_povray_obj_parametric(PovDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Parametric surface"
    COMPAT_ENGINES = {'POVRAY_RENDER'}
    #bl_options = {'HIDE_HEADER'}
    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        obj = context.object
        return (obj and obj.pov.object_as == 'PARAMETRIC' and (engine in cls.COMPAT_ENGINES))
    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()

        if obj.pov.object_as == 'PARAMETRIC':
            if obj.pov.unlock_parameters == False:
                col.prop(obj.pov, "unlock_parameters", text="Exported parameters below", icon='LOCKED')
                col.label(text="Minimum U: " + str(obj.pov.u_min))
                col.label(text="Minimum V: " + str(obj.pov.v_min))
                col.label(text="Maximum U: " + str(obj.pov.u_max))
                col.label(text="Minimum V: " + str(obj.pov.v_min))
                col.label(text="X Function: " + str(obj.pov.x_eq))
                col.label(text="Y Function: " + str(obj.pov.y_eq))
                col.label(text="Z Function: " + str(obj.pov.x_eq))

            else:
                col.prop(obj.pov, "unlock_parameters", text="Edit exported parameters", icon='UNLOCKED')
                col.label(text="3D view proxy may get out of synch")
                col.active = obj.pov.unlock_parameters


                layout.operator("pov.parametric_update", text="Update",icon="SCRIPTPLUGINS")

                col.prop(obj.pov, "u_min", text="Minimum U")
                col.prop(obj.pov, "v_min", text="Minimum V")
                col.prop(obj.pov, "u_max", text="Maximum U")
                col.prop(obj.pov, "v_max", text="Minimum V")
                col.prop(obj.pov, "x_eq", text="X Function")
                col.prop(obj.pov, "y_eq", text="Y Function")
                col.prop(obj.pov, "z_eq", text="Z Function")


class OBJECT_PT_povray_replacement_text(ObjectButtonsPanel, bpy.types.Panel):
    bl_label = "Custom POV Code"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw(self, context):
        layout = self.layout

        obj = context.object

        col = layout.column()
        col.label(text="Replace properties with:")
        col.prop(obj.pov, "replacement_text", text="")

###############################################################################
# Add Povray Objects
###############################################################################


class Povray_primitives_add_menu(bpy.types.Menu):
    """Define the menu with presets"""
    bl_idname = "Povray_primitives_add_menu"
    bl_label = "Povray"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (engine == 'POVRAY_RENDER')

    def draw(self,context):
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.menu(BasicShapesMenu.bl_idname, text = "Primitives",icon="GROUP")
        layout.menu(ImportMenu.bl_idname, text = "Import",icon="IMPORT")

class BasicShapesMenu(bpy.types.Menu):
    bl_idname = "Basic_shapes_calls"
    bl_label = "Basic_shapes"

    def draw(self,context):
        pov = bpy.types.Object.pov #context.object.pov ?
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("pov.addplane", text="Infinite Plane",icon = 'MESH_PLANE')
        layout.operator("pov.addbox", text="Box",icon = 'MESH_CUBE')
        layout.operator("pov.addsphere", text="Sphere",icon = 'SOLID')
        layout.operator("pov.addcylinder", text="Cylinder",icon="MESH_CYLINDER")
        layout.operator("pov.cone_add", text="Cone",icon="MESH_CONE")
        layout.operator("pov.addtorus", text="Torus",icon = 'MESH_TORUS')
        layout.separator()
        layout.operator("pov.addrainbow", text="Rainbow",icon="COLOR")
        layout.operator("pov.addlathe", text="Lathe",icon = 'MOD_SCREW')
        layout.operator("pov.addprism", text="Prism",icon = 'MOD_SOLIDIFY')
        layout.operator("pov.addsuperellipsoid", text="Superquadric Ellipsoid",icon = 'MOD_SUBSURF')
        layout.operator("pov.addheightfield", text="Height Field",icon="RNDCURVE")
        layout.operator("pov.addspheresweep", text="Sphere Sweep",icon = 'FORCE_CURVE')
        layout.separator()
        layout.operator("pov.addblobsphere", text="Blob Sphere",icon = 'META_DATA')
        layout.separator()
        layout.label("Isosurfaces")
        layout.operator("pov.addisosurfacebox", text="Isosurface Box",icon="META_CUBE")
        layout.operator("pov.addisosurfacesphere", text="Isosurface Sphere",icon="META_BALL")
        layout.operator("pov.addsupertorus", text="Supertorus",icon="SURFACE_NTORUS")
        layout.separator()
        layout.label(text = "Macro based")
        layout.operator("pov.addpolygontocircle", text="Polygon To Circle Blending",icon="RETOPO")
        layout.operator("pov.addloft", text="Loft",icon="SURFACE_NSURFACE")
        layout.separator()
        # Warning if the Add Advanced Objects addon containing
        # Add mesh extra objects is not enabled
        if not check_add_mesh_extra_objects():
            #col = box.column()
            layout.label(text="Please enable Add Mesh: Extra Objects addon", icon="INFO")
            #layout.separator()
            layout.operator("wm.addon_userpref_show",
                         text="Go to Add Mesh: Extra Objects addon",
                         icon="PREFERENCES").module = "add_mesh_extra_objects"

            #layout.separator()
            return
        else:
            layout.operator("pov.addparametric", text="Parametric",icon = 'SCRIPTPLUGINS')

class ImportMenu(bpy.types.Menu):
    bl_idname = "Importer_calls"
    bl_label = "Import"

    def draw(self,context):
        pov = bpy.types.Object.pov #context.object.pov ?
        layout = self.layout
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("import_scene.pov",icon="FORCE_LENNARDJONES")

def menu_func_add(self, context):
    engine = context.scene.render.engine
    if engine == 'POVRAY_RENDER':
        self.layout.menu("Povray_primitives_add_menu", icon="PLUGIN")

def menu_func_import(self, context):
    engine = context.scene.render.engine
    if engine == 'POVRAY_RENDER':
        self.layout.operator("import_scene.pov",icon="FORCE_LENNARDJONES")


##############Nodes

# def find_node_input(node, name):
    # for input in node.inputs:
        # if input.name == name:
            # return input

# def panel_node_draw(layout, id_data, output_type, input_name):
    # if not id_data.use_nodes:
        # #layout.operator("pov.material_use_nodes", icon='SOUND')#'NODETREE')
        # #layout.operator("pov.use_shading_nodes", icon='NODETREE')
        # layout.operator("WM_OT_context_toggle", icon='NODETREE').data_path = \
                        # "material.pov.material_use_nodes"
        # return False

    # ntree = id_data.node_tree

    # node = find_node(id_data, output_type)
    # if not node:
        # layout.label(text="No output node")
    # else:
        # input = find_node_input(node, input_name)
        # layout.template_node_view(ntree, node, input)

    # return True

class Node_map_create_menu(bpy.types.Menu):
    """Create maps"""
    bl_idname = "Node_map_create_menu"
    bl_label = "Create map"

    def draw(self,context):
        layout = self.layout
        layout.operator("node.map_create")

def menu_func_nodes(self, context):
    ob = context.object
    if hasattr(ob,'active_material'):
        mat=context.object.active_material
        if mat and context.space_data.tree_type == 'ObjectNodeTree':
            self.layout.prop(mat.pov,"material_use_nodes")
            self.layout.menu("Node_map_create_menu")
            self.layout.operator("wm.updatepreviewkey")
        if hasattr(mat,'active_texture') and context.scene.render.engine == 'POVRAY_RENDER':
            tex=mat.active_texture
            if tex and context.space_data.tree_type == 'TextureNodeTree':
                self.layout.prop(tex.pov,"texture_use_nodes")


###############################################################################
# Camera Povray Settings
###############################################################################
class CAMERA_PT_povray_cam_dof(CameraDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Depth Of Field"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw_header(self, context):
        cam = context.camera

        self.layout.prop(cam.pov, "dof_enable", text="")

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        layout.active = cam.pov.dof_enable

        layout.prop(cam.pov, "dof_aperture")

        split = layout.split()

        col = split.column()
        col.prop(cam.pov, "dof_samples_min")
        col.prop(cam.pov, "dof_variance")

        col = split.column()
        col.prop(cam.pov, "dof_samples_max")
        col.prop(cam.pov, "dof_confidence")



class CAMERA_PT_povray_cam_nor(CameraDataButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray Perturbation"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw_header(self, context):
        cam = context.camera

        self.layout.prop(cam.pov, "normal_enable", text="")

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        layout.active = cam.pov.normal_enable

        layout.prop(cam.pov,"normal_patterns")
        layout.prop(cam.pov,"cam_normal")
        layout.prop(cam.pov,"turbulence")
        layout.prop(cam.pov,"scale")


class CAMERA_PT_povray_replacement_text(CameraDataButtonsPanel, bpy.types.Panel):
    bl_label = "Custom POV Code"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw(self, context):
        layout = self.layout

        cam = context.camera

        col = layout.column()
        col.label(text="Replace properties with:")
        col.prop(cam.pov, "replacement_text", text="")

###############################################################################
# Text Povray Settings
###############################################################################

class TEXT_OT_povray_insert(bpy.types.Operator):
    """Tooltip"""
    bl_idname = "text.povray_insert"
    bl_label = "Insert"

    filepath = bpy.props.StringProperty(name="Filepath", subtype='FILE_PATH')

    @classmethod
    def poll(cls, context):
        # context.area.type == 'TEXT_EDITOR'
        return bpy.ops.text.insert.poll()

    def execute(self, context):
        if self.filepath and isfile(self.filepath):
            file = open(self.filepath, "r")
            bpy.ops.text.insert(text=file.read())

            # places the cursor at the end without scrolling -.-
            # context.space_data.text.write(file.read())
            file.close()
        return {'FINISHED'}

def validinsert(ext):
	return ext in {".txt",".inc",".pov"}

class TEXT_MT_insert(bpy.types.Menu):
    bl_label = "Insert"
    bl_idname = "TEXT_MT_insert"

    def draw(self, context):
        pov_documents = locate_docpath()
        prop = self.layout.operator("wm.path_open", text="Open folder", icon='FILE_FOLDER')
        prop.filepath = pov_documents
        self.layout.separator()

        list=[]
        for root,dirs,files in os.walk(pov_documents):
            list.append(root)
        print(list)
        self.path_menu(list,
                       "text.povray_insert",
                       #{"internal": True},
					   filter_ext= validinsert
                       )

class TEXT_PT_povray_custom_code(TextButtonsPanel, bpy.types.Panel):
    bl_label = "POV-Ray"
    COMPAT_ENGINES = {'POVRAY_RENDER'}

    def draw(self, context):
        layout = self.layout

        text = context.space_data.text
            
        pov_documents = locate_docpath()
        if not pov_documents :
            layout.label(text="Please configure ", icon="INFO")
            layout.label(text="default pov include path ")
            layout.label(text="in addon preferences")
            #layout.separator()
            layout.operator("wm.addon_userpref_show",
                         text="Go to Render: POV-Ray addon",
                         icon="PREFERENCES").module = "render_povray"

            #layout.separator()
        else:
            #print(pov_documents)
            layout.menu(TEXT_MT_insert.bl_idname)

        if text:
            box = layout.box()
            box.label('Source to render:', icon='RENDER_STILL')
            row = box.row()
            row.prop(text.pov, "custom_code",expand = True)
            if text.pov.custom_code in {'3dview'}:
                box.operator("render.render", icon='OUTLINER_DATA_POSE')  
            if text.pov.custom_code in {'text'}:
                rtext = bpy.context.space_data.text
                box.operator("text.run", icon='POSE_DATA')
            #layout.prop(text.pov, "custom_code")
            elif text.pov.custom_code in {'both'}:
                box.operator("render.render", icon='POSE_HLT')
                layout.label(text="Please specify declared", icon="INFO")
                layout.label(text="items in properties ")
                #layout.label(text="")                
                layout.label(text="replacement fields")


###############################################
# Text editor templates from header menu

class TEXT_MT_templates_pov(bpy.types.Menu):
    bl_label = "POV-Ray"

    # We list templates on file evaluation, we can assume they are static data,
    # and better avoid running this on every draw call.
    import os
    template_paths = [os.path.join(os.path.dirname(__file__), "templates_pov")]

    def draw(self, context):
        self.path_menu(
            self.template_paths,
            "text.open",
            props_default={"internal": True},
        )

def menu_func_templates(self, context):
    # Do not depend on POV-Ray being active renderer here...
    self.layout.menu("TEXT_MT_templates_pov")
