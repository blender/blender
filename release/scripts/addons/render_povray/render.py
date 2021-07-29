# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# #**** END GPL LICENSE BLOCK #****

# <pep8 compliant>

import bpy
import subprocess
import os
import sys
import time
from math import atan, pi, degrees, sqrt, cos, sin
import re
import random
import platform#
import subprocess#
import tempfile #generate temporary files with random names
from bpy.types import(Operator)
from imghdr import what #imghdr is a python lib to identify image file types

from . import df3 # for smoke rendering
from . import shading # for BI POV haders emulation
from . import primitives # for import and export of POV specific primitives
from . import nodes # for POV specific nodes
##############################SF###########################
##############find image texture
def imageFormat(imgF):
    ext = {
        'JPG': "jpeg",
        'JPEG': "jpeg",
        'GIF': "gif",
        'TGA': "tga",
        'IFF': "iff",
        'PPM': "ppm",
        'PNG': "png",
        'SYS': "sys",
        'TIFF': "tiff",
        'TIF': "tiff",
        'EXR': "exr",
        'HDR': "hdr",
    }.get(os.path.splitext(imgF)[-1].upper(), "")

    if not ext:
        #maybe add a check for if path exists here?
        print(" WARNING: texture image has no extension") #too verbose

        ext = what(imgF) #imghdr is a python lib to identify image file types
    return ext


def imgMap(ts):
    image_map = ""
    if ts.mapping == 'FLAT':
        image_map = "map_type 0 "
    elif ts.mapping == 'SPHERE':
        image_map = "map_type 1 "
    elif ts.mapping == 'TUBE':
        image_map = "map_type 2 "

    ## map_type 3 and 4 in development (?)
    ## for POV-Ray, currently they just seem to default back to Flat (type 0)
    #elif ts.mapping=="?":
    #    image_map = " map_type 3 "
    #elif ts.mapping=="?":
    #    image_map = " map_type 4 "
    if ts.texture.use_interpolation:
        image_map += " interpolate 2 "
    if ts.texture.extension == 'CLIP':
        image_map += " once "
    #image_map += "}"
    #if ts.mapping=='CUBE':
    #    image_map+= "warp { cubic } rotate <-90,0,180>"
    # no direct cube type mapping. Though this should work in POV 3.7
    # it doesn't give that good results(best suited to environment maps?)
    #if image_map == "":
    #    print(" No texture image  found ")
    return image_map


def imgMapTransforms(ts):
    # XXX TODO: unchecked textures give error of variable referenced before assignment XXX
    # POV-Ray "scale" is not a number of repetitions factor, but ,its
    # inverse, a standard scale factor.
    # 0.5 Offset is needed relatively to scale because center of the
    # scale is 0.5,0.5 in blender and 0,0 in POV
            # Strange that the translation factor for scale is not the same as for
            # translate.
            # TODO: verify both matches with blender internal.
    image_map_transforms = ""
    image_map_transforms = ("scale <%.4g,%.4g,%.4g> translate <%.4g,%.4g,%.4g>" % \
                  ( 1.0 / ts.scale.x,
                  1.0 / ts.scale.y,
                  1.0 / ts.scale.z,
                  0.5-(0.5/ts.scale.x) - (ts.offset.x),
                  0.5-(0.5/ts.scale.y) - (ts.offset.y),
                  ts.offset.z))
    # image_map_transforms = (" translate <-0.5,-0.5,0.0> scale <%.4g,%.4g,%.4g> translate <%.4g,%.4g,%.4g>" % \
                  # ( 1.0 / ts.scale.x,
                  # 1.0 / ts.scale.y,
                  # 1.0 / ts.scale.z,
                  # (0.5 / ts.scale.x) + ts.offset.x,
                  # (0.5 / ts.scale.y) + ts.offset.y,
                  # ts.offset.z))
    # image_map_transforms = ("translate <-0.5,-0.5,0> scale <-1,-1,1> * <%.4g,%.4g,%.4g> translate <0.5,0.5,0> + <%.4g,%.4g,%.4g>" % \
                  # (1.0 / ts.scale.x,
                  # 1.0 / ts.scale.y,
                  # 1.0 / ts.scale.z,
                  # ts.offset.x,
                  # ts.offset.y,
                  # ts.offset.z))
    return image_map_transforms

def imgMapBG(wts):
    image_mapBG = ""
    # texture_coords refers to the mapping of world textures:
    if wts.texture_coords == 'VIEW' or wts.texture_coords == 'GLOBAL':
        image_mapBG = " map_type 0 "
    elif wts.texture_coords == 'ANGMAP':
        image_mapBG = " map_type 1 "
    elif wts.texture_coords == 'TUBE':
        image_mapBG = " map_type 2 "

    if wts.texture.use_interpolation:
        image_mapBG += " interpolate 2 "
    if wts.texture.extension == 'CLIP':
        image_mapBG += " once "
    #image_mapBG += "}"
    #if wts.mapping == 'CUBE':
    #   image_mapBG += "warp { cubic } rotate <-90,0,180>"
    # no direct cube type mapping. Though this should work in POV 3.7
    # it doesn't give that good results(best suited to environment maps?)
    #if image_mapBG == "":
    #    print(" No background texture image  found ")
    return image_mapBG


def path_image(image):
    return bpy.path.abspath(image.filepath, library=image.library).replace("\\","/")
    # .replace("\\","/") to get only forward slashes as it's what POV prefers,
    # even on windows
# end find image texture
# -----------------------------------------------------------------------------


def string_strip_hyphen(name):
    return name.replace("-", "")


def safety(name, Level):
    # safety string name material
    #
    # Level=1 is for texture with No specular nor Mirror reflection
    # Level=2 is for texture with translation of spec and mir levels
    # for when no map influences them
    # Level=3 is for texture with Maximum Spec and Mirror

    try:
        if int(name) > 0:
            prefix = "shader"
    except:
        prefix = ""
    prefix = "shader_"
    name = string_strip_hyphen(name)
    if Level == 2:
        return prefix + name
    elif Level == 1:
        return prefix + name + "0"  # used for 0 of specular map
    elif Level == 3:
        return prefix + name + "1"  # used for 1 of specular map


##############end safety string name material
##############################EndSF###########################

def is_renderable(scene, ob):
    return (ob.is_visible(scene) and not ob.hide_render)


def renderable_objects(scene):
    return [ob for ob in bpy.data.objects if is_renderable(scene, ob)]


tabLevel = 0
unpacked_images=[]

user_dir = bpy.utils.resource_path('USER')
preview_dir = os.path.join(user_dir, "preview")

## Make sure Preview directory exists and is empty
smokePath = os.path.join(preview_dir, "smoke.df3")
'''
def write_global_setting(scene,file):
    file.write("global_settings {\n")
    file.write("    assumed_gamma %.6f\n"%scene.pov.assumed_gamma)
    if scene.pov.global_settings_advanced:
        if scene.pov.radio_enable == False:
            file.write("    adc_bailout %.6f\n"%scene.pov.adc_bailout)
        file.write("    ambient_light <%.6f,%.6f,%.6f>\n"%scene.pov.ambient_light[:])
        file.write("    irid_wavelength <%.6f,%.6f,%.6f>\n"%scene.pov.irid_wavelength[:])
        file.write("    charset %s\n"%scene.pov.charset)
        file.write("    max_trace_level %s\n"%scene.pov.max_trace_level)
        file.write("    max_intersections %s\n"%scene.pov.max_intersections)
        file.write("    number_of_waves %s\n"%scene.pov.number_of_waves)
        file.write("    noise_generator %s\n"%scene.pov.noise_generator)

    # below properties not added to __init__ yet to avoid conflicts with material sss scale
    # unless it would override then should be interfaced also in scene units property tab

    # if scene.pov.sslt_enable:
        # file.write("    mm_per_unit %s\n"%scene.pov.mm_per_unit)
        # file.write("    subsurface {\n")
        # file.write("        samples %s, %s\n"%(scene.pov.sslt_samples_max,scene.pov.sslt_samples_min))
        # if scene.pov.sslt_radiosity:
            # file.write("        radiosity on\n")
        # file.write("}\n")

    if scene.pov.radio_enable:
        file.write("    radiosity {\n")
        file.write("        pretrace_start %.6f\n"%scene.pov.radio_pretrace_start)
        file.write("        pretrace_end %.6f\n"%scene.pov.radio_pretrace_end)
        file.write("        count %s\n"%scene.pov.radio_count)
        file.write("        nearest_count %s\n"%scene.pov.radio_nearest_count)
        file.write("        error_bound %.6f\n"%scene.pov.radio_error_bound)
        file.write("        recursion_limit %s\n"%scene.pov.radio_recursion_limit)
        file.write("        low_error_factor %.6f\n"%scene.pov.radio_low_error_factor)
        file.write("        gray_threshold %.6f\n"%scene.pov.radio_gray_threshold)
        file.write("        maximum_reuse %.6f\n"%scene.pov.radio_maximum_reuse)
        file.write("        minimum_reuse %.6f\n"%scene.pov.radio_minimum_reuse)
        file.write("        brightness %.6f\n"%scene.pov.radio_brightness)
        file.write("        adc_bailout %.6f\n"%scene.pov.radio_adc_bailout)
        if scene.pov.radio_normal:
            file.write("        normal on\n")
        if scene.pov.radio_always_sample:
            file.write("        always_sample on\n")
        if scene.pov.radio_media:
            file.write("        media on\n")
        if scene.pov.radio_subsurface:
            file.write("        subsurface on\n")
        file.write("    }\n")

    if scene.pov.photon_enable:
        file.write("    photons {\n")
        if scene.pov.photon_enable_count:
            file.write("        count %s\n"%scene.pov.photon_count)
        else:
            file.write("        spacing %.6g\n"%scene.pov.photon_spacing)
        if scene.pov.photon_gather:
            file.write("        gather %s, %s\n"%(scene.pov.photon_gather_min,scene.pov.photon_gather_max))
        if scene.pov.photon_autostop:
            file.write("        autostop %.4g\n"%scene.pov.photon_autostop_value)
        if scene.pov.photon_jitter_enable:
            file.write("        jitter %.4g\n"%scene.pov.photon_jitter)
        file.write("        max_trace_level %s\n"%scene.pov.photon_max_trace_level)
        if scene.pov.photon_adc:
            file.write("        adc_bailout %.6f\n"%scene.pov.photon_adc_bailout)
        if scene.pov.photon_media_enable:
            file.write("        media %s, %s\n"%(scene.pov.photon_media_steps,scene.pov.photon_media_factor))
        if scene.pov.photon_map_file_save_load in {'save'}:
            filePhName = 'Photon_map_file.ph'
            if scene.pov.photon_map_file != '':
                filePhName = scene.pov.photon_map_file+'.ph'
            filePhDir = tempfile.gettempdir()
            path = bpy.path.abspath(scene.pov.photon_map_dir)
            if os.path.exists(path):
                filePhDir = path
            fullFileName = os.path.join(filePhDir,filePhName)
            file.write('        save_file "%s"\n'%fullFileName)
            scene.pov.photon_map_file = fullFileName
        if scene.pov.photon_map_file_save_load in {'load'}:
            fullFileName = bpy.path.abspath(scene.pov.photon_map_file)
            if os.path.exists(fullFileName):
                file.write('        load_file "%s"\n'%fullFileName)
        file.write("}\n")
    file.write("}\n")
'''
def write_object_modifiers(scene,ob,File):
    '''XXX WIP
    onceCSG = 0
    for mod in ob.modifiers:
        if onceCSG == 0:
            if mod :
                if mod.type == 'BOOLEAN':
                    if ob.pov.boolean_mod == "POV":
                        File.write("\tinside_vector <%.6g, %.6g, %.6g>\n" %
                                   (ob.pov.inside_vector[0],
                                    ob.pov.inside_vector[1],
                                    ob.pov.inside_vector[2]))
                        onceCSG = 1
    '''

    if ob.pov.hollow:
        File.write("\thollow\n")
    if ob.pov.double_illuminate:
        File.write("\tdouble_illuminate\n")
    if ob.pov.sturm:
        File.write("\tsturm\n")
    if ob.pov.no_shadow:
        File.write("\tno_shadow\n")
    if ob.pov.no_image:
        File.write("\tno_image\n")
    if ob.pov.no_reflection:
        File.write("\tno_reflection\n")
    if ob.pov.no_radiosity:
        File.write("\tno_radiosity\n")
    if ob.pov.inverse:
        File.write("\tinverse\n")
    if ob.pov.hierarchy:
        File.write("\thierarchy\n")

    # XXX, Commented definitions
    '''
    if scene.pov.photon_enable:
        File.write("photons {\n")
        if ob.pov.target:
            File.write("target %.4g\n"%ob.pov.target_value)
        if ob.pov.refraction:
            File.write("refraction on\n")
        if ob.pov.reflection:
            File.write("reflection on\n")
        if ob.pov.pass_through:
            File.write("pass_through\n")
        File.write("}\n")
    if ob.pov.object_ior > 1:
        File.write("interior {\n")
        File.write("ior %.4g\n"%ob.pov.object_ior)
        if scene.pov.photon_enable and ob.pov.target and ob.pov.refraction and ob.pov.dispersion:
            File.write("ior %.4g\n"%ob.pov.dispersion_value)
            File.write("ior %s\n"%ob.pov.dispersion_samples)
        if scene.pov.photon_enable == False:
            File.write("caustics %.4g\n"%ob.pov.fake_caustics_power)
    '''



def write_pov(filename, scene=None, info_callback=None):
    import mathutils
    #file = filename
    file = open(filename, "w")

    # Only for testing
    if not scene:
        scene = bpy.data.scenes[0]

    render = scene.render
    world = scene.world
    global_matrix = mathutils.Matrix.Rotation(-pi / 2.0, 4, 'X')
    comments = scene.pov.comments_enable and not scene.pov.tempfiles_enable
    linebreaksinlists = scene.pov.list_lf_enable and not scene.pov.tempfiles_enable
    feature_set = bpy.context.user_preferences.addons[__package__].preferences.branch_feature_set_povray
    using_uberpov = (feature_set=='uberpov')
    pov_binary = PovrayRender._locate_binary()

    if using_uberpov:
        print("Unofficial UberPOV feature set chosen in preferences")
    else:
        print("Official POV-Ray 3.7 feature set chosen in preferences")
    if 'uber' in pov_binary:
        print("The name of the binary suggests you are probably rendering with Uber POV engine")
    else:
        print("The name of the binary suggests you are probably rendering with standard POV engine")
    def setTab(tabtype, spaces):
        TabStr = ""
        if tabtype == 'NONE':
            TabStr = ""
        elif tabtype == 'TAB':
            TabStr = "\t"
        elif tabtype == 'SPACE':
            TabStr = spaces * " "
        return TabStr

    tab = setTab(scene.pov.indentation_character, scene.pov.indentation_spaces)
    if not scene.pov.tempfiles_enable:
        def tabWrite(str_o):
            global tabLevel
            brackets = str_o.count("{") - str_o.count("}") + str_o.count("[") - str_o.count("]")
            if brackets < 0:
                tabLevel = tabLevel + brackets
            if tabLevel < 0:
                print("Indentation Warning: tabLevel = %s" % tabLevel)
                tabLevel = 0
            if tabLevel >= 1:
                file.write("%s" % tab * tabLevel)
            file.write(str_o)
            if brackets > 0:
                tabLevel = tabLevel + brackets
    else:
        def tabWrite(str_o):
            file.write(str_o)

    def uniqueName(name, nameSeq):

        if name not in nameSeq:
            name = string_strip_hyphen(name)
            return name

        name_orig = name
        i = 1
        while name in nameSeq:
            name = "%s_%.3d" % (name_orig, i)
            i += 1
        name = string_strip_hyphen(name)
        return name

    def writeMatrix(matrix):
        tabWrite("matrix <%.6f, %.6f, %.6f,  %.6f, %.6f, %.6f,  %.6f, %.6f, %.6f,  %.6f, %.6f, %.6f>\n" %
                 (matrix[0][0], matrix[1][0], matrix[2][0],
                  matrix[0][1], matrix[1][1], matrix[2][1],
                  matrix[0][2], matrix[1][2], matrix[2][2],
                  matrix[0][3], matrix[1][3], matrix[2][3]))

    def MatrixAsPovString(matrix):
        sMatrix = ("matrix <%.6f, %.6f, %.6f,  %.6f, %.6f, %.6f,  %.6f, %.6f, %.6f,  %.6f, %.6f, %.6f>\n" %
                   (matrix[0][0], matrix[1][0], matrix[2][0],
                    matrix[0][1], matrix[1][1], matrix[2][1],
                    matrix[0][2], matrix[1][2], matrix[2][2],
                    matrix[0][3], matrix[1][3], matrix[2][3]))
        return sMatrix

    def writeObjectMaterial(material, ob):

        # DH - modified some variables to be function local, avoiding RNA write
        # this should be checked to see if it is functionally correct

        # Commented out: always write IOR to be able to use it for SSS, Fresnel reflections...
        #if material and material.transparency_method == 'RAYTRACE':
        if material:
            # But there can be only one!
            if material.subsurface_scattering.use:  # SSS IOR get highest priority
                tabWrite("interior {\n")
                tabWrite("ior %.6f\n" % material.subsurface_scattering.ior)
            # Then the raytrace IOR taken from raytrace transparency properties and used for
            # reflections if IOR Mirror option is checked.
            elif material.pov.mirror_use_IOR:
                tabWrite("interior {\n")
                tabWrite("ior %.6f\n" % material.raytrace_transparency.ior)
            else:
                tabWrite("interior {\n")
                tabWrite("ior %.6f\n" % material.raytrace_transparency.ior)

            pov_fake_caustics = False
            pov_photons_refraction = False
            pov_photons_reflection = False

            if material.pov.photons_reflection:
                pov_photons_reflection = True
            if not material.pov.refraction_caustics:
                pov_fake_caustics = False
                pov_photons_refraction = False
            elif material.pov.refraction_type == "1":
                pov_fake_caustics = True
                pov_photons_refraction = False
            elif material.pov.refraction_type == "2":
                pov_fake_caustics = False
                pov_photons_refraction = True

            # If only Raytrace transparency is set, its IOR will be used for refraction, but user
            # can set up 'un-physical' fresnel reflections in raytrace mirror parameters.
            # Last, if none of the above is specified, user can set up 'un-physical' fresnel
            # reflections in raytrace mirror parameters. And pov IOR defaults to 1.
            if material.pov.caustics_enable:
                if pov_fake_caustics:
                    tabWrite("caustics %.3g\n" % material.pov.fake_caustics_power)
                if pov_photons_refraction:
                    # Default of 1 means no dispersion
                    tabWrite("dispersion %.6f\n" % material.pov.photons_dispersion)
                    tabWrite("dispersion_samples %.d\n" % material.pov.photons_dispersion_samples)
            #TODO
            # Other interior args
            if material.use_transparency and material.transparency_method == 'RAYTRACE':
                # fade_distance
                # In Blender this value has always been reversed compared to what tooltip says.
                # 100.001 rather than 100 so that it does not get to 0
                # which deactivates the feature in POV
                tabWrite("fade_distance %.3g\n" % \
                         (100.001 - material.raytrace_transparency.depth_max))
                # fade_power
                tabWrite("fade_power %.3g\n" % material.raytrace_transparency.falloff)
                # fade_color
                tabWrite("fade_color <%.3g, %.3g, %.3g>\n" % material.pov.interior_fade_color[:])

            # (variable) dispersion_samples (constant count for now)
            tabWrite("}\n")
            if material.pov.photons_reflection or material.pov.refraction_type=="2":
                tabWrite("photons{")
                tabWrite("target %.3g\n" % ob.pov.spacing_multiplier)
                if not ob.pov.collect_photons:
                    tabWrite("collect off\n")
                if pov_photons_refraction:
                    tabWrite("refraction on\n")
                if pov_photons_reflection:
                    tabWrite("reflection on\n")
                tabWrite("}\n")

    materialNames = {}
    DEF_MAT_NAME = "" #or "Default"?

    def exportCamera():
        camera = scene.camera

        # DH disabled for now, this isn't the correct context
        active_object = None  # bpy.context.active_object # does not always work  MR
        matrix = global_matrix * camera.matrix_world
        focal_point = camera.data.dof_distance

        # compute resolution
        Qsize = render.resolution_x / render.resolution_y
        tabWrite("#declare camLocation  = <%.6f, %.6f, %.6f>;\n" %
                 matrix.translation[:])
        tabWrite("#declare camLookAt = <%.6f, %.6f, %.6f>;\n" %
                 tuple([degrees(e) for e in matrix.to_3x3().to_euler()]))

        tabWrite("camera {\n")
        if scene.pov.baking_enable and active_object and active_object.type == 'MESH':
            tabWrite("mesh_camera{ 1 3\n")  # distribution 3 is what we want here
            tabWrite("mesh{%s}\n" % active_object.name)
            tabWrite("}\n")
            tabWrite("location <0,0,.01>")
            tabWrite("direction <0,0,-1>")
        # Using standard camera otherwise
        else:
            tabWrite("location  <0, 0, 0>\n")
            tabWrite("look_at  <0, 0, -1>\n")
            tabWrite("right <%s, 0, 0>\n" % - Qsize)
            tabWrite("up <0, 1, 0>\n")
            tabWrite("angle  %f\n" % (360.0 * atan(16.0 / camera.data.lens) / pi))

            tabWrite("rotate  <%.6f, %.6f, %.6f>\n" % \
                     tuple([degrees(e) for e in matrix.to_3x3().to_euler()]))
            tabWrite("translate <%.6f, %.6f, %.6f>\n" % matrix.translation[:])
            if camera.data.pov.dof_enable and (focal_point != 0 or camera.data.dof_object):
                tabWrite("aperture %.3g\n" % camera.data.pov.dof_aperture)
                tabWrite("blur_samples %d %d\n" % \
                         (camera.data.pov.dof_samples_min, camera.data.pov.dof_samples_max))
                tabWrite("variance 1/%d\n" % camera.data.pov.dof_variance)
                tabWrite("confidence %.3g\n" % camera.data.pov.dof_confidence)
                if camera.data.dof_object:
                    focalOb = scene.objects[camera.data.dof_object.name]
                    matrixBlur = global_matrix * focalOb.matrix_world
                    tabWrite("focal_point <%.4f,%.4f,%.4f>\n"% matrixBlur.translation[:])
                else:
                    tabWrite("focal_point <0, 0, %f>\n" % focal_point)
        if camera.data.pov.normal_enable:
            tabWrite("normal {%s %.4f turbulence %.4f scale %.4f}\n"%
                    (camera.data.pov.normal_patterns,
                    camera.data.pov.cam_normal,
                    camera.data.pov.turbulence,
                    camera.data.pov.scale))
        tabWrite("}\n")



    def exportLamps(lamps):
        # Incremented after each lamp export to declare its target
        # currently used for Fresnel diffuse shader as their slope vector:
        global lampCount
        lampCount = 0
        # Get all lamps
        for ob in lamps:
            lamp = ob.data

            matrix = global_matrix * ob.matrix_world

            # Color is modified by energy #muiltiplie by 2 for a better match --Maurice
            color = tuple([c * (lamp.energy) for c in lamp.color])

            tabWrite("light_source {\n")
            tabWrite("< 0,0,0 >\n")
            tabWrite("color srgb<%.3g, %.3g, %.3g>\n" % color)

            if lamp.type == 'POINT':
                pass
            elif lamp.type == 'SPOT':
                tabWrite("spotlight\n")

                # Falloff is the main radius from the centre line
                tabWrite("falloff %.2f\n" % (degrees(lamp.spot_size) / 2.0))  # 1 TO 179 FOR BOTH
                tabWrite("radius %.6f\n" % \
                         ((degrees(lamp.spot_size) / 2.0) * (1.0 - lamp.spot_blend)))

                # Blender does not have a tightness equivilent, 0 is most like blender default.
                tabWrite("tightness 0\n")  # 0:10f

                tabWrite("point_at  <0, 0, -1>\n")
                if lamp.use_halo:
                    tabWrite("looks_like{\n")
                    tabWrite("sphere{<0,0,0>,%.6f\n" %lamp.distance)
                    tabWrite("hollow\n")
                    tabWrite("material{\n")
                    tabWrite("texture{\n")
                    tabWrite("pigment{rgbf<1,1,1,%.4f>}\n" % (lamp.halo_intensity*5.0))
                    tabWrite("}\n")
                    tabWrite("interior{\n")
                    tabWrite("media{\n")
                    tabWrite("emission 1\n")
                    tabWrite("scattering {1, 0.5}\n")
                    tabWrite("density{\n")
                    tabWrite("spherical\n")
                    tabWrite("color_map{\n")
                    tabWrite("[0.0 rgb <0,0,0>]\n")
                    tabWrite("[0.5 rgb <1,1,1>]\n")
                    tabWrite("[1.0 rgb <1,1,1>]\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
            elif lamp.type == 'SUN':
                tabWrite("parallel\n")
                tabWrite("point_at  <0, 0, -1>\n")  # *must* be after 'parallel'

            elif lamp.type == 'AREA':
                tabWrite("fade_distance %.6f\n" % (lamp.distance / 2.0))
                # Area lights have no falloff type, so always use blenders lamp quad equivalent
                # for those?
                tabWrite("fade_power %d\n" % 0)
                size_x = lamp.size
                samples_x = lamp.shadow_ray_samples_x
                if lamp.shape == 'SQUARE':
                    size_y = size_x
                    samples_y = samples_x
                else:
                    size_y = lamp.size_y
                    samples_y = lamp.shadow_ray_samples_y

                tabWrite("area_light <%.6f,0,0>,<0,%.6f,0> %d, %d\n" % \
                         (size_x, size_y, samples_x, samples_y))
                tabWrite("area_illumination\n")
                if lamp.shadow_ray_sample_method == 'CONSTANT_JITTERED':
                    if lamp.use_jitter:
                        tabWrite("jitter\n")
                else:
                    tabWrite("adaptive 1\n")
                    tabWrite("jitter\n")

            # HEMI never has any shadow_method attribute
            if(not scene.render.use_shadows or lamp.type == 'HEMI' or
               (lamp.type != 'HEMI' and lamp.shadow_method == 'NOSHADOW')):
                tabWrite("shadowless\n")

            # Sun shouldn't be attenuated. Hemi and area lights have no falloff attribute so they
            # are put to type 2 attenuation a little higher above.
            if lamp.type not in {'SUN', 'AREA', 'HEMI'}:
                if lamp.falloff_type == 'INVERSE_SQUARE':
                    tabWrite("fade_distance %.6f\n" % (sqrt(lamp.distance/2.0)))
                    tabWrite("fade_power %d\n" % 2)  # Use blenders lamp quad equivalent
                elif lamp.falloff_type == 'INVERSE_LINEAR':
                    tabWrite("fade_distance %.6f\n" % (lamp.distance / 2.0))                
                    tabWrite("fade_power %d\n" % 1)  # Use blenders lamp linear
                elif lamp.falloff_type == 'CONSTANT':
                    tabWrite("fade_distance %.6f\n" % (lamp.distance / 2.0))
                    tabWrite("fade_power %d\n" % 3)  
                    # Use blenders lamp constant equivalent no attenuation.
                # Using Custom curve for fade power 3 for now.
                elif lamp.falloff_type == 'CUSTOM_CURVE':
                    tabWrite("fade_power %d\n" % 4)

            writeMatrix(matrix)

            tabWrite("}\n")

            lampCount += 1

            # v(A,B) rotates vector A about origin by vector B.
            file.write("#declare lampTarget%s= vrotate(<%.4g,%.4g,%.4g>,<%.4g,%.4g,%.4g>);\n" % \
                       (lampCount, -(ob.location.x), -(ob.location.y), -(ob.location.z),
                        ob.rotation_euler.x, ob.rotation_euler.y, ob.rotation_euler.z))

####################################################################################################
    def exportRainbows(rainbows):
        for ob in rainbows:
            povdataname = ob.data.name #enough?
            angle = degrees(ob.data.spot_size/2.5) #radians in blender (2
            width = ob.data.spot_blend *10
            distance = ob.data.shadow_buffer_clip_start
            #eps=0.0000001
            #angle = br/(cr+eps) * 10 #eps is small epsilon variable to avoid dividing by zero
            #width = ob.dimensions[2] #now let's say width of rainbow is the actual proxy height
            # formerly:
            #cz-bz # let's say width of the rainbow is height of the cone (interfacing choice

            # v(A,B) rotates vector A about origin by vector B.
            # and avoid a 0 length vector by adding 1

            # file.write("#declare %s_Target= vrotate(<%.6g,%.6g,%.6g>,<%.4g,%.4g,%.4g>);\n" % \
                       # (povdataname, -(ob.location.x+0.1), -(ob.location.y+0.1), -(ob.location.z+0.1),
                        # ob.rotation_euler.x, ob.rotation_euler.y, ob.rotation_euler.z))

            direction = (ob.location.x,ob.location.y,ob.location.z) # not taking matrix into account
            rmatrix = global_matrix * ob.matrix_world

            #ob.rotation_euler.to_matrix().to_4x4() * mathutils.Vector((0,0,1))
            # XXX Is result of the below offset by 90 degrees?
            up =ob.matrix_world.to_3x3()[1].xyz #* global_matrix


            # XXX TO CHANGE:
            #formerly:
            #tabWrite("#declare %s = rainbow {\n"%povdataname)

            # clumsy for now but remove the rainbow from instancing
            # system because not an object. use lamps later instead of meshes

            #del data_ref[dataname]
            tabWrite("rainbow {\n")

            tabWrite("angle %.4f\n"%angle)
            tabWrite("width %.4f\n"%width)
            tabWrite("distance %.4f\n"%distance)
            tabWrite("arc_angle %.4f\n"%ob.pov.arc_angle)
            tabWrite("falloff_angle %.4f\n"%ob.pov.falloff_angle)
            tabWrite("direction <%.4f,%.4f,%.4f>\n"%rmatrix.translation[:])
            tabWrite("up <%.4f,%.4f,%.4f>\n"%(up[0],up[1],up[2]))
            tabWrite("color_map {\n")
            tabWrite("[0.000  color srgbt<1.0, 0.5, 1.0, 1.0>]\n")
            tabWrite("[0.130  color srgbt<0.5, 0.5, 1.0, 0.9>]\n")
            tabWrite("[0.298  color srgbt<0.2, 0.2, 1.0, 0.7>]\n")
            tabWrite("[0.412  color srgbt<0.2, 1.0, 1.0, 0.4>]\n")
            tabWrite("[0.526  color srgbt<0.2, 1.0, 0.2, 0.4>]\n")
            tabWrite("[0.640  color srgbt<1.0, 1.0, 0.2, 0.4>]\n")
            tabWrite("[0.754  color srgbt<1.0, 0.5, 0.2, 0.6>]\n")
            tabWrite("[0.900  color srgbt<1.0, 0.2, 0.2, 0.7>]\n")
            tabWrite("[1.000  color srgbt<1.0, 0.2, 0.2, 1.0>]\n")
            tabWrite("}\n")


            povMatName = "Default_texture"
            #tabWrite("texture {%s}\n"%povMatName)
            write_object_modifiers(scene,ob,file)
            #tabWrite("rotate x*90\n")
            #matrix = global_matrix * ob.matrix_world
            #writeMatrix(matrix)
            tabWrite("}\n")
            #continue #Don't render proxy mesh, skip to next object

################################XXX LOFT, ETC.
    def exportCurves(scene, ob):
        name_orig = "OB" + ob.name
        dataname_orig = "DATA" + ob.data.name

        name = string_strip_hyphen(bpy.path.clean_name(name_orig))
        dataname = string_strip_hyphen(bpy.path.clean_name(dataname_orig))

        global_matrix = mathutils.Matrix.Rotation(-pi / 2.0, 4, 'X')
        matrix=global_matrix*ob.matrix_world
        bezier_sweep = False
        if ob.pov.curveshape == 'sphere_sweep':
            #inlined spheresweep macro, which itself calls Shapes.inc:
            file.write('        #include "shapes.inc"\n')

            file.write('        #macro Shape_Bezierpoints_Sphere_Sweep(_merge_shape, _resolution, _points_array, _radius_array)\n')
            file.write('        //input adjusting and inspection\n')
            file.write('        #if(_resolution <= 1)\n')
            file.write('            #local res = 1;\n')
            file.write('        #else\n')
            file.write('            #local res = int(_resolution);\n')
            file.write('        #end\n')
            file.write('        #if(dimensions(_points_array) != 1 | dimensions(_radius_array) != 1)\n')
            file.write('            #error ""\n')
            file.write('        #elseif(div(dimension_size(_points_array,1),4) - dimension_size(_points_array,1)/4 != 0)\n')
            file.write('            #error ""\n')
            file.write('        #elseif(dimension_size(_points_array,1) != dimension_size(_radius_array,1))\n')
            file.write('            #error ""\n')
            file.write('        #else\n')
            file.write('            #local n_of_seg = div(dimension_size(_points_array,1), 4);\n')
            file.write('            #local ctrl_pts_array = array[n_of_seg]\n')
            file.write('            #local ctrl_rs_array = array[n_of_seg]\n')
            file.write('            #for(i, 0, n_of_seg-1)\n')
            file.write('                #local ctrl_pts_array[i] = array[4] {_points_array[4*i], _points_array[4*i+1], _points_array[4*i+2], _points_array[4*i+3]}\n')
            file.write('                #local ctrl_rs_array[i] = array[4] {abs(_radius_array[4*i]), abs(_radius_array[4*i+1]), abs(_radius_array[4*i+2]), abs(_radius_array[4*i+3])}\n')
            file.write('            #end\n')
            file.write('        #end\n')

            file.write('        //drawing\n')
            file.write('        #local mockup1 =\n')
            file.write('        #if(_merge_shape) merge{ #else union{ #end\n')
            file.write('            #for(i, 0, n_of_seg-1)\n')
            file.write('                #local has_head = true;\n')
            file.write('                #if(i = 0)\n')
            file.write('                    #if(vlength(ctrl_pts_array[i][0]-ctrl_pts_array[n_of_seg-1][3]) = 0 & ctrl_rs_array[i][0]-ctrl_rs_array[n_of_seg-1][3] <= 0)\n')
            file.write('                        #local has_head = false;\n')
            file.write('                    #end\n')
            file.write('                #else\n')
            file.write('                    #if(vlength(ctrl_pts_array[i][0]-ctrl_pts_array[i-1][3]) = 0 & ctrl_rs_array[i][0]-ctrl_rs_array[i-1][3] <= 0)\n')
            file.write('                        #local has_head = false;\n')
            file.write('                    #end\n')
            file.write('                #end\n')
            file.write('                #if(has_head = true)\n')
            file.write('                    sphere{\n')
            file.write('                    ctrl_pts_array[i][0], ctrl_rs_array[i][0]\n')
            file.write('                    }\n')
            file.write('                #end\n')
            file.write('                #local para_t = (1/2)/res;\n')
            file.write('                #local this_point = ctrl_pts_array[i][0]*pow(1-para_t,3) + ctrl_pts_array[i][1]*3*pow(1-para_t,2)*para_t + ctrl_pts_array[i][2]*3*(1-para_t)*pow(para_t,2) + ctrl_pts_array[i][3]*pow(para_t,3);\n')
            file.write('                #local this_radius = ctrl_rs_array[i][0]*pow(1-para_t,3) + ctrl_rs_array[i][1]*3*pow(1-para_t,2)*para_t + ctrl_rs_array[i][2]*3*(1-para_t)*pow(para_t,2) + ctrl_rs_array[i][3]*pow(para_t,3);\n')
            file.write('                #if(vlength(this_point-ctrl_pts_array[i][0]) > abs(this_radius-ctrl_rs_array[i][0]))\n')
            file.write('                    object{\n')
            file.write('                    Connect_Spheres(ctrl_pts_array[i][0], ctrl_rs_array[i][0], this_point, this_radius)\n')
            file.write('                    }\n')
            file.write('                #end\n')
            file.write('                sphere{\n')
            file.write('                this_point, this_radius\n')
            file.write('                }\n')
            file.write('                #for(j, 1, res-1)\n')
            file.write('                    #local last_point = this_point;\n')
            file.write('                    #local last_radius = this_radius;\n')
            file.write('                    #local para_t = (1/2+j)/res;\n')
            file.write('                    #local this_point = ctrl_pts_array[i][0]*pow(1-para_t,3) + ctrl_pts_array[i][1]*3*pow(1-para_t,2)*para_t + ctrl_pts_array[i][2]*3*(1-para_t)*pow(para_t,2) + ctrl_pts_array[i][3]*pow(para_t,3);\n')
            file.write('                    #local this_radius = ctrl_rs_array[i][0]*pow(1-para_t,3) + ctrl_rs_array[i][1]*3*pow(1-para_t,2)*para_t + ctrl_rs_array[i][2]*3*(1-para_t)*pow(para_t,2) + ctrl_rs_array[i][3]*pow(para_t,3);\n')
            file.write('                    #if(vlength(this_point-last_point) > abs(this_radius-last_radius))\n')
            file.write('                        object{\n')
            file.write('                        Connect_Spheres(last_point, last_radius, this_point, this_radius)\n')
            file.write('                        }\n')
            file.write('                    #end\n')
            file.write('                    sphere{\n')
            file.write('                    this_point, this_radius\n')
            file.write('                    }\n')
            file.write('                #end\n')
            file.write('                #local last_point = this_point;\n')
            file.write('                #local last_radius = this_radius;\n')
            file.write('                #local this_point = ctrl_pts_array[i][3];\n')
            file.write('                #local this_radius = ctrl_rs_array[i][3];\n')
            file.write('                #if(vlength(this_point-last_point) > abs(this_radius-last_radius))\n')
            file.write('                    object{\n')
            file.write('                    Connect_Spheres(last_point, last_radius, this_point, this_radius)\n')
            file.write('                    }\n')
            file.write('                #end\n')
            file.write('                sphere{\n')
            file.write('                this_point, this_radius\n')
            file.write('                }\n')
            file.write('            #end\n')
            file.write('        }\n')
            file.write('        mockup1\n')
            file.write('        #end\n')

            for spl in ob.data.splines:
                if spl.type == "BEZIER":
                    bezier_sweep = True
        if ob.pov.curveshape in {'loft','birail'}:
            n=0
            for spline in ob.data.splines:
                n+=1
                tabWrite('#declare %s%s=spline {\n'%(dataname,n))
                tabWrite('cubic_spline\n')
                lp = len(spline.points)
                delta = 1/(lp)
                d=-delta
                point = spline.points[lp-1]
                x,y,z,w  = point.co[:]
                tabWrite('%.6f, <%.6f,%.6f,%.6f>\n'%(d,x,y,z))
                d+=delta
                for point in spline.points:
                    x,y,z,w  = point.co[:]
                    tabWrite('%.6f, <%.6f,%.6f,%.6f>\n'%(d,x,y,z))
                    d+=delta
                for i in range(2):
                    point = spline.points[i]
                    x,y,z,w  = point.co[:]
                    tabWrite('%.6f, <%.6f,%.6f,%.6f>\n'%(d,x,y,z))
                    d+=delta
                tabWrite('}\n')
            if ob.pov.curveshape in {'loft'}:
                n = len(ob.data.splines)
                tabWrite('#declare %s = array[%s]{\n'%(dataname,(n+3)))
                tabWrite('spline{%s%s},\n'%(dataname,n))
                for i in range(n):
                    tabWrite('spline{%s%s},\n'%(dataname,(i+1)))
                tabWrite('spline{%s1},\n'%(dataname))
                tabWrite('spline{%s2}\n'%(dataname))
                tabWrite('}\n')
            # Use some of the Meshmaker.inc macro, here inlined
            file.write('#macro CheckFileName(FileName)\n')
            file.write('   #local Len=strlen(FileName);\n')
            file.write('   #if(Len>0)\n')
            file.write('      #if(file_exists(FileName))\n')
            file.write('         #if(Len>=4)\n')
            file.write('            #local Ext=strlwr(substr(FileName,Len-3,4))\n')
            file.write('            #if (strcmp(Ext,".obj")=0 | strcmp(Ext,".pcm")=0 | strcmp(Ext,".arr")=0)\n')
            file.write('               #local Return=99;\n')
            file.write('            #else\n')
            file.write('               #local Return=0;\n')
            file.write('            #end\n')
            file.write('         #else\n')
            file.write('            #local Return=0;\n')
            file.write('         #end\n')
            file.write('      #else\n')
            file.write('         #if(Len>=4)\n')
            file.write('            #local Ext=strlwr(substr(FileName,Len-3,4))\n')
            file.write('            #if (strcmp(Ext,".obj")=0 | strcmp(Ext,".pcm")=0 | strcmp(Ext,".arr")=0)\n')
            file.write('               #if (strcmp(Ext,".obj")=0)\n')
            file.write('                  #local Return=2;\n')
            file.write('               #end\n')
            file.write('               #if (strcmp(Ext,".pcm")=0)\n')
            file.write('                  #local Return=3;\n')
            file.write('               #end\n')
            file.write('               #if (strcmp(Ext,".arr")=0)\n')
            file.write('                  #local Return=4;\n')
            file.write('               #end\n')
            file.write('            #else\n')
            file.write('               #local Return=1;\n')
            file.write('            #end\n')
            file.write('         #else\n')
            file.write('            #local Return=1;\n')
            file.write('         #end\n')
            file.write('      #end\n')
            file.write('   #else\n')
            file.write('      #local Return=1;\n')
            file.write('   #end\n')
            file.write('   (Return)\n')
            file.write('#end\n')

            file.write('#macro BuildSpline(Arr, SplType)\n')
            file.write('   #local Ds=dimension_size(Arr,1);\n')
            file.write('   #local Asc=asc(strupr(SplType));\n')
            file.write('   #if(Asc!=67 & Asc!=76 & Asc!=81) \n')
            file.write('      #local Asc=76;\n')
            file.write('      #debug "\nWrong spline type defined (C/c/L/l/N/n/Q/q), using default linear_spline\\n"\n')
            file.write('   #end\n')
            file.write('   spline {\n')
            file.write('      #switch (Asc)\n')
            file.write('         #case (67) //C  cubic_spline\n')
            file.write('            cubic_spline\n')
            file.write('         #break\n')
            file.write('         #case (76) //L  linear_spline\n')
            file.write('            linear_spline\n')
            file.write('         #break\n')
            file.write('         #case (78) //N  linear_spline\n')
            file.write('            natural_spline\n')
            file.write('         #break\n')
            file.write('         #case (81) //Q  Quadratic_spline\n')
            file.write('            quadratic_spline\n')
            file.write('         #break\n')
            file.write('      #end\n')
            file.write('      #local Add=1/((Ds-2)-1);\n')
            file.write('      #local J=0-Add;\n')
            file.write('      #local I=0;\n')
            file.write('      #while (I<Ds)\n')
            file.write('         J\n')
            file.write('         Arr[I]\n')
            file.write('         #local I=I+1;\n')
            file.write('         #local J=J+Add;\n')
            file.write('      #end\n')
            file.write('   }\n')
            file.write('#end\n')


            file.write('#macro BuildWriteMesh2(VecArr, NormArr, UVArr, U, V, FileName)\n')
            #suppressed some file checking from original macro because no more separate files
            file.write(' #local Write=0;\n')
            file.write(' #debug concat("\\n\\n Building mesh2: \\n   - vertex_vectors\\n")\n')
            file.write('  #local NumVertices=dimension_size(VecArr,1);\n')
            file.write('  #switch (Write)\n')
            file.write('     #case(1)\n')
            file.write('        #write(\n')
            file.write('           MeshFile,\n')
            file.write('           "  vertex_vectors {\\n",\n')
            file.write('           "    ", str(NumVertices,0,0),"\\n    "\n')
            file.write('        )\n')
            file.write('     #break\n')
            file.write('     #case(2)\n')
            file.write('        #write(\n')
            file.write('           MeshFile,\n')
            file.write('           "# Vertices: ",str(NumVertices,0,0),"\\n"\n')
            file.write('        )\n')
            file.write('     #break\n')
            file.write('     #case(3)\n')
            file.write('        #write(\n')
            file.write('           MeshFile,\n')
            file.write('           str(2*NumVertices,0,0),",\\n"\n')
            file.write('        )\n')
            file.write('     #break\n')
            file.write('     #case(4)\n')
            file.write('        #write(\n')
            file.write('           MeshFile,\n')
            file.write('           "#declare VertexVectors= array[",str(NumVertices,0,0),"] {\\n  "\n')
            file.write('        )\n')
            file.write('     #break\n')
            file.write('  #end\n')
            file.write('  mesh2 {\n')
            file.write('     vertex_vectors {\n')
            file.write('        NumVertices\n')
            file.write('        #local I=0;\n')
            file.write('        #while (I<NumVertices)\n')
            file.write('           VecArr[I]\n')
            file.write('           #switch(Write)\n')
            file.write('              #case(1)\n')
            file.write('                 #write(MeshFile, VecArr[I])\n')
            file.write('              #break\n')
            file.write('              #case(2)\n')
            file.write('                 #write(\n')
            file.write('                    MeshFile,\n')
            file.write('                    "v ", VecArr[I].x," ", VecArr[I].y," ", VecArr[I].z,"\\n"\n')
            file.write('                 )\n')
            file.write('              #break\n')
            file.write('              #case(3)\n')
            file.write('                 #write(\n')
            file.write('                    MeshFile,\n')
            file.write('                    VecArr[I].x,",", VecArr[I].y,",", VecArr[I].z,",\\n"\n')
            file.write('                 )\n')
            file.write('              #break\n')
            file.write('              #case(4)\n')
            file.write('                 #write(MeshFile, VecArr[I])\n')
            file.write('              #break\n')
            file.write('           #end\n')
            file.write('           #local I=I+1;\n')
            file.write('           #if(Write=1 | Write=4)\n')
            file.write('              #if(mod(I,3)=0)\n')
            file.write('                 #write(MeshFile,"\\n    ")\n')
            file.write('              #end\n')
            file.write('           #end \n')
            file.write('        #end\n')
            file.write('        #switch(Write)\n')
            file.write('           #case(1)\n')
            file.write('              #write(MeshFile,"\\n  }\\n")\n')
            file.write('           #break\n')
            file.write('           #case(2)\n')
            file.write('              #write(MeshFile,"\\n")\n')
            file.write('           #break\n')
            file.write('           #case(3)\n')
            file.write('              // do nothing\n')
            file.write('           #break\n')
            file.write('           #case(4) \n')
            file.write('              #write(MeshFile,"\\n}\\n")\n')
            file.write('           #break\n')
            file.write('        #end\n')
            file.write('     }\n')

            file.write('     #debug concat("   - normal_vectors\\n")    \n')
            file.write('     #local NumVertices=dimension_size(NormArr,1);\n')
            file.write('     #switch(Write)\n')
            file.write('        #case(1)\n')
            file.write('           #write(\n')
            file.write('              MeshFile,\n')
            file.write('              "  normal_vectors {\\n",\n')
            file.write('              "    ", str(NumVertices,0,0),"\\n    "\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('        #case(2)\n')
            file.write('           #write(\n')
            file.write('              MeshFile,\n')
            file.write('              "# Normals: ",str(NumVertices,0,0),"\\n"\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('        #case(3)\n')
            file.write('           // do nothing\n')
            file.write('        #break\n')
            file.write('        #case(4)\n')
            file.write('           #write(\n')
            file.write('              MeshFile,\n')
            file.write('              "#declare NormalVectors= array[",str(NumVertices,0,0),"] {\\n  "\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('     #end\n')
            file.write('     normal_vectors {\n')
            file.write('        NumVertices\n')
            file.write('        #local I=0;\n')
            file.write('        #while (I<NumVertices)\n')
            file.write('           NormArr[I]\n')
            file.write('           #switch(Write)\n')
            file.write('              #case(1)\n')
            file.write('                 #write(MeshFile NormArr[I])\n')
            file.write('              #break\n')
            file.write('              #case(2)\n')
            file.write('                 #write(\n')
            file.write('                    MeshFile,\n')
            file.write('                    "vn ", NormArr[I].x," ", NormArr[I].y," ", NormArr[I].z,"\\n"\n')
            file.write('                 )\n')
            file.write('              #break\n')
            file.write('              #case(3)\n')
            file.write('                 #write(\n')
            file.write('                    MeshFile,\n')
            file.write('                    NormArr[I].x,",", NormArr[I].y,",", NormArr[I].z,",\\n"\n')
            file.write('                 )\n')
            file.write('              #break\n')
            file.write('              #case(4)\n')
            file.write('                 #write(MeshFile NormArr[I])\n')
            file.write('              #break\n')
            file.write('           #end\n')
            file.write('           #local I=I+1;\n')
            file.write('           #if(Write=1 | Write=4) \n')
            file.write('              #if(mod(I,3)=0)\n')
            file.write('                 #write(MeshFile,"\\n    ")\n')
            file.write('              #end\n')
            file.write('           #end\n')
            file.write('        #end\n')
            file.write('        #switch(Write)\n')
            file.write('           #case(1)\n')
            file.write('              #write(MeshFile,"\\n  }\\n")\n')
            file.write('           #break\n')
            file.write('           #case(2)\n')
            file.write('              #write(MeshFile,"\\n")\n')
            file.write('           #break\n')
            file.write('           #case(3)\n')
            file.write('              //do nothing\n')
            file.write('           #break\n')
            file.write('           #case(4)\n')
            file.write('              #write(MeshFile,"\\n}\\n")\n')
            file.write('           #break\n')
            file.write('        #end\n')
            file.write('     }\n')

            file.write('     #debug concat("   - uv_vectors\\n")   \n')
            file.write('     #local NumVertices=dimension_size(UVArr,1);\n')
            file.write('     #switch(Write)\n')
            file.write('        #case(1)\n')
            file.write('           #write(\n')
            file.write('              MeshFile, \n')
            file.write('              "  uv_vectors {\\n",\n')
            file.write('              "    ", str(NumVertices,0,0),"\\n    "\n')
            file.write('           )\n')
            file.write('         #break\n')
            file.write('         #case(2)\n')
            file.write('           #write(\n')
            file.write('              MeshFile,\n')
            file.write('              "# UV-vectors: ",str(NumVertices,0,0),"\\n"\n')
            file.write('           )\n')
            file.write('         #break\n')
            file.write('         #case(3)\n')
            file.write('           // do nothing, *.pcm does not support uv-vectors\n')
            file.write('         #break\n')
            file.write('         #case(4)\n')
            file.write('            #write(\n')
            file.write('               MeshFile,\n')
            file.write('               "#declare UVVectors= array[",str(NumVertices,0,0),"] {\\n  "\n')
            file.write('            )\n')
            file.write('         #break\n')
            file.write('     #end\n')
            file.write('     uv_vectors {\n')
            file.write('        NumVertices\n')
            file.write('        #local I=0;\n')
            file.write('        #while (I<NumVertices)\n')
            file.write('           UVArr[I]\n')
            file.write('           #switch(Write)\n')
            file.write('              #case(1)\n')
            file.write('                 #write(MeshFile UVArr[I])\n')
            file.write('              #break\n')
            file.write('              #case(2)\n')
            file.write('                 #write(\n')
            file.write('                    MeshFile,\n')
            file.write('                    "vt ", UVArr[I].u," ", UVArr[I].v,"\\n"\n')
            file.write('                 )\n')
            file.write('              #break\n')
            file.write('              #case(3)\n')
            file.write('                 //do nothing\n')
            file.write('              #break\n')
            file.write('              #case(4)\n')
            file.write('                 #write(MeshFile UVArr[I])\n')
            file.write('              #break\n')
            file.write('           #end\n')
            file.write('           #local I=I+1; \n')
            file.write('           #if(Write=1 | Write=4)\n')
            file.write('              #if(mod(I,3)=0)\n')
            file.write('                 #write(MeshFile,"\\n    ")\n')
            file.write('              #end \n')
            file.write('           #end\n')
            file.write('        #end \n')
            file.write('        #switch(Write)\n')
            file.write('           #case(1)\n')
            file.write('              #write(MeshFile,"\\n  }\\n")\n')
            file.write('           #break\n')
            file.write('           #case(2)\n')
            file.write('              #write(MeshFile,"\\n")\n')
            file.write('           #break\n')
            file.write('           #case(3)\n')
            file.write('              //do nothing\n')
            file.write('           #break\n')
            file.write('           #case(4)\n')
            file.write('              #write(MeshFile,"\\n}\\n")\n')
            file.write('           #break\n')
            file.write('        #end\n')
            file.write('     }\n')
            file.write('\n')
            file.write('     #debug concat("   - face_indices\\n")   \n')
            file.write('     #declare NumFaces=U*V*2;\n')
            file.write('     #switch(Write)\n')
            file.write('        #case(1)\n')
            file.write('           #write(\n')
            file.write('              MeshFile,\n')
            file.write('              "  face_indices {\\n"\n')
            file.write('              "    ", str(NumFaces,0,0),"\\n    "\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('        #case(2)\n')
            file.write('           #write (\n')
            file.write('              MeshFile,\n')
            file.write('              "# faces: ",str(NumFaces,0,0),"\\n"\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('        #case(3)\n')
            file.write('           #write (\n')
            file.write('              MeshFile,\n')
            file.write('              "0,",str(NumFaces,0,0),",\\n"\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('        #case(4)\n')
            file.write('           #write(\n')
            file.write('              MeshFile,\n')
            file.write('              "#declare FaceIndices= array[",str(NumFaces,0,0),"] {\\n  "\n')
            file.write('           )\n')
            file.write('        #break\n')
            file.write('     #end\n')
            file.write('     face_indices {\n')
            file.write('        NumFaces\n')
            file.write('        #local I=0;\n')
            file.write('        #local H=0;\n')
            file.write('        #local NumVertices=dimension_size(VecArr,1);\n')
            file.write('        #while (I<V)\n')
            file.write('           #local J=0;\n')
            file.write('           #while (J<U)\n')
            file.write('              #local Ind=(I*U)+I+J;\n')
            file.write('              <Ind, Ind+1, Ind+U+2>, <Ind, Ind+U+1, Ind+U+2>\n')
            file.write('              #switch(Write)\n')
            file.write('                 #case(1)\n')
            file.write('                    #write(\n')
            file.write('                       MeshFile,\n')
            file.write('                       <Ind, Ind+1, Ind+U+2>, <Ind, Ind+U+1, Ind+U+2>\n')
            file.write('                    )\n')
            file.write('                 #break\n')
            file.write('                 #case(2)\n')
            file.write('                    #write(\n')
            file.write('                       MeshFile,\n')
            file.write('                       "f ",Ind+1,"/",Ind+1,"/",Ind+1," ",Ind+1+1,"/",Ind+1+1,"/",Ind+1+1," ",Ind+U+2+1,"/",Ind+U+2+1,"/",Ind+U+2+1,"\\n",\n')
            file.write('                       "f ",Ind+U+1+1,"/",Ind+U+1+1,"/",Ind+U+1+1," ",Ind+1,"/",Ind+1,"/",Ind+1," ",Ind+U+2+1,"/",Ind+U+2+1,"/",Ind+U+2+1,"\\n"\n')
            file.write('                    )\n')
            file.write('                 #break\n')
            file.write('                 #case(3)\n')
            file.write('                    #write(\n')
            file.write('                       MeshFile,\n')
            file.write('                       Ind,",",Ind+NumVertices,",",Ind+1,",",Ind+1+NumVertices,",",Ind+U+2,",",Ind+U+2+NumVertices,",\\n"\n')
            file.write('                       Ind+U+1,",",Ind+U+1+NumVertices,",",Ind,",",Ind+NumVertices,",",Ind+U+2,",",Ind+U+2+NumVertices,",\\n"\n')
            file.write('                    )\n')
            file.write('                 #break\n')
            file.write('                 #case(4)\n')
            file.write('                    #write(\n')
            file.write('                       MeshFile,\n')
            file.write('                       <Ind, Ind+1, Ind+U+2>, <Ind, Ind+U+1, Ind+U+2>\n')
            file.write('                    )\n')
            file.write('                 #break\n')
            file.write('              #end\n')
            file.write('              #local J=J+1;\n')
            file.write('              #local H=H+1;\n')
            file.write('              #if(Write=1 | Write=4)\n')
            file.write('                 #if(mod(H,3)=0)\n')
            file.write('                    #write(MeshFile,"\\n    ")\n')
            file.write('                 #end \n')
            file.write('              #end\n')
            file.write('           #end\n')
            file.write('           #local I=I+1;\n')
            file.write('        #end\n')
            file.write('     }\n')
            file.write('     #switch(Write)\n')
            file.write('        #case(1)\n')
            file.write('           #write(MeshFile, "\\n  }\\n}")\n')
            file.write('           #fclose MeshFile\n')
            file.write('           #debug concat(" Done writing\\n")\n')
            file.write('        #break\n')
            file.write('        #case(2)\n')
            file.write('           #fclose MeshFile\n')
            file.write('           #debug concat(" Done writing\\n")\n')
            file.write('        #break\n')
            file.write('        #case(3)\n')
            file.write('           #fclose MeshFile\n')
            file.write('           #debug concat(" Done writing\\n")\n')
            file.write('        #break\n')
            file.write('        #case(4)\n')
            file.write('           #write(MeshFile, "\\n}\\n}")\n')
            file.write('           #fclose MeshFile\n')
            file.write('           #debug concat(" Done writing\\n")\n')
            file.write('        #break\n')
            file.write('     #end\n')
            file.write('  }\n')
            file.write('#end\n')

            file.write('#macro MSM(SplineArray, SplRes, Interp_type,  InterpRes, FileName)\n')
            file.write('    #declare Build=CheckFileName(FileName);\n')
            file.write('    #if(Build=0)\n')
            file.write('        #debug concat("\\n Parsing mesh2 from file: ", FileName, "\\n")\n')
            file.write('        #include FileName\n')
            file.write('        object{Surface}\n')
            file.write('    #else\n')
            file.write('        #local NumVertices=(SplRes+1)*(InterpRes+1);\n')
            file.write('        #local NumFaces=SplRes*InterpRes*2;\n')
            file.write('        #debug concat("\\n Calculating ",str(NumVertices,0,0)," vertices for ", str(NumFaces,0,0)," triangles\\n\\n")\n')
            file.write('        #local VecArr=array[NumVertices]\n')
            file.write('        #local NormArr=array[NumVertices]\n')
            file.write('        #local UVArr=array[NumVertices]\n')
            file.write('        #local N=dimension_size(SplineArray,1);\n')
            file.write('        #local TempSplArr0=array[N];\n')
            file.write('        #local TempSplArr1=array[N];\n')
            file.write('        #local TempSplArr2=array[N];\n')
            file.write('        #local PosStep=1/SplRes;\n')
            file.write('        #local InterpStep=1/InterpRes;\n')
            file.write('        #local Count=0;\n')
            file.write('        #local Pos=0;\n')
            file.write('        #while(Pos<=1)\n')
            file.write('            #local I=0;\n')
            file.write('            #if (Pos=0)\n')
            file.write('                #while (I<N)\n')
            file.write('                    #local Spl=spline{SplineArray[I]}\n')
            file.write('                    #local TempSplArr0[I]=<0,0,0>+Spl(Pos);\n')
            file.write('                    #local TempSplArr1[I]=<0,0,0>+Spl(Pos+PosStep);\n')
            file.write('                    #local TempSplArr2[I]=<0,0,0>+Spl(Pos-PosStep);\n')
            file.write('                    #local I=I+1;\n')
            file.write('                #end\n')
            file.write('                #local S0=BuildSpline(TempSplArr0, Interp_type)\n')
            file.write('                #local S1=BuildSpline(TempSplArr1, Interp_type)\n')
            file.write('                #local S2=BuildSpline(TempSplArr2, Interp_type)\n')
            file.write('            #else\n')
            file.write('                #while (I<N)\n')
            file.write('                    #local Spl=spline{SplineArray[I]}\n')
            file.write('                    #local TempSplArr1[I]=<0,0,0>+Spl(Pos+PosStep);\n')
            file.write('                    #local I=I+1;\n')
            file.write('                #end\n')
            file.write('                #local S1=BuildSpline(TempSplArr1, Interp_type)\n')
            file.write('            #end\n')
            file.write('            #local J=0;\n')
            file.write('            #while (J<=1)\n')
            file.write('                #local P0=<0,0,0>+S0(J);\n')
            file.write('                #local P1=<0,0,0>+S1(J);\n')
            file.write('                #local P2=<0,0,0>+S2(J);\n')
            file.write('                #local P3=<0,0,0>+S0(J+InterpStep);\n')
            file.write('                #local P4=<0,0,0>+S0(J-InterpStep);\n')
            file.write('                #local B1=P4-P0;\n')
            file.write('                #local B2=P2-P0;\n')
            file.write('                #local B3=P3-P0;\n')
            file.write('                #local B4=P1-P0;\n')
            file.write('                #local N1=vcross(B1,B2);\n')
            file.write('                #local N2=vcross(B2,B3);\n')
            file.write('                #local N3=vcross(B3,B4);\n')
            file.write('                #local N4=vcross(B4,B1);\n')
            file.write('                #local Norm=vnormalize((N1+N2+N3+N4));\n')
            file.write('                #local VecArr[Count]=P0;\n')
            file.write('                #local NormArr[Count]=Norm;\n')
            file.write('                #local UVArr[Count]=<J,Pos>;\n')
            file.write('                #local J=J+InterpStep;\n')
            file.write('                #local Count=Count+1;\n')
            file.write('            #end\n')
            file.write('            #local S2=spline{S0}\n')
            file.write('            #local S0=spline{S1}\n')
            file.write('            #debug concat("\\r Done ", str(Count,0,0)," vertices : ", str(100*Count/NumVertices,0,2)," %")\n')
            file.write('            #local Pos=Pos+PosStep;\n')
            file.write('        #end\n')
            file.write('        BuildWriteMesh2(VecArr, NormArr, UVArr, InterpRes, SplRes, "")\n')
            file.write('    #end\n')
            file.write('#end\n\n')

            file.write('#macro Coons(Spl1, Spl2, Spl3, Spl4, Iter_U, Iter_V, FileName)\n')
            file.write('   #declare Build=CheckFileName(FileName);\n')
            file.write('   #if(Build=0)\n')
            file.write('      #debug concat("\\n Parsing mesh2 from file: ", FileName, "\\n")\n')
            file.write('      #include FileName\n')
            file.write('      object{Surface}\n')
            file.write('   #else\n')
            file.write('      #local NumVertices=(Iter_U+1)*(Iter_V+1);\n')
            file.write('      #local NumFaces=Iter_U*Iter_V*2;\n')
            file.write('      #debug concat("\\n Calculating ", str(NumVertices,0,0), " vertices for ",str(NumFaces,0,0), " triangles\\n\\n")\n')
            file.write('      #declare VecArr=array[NumVertices]   \n')
            file.write('      #declare NormArr=array[NumVertices]   \n')
            file.write('      #local UVArr=array[NumVertices]      \n')
            file.write('      #local Spl1_0=Spl1(0);\n')
            file.write('      #local Spl2_0=Spl2(0);\n')
            file.write('      #local Spl3_0=Spl3(0);\n')
            file.write('      #local Spl4_0=Spl4(0);\n')
            file.write('      #local UStep=1/Iter_U;\n')
            file.write('      #local VStep=1/Iter_V;\n')
            file.write('      #local Count=0;\n')
            file.write('      #local I=0;\n')
            file.write('      #while (I<=1)\n')
            file.write('         #local Im=1-I;\n')
            file.write('         #local J=0;\n')
            file.write('         #while (J<=1)\n')
            file.write('            #local Jm=1-J;\n')
            file.write('            #local C0=Im*Jm*(Spl1_0)+Im*J*(Spl2_0)+I*J*(Spl3_0)+I*Jm*(Spl4_0);\n')
            file.write('            #local P0=LInterpolate(I, Spl1(J), Spl3(Jm)) + \n')
            file.write('               LInterpolate(Jm, Spl2(I), Spl4(Im))-C0;\n')
            file.write('            #declare VecArr[Count]=P0;\n')
            file.write('            #local UVArr[Count]=<J,I>;\n')
            file.write('            #local J=J+UStep;\n')
            file.write('            #local Count=Count+1;\n')
            file.write('         #end\n')
            file.write('         #debug concat(\n')
            file.write('            "\r Done ", str(Count,0,0)," vertices :         ",\n')
            file.write('            str(100*Count/NumVertices,0,2)," %"\n')
            file.write('         )\n')
            file.write('         #local I=I+VStep;\n')
            file.write('      #end\n')
            file.write('      #debug "\r Normals                                  "\n')
            file.write('      #local Count=0;\n')
            file.write('      #local I=0;\n')
            file.write('      #while (I<=Iter_V)\n')
            file.write('         #local J=0;\n')
            file.write('         #while (J<=Iter_U)\n')
            file.write('            #local Ind=(I*Iter_U)+I+J;\n')
            file.write('            #local P0=VecArr[Ind];\n')
            file.write('            #if(J=0)\n')
            file.write('               #local P1=P0+(P0-VecArr[Ind+1]);\n')
            file.write('            #else\n')
            file.write('               #local P1=VecArr[Ind-1];\n')
            file.write('            #end\n')
            file.write('            #if (J=Iter_U)\n')
            file.write('               #local P2=P0+(P0-VecArr[Ind-1]);\n')
            file.write('            #else\n')
            file.write('               #local P2=VecArr[Ind+1];\n')
            file.write('            #end\n')
            file.write('            #if (I=0)\n')
            file.write('               #local P3=P0+(P0-VecArr[Ind+Iter_U+1]);\n')
            file.write('            #else\n')
            file.write('               #local P3=VecArr[Ind-Iter_U-1];\n')
            file.write('            #end\n')
            file.write('            #if (I=Iter_V)\n')
            file.write('               #local P4=P0+(P0-VecArr[Ind-Iter_U-1]);\n')
            file.write('            #else\n')
            file.write('               #local P4=VecArr[Ind+Iter_U+1];\n')
            file.write('            #end\n')
            file.write('            #local B1=P4-P0;\n')
            file.write('            #local B2=P2-P0;\n')
            file.write('            #local B3=P3-P0;\n')
            file.write('            #local B4=P1-P0;\n')
            file.write('            #local N1=vcross(B1,B2);\n')
            file.write('            #local N2=vcross(B2,B3);\n')
            file.write('            #local N3=vcross(B3,B4);\n')
            file.write('            #local N4=vcross(B4,B1);\n')
            file.write('            #local Norm=vnormalize((N1+N2+N3+N4));\n')
            file.write('            #declare NormArr[Count]=Norm;\n')
            file.write('            #local J=J+1;\n')
            file.write('            #local Count=Count+1;\n')
            file.write('         #end\n')
            file.write('         #debug concat("\r Done ", str(Count,0,0)," normals : ",str(100*Count/NumVertices,0,2), " %")\n')
            file.write('         #local I=I+1;\n')
            file.write('      #end\n')
            file.write('      BuildWriteMesh2(VecArr, NormArr, UVArr, Iter_U, Iter_V, FileName)\n')
            file.write('   #end\n')
            file.write('#end\n\n')
        # Empty curves    
        if len(ob.data.splines)==0:
            tabWrite("\n//dummy sphere to represent empty curve location\n")        
            tabWrite("#declare %s =\n"%dataname)
            tabWrite("sphere {<%.6g, %.6g, %.6g>,0 pigment{rgbt 1} no_image no_reflection no_radiosity photons{pass_through collect off} hollow}\n\n" % (ob.location.x, ob.location.y, ob.location.z)) # ob.name > povdataname)    
        # And non empty curves    
        else:
            if bezier_sweep == False:
                tabWrite("#declare %s =\n"%dataname)
            if ob.pov.curveshape == 'sphere_sweep' and bezier_sweep == False:
                tabWrite("union {\n")
                for spl in ob.data.splines:
                    if spl.type != "BEZIER":
                        spl_type = "linear"
                        if spl.type == "NURBS":
                            spl_type = "cubic"
                        points=spl.points
                        numPoints=len(points)
                        if spl.use_cyclic_u:
                            numPoints+=3

                        tabWrite("sphere_sweep { %s_spline %s,\n"%(spl_type,numPoints))
                        if spl.use_cyclic_u:
                            pt1 = points[len(points)-1]
                            wpt1 = pt1.co
                            tabWrite("<%.4g,%.4g,%.4g>,%.4g\n" %(wpt1[0], wpt1[1], wpt1[2], pt1.radius*ob.data.bevel_depth))
                        for pt in points:
                            wpt = pt.co
                            tabWrite("<%.4g,%.4g,%.4g>,%.4g\n" %(wpt[0], wpt[1], wpt[2], pt.radius*ob.data.bevel_depth))
                        if spl.use_cyclic_u:
                            for i in range (0,2):
                                endPt=points[i]
                                wpt = endPt.co
                                tabWrite("<%.4g,%.4g,%.4g>,%.4g\n" %(wpt[0], wpt[1], wpt[2], endPt.radius*ob.data.bevel_depth))


                    tabWrite("}\n")
            # below not used yet?
            if ob.pov.curveshape == 'sor':
                for spl in ob.data.splines:
                    if spl.type in {'POLY','NURBS'}:
                        points=spl.points
                        numPoints=len(points)
                        tabWrite("sor { %s,\n"%numPoints)
                        for pt in points:
                            wpt = pt.co
                            tabWrite("<%.4g,%.4g>\n" %(wpt[0], wpt[1]))
                    else:
                        tabWrite("box { 0,0\n")
            if ob.pov.curveshape in {'lathe','prism'}:
                spl = ob.data.splines[0]
                if spl.type == "BEZIER":
                    points=spl.bezier_points
                    lenCur=len(points)-1
                    lenPts=lenCur*4
                    ifprism = ''
                    if ob.pov.curveshape in {'prism'}:
                        height = ob.data.extrude
                        ifprism = '-%s, %s,'%(height, height)
                        lenCur+=1
                        lenPts+=4
                    tabWrite("%s { bezier_spline %s %s,\n"%(ob.pov.curveshape,ifprism,lenPts))
                    for i in range(0,lenCur):
                        p1=points[i].co
                        pR=points[i].handle_right
                        end = i+1
                        if i == lenCur-1 and ob.pov.curveshape in {'prism'}:
                            end = 0
                        pL=points[end].handle_left
                        p2=points[end].co
                        line="<%.4g,%.4g>"%(p1[0],p1[1])
                        line+="<%.4g,%.4g>"%(pR[0],pR[1])
                        line+="<%.4g,%.4g>"%(pL[0],pL[1])
                        line+="<%.4g,%.4g>"%(p2[0],p2[1])
                        tabWrite("%s\n" %line)
                else:
                    points=spl.points
                    lenCur=len(points)
                    lenPts=lenCur
                    ifprism = ''
                    if ob.pov.curveshape in {'prism'}:
                        height = ob.data.extrude
                        ifprism = '-%s, %s,'%(height, height)
                        lenPts+=3
                    spl_type = 'quadratic'
                    if spl.type == 'POLY':
                        spl_type = 'linear'
                    tabWrite("%s { %s_spline %s %s,\n"%(ob.pov.curveshape,spl_type,ifprism,lenPts))
                    if ob.pov.curveshape in {'prism'}:
                        pt = points[len(points)-1]
                        wpt = pt.co
                        tabWrite("<%.4g,%.4g>\n" %(wpt[0], wpt[1]))
                    for pt in points:
                        wpt = pt.co
                        tabWrite("<%.4g,%.4g>\n" %(wpt[0], wpt[1]))
                    if ob.pov.curveshape in {'prism'}:
                        for i in range(2):
                            pt = points[i]
                            wpt = pt.co
                            tabWrite("<%.4g,%.4g>\n" %(wpt[0], wpt[1]))
            if bezier_sweep:
                for p in range(len(ob.data.splines)):
                    br = []
                    depth = ob.data.bevel_depth
                    spl = ob.data.splines[p]
                    points=spl.bezier_points
                    lenCur = len(points)-1
                    numPoints = lenCur*4
                    if spl.use_cyclic_u:
                        lenCur += 1
                        numPoints += 4
                    tabWrite("#declare %s_points_%s = array[%s]{\n"%(dataname,p,numPoints))
                    for i in range(lenCur):
                        p1=points[i].co
                        pR=points[i].handle_right
                        end = i+1
                        if spl.use_cyclic_u and i == (lenCur - 1):
                            end = 0
                        pL=points[end].handle_left
                        p2=points[end].co
                        r3 = points[end].radius * depth
                        r0 = points[i].radius * depth
                        r1 = 2/3*r0 + 1/3*r3
                        r2 = 1/3*r0 + 2/3*r3
                        br.append((r0,r1,r2,r3))
                        line="<%.4g,%.4g,%.4f>"%(p1[0],p1[1],p1[2])
                        line+="<%.4g,%.4g,%.4f>"%(pR[0],pR[1],pR[2])
                        line+="<%.4g,%.4g,%.4f>"%(pL[0],pL[1],pL[2])
                        line+="<%.4g,%.4g,%.4f>"%(p2[0],p2[1],p2[2])
                        tabWrite("%s\n" %line)
                    tabWrite("}\n")
                    tabWrite("#declare %s_radii_%s = array[%s]{\n"%(dataname,p,len(br)*4))
                    for Tuple in br:
                        tabWrite('%.4f,%.4f,%.4f,%.4f\n'%(Tuple[0],Tuple[1],Tuple[2],Tuple[3]))
                    tabWrite("}\n")
                if len(ob.data.splines)== 1:
                    tabWrite('#declare %s = object{\n'%dataname)
                    tabWrite('    Shape_Bezierpoints_Sphere_Sweep(yes,%s, %s_points_%s, %s_radii_%s) \n'%(ob.data.resolution_u,dataname,p,dataname,p))
                else:
                    tabWrite('#declare %s = union{\n'%dataname)
                    for p in range(len(ob.data.splines)):
                        tabWrite('    object{Shape_Bezierpoints_Sphere_Sweep(yes,%s, %s_points_%s, %s_radii_%s)} \n'%(ob.data.resolution_u,dataname,p,dataname,p))
                    #tabWrite('#include "bezier_spheresweep.inc"\n') #now inlined
                   # tabWrite('#declare %s = object{Shape_Bezierpoints_Sphere_Sweep(yes,%s, %s_bezier_points, %.4f) \n'%(dataname,ob.data.resolution_u,dataname,ob.data.bevel_depth))
            if ob.pov.curveshape in {'loft'}:
                tabWrite('object {MSM(%s,%s,"c",%s,"")\n'%(dataname,ob.pov.res_u,ob.pov.res_v))
            if ob.pov.curveshape in {'birail'}:
                splines = '%s1,%s2,%s3,%s4'%(dataname,dataname,dataname,dataname)
                tabWrite('object {Coons(%s, %s, %s, "")\n'%(splines,ob.pov.res_u,ob.pov.res_v))
            povMatName = "Default_texture"
            if ob.active_material:
                #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                try:
                    material = ob.active_material
                    writeObjectMaterial(material, ob)
                except IndexError:
                    print(me)
            #tabWrite("texture {%s}\n"%povMatName)
            if ob.pov.curveshape in {'prism'}:
                tabWrite("rotate <90,0,0>\n")
                tabWrite("scale y*-1\n" )
            tabWrite("}\n")

#################################################################


    def exportMeta(metas):

        # TODO - blenders 'motherball' naming is not supported.

        if comments and len(metas) >= 1:
            file.write("//--Blob objects--\n\n")
        # Get groups of metaballs by blender name prefix.
        meta_group = {}
        meta_elems = {}
        for ob in metas:
            prefix = ob.name.split(".")[0]
            if not prefix in meta_group:
                meta_group[prefix] = ob  # .data.threshold
            elems = [(elem, ob) for elem in ob.data.elements if elem.type in {'BALL', 'ELLIPSOID','CAPSULE','CUBE','PLANE'}]
            if prefix in meta_elems:
                meta_elems[prefix].extend(elems)
            else:
                meta_elems[prefix] = elems
                
            # empty metaball
            if len(elems)==0:
                tabWrite("\n//dummy sphere to represent empty meta location\n")
                tabWrite("sphere {<%.6g, %.6g, %.6g>,0 pigment{rgbt 1} no_image no_reflection no_radiosity photons{pass_through collect off} hollow}\n\n" % (ob.location.x, ob.location.y, ob.location.z)) # ob.name > povdataname)    
            # other metaballs    
            else:                
                for mg, ob in meta_group.items():
                    if len(meta_elems[mg])!=0:                
                        tabWrite("blob{threshold %.4g // %s \n" % (ob.data.threshold, mg))
                        for elems in meta_elems[mg]:
                            elem = elems[0]
                            loc = elem.co
                            stiffness = elem.stiffness
                            if elem.use_negative:
                                stiffness = - stiffness
                            if elem.type == 'BALL':
                                tabWrite("sphere { <%.6g, %.6g, %.6g>, %.4g, %.4g " %
                                         (loc.x, loc.y, loc.z, elem.radius, stiffness))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")                                         
                            elif elem.type == 'ELLIPSOID':
                                tabWrite("sphere{ <%.6g, %.6g, %.6g>,%.4g,%.4g " %
                                         (loc.x / elem.size_x, loc.y / elem.size_y, loc.z / elem.size_z,
                                          elem.radius, stiffness))
                                tabWrite("scale <%.6g, %.6g, %.6g>" % (elem.size_x, elem.size_y, elem.size_z))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")                                
                            elif elem.type == 'CAPSULE':
                                tabWrite("cylinder{ <%.6g, %.6g, %.6g>,<%.6g, %.6g, %.6g>,%.4g,%.4g " %
                                         ((loc.x - elem.size_x), (loc.y), (loc.z),
                                          (loc.x + elem.size_x), (loc.y), (loc.z),
                                          elem.radius, stiffness))
                                #tabWrite("scale <%.6g, %.6g, %.6g>" % (elem.size_x, elem.size_y, elem.size_z))                                
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")

                            elif elem.type == 'CUBE':
                                tabWrite("cylinder { -x*8, +x*8,%.4g,%.4g translate<%.6g,%.6g,%.6g> scale  <1/4,1,1> scale <%.6g, %.6g, %.6g>\n" % (elem.radius*2.0, stiffness/4.0, loc.x, loc.y, loc.z, elem.size_x, elem.size_y, elem.size_z))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")
                                tabWrite("cylinder { -y*8, +y*8,%.4g,%.4g translate<%.6g,%.6g,%.6g> scale <1,1/4,1> scale <%.6g, %.6g, %.6g>\n" % (elem.radius*2.0, stiffness/4.0, loc.x, loc.y, loc.z, elem.size_x, elem.size_y, elem.size_z))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")                                
                                tabWrite("cylinder { -z*8, +z*8,%.4g,%.4g translate<%.6g,%.6g,%.6g> scale <1,1,1/4> scale <%.6g, %.6g, %.6g>\n" % (elem.radius*2.0, stiffness/4.0, loc.x, loc.y, loc.z, elem.size_x, elem.size_y, elem.size_z))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")
                                
                            elif elem.type == 'PLANE':
                                tabWrite("cylinder { -x*8, +x*8,%.4g,%.4g translate<%.6g,%.6g,%.6g> scale  <1/4,1,1> scale <%.6g, %.6g, %.6g>\n" % (elem.radius*2.0, stiffness/4.0, loc.x, loc.y, loc.z, elem.size_x, elem.size_y, elem.size_z))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")
                                tabWrite("cylinder { -y*8, +y*8,%.4g,%.4g translate<%.6g,%.6g,%.6g> scale <1,1/4,1> scale <%.6g, %.6g, %.6g>\n" % (elem.radius*2.0, stiffness/4.0, loc.x, loc.y, loc.z, elem.size_x, elem.size_y, elem.size_z))
                                writeMatrix(global_matrix * elems[1].matrix_world)
                                tabWrite("}\n")            

                        try:
                            material = elems[1].data.materials[0]  # lame! - blender cant do enything else.
                        except:
                            material = None
                        if material:
                            diffuse_color = material.diffuse_color
                            trans = 1.0 - material.alpha
                            if material.use_transparency and material.transparency_method == 'RAYTRACE':
                                povFilter = material.raytrace_transparency.filter * (1.0 - material.alpha)
                                trans = (1.0 - material.alpha) - povFilter
                            else:
                                povFilter = 0.0
                            material_finish = materialNames[material.name]
                            tabWrite("pigment {srgbft<%.3g, %.3g, %.3g, %.3g, %.3g>} \n" %
                                     (diffuse_color[0], diffuse_color[1], diffuse_color[2],
                                      povFilter, trans))
                            tabWrite("finish{%s} " % safety(material_finish, Level=2))
                        else:
                            tabWrite("pigment{srgb 1} finish{%s} " % (safety(DEF_MAT_NAME, Level=2)))
                        

                            writeObjectMaterial(material, ob)
                            #writeObjectMaterial(material, elems[1])
                            tabWrite("radiosity{importance %3g}\n" % ob.pov.importance_value)
                            tabWrite("}\n\n")  # End of Metaball block                


    '''
            meta = ob.data

            # important because no elements will break parsing.
            elements = [elem for elem in meta.elements if elem.type in {'BALL', 'ELLIPSOID'}]

            if elements:
                tabWrite("blob {\n")
                tabWrite("threshold %.4g\n" % meta.threshold)
                importance = ob.pov.importance_value

                try:
                    material = meta.materials[0]  # lame! - blender cant do enything else.
                except:
                    material = None

                for elem in elements:
                    loc = elem.co

                    stiffness = elem.stiffness
                    if elem.use_negative:
                        stiffness = - stiffness

                    if elem.type == 'BALL':

                        tabWrite("sphere { <%.6g, %.6g, %.6g>, %.4g, %.4g }\n" %
                                 (loc.x, loc.y, loc.z, elem.radius, stiffness))

                        # After this wecould do something simple like...
                        #     "pigment {Blue} }"
                        # except we'll write the color

                    elif elem.type == 'ELLIPSOID':
                        # location is modified by scale
                        tabWrite("sphere { <%.6g, %.6g, %.6g>, %.4g, %.4g }\n" %
                                 (loc.x / elem.size_x,
                                  loc.y / elem.size_y,
                                  loc.z / elem.size_z,
                                  elem.radius, stiffness))
                        tabWrite("scale <%.6g, %.6g, %.6g> \n" %
                                 (elem.size_x, elem.size_y, elem.size_z))

                if material:
                    diffuse_color = material.diffuse_color
                    trans = 1.0 - material.alpha
                    if material.use_transparency and material.transparency_method == 'RAYTRACE':
                        povFilter = material.raytrace_transparency.filter * (1.0 - material.alpha)
                        trans = (1.0 - material.alpha) - povFilter
                    else:
                        povFilter = 0.0

                    material_finish = materialNames[material.name]

                    tabWrite("pigment {srgbft<%.3g, %.3g, %.3g, %.3g, %.3g>} \n" %
                             (diffuse_color[0], diffuse_color[1], diffuse_color[2],
                              povFilter, trans))
                    tabWrite("finish {%s}\n" % safety(material_finish, Level=2))

                else:
                    tabWrite("pigment {srgb 1} \n")
                    # Write the finish last.
                    tabWrite("finish {%s}\n" % (safety(DEF_MAT_NAME, Level=2)))

                writeObjectMaterial(material, elems[1])

                writeMatrix(global_matrix * ob.matrix_world)
                # Importance for radiosity sampling added here
                tabWrite("radiosity { \n")
                # importance > ob.pov.importance_value
                tabWrite("importance %3g \n" % importance)
                tabWrite("}\n")

                tabWrite("}\n")  # End of Metaball block

                if comments and len(metas) >= 1:
                    file.write("\n")
    '''
#    objectNames = {}
    DEF_OBJ_NAME = "Default"

    def exportMeshes(scene, sel):
#        obmatslist = []
#        def hasUniqueMaterial():
#            # Grab materials attached to object instances ...
#            if hasattr(ob, 'material_slots'):
#                for ms in ob.material_slots:
#                    if ms.material is not None and ms.link == 'OBJECT':
#                        if ms.material in obmatslist:
#                            return False
#                        else:
#                            obmatslist.append(ms.material)
#                            return True
#        def hasObjectMaterial(ob):
#            # Grab materials attached to object instances ...
#            if hasattr(ob, 'material_slots'):
#                for ms in ob.material_slots:
#                    if ms.material is not None and ms.link == 'OBJECT':
#                        # If there is at least one material slot linked to the object
#                        # and not the data (mesh), always create a new, "private" data instance.
#                        return True
#            return False
        # For objects using local material(s) only!
        # This is a mapping between a tuple (dataname, materialnames, ...), and the POV dataname.
        # As only objects using:
        #     * The same data.
        #     * EXACTLY the same materials, in EXACTLY the same sockets.
        # ... can share a same instance in POV export.
        obmats2data = {}

        def checkObjectMaterials(ob, name, dataname):
            if hasattr(ob, 'material_slots'):
                has_local_mats = False
                key = [dataname]
                for ms in ob.material_slots:
                    if ms.material is not None:
                        key.append(ms.material.name)
                        if ms.link == 'OBJECT' and not has_local_mats:
                            has_local_mats = True
                    else:
                        # Even if the slot is empty, it is important to grab it...
                        key.append("")
                if has_local_mats:
                    # If this object uses local material(s), lets find if another object
                    # using the same data and exactly the same list of materials
                    # (in the same slots) has already been processed...
                    # Note that here also, we use object name as new, unique dataname for Pov.
                    key = tuple(key)  # Lists are not hashable...
                    if key not in obmats2data:
                        obmats2data[key] = name
                    return obmats2data[key]
            return None

        data_ref = {}

        def store(scene, ob, name, dataname, matrix):
            # The Object needs to be written at least once but if its data is
            # already in data_ref this has already been done.
            # This func returns the "povray" name of the data, or None
            # if no writing is needed.
            if ob.is_modified(scene, 'RENDER'):
                # Data modified.
                # Create unique entry in data_ref by using object name
                # (always unique in Blender) as data name.
                data_ref[name] = [(name, MatrixAsPovString(matrix))]
                return name
            # Here, we replace dataname by the value returned by checkObjectMaterials, only if
            # it is not evaluated to False (i.e. only if the object uses some local material(s)).
            dataname = checkObjectMaterials(ob, name, dataname) or dataname
            if dataname in data_ref:
                # Data already known, just add the object instance.
                data_ref[dataname].append((name, MatrixAsPovString(matrix)))
                # No need to write data
                return None
            else:
                # Data not yet processed, create a new entry in data_ref.
                data_ref[dataname] = [(name, MatrixAsPovString(matrix))]
                return dataname


        def exportSmoke(smoke_obj_name):
            #if LuxManager.CurrentScene.name == 'preview':
                #return 1, 1, 1, 1.0
            #else:
            flowtype = -1
            smoke_obj = bpy.data.objects[smoke_obj_name]
            domain = None

            # Search smoke domain target for smoke modifiers
            for mod in smoke_obj.modifiers:
                if mod.name == 'Smoke':
                    if mod.smoke_type == 'FLOW':
                        if mod.flow_settings.smoke_flow_type == 'BOTH':
                            flowtype = 2
                        else:
                            if mod.flow_settings.smoke_flow_type == 'SMOKE':
                                flowtype = 0
                            else:
                                if mod.flow_settings.smoke_flow_type == 'FIRE':
                                    flowtype = 1

                    if mod.smoke_type == 'DOMAIN':
                        domain = smoke_obj
                        smoke_modifier = mod

            eps = 0.000001
            if domain is not None:
                #if bpy.app.version[0] >= 2 and bpy.app.version[1] >= 71:
                # Blender version 2.71 supports direct access to smoke data structure
                set = mod.domain_settings
                channeldata = []
                for v in set.density_grid:
                    channeldata.append(v.real)
                    print(v.real)
                ## Usage en voxel texture:
                # channeldata = []
                # if channel == 'density':
                    # for v in set.density_grid:
                        # channeldata.append(v.real)

                # if channel == 'fire':
                    # for v in set.flame_grid:
                        # channeldata.append(v.real)

                resolution = set.resolution_max
                big_res = []
                big_res.append(set.domain_resolution[0])
                big_res.append(set.domain_resolution[1])
                big_res.append(set.domain_resolution[2])

                if set.use_high_resolution:
                    big_res[0] = big_res[0] * (set.amplify + 1)
                    big_res[1] = big_res[1] * (set.amplify + 1)
                    big_res[2] = big_res[2] * (set.amplify + 1)
                # else:
                    # p = []
                    ##gather smoke domain settings
                    # BBox = domain.bound_box
                    # p.append([BBox[0][0], BBox[0][1], BBox[0][2]])
                    # p.append([BBox[6][0], BBox[6][1], BBox[6][2]])
                    # set = mod.domain_settings
                    # resolution = set.resolution_max
                    # smokecache = set.point_cache
                    # ret = read_cache(smokecache, set.use_high_resolution, set.amplify + 1, flowtype)
                    # res_x = ret[0]
                    # res_y = ret[1]
                    # res_z = ret[2]
                    # density = ret[3]
                    # fire = ret[4]

                    # if res_x * res_y * res_z > 0:
                        ##new cache format
                        # big_res = []
                        # big_res.append(res_x)
                        # big_res.append(res_y)
                        # big_res.append(res_z)
                    # else:
                        # max = domain.dimensions[0]
                        # if (max - domain.dimensions[1]) < -eps:
                            # max = domain.dimensions[1]

                        # if (max - domain.dimensions[2]) < -eps:
                            # max = domain.dimensions[2]

                        # big_res = [int(round(resolution * domain.dimensions[0] / max, 0)),
                                   # int(round(resolution * domain.dimensions[1] / max, 0)),
                                   # int(round(resolution * domain.dimensions[2] / max, 0))]

                    # if set.use_high_resolution:
                        # big_res = [big_res[0] * (set.amplify + 1), big_res[1] * (set.amplify + 1),
                                   # big_res[2] * (set.amplify + 1)]

                    # if channel == 'density':
                        # channeldata = density

                    # if channel == 'fire':
                        # channeldata = fire

                        # sc_fr = '%s/%s/%s/%05d' % (efutil.export_path, efutil.scene_filename(), bpy.context.scene.name, bpy.context.scene.frame_current)
                        #               if not os.path.exists( sc_fr ):
                        #                   os.makedirs(sc_fr)
                        #
                        #               smoke_filename = '%s.smoke' % bpy.path.clean_name(domain.name)
                        #               smoke_path = '/'.join([sc_fr, smoke_filename])
                        #
                        #               with open(smoke_path, 'wb') as smoke_file:
                        #                   # Binary densitygrid file format
                        #                   #
                        #                   # File header
                        #                   smoke_file.write(b'SMOKE')        #magic number
                        #                   smoke_file.write(struct.pack('<I', big_res[0]))
                        #                   smoke_file.write(struct.pack('<I', big_res[1]))
                        #                   smoke_file.write(struct.pack('<I', big_res[2]))
                        # Density data
                        #                   smoke_file.write(struct.pack('<%df'%len(channeldata), *channeldata))
                        #
                        #               LuxLog('Binary SMOKE file written: %s' % (smoke_path))

        #return big_res[0], big_res[1], big_res[2], channeldata

                mydf3 = df3.df3(big_res[0],big_res[1],big_res[2])
                sim_sizeX, sim_sizeY, sim_sizeZ = mydf3.size()
                for x in range(sim_sizeX):
                    for y in range(sim_sizeY):
                        for z in range(sim_sizeZ):
                            mydf3.set(x, y, z, channeldata[((z * sim_sizeY + y) * sim_sizeX + x)])

                mydf3.exportDF3(smokePath)
                print('Binary smoke.df3 file written in preview directory')
                if comments:
                    file.write("\n//--Smoke--\n\n")

                # Note: We start with a default unit cube.
                #       This is mandatory to read correctly df3 data - otherwise we could just directly use bbox
                #       coordinates from the start, and avoid scale/translate operations at the end...
                file.write("box{<0,0,0>, <1,1,1>\n")
                file.write("    pigment{ rgbt 1 }\n")
                file.write("    hollow\n")
                file.write("    interior{ //---------------------\n")
                file.write("        media{ method 3\n")
                file.write("               emission <1,1,1>*1\n")# 0>1 for dark smoke to white vapour
                file.write("               scattering{ 1, // Type\n")
                file.write("                  <1,1,1>*0.1\n")
                file.write("                } // end scattering\n")
                file.write("                density{density_file df3 \"%s\"\n" % (smokePath))
                file.write("                        color_map {\n")
                file.write("                        [0.00 rgb 0]\n")
                file.write("                        [0.05 rgb 0]\n")
                file.write("                        [0.20 rgb 0.2]\n")
                file.write("                        [0.30 rgb 0.6]\n")
                file.write("                        [0.40 rgb 1]\n")
                file.write("                        [1.00 rgb 1]\n")
                file.write("                       } // end color_map\n")
                file.write("               } // end of density\n")
                file.write("               samples %i // higher = more precise\n" % resolution)
                file.write("         } // end of media --------------------------\n")
                file.write("    } // end of interior\n")

                # START OF TRANSFORMATIONS

                # Size to consider here are bbox dimensions (i.e. still in object space, *before* applying
                # loc/rot/scale and other transformations (like parent stuff), aka matrix_world).
                bbox = smoke_obj.bound_box
                dim = [abs(bbox[6][0] - bbox[0][0]), abs(bbox[6][1] - bbox[0][1]), abs(bbox[6][2] - bbox[0][2])]

                # We scale our cube to get its final size and shapes but still in *object* space (same as Blender's bbox).
                file.write("scale<%.6g,%.6g,%.6g>\n" % (dim[0], dim[1], dim[2]))

                # We offset our cube such that (0,0,0) coordinate matches Blender's object center.
                file.write("translate<%.6g,%.6g,%.6g>\n" % (bbox[0][0], bbox[0][1], bbox[0][2]))

                # We apply object's transformations to get final loc/rot/size in world space!
                # Note: we could combine the two previous transformations with this matrix directly...
                writeMatrix(global_matrix * smoke_obj.matrix_world)

                # END OF TRANSFORMATIONS

                file.write("}\n")


                #file.write("               interpolate 1\n")
                #file.write("               frequency 0\n")
                #file.write("   }\n")
                #file.write("}\n")

        ob_num = 0
        for ob in sel:
            ob_num += 1

            # XXX I moved all those checks here, as there is no need to compute names
            #     for object we won't export here!
            if (ob.type in {'LAMP', 'CAMERA', #'EMPTY', #empties can bear dupligroups
                            'META', 'ARMATURE', 'LATTICE'}):
                continue
            smokeFlag=False
            for mod in ob.modifiers:
                if mod and hasattr(mod, 'smoke_type'):
                    smokeFlag=True
                    if (mod.smoke_type == 'DOMAIN'):
                        exportSmoke(ob.name)
                    break # don't render domain mesh or flow emitter mesh, skip to next object.
            if not smokeFlag:
                # Export Hair
                renderEmitter = True
                if hasattr(ob, 'particle_systems'):
                    renderEmitter = False
                    for pSys in ob.particle_systems:
                        if pSys.settings.use_render_emitter:
                            renderEmitter = True
                        for mod in [m for m in ob.modifiers if (m is not None) and (m.type == 'PARTICLE_SYSTEM')]:
                            if (pSys.settings.render_type == 'PATH') and mod.show_render and (pSys.name == mod.particle_system.name):
                                tstart = time.time()
                                texturedHair=0
                                if ob.material_slots[pSys.settings.material - 1].material and ob.active_material is not None:
                                    pmaterial = ob.material_slots[pSys.settings.material - 1].material
                                    for th in pmaterial.texture_slots:
                                        if th and th.use:
                                            if (th.texture.type == 'IMAGE' and th.texture.image) or th.texture.type != 'IMAGE':
                                                if th.use_map_color_diffuse:
                                                    texturedHair=1
                                    if pmaterial.strand.use_blender_units:
                                        strandStart = pmaterial.strand.root_size
                                        strandEnd = pmaterial.strand.tip_size
                                        strandShape = pmaterial.strand.shape
                                    else:  # Blender unit conversion
                                        strandStart = pmaterial.strand.root_size / 200.0
                                        strandEnd = pmaterial.strand.tip_size / 200.0
                                        strandShape = pmaterial.strand.shape
                                else:
                                    pmaterial = "default"  # No material assigned in blender, use default one
                                    strandStart = 0.01
                                    strandEnd = 0.01
                                    strandShape = 0.0
                                # Set the number of particles to render count rather than 3d view display
                                pSys.set_resolution(scene, ob, 'RENDER')
                                steps = pSys.settings.draw_step
                                steps = 3 ** steps # or (power of 2 rather than 3) + 1 # Formerly : len(particle.hair_keys)

                                totalNumberOfHairs = ( len(pSys.particles) + len(pSys.child_particles) )
                                #hairCounter = 0
                                file.write('#declare HairArray = array[%i] {\n' % totalNumberOfHairs)
                                for pindex in range(0, totalNumberOfHairs):

                                    #if particle.is_exist and particle.is_visible:
                                        #hairCounter += 1
                                        #controlPointCounter = 0
                                        # Each hair is represented as a separate sphere_sweep in POV-Ray.

                                        file.write('sphere_sweep{')
                                        if pSys.settings.use_hair_bspline:
                                            file.write('b_spline ')
                                            file.write('%i,\n' % (steps + 2))  # +2 because the first point needs tripling to be more than a handle in POV
                                        else:
                                            file.write('linear_spline ')
                                            file.write('%i,\n' % (steps))
                                        #changing world coordinates to object local coordinates by multiplying with inverted matrix
                                        initCo = ob.matrix_world.inverted()*(pSys.co_hair(ob, pindex, 0))
                                        if ob.material_slots[pSys.settings.material - 1].material and ob.active_material is not None:
                                            pmaterial = ob.material_slots[pSys.settings.material-1].material
                                            for th in pmaterial.texture_slots:
                                                if th and th.use and th.use_map_color_diffuse:
                                                    #treat POV textures as bitmaps
                                                    if (th.texture.type == 'IMAGE' and th.texture.image and th.texture_coords == 'UV' and ob.data.uv_textures is not None): # or (th.texture.pov.tex_pattern_type != 'emulator' and th.texture_coords == 'UV' and ob.data.uv_textures is not None):
                                                        image=th.texture.image
                                                        image_width = image.size[0]
                                                        image_height = image.size[1]
                                                        image_pixels = image.pixels[:]
                                                        uv_co = pSys.uv_on_emitter(mod, pSys.particles[pindex], pindex, 0)
                                                        x_co = round(uv_co[0] * (image_width - 1))
                                                        y_co = round(uv_co[1] * (image_height - 1))
                                                        pixelnumber = (image_width * y_co) + x_co
                                                        r = image_pixels[pixelnumber*4]
                                                        g = image_pixels[pixelnumber*4+1]
                                                        b = image_pixels[pixelnumber*4+2]
                                                        a = image_pixels[pixelnumber*4+3]
                                                        initColor=(r,g,b,a)
                                                    else:
                                                        #only overwrite variable for each competing texture for now
                                                        initColor=th.texture.evaluate((initCo[0],initCo[1],initCo[2]))
                                        for step in range(0, steps):
                                            co = ob.matrix_world.inverted()*(pSys.co_hair(ob, pindex, step))
                                        #for controlPoint in particle.hair_keys:
                                            if pSys.settings.clump_factor != 0:
                                                hDiameter = pSys.settings.clump_factor / 200.0 * random.uniform(0.5, 1)
                                            elif step == 0:
                                                hDiameter = strandStart
                                            else:
                                                hDiameter += (strandEnd-strandStart)/(pSys.settings.draw_step+1) #XXX +1 or not?
                                            if step == 0 and pSys.settings.use_hair_bspline:
                                                # Write three times the first point to compensate pov Bezier handling
                                                file.write('<%.6g,%.6g,%.6g>,%.7g,\n' % (co[0], co[1], co[2], abs(hDiameter)))
                                                file.write('<%.6g,%.6g,%.6g>,%.7g,\n' % (co[0], co[1], co[2], abs(hDiameter)))
                                                #file.write('<%.6g,%.6g,%.6g>,%.7g' % (particle.location[0], particle.location[1], particle.location[2], abs(hDiameter))) # Useless because particle location is the tip, not the root.
                                                #file.write(',\n')
                                            #controlPointCounter += 1
                                            #totalNumberOfHairs += len(pSys.particles)# len(particle.hair_keys)

                                          # Each control point is written out, along with the radius of the
                                          # hair at that point.
                                            file.write('<%.6g,%.6g,%.6g>,%.7g' % (co[0], co[1], co[2], abs(hDiameter)))

                                          # All coordinates except the last need a following comma.

                                            if step != steps - 1:
                                                file.write(',\n')
                                            else:
                                                if texturedHair:
                                                    # Write pigment and alpha (between Pov and Blender alpha 0 and 1 are reversed)
                                                    file.write('\npigment{ color srgbf < %.3g, %.3g, %.3g, %.3g> }\n' %(initColor[0], initColor[1], initColor[2], 1.0-initColor[3]))
                                                # End the sphere_sweep declaration for this hair
                                                file.write('}\n')

                                          # All but the final sphere_sweep (each array element) needs a terminating comma.
                                        if pindex != totalNumberOfHairs:
                                            file.write(',\n')
                                        else:
                                            file.write('\n')

                                # End the array declaration.

                                file.write('}\n')
                                file.write('\n')

                                if not texturedHair:
                                    # Pick up the hair material diffuse color and create a default POV-Ray hair texture.

                                    file.write('#ifndef (HairTexture)\n')
                                    file.write('  #declare HairTexture = texture {\n')
                                    file.write('    pigment {srgbt <%s,%s,%s,%s>}\n' % (pmaterial.diffuse_color[0], pmaterial.diffuse_color[1], pmaterial.diffuse_color[2], (pmaterial.strand.width_fade + 0.05)))
                                    file.write('  }\n')
                                    file.write('#end\n')
                                    file.write('\n')

                                # Dynamically create a union of the hairstrands (or a subset of them).
                                # By default use every hairstrand, commented line is for hand tweaking test renders.
                                file.write('//Increasing HairStep divides the amount of hair for test renders.\n')
                                file.write('#ifndef(HairStep) #declare HairStep = 1; #end\n')
                                file.write('union{\n')
                                file.write('  #local I = 0;\n')
                                file.write('  #while (I < %i)\n' % totalNumberOfHairs)
                                file.write('    object {HairArray[I]')
                                if not texturedHair:
                                    file.write(' texture{HairTexture}\n')
                                else:
                                    file.write('\n')
                                # Translucency of the hair:
                                file.write('        hollow\n')
                                file.write('        double_illuminate\n')
                                file.write('        interior {\n')
                                file.write('            ior 1.45\n')
                                file.write('            media {\n')
                                file.write('                scattering { 1, 10*<0.73, 0.35, 0.15> /*extinction 0*/ }\n')
                                file.write('                absorption 10/<0.83, 0.75, 0.15>\n')
                                file.write('                samples 1\n')
                                file.write('                method 2\n')
                                file.write('                density {cylindrical\n')
                                file.write('                    color_map {\n')
                                file.write('                        [0.0 rgb <0.83, 0.45, 0.35>]\n')
                                file.write('                        [0.5 rgb <0.8, 0.8, 0.4>]\n')
                                file.write('                        [1.0 rgb <1,1,1>]\n')
                                file.write('                    }\n')
                                file.write('                }\n')
                                file.write('            }\n')
                                file.write('        }\n')
                                file.write('    }\n')

                                file.write('    #local I = I + HairStep;\n')
                                file.write('  #end\n')

                                writeMatrix(global_matrix * ob.matrix_world)


                                file.write('}')
                                print('Totals hairstrands written: %i' % totalNumberOfHairs)
                                print('Number of tufts (particle systems)', len(ob.particle_systems))

                                # Set back the displayed number of particles to preview count
                                pSys.set_resolution(scene, ob, 'PREVIEW')

                                if renderEmitter == False:
                                    continue #don't render mesh, skip to next object.

    #############################################
                # Generating a name for object just like materials to be able to use it
                # (baking for now or anything else).
                # XXX I don't understand that if we are here, sel if a non-empty iterable,
                #     so this condition is always True, IMO -- mont29
                if ob.data:
                    name_orig = "OB" + ob.name
                    dataname_orig = "DATA" + ob.data.name
                elif ob.is_duplicator:
                    if ob.dupli_type == 'GROUP':
                        name_orig = "OB" + ob.name
                        dataname_orig = "DATA" + ob.dupli_group.name
                    else:
                        #hoping only dupligroups have several source datablocks
                        ob.dupli_list_create(scene)
                        for eachduplicate in ob.dupli_list:
                            dataname_orig = "DATA" + eachduplicate.object.name
                        ob.dupli_list_clear()
                elif ob.type == 'EMPTY':
                    name_orig = "OB" + ob.name
                    dataname_orig = "DATA" + ob.name
                else:
                    name_orig = DEF_OBJ_NAME
                    dataname_orig = DEF_OBJ_NAME
                name = string_strip_hyphen(bpy.path.clean_name(name_orig))
                dataname = string_strip_hyphen(bpy.path.clean_name(dataname_orig))
    ##            for slot in ob.material_slots:
    ##                if slot.material is not None and slot.link == 'OBJECT':
    ##                    obmaterial = slot.material

    #############################################

                if info_callback:
                    info_callback("Object %2.d of %2.d (%s)" % (ob_num, len(sel), ob.name))

                #if ob.type != 'MESH':
                #    continue
                # me = ob.data

                matrix = global_matrix * ob.matrix_world
                povdataname = store(scene, ob, name, dataname, matrix)
                if povdataname is None:
                    print("This is an instance of " + name)
                    continue

                print("Writing Down First Occurence of " + name)

############################################Povray Primitives
                # special exportCurves() function takes care of writing
                # lathe, sphere_sweep, birail, and loft except with modifiers
                # converted to mesh
                if not ob.is_modified(scene, 'RENDER'):
                    if ob.type == 'CURVE' and (ob.pov.curveshape in
                                    {'lathe', 'sphere_sweep', 'loft'}):
                        continue #Don't render proxy mesh, skip to next object

                if ob.pov.object_as == 'ISOSURFACE':
                    tabWrite("#declare %s = isosurface{ \n"% povdataname)
                    tabWrite("function{ \n")
                    textName = ob.pov.iso_function_text
                    if textName:
                        node_tree = bpy.context.scene.node_tree
                        for node in node_tree.nodes:
                            if node.bl_idname == "IsoPropsNode" and node.label == ob.name:
                                for inp in node.inputs:
                                    if inp:
                                        tabWrite("#declare %s = %.6g;\n"%(inp.name,inp.default_value))

                        text = bpy.data.texts[textName]
                        for line in text.lines:
                            split = line.body.split()
                            if split[0] != "#declare":
                                tabWrite("%s\n"%line.body)
                    else:
                        tabWrite("abs(x) - 2 + y")
                    tabWrite("}\n")
                    tabWrite("threshold %.6g\n"%ob.pov.threshold)
                    tabWrite("max_gradient %.6g\n"%ob.pov.max_gradient)
                    tabWrite("accuracy  %.6g\n"%ob.pov.accuracy)
                    tabWrite("contained_by { ")
                    if ob.pov.contained_by == "sphere":
                        tabWrite("sphere {0,%.6g}}\n"%ob.pov.container_scale)
                    else:
                        tabWrite("box {-%.6g,%.6g}}\n"%(ob.pov.container_scale,ob.pov.container_scale))
                    if ob.pov.all_intersections:
                        tabWrite("all_intersections\n")
                    else:
                        if ob.pov.max_trace > 1:
                            tabWrite("max_trace %.6g\n"%ob.pov.max_trace)
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    tabWrite("scale %.6g\n"%(1/ob.pov.container_scale))
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object

                if ob.pov.object_as == 'SUPERELLIPSOID':
                    tabWrite("#declare %s = superellipsoid{ <%.4f,%.4f>\n"%(povdataname,ob.pov.se_n2,ob.pov.se_n1))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'SUPERTORUS':
                    rMajor = ob.pov.st_major_radius
                    rMinor = ob.pov.st_minor_radius
                    ring = ob.pov.st_ring
                    cross = ob.pov.st_cross
                    accuracy=ob.pov.st_accuracy
                    gradient=ob.pov.st_max_gradient
                    ############Inline Supertorus macro
                    file.write("#macro Supertorus(RMj, RMn, MajorControl, MinorControl, Accuracy, MaxGradient)\n")
                    file.write("   #local CP = 2/MinorControl;\n")
                    file.write("   #local RP = 2/MajorControl;\n")
                    file.write("   isosurface {\n")
                    file.write("      function { pow( pow(abs(pow(pow(abs(x),RP) + pow(abs(z),RP), 1/RP) - RMj),CP) + pow(abs(y),CP) ,1/CP) - RMn }\n")
                    file.write("      threshold 0\n")
                    file.write("      contained_by {box {<-RMj-RMn,-RMn,-RMj-RMn>, < RMj+RMn, RMn, RMj+RMn>}}\n")
                    file.write("      #if(MaxGradient >= 1)\n")
                    file.write("         max_gradient MaxGradient\n")
                    file.write("      #else\n")
                    file.write("         evaluate 1, 10, 0.1\n")
                    file.write("      #end\n")
                    file.write("      accuracy Accuracy\n")
                    file.write("   }\n")
                    file.write("#end\n")
                    ############
                    tabWrite("#declare %s = object{ Supertorus( %.4g,%.4g,%.4g,%.4g,%.4g,%.4g)\n"%(povdataname,rMajor,rMinor,ring,cross,accuracy,gradient))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'PLANE':
                    tabWrite("#declare %s = plane{ <0,0,1>,1\n"%povdataname)
                    povMatName = "Default_texture"
                    if ob.active_material:
                         #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    #tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'BOX':
                    tabWrite("#declare %s = box { -1,1\n"%povdataname)
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    #tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'CONE':
                    br = ob.pov.cone_base_radius
                    cr = ob.pov.cone_cap_radius
                    bz = ob.pov.cone_base_z
                    cz = ob.pov.cone_cap_z
                    tabWrite("#declare %s = cone { <0,0,%.4f>,%.4f,<0,0,%.4f>,%.4f\n"%(povdataname,bz,br,cz,cr))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    #tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object

                if ob.pov.object_as == 'CYLINDER':
                    r = ob.pov.cylinder_radius
                    x2 = ob.pov.cylinder_location_cap[0]
                    y2 = ob.pov.cylinder_location_cap[1]
                    z2 = ob.pov.cylinder_location_cap[2]
                    tabWrite("#declare %s = cylinder { <0,0,0>,<%6f,%6f,%6f>,%6f\n"%(
                            povdataname,
                            x2,
                            y2,
                            z2,
                            r))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    #cylinders written at origin, translated below
                    write_object_modifiers(scene,ob,file)
                    #tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'HEIGHT_FIELD':
                    data = ""
                    filename = ob.pov.hf_filename
                    data += '"%s"'%filename
                    gamma = ' gamma %.4f'%ob.pov.hf_gamma
                    data += gamma
                    if ob.pov.hf_premultiplied:
                        data += ' premultiplied on'
                    if ob.pov.hf_smooth:
                        data += ' smooth'
                    if ob.pov.hf_water > 0:
                        data += ' water_level %.4f'%ob.pov.hf_water
                    #hierarchy = ob.pov.hf_hierarchy
                    tabWrite('#declare %s = height_field { %s\n'%(povdataname,data))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    tabWrite("rotate x*90\n")
                    tabWrite("translate <-0.5,0.5,0>\n")
                    tabWrite("scale <0,-1,0>\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'SPHERE':

                    tabWrite("#declare %s = sphere { 0,%6f\n"%(povdataname,ob.pov.sphere_radius))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    #tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object

                if ob.pov.object_as == 'TORUS':
                    tabWrite("#declare %s = torus { %.4f,%.4f\n"%(povdataname,ob.pov.torus_major_radius,ob.pov.torus_minor_radius))
                    povMatName = "Default_texture"
                    if ob.active_material:
                        #povMatName = string_strip_hyphen(bpy.path.clean_name(ob.active_material.name))
                        try:
                            material = ob.active_material
                            writeObjectMaterial(material, ob)
                        except IndexError:
                            print(me)
                    #tabWrite("texture {%s}\n"%povMatName)
                    write_object_modifiers(scene,ob,file)
                    tabWrite("rotate x*90\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


                if ob.pov.object_as == 'PARAMETRIC':
                    tabWrite("#declare %s = parametric {\n"%povdataname)
                    tabWrite("function { %s }\n"%ob.pov.x_eq)
                    tabWrite("function { %s }\n"%ob.pov.y_eq)
                    tabWrite("function { %s }\n"%ob.pov.z_eq)
                    tabWrite("<%.4f,%.4f>, <%.4f,%.4f>\n"%(ob.pov.u_min,ob.pov.v_min,ob.pov.u_max,ob.pov.v_max))
                    if ob.pov.contained_by == "sphere":
                        tabWrite("contained_by { sphere{0, 2} }\n")
                    else:
                        tabWrite("contained_by { box{-2, 2} }\n")
                    tabWrite("max_gradient %.6f\n"%ob.pov.max_gradient)
                    tabWrite("accuracy %.6f\n"%ob.pov.accuracy)
                    tabWrite("precompute 10 x,y,z\n")
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object

                if ob.pov.object_as == 'POLYCIRCLE':
                    #TODO write below macro Once:
                    #if write_polytocircle_macro_once == 0:
                    file.write("/****************************\n")
                    file.write("This macro was written by 'And'.\n")
                    file.write("Link:(http://news.povray.org/povray.binaries.scene-files/)\n")
                    file.write("****************************/\n")
                    file.write("//from math.inc:\n")
                    file.write("#macro VPerp_Adjust(V, Axis)\n")
                    file.write("   vnormalize(vcross(vcross(Axis, V), Axis))\n")
                    file.write("#end\n")
                    file.write("//Then for the actual macro\n")
                    file.write("#macro Shape_Slice_Plane_2P_1V(point1, point2, clip_direct)\n")
                    file.write("#local p1 = point1 + <0,0,0>;\n")
                    file.write("#local p2 = point2 + <0,0,0>;\n")
                    file.write("#local clip_v = vnormalize(clip_direct + <0,0,0>);\n")
                    file.write("#local direct_v1 = vnormalize(p2 - p1);\n")
                    file.write("#if(vdot(direct_v1, clip_v) = 1)\n")
                    file.write('    #error "Shape_Slice_Plane_2P_1V error: Can\'t decide plane"\n')
                    file.write("#end\n\n")
                    file.write("#local norm = -vnormalize(clip_v - direct_v1*vdot(direct_v1,clip_v));\n")
                    file.write("#local d = vdot(norm, p1);\n")
                    file.write("plane{\n")
                    file.write("norm, d\n")
                    file.write("}\n")
                    file.write("#end\n\n")
                    file.write("//polygon to circle\n")
                    file.write("#macro Shape_Polygon_To_Circle_Blending(_polygon_n, _side_face, _polygon_circumscribed_radius, _circle_radius, _height)\n")
                    file.write("#local n = int(_polygon_n);\n")
                    file.write("#if(n < 3)\n")
                    file.write("    #error ""\n")
                    file.write("#end\n\n")
                    file.write("#local front_v = VPerp_Adjust(_side_face, z);\n")
                    file.write("#if(vdot(front_v, x) >= 0)\n")
                    file.write("    #local face_ang = acos(vdot(-y, front_v));\n")
                    file.write("#else\n")
                    file.write("    #local face_ang = -acos(vdot(-y, front_v));\n")
                    file.write("#end\n")
                    file.write("#local polyg_ext_ang = 2*pi/n;\n")
                    file.write("#local polyg_outer_r = _polygon_circumscribed_radius;\n")
                    file.write("#local polyg_inner_r = polyg_outer_r*cos(polyg_ext_ang/2);\n")
                    file.write("#local cycle_r = _circle_radius;\n")
                    file.write("#local h = _height;\n")
                    file.write("#if(polyg_outer_r < 0 | cycle_r < 0 | h <= 0)\n")
                    file.write('    #error "error: each side length must be positive"\n')
                    file.write("#end\n\n")
                    file.write("#local multi = 1000;\n")
                    file.write("#local poly_obj =\n")
                    file.write("polynomial{\n")
                    file.write("4,\n")
                    file.write("xyz(0,2,2): multi*1,\n")
                    file.write("xyz(2,0,1): multi*2*h,\n")
                    file.write("xyz(1,0,2): multi*2*(polyg_inner_r-cycle_r),\n")
                    file.write("xyz(2,0,0): multi*(-h*h),\n")
                    file.write("xyz(0,0,2): multi*(-pow(cycle_r - polyg_inner_r, 2)),\n")
                    file.write("xyz(1,0,1): multi*2*h*(-2*polyg_inner_r + cycle_r),\n")
                    file.write("xyz(1,0,0): multi*2*h*h*polyg_inner_r,\n")
                    file.write("xyz(0,0,1): multi*2*h*polyg_inner_r*(polyg_inner_r - cycle_r),\n")
                    file.write("xyz(0,0,0): multi*(-pow(polyg_inner_r*h, 2))\n")
                    file.write("sturm\n")
                    file.write("}\n\n")
                    file.write("#local mockup1 =\n")
                    file.write("difference{\n")
                    file.write("    cylinder{\n")
                    file.write("    <0,0,0.0>,<0,0,h>, max(polyg_outer_r, cycle_r)\n")
                    file.write("    }\n\n")
                    file.write("    #for(i, 0, n-1)\n")
                    file.write("        object{\n")
                    file.write("        poly_obj\n")
                    file.write("        inverse\n")
                    file.write("        rotate <0, 0, -90 + degrees(polyg_ext_ang*i)>\n")
                    file.write("        }\n")
                    file.write("        object{\n")
                    file.write("        Shape_Slice_Plane_2P_1V(<polyg_inner_r,0,0>,<cycle_r,0,h>,x)\n")
                    file.write("        rotate <0, 0, -90 + degrees(polyg_ext_ang*i)>\n")
                    file.write("        }\n")
                    file.write("    #end\n")
                    file.write("}\n\n")
                    file.write("object{\n")
                    file.write("mockup1\n")
                    file.write("rotate <0, 0, degrees(face_ang)>\n")
                    file.write("}\n")
                    file.write("#end\n")
                    #Use the macro
                    ngon = ob.pov.polytocircle_ngon
                    ngonR = ob.pov.polytocircle_ngonR
                    circleR = ob.pov.polytocircle_circleR
                    tabWrite("#declare %s = object { Shape_Polygon_To_Circle_Blending(%s, z, %.4f, %.4f, 2) rotate x*180 translate z*1\n"%(povdataname,ngon,ngonR,circleR))
                    tabWrite("}\n")
                    continue #Don't render proxy mesh, skip to next object


############################################else try to export mesh
                elif ob.is_duplicator == False: #except duplis which should be instances groups for now but all duplis later
                    if ob.type == 'EMPTY':
                        tabWrite("\n//dummy sphere to represent Empty location\n")
                        tabWrite("#declare %s =sphere {<0, 0, 0>,0 pigment{rgbt 1} no_image no_reflection no_radiosity photons{pass_through collect off} hollow}\n" % povdataname)

                    try:
                        me = ob.to_mesh(scene, True, 'RENDER')

                    #XXX Here? identify the specific exception for mesh object with no data
                    #XXX So that we can write something for the dataname !
                    except:

                        # also happens when curves cant be made into meshes because of no-data
                        continue

                    importance = ob.pov.importance_value
                    if me:
                        me_materials = me.materials
                        me_faces = me.tessfaces[:]
                    #if len(me_faces)==0:
                        #tabWrite("\n//dummy sphere to represent empty mesh location\n")
                        #tabWrite("#declare %s =sphere {<0, 0, 0>,0 pigment{rgbt 1} no_image no_reflection no_radiosity photons{pass_through collect off} hollow}\n" % povdataname)


                    if not me or not me_faces:
                        tabWrite("\n//dummy sphere to represent empty mesh location\n")
                        tabWrite("#declare %s =sphere {<0, 0, 0>,0 pigment{rgbt 1} no_image no_reflection no_radiosity photons{pass_through collect off} hollow}\n" % povdataname)                    
                        continue

                    uv_textures = me.tessface_uv_textures
                    if len(uv_textures) > 0:
                        if me.uv_textures.active and uv_textures.active.data:
                            uv_layer = uv_textures.active.data
                    else:
                        uv_layer = None

                    try:
                        #vcol_layer = me.vertex_colors.active.data
                        vcol_layer = me.tessface_vertex_colors.active.data
                    except AttributeError:
                        vcol_layer = None

                    faces_verts = [f.vertices[:] for f in me_faces]
                    faces_normals = [f.normal[:] for f in me_faces]
                    verts_normals = [v.normal[:] for v in me.vertices]

                    # quads incur an extra face
                    quadCount = sum(1 for f in faces_verts if len(f) == 4)

                    # Use named declaration to allow reference e.g. for baking. MR
                    file.write("\n")
                    tabWrite("#declare %s =\n" % povdataname)
                    tabWrite("mesh2 {\n")
                    tabWrite("vertex_vectors {\n")
                    tabWrite("%d" % len(me.vertices))  # vert count

                    tabStr = tab * tabLevel
                    for v in me.vertices:
                        if linebreaksinlists:
                            file.write(",\n")
                            file.write(tabStr + "<%.6f, %.6f, %.6f>" % v.co[:])  # vert count
                        else:
                            file.write(", ")
                            file.write("<%.6f, %.6f, %.6f>" % v.co[:])  # vert count
                        #tabWrite("<%.6f, %.6f, %.6f>" % v.co[:])  # vert count
                    file.write("\n")
                    tabWrite("}\n")

                    # Build unique Normal list
                    uniqueNormals = {}
                    for fi, f in enumerate(me_faces):
                        fv = faces_verts[fi]
                        # [-1] is a dummy index, use a list so we can modify in place
                        if f.use_smooth:  # Use vertex normals
                            for v in fv:
                                key = verts_normals[v]
                                uniqueNormals[key] = [-1]
                        else:  # Use face normal
                            key = faces_normals[fi]
                            uniqueNormals[key] = [-1]

                    tabWrite("normal_vectors {\n")
                    tabWrite("%d" % len(uniqueNormals))  # vert count
                    idx = 0
                    tabStr = tab * tabLevel
                    for no, index in uniqueNormals.items():
                        if linebreaksinlists:
                            file.write(",\n")
                            file.write(tabStr + "<%.6f, %.6f, %.6f>" % no)  # vert count
                        else:
                            file.write(", ")
                            file.write("<%.6f, %.6f, %.6f>" % no)  # vert count
                        index[0] = idx
                        idx += 1
                    file.write("\n")
                    tabWrite("}\n")

                    # Vertex colors
                    vertCols = {}  # Use for material colors also.

                    if uv_layer:
                        # Generate unique UV's
                        uniqueUVs = {}
                        #n = 0
                        for fi, uv in enumerate(uv_layer):

                            if len(faces_verts[fi]) == 4:
                                uvs = uv_layer[fi].uv[0], uv_layer[fi].uv[1], uv_layer[fi].uv[2], uv_layer[fi].uv[3]
                            else:
                                uvs = uv_layer[fi].uv[0], uv_layer[fi].uv[1], uv_layer[fi].uv[2]

                            for uv in uvs:
                                uniqueUVs[uv[:]] = [-1]

                        tabWrite("uv_vectors {\n")
                        #print unique_uvs
                        tabWrite("%d" % len(uniqueUVs))  # vert count
                        idx = 0
                        tabStr = tab * tabLevel
                        for uv, index in uniqueUVs.items():
                            if linebreaksinlists:
                                file.write(",\n")
                                file.write(tabStr + "<%.6f, %.6f>" % uv)
                            else:
                                file.write(", ")
                                file.write("<%.6f, %.6f>" % uv)
                            index[0] = idx
                            idx += 1
                        '''
                        else:
                            # Just add 1 dummy vector, no real UV's
                            tabWrite('1') # vert count
                            file.write(',\n\t\t<0.0, 0.0>')
                        '''
                        file.write("\n")
                        tabWrite("}\n")

                    if me.vertex_colors:
                        #Write down vertex colors as a texture for each vertex
                        tabWrite("texture_list {\n")
                        tabWrite("%d\n" % (((len(me_faces)-quadCount) * 3 )+ quadCount * 4)) # works only with tris and quad mesh for now
                        VcolIdx=0
                        if comments:
                            file.write("\n  //Vertex colors: one simple pigment texture per vertex\n")
                        for fi, f in enumerate(me_faces):
                            # annoying, index may be invalid
                            material_index = f.material_index
                            try:
                                material = me_materials[material_index]
                            except:
                                material = None
                            if material: #and material.use_vertex_color_paint: #Always use vertex color when there is some for now

                                col = vcol_layer[fi]

                                if len(faces_verts[fi]) == 4:
                                    cols = col.color1, col.color2, col.color3, col.color4
                                else:
                                    cols = col.color1, col.color2, col.color3

                                for col in cols:
                                    key = col[0], col[1], col[2], material_index  # Material index!
                                    VcolIdx+=1
                                    vertCols[key] = [VcolIdx]
                                    if linebreaksinlists:
                                        tabWrite("texture {pigment{ color srgb <%6f,%6f,%6f> }}\n" % (col[0], col[1], col[2]))
                                    else:
                                        tabWrite("texture {pigment{ color srgb <%6f,%6f,%6f> }}" % (col[0], col[1], col[2]))
                                        tabStr = tab * tabLevel
                            else:
                                if material:
                                    # Multiply diffuse with SSS Color
                                    if material.subsurface_scattering.use:
                                        diffuse_color = [i * j for i, j in zip(material.subsurface_scattering.color[:], material.diffuse_color[:])]
                                        key = diffuse_color[0], diffuse_color[1], diffuse_color[2], \
                                              material_index
                                        vertCols[key] = [-1]
                                    else:
                                        diffuse_color = material.diffuse_color[:]
                                        key = diffuse_color[0], diffuse_color[1], diffuse_color[2], \
                                              material_index
                                        vertCols[key] = [-1]

                        tabWrite("\n}\n")
                        # Face indices
                        tabWrite("\nface_indices {\n")
                        tabWrite("%d" % (len(me_faces) + quadCount))  # faces count
                        tabStr = tab * tabLevel

                        for fi, f in enumerate(me_faces):
                            fv = faces_verts[fi]
                            material_index = f.material_index
                            if len(fv) == 4:
                                indices = (0, 1, 2), (0, 2, 3)
                            else:
                                indices = ((0, 1, 2),)

                            if vcol_layer:
                                col = vcol_layer[fi]

                                if len(fv) == 4:
                                    cols = col.color1, col.color2, col.color3, col.color4
                                else:
                                    cols = col.color1, col.color2, col.color3

                            if not me_materials or me_materials[material_index] is None:  # No materials
                                for i1, i2, i3 in indices:
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        # vert count
                                        file.write(tabStr + "<%d,%d,%d>" % (fv[i1], fv[i2], fv[i3]))
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" % (fv[i1], fv[i2], fv[i3]))  # vert count
                            else:
                                material = me_materials[material_index]
                                for i1, i2, i3 in indices:
                                    if me.vertex_colors: #and material.use_vertex_color_paint:
                                        # Color per vertex - vertex color

                                        col1 = cols[i1]
                                        col2 = cols[i2]
                                        col3 = cols[i3]

                                        ci1 = vertCols[col1[0], col1[1], col1[2], material_index][0]
                                        ci2 = vertCols[col2[0], col2[1], col2[2], material_index][0]
                                        ci3 = vertCols[col3[0], col3[1], col3[2], material_index][0]
                                    else:
                                        # Color per material - flat material color
                                        if material.subsurface_scattering.use:
                                            diffuse_color = [i * j for i, j in zip(material.subsurface_scattering.color[:], material.diffuse_color[:])]
                                        else:
                                            diffuse_color = material.diffuse_color[:]
                                        ci1 = ci2 = ci3 = vertCols[diffuse_color[0], diffuse_color[1], \
                                                          diffuse_color[2], f.material_index][0]
                                        # ci are zero based index so we'll subtract 1 from them
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>, %d,%d,%d" % \
                                                   (fv[i1], fv[i2], fv[i3], ci1-1, ci2-1, ci3-1))  # vert count
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>, %d,%d,%d" % \
                                                   (fv[i1], fv[i2], fv[i3], ci1-1, ci2-1, ci3-1))  # vert count

                        file.write("\n")
                        tabWrite("}\n")

                        # normal_indices indices
                        tabWrite("normal_indices {\n")
                        tabWrite("%d" % (len(me_faces) + quadCount))  # faces count
                        tabStr = tab * tabLevel
                        for fi, fv in enumerate(faces_verts):

                            if len(fv) == 4:
                                indices = (0, 1, 2), (0, 2, 3)
                            else:
                                indices = ((0, 1, 2),)

                            for i1, i2, i3 in indices:
                                if me_faces[fi].use_smooth:
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>" %\
                                        (uniqueNormals[verts_normals[fv[i1]]][0],\
                                         uniqueNormals[verts_normals[fv[i2]]][0],\
                                         uniqueNormals[verts_normals[fv[i3]]][0]))  # vert count
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" %\
                                        (uniqueNormals[verts_normals[fv[i1]]][0],\
                                         uniqueNormals[verts_normals[fv[i2]]][0],\
                                         uniqueNormals[verts_normals[fv[i3]]][0]))  # vert count
                                else:
                                    idx = uniqueNormals[faces_normals[fi]][0]
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>" % (idx, idx, idx))  # vert count
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" % (idx, idx, idx))  # vert count

                        file.write("\n")
                        tabWrite("}\n")

                        if uv_layer:
                            tabWrite("uv_indices {\n")
                            tabWrite("%d" % (len(me_faces) + quadCount))  # faces count
                            tabStr = tab * tabLevel
                            for fi, fv in enumerate(faces_verts):

                                if len(fv) == 4:
                                    indices = (0, 1, 2), (0, 2, 3)
                                else:
                                    indices = ((0, 1, 2),)

                                uv = uv_layer[fi]
                                if len(faces_verts[fi]) == 4:
                                    uvs = uv.uv[0][:], uv.uv[1][:], uv.uv[2][:], uv.uv[3][:]
                                else:
                                    uvs = uv.uv[0][:], uv.uv[1][:], uv.uv[2][:]

                                for i1, i2, i3 in indices:
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>" % (
                                                 uniqueUVs[uvs[i1]][0],\
                                                 uniqueUVs[uvs[i2]][0],\
                                                 uniqueUVs[uvs[i3]][0]))
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" % (
                                                 uniqueUVs[uvs[i1]][0],\
                                                 uniqueUVs[uvs[i2]][0],\
                                                 uniqueUVs[uvs[i3]][0]))

                            file.write("\n")
                            tabWrite("}\n")
                            
                        #XXX BOOLEAN
                        onceCSG = 0
                        for mod in ob.modifiers:
                            if onceCSG == 0:
                                if mod :
                                    if mod.type == 'BOOLEAN':
                                        if ob.pov.boolean_mod == "POV":
                                            file.write("\tinside_vector <%.6g, %.6g, %.6g>\n" %
                                                       (ob.pov.inside_vector[0],
                                                        ob.pov.inside_vector[1],
                                                        ob.pov.inside_vector[2]))
                                            onceCSG = 1
 
                        if me.materials:
                            try:
                                material = me.materials[0]  # dodgy
                                writeObjectMaterial(material, ob)
                            except IndexError:
                                print(me)

                        # POV object modifiers such as 
                        # hollow / sturm / double_illuminate etc.
                        write_object_modifiers(scene,ob,file)                        
 
                        #Importance for radiosity sampling added here:
                        tabWrite("radiosity { \n")
                        tabWrite("importance %3g \n" % importance)
                        tabWrite("}\n")

                        tabWrite("}\n")  # End of mesh block
                    else:
                        facesMaterials = [] # WARNING!!!!!!!!!!!!!!!!!!!!!!
                        if me_materials:
                            for f in me_faces:
                                if f.material_index not in facesMaterials:
                                    facesMaterials.append(f.material_index)
                        # No vertex colors, so write material colors as vertex colors
                        for i, material in enumerate(me_materials):

                            if material and material.pov.material_use_nodes == False:  # WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                                # Multiply diffuse with SSS Color
                                if material.subsurface_scattering.use:
                                    diffuse_color = [i * j for i, j in zip(material.subsurface_scattering.color[:], material.diffuse_color[:])]
                                    key = diffuse_color[0], diffuse_color[1], diffuse_color[2], i  # i == f.mat
                                    vertCols[key] = [-1]
                                else:
                                    diffuse_color = material.diffuse_color[:]
                                    key = diffuse_color[0], diffuse_color[1], diffuse_color[2], i  # i == f.mat
                                    vertCols[key] = [-1]

                                idx = 0
                                LocalMaterialNames = []
                                for col, index in vertCols.items():
                                    #if me_materials:
                                    mater = me_materials[col[3]]
                                    if me_materials is None: #XXX working?
                                        material_finish = DEF_MAT_NAME  # not working properly,
                                        trans = 0.0

                                    else:
                                        shading.writeTextureInfluence(mater, materialNames,
                                                                        LocalMaterialNames,
                                                                        path_image, lampCount,
                                                                        imageFormat, imgMap,
                                                                        imgMapTransforms,
                                                                        tabWrite, comments,
                                                                        string_strip_hyphen,
                                                                        safety, col, os, preview_dir,  unpacked_images)
                                    ###################################################################
                                    index[0] = idx
                                    idx += 1



                        # Vert Colors
                        tabWrite("texture_list {\n")
                        # In case there's is no material slot, give at least one texture
                        #(an empty one so it uses pov default)
                        if len(vertCols)==0:
                            file.write(tabStr + "1")
                        else:
                            file.write(tabStr + "%s" % (len(vertCols)))  # vert count




                        # below "material" alias, added check ob.active_material
                        # to avoid variable referenced before assignment error
                        try:
                            material = ob.active_material
                        except IndexError:
                            #when no material slot exists,
                            material=None

                        # WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        if material and ob.active_material is not None and material.pov.material_use_nodes == False:
                            if material.pov.replacement_text != "":
                                file.write("\n")
                                file.write(" texture{%s}\n" % material.pov.replacement_text)

                            else:
                                # Loop through declared materials list
                                for cMN in LocalMaterialNames:
                                    if material != "Default":
                                        file.write("\n texture{MAT_%s}\n" % cMN)
                                        #use string_strip_hyphen(materialNames[material]))
                                        #or Something like that to clean up the above?
                        elif material and material.pov.material_use_nodes:
                            for index in facesMaterials:
                                faceMaterial = string_strip_hyphen(bpy.path.clean_name(me_materials[index].name))
                                file.write("\n texture{%s}\n" % faceMaterial)
                        # END!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        else:
                            file.write(" texture{}\n")
                        tabWrite("}\n")

                        # Face indices
                        tabWrite("face_indices {\n")
                        tabWrite("%d" % (len(me_faces) + quadCount))  # faces count
                        tabStr = tab * tabLevel

                        for fi, f in enumerate(me_faces):
                            fv = faces_verts[fi]
                            material_index = f.material_index
                            if len(fv) == 4:
                                indices = (0, 1, 2), (0, 2, 3)
                            else:
                                indices = ((0, 1, 2),)

                            if vcol_layer:
                                col = vcol_layer[fi]

                                if len(fv) == 4:
                                    cols = col.color1, col.color2, col.color3, col.color4
                                else:
                                    cols = col.color1, col.color2, col.color3

                            if not me_materials or me_materials[material_index] is None:  # No materials
                                for i1, i2, i3 in indices:
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        # vert count
                                        file.write(tabStr + "<%d,%d,%d>" % (fv[i1], fv[i2], fv[i3]))
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" % (fv[i1], fv[i2], fv[i3]))  # vert count
                            else:
                                material = me_materials[material_index]
                                for i1, i2, i3 in indices:
                                    ci1 = ci2 = ci3 = f.material_index
                                    if me.vertex_colors: #and material.use_vertex_color_paint:
                                        # Color per vertex - vertex color

                                        col1 = cols[i1]
                                        col2 = cols[i2]
                                        col3 = cols[i3]

                                        ci1 = vertCols[col1[0], col1[1], col1[2], material_index][0]
                                        ci2 = vertCols[col2[0], col2[1], col2[2], material_index][0]
                                        ci3 = vertCols[col3[0], col3[1], col3[2], material_index][0]
                                    elif material.pov.material_use_nodes:
                                        ci1 = ci2 = ci3 = 0
                                    else:
                                        # Color per material - flat material color
                                        if material.subsurface_scattering.use:
                                            diffuse_color = [i * j for i, j in
                                                zip(material.subsurface_scattering.color[:],
                                                    material.diffuse_color[:])]
                                        else:
                                            diffuse_color = material.diffuse_color[:]
                                        ci1 = ci2 = ci3 = vertCols[diffuse_color[0], diffuse_color[1], \
                                                          diffuse_color[2], f.material_index][0]

                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>, %d,%d,%d" % \
                                                   (fv[i1], fv[i2], fv[i3], ci1, ci2, ci3))  # vert count
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>, %d,%d,%d" % \
                                                   (fv[i1], fv[i2], fv[i3], ci1, ci2, ci3))  # vert count

                        file.write("\n")
                        tabWrite("}\n")

                        # normal_indices indices
                        tabWrite("normal_indices {\n")
                        tabWrite("%d" % (len(me_faces) + quadCount))  # faces count
                        tabStr = tab * tabLevel
                        for fi, fv in enumerate(faces_verts):

                            if len(fv) == 4:
                                indices = (0, 1, 2), (0, 2, 3)
                            else:
                                indices = ((0, 1, 2),)

                            for i1, i2, i3 in indices:
                                if me_faces[fi].use_smooth:
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>" %\
                                        (uniqueNormals[verts_normals[fv[i1]]][0],\
                                         uniqueNormals[verts_normals[fv[i2]]][0],\
                                         uniqueNormals[verts_normals[fv[i3]]][0]))  # vert count
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" %\
                                        (uniqueNormals[verts_normals[fv[i1]]][0],\
                                         uniqueNormals[verts_normals[fv[i2]]][0],\
                                         uniqueNormals[verts_normals[fv[i3]]][0]))  # vert count
                                else:
                                    idx = uniqueNormals[faces_normals[fi]][0]
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>" % (idx, idx, idx)) # vertcount
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" % (idx, idx, idx))  # vert count

                        file.write("\n")
                        tabWrite("}\n")

                        if uv_layer:
                            tabWrite("uv_indices {\n")
                            tabWrite("%d" % (len(me_faces) + quadCount))  # faces count
                            tabStr = tab * tabLevel
                            for fi, fv in enumerate(faces_verts):

                                if len(fv) == 4:
                                    indices = (0, 1, 2), (0, 2, 3)
                                else:
                                    indices = ((0, 1, 2),)

                                uv = uv_layer[fi]
                                if len(faces_verts[fi]) == 4:
                                    uvs = uv.uv[0][:], uv.uv[1][:], uv.uv[2][:], uv.uv[3][:]
                                else:
                                    uvs = uv.uv[0][:], uv.uv[1][:], uv.uv[2][:]

                                for i1, i2, i3 in indices:
                                    if linebreaksinlists:
                                        file.write(",\n")
                                        file.write(tabStr + "<%d,%d,%d>" % (
                                                 uniqueUVs[uvs[i1]][0],\
                                                 uniqueUVs[uvs[i2]][0],\
                                                 uniqueUVs[uvs[i3]][0]))
                                    else:
                                        file.write(", ")
                                        file.write("<%d,%d,%d>" % (
                                                 uniqueUVs[uvs[i1]][0],\
                                                 uniqueUVs[uvs[i2]][0],\
                                                 uniqueUVs[uvs[i3]][0]))

                            file.write("\n")
                            tabWrite("}\n")
                            
                        #XXX BOOLEAN
                        onceCSG = 0
                        for mod in ob.modifiers:
                            if onceCSG == 0:
                                if mod :
                                    if mod.type == 'BOOLEAN':
                                        if ob.pov.boolean_mod == "POV":
                                            file.write("\tinside_vector <%.6g, %.6g, %.6g>\n" %
                                                       (ob.pov.inside_vector[0],
                                                        ob.pov.inside_vector[1],
                                                        ob.pov.inside_vector[2]))
                                            onceCSG = 1
 
                        if me.materials:
                            try:
                                material = me.materials[0]  # dodgy
                                writeObjectMaterial(material, ob)
                            except IndexError:
                                print(me)

                        # POV object modifiers such as 
                        # hollow / sturm / double_illuminate etc.
                        write_object_modifiers(scene,ob,file)                        
                                
                        #Importance for radiosity sampling added here:
                        tabWrite("radiosity { \n")
                        tabWrite("importance %3g \n" % importance)
                        tabWrite("}\n")

                        tabWrite("}\n")  # End of mesh block



                    bpy.data.meshes.remove(me)

        duplidata_ref = []
        for ob in sel:
            #matrix = global_matrix * ob.matrix_world
            if ob.is_duplicator:
                tabWrite("\n//--DupliObjects in %s--\n\n"% ob.name)
                ob.dupli_list_create(scene)
                dup = ""
                if ob.is_modified(scene, 'RENDER'):
                    #modified object always unique so using object name rather than data name
                    dup = "#declare OB%s = union{\n" %(string_strip_hyphen(bpy.path.clean_name(ob.name)))
                else:
                    dup = "#declare DATA%s = union{\n" %(string_strip_hyphen(bpy.path.clean_name(ob.name)))
                for eachduplicate in ob.dupli_list:
                    duplidataname = "OB"+string_strip_hyphen(bpy.path.clean_name(bpy.data.objects[eachduplicate.object.name].data.name))
                    dup += ("\tobject {\n\t\tDATA%s\n\t\t%s\t}\n" %(string_strip_hyphen(bpy.path.clean_name(bpy.data.objects[eachduplicate.object.name].data.name)), MatrixAsPovString(ob.matrix_world.inverted() * eachduplicate.matrix)))
                    #add object to a list so that it is not rendered for some dupli_types
                    if ob.dupli_type not in {'GROUP'} and duplidataname not in duplidata_ref:
                        duplidata_ref.append(duplidataname) #older key [string_strip_hyphen(bpy.path.clean_name("OB"+ob.name))]
                dup += "}\n"
                ob.dupli_list_clear()
                tabWrite(dup)
            else:
                continue
        print(duplidata_ref)
        for data_name, inst in data_ref.items():
            for ob_name, matrix_str in inst:
                if ob_name not in duplidata_ref: #.items() for a dictionary
                    tabWrite("\n//----Blender Object Name:%s----\n" % ob_name)
                    tabWrite("object { \n")
                    tabWrite("%s\n" % data_name)
                    tabWrite("%s\n" % matrix_str)
                    tabWrite("}\n")

    def exportWorld(world):
        render = scene.render
        camera = scene.camera
        matrix = global_matrix * camera.matrix_world
        if not world:
            return
        #############Maurice####################################
        #These lines added to get sky gradient (visible with PNG output)
        if world:
            #For simple flat background:
            if not world.use_sky_blend:
                # Non fully transparent background could premultiply alpha and avoid anti-aliasing
                # display issue:
                if render.alpha_mode == 'TRANSPARENT':
                    tabWrite("background {rgbt<%.3g, %.3g, %.3g, 0.75>}\n" % \
                             (world.horizon_color[:]))
                #Currently using no alpha with Sky option:
                elif render.alpha_mode == 'SKY':
                    tabWrite("background {rgbt<%.3g, %.3g, %.3g, 0>}\n" % (world.horizon_color[:]))
                #StraightAlpha:
                # XXX Does not exists anymore
                #else:
                    #tabWrite("background {rgbt<%.3g, %.3g, %.3g, 1>}\n" % (world.horizon_color[:]))

            worldTexCount = 0
            #For Background image textures
            for t in world.texture_slots:  # risk to write several sky_spheres but maybe ok.
                if t and t.texture.type is not None:
                    worldTexCount += 1
                # XXX No enable checkbox for world textures yet (report it?)
                #if t and t.texture.type == 'IMAGE' and t.use:
                if t and t.texture.type == 'IMAGE':
                    image_filename = path_image(t.texture.image)
                    if t.texture.image.filepath != image_filename:
                        t.texture.image.filepath = image_filename
                    if image_filename != "" and t.use_map_blend:
                        texturesBlend = image_filename
                        #colvalue = t.default_value
                        t_blend = t

                    # Commented below was an idea to make the Background image oriented as camera
                    # taken here:
#http://news.pov.org/pov.newusers/thread/%3Cweb.4a5cddf4e9c9822ba2f93e20@news.pov.org%3E/
                    # Replace 4/3 by the ratio of each image found by some custom or existing
                    # function
                    #mappingBlend = (" translate <%.4g,%.4g,%.4g> rotate z*degrees" \
                    #                "(atan((camLocation - camLookAt).x/(camLocation - " \
                    #                "camLookAt).y)) rotate x*degrees(atan((camLocation - " \
                    #                "camLookAt).y/(camLocation - camLookAt).z)) rotate y*" \
                    #                "degrees(atan((camLocation - camLookAt).z/(camLocation - " \
                    #                "camLookAt).x)) scale <%.4g,%.4g,%.4g>b" % \
                    #                (t_blend.offset.x / 10 , t_blend.offset.y / 10 ,
                    #                 t_blend.offset.z / 10, t_blend.scale.x ,
                    #                 t_blend.scale.y , t_blend.scale.z))
                    #using camera rotation valuesdirectly from blender seems much easier
                    if t_blend.texture_coords == 'ANGMAP':
                        mappingBlend = ""
                    else:
                        # POV-Ray "scale" is not a number of repetitions factor, but its
                        # inverse, a standard scale factor.
                        # 0.5 Offset is needed relatively to scale because center of the
                        # UV scale is 0.5,0.5 in blender and 0,0 in POV
                        # Further Scale by 2 and translate by -1 are
                        # required for the sky_sphere not to repeat

                        mappingBlend = "scale 2 scale <%.4g,%.4g,%.4g> translate -1 " \
                                       "translate <%.4g,%.4g,%.4g> rotate<0,0,0> " % \
                                       ((1.0 / t_blend.scale.x),
                                       (1.0 / t_blend.scale.y),
                                       (1.0 / t_blend.scale.z),
                                       0.5-(0.5/t_blend.scale.x)- t_blend.offset.x,
                                       0.5-(0.5/t_blend.scale.y)- t_blend.offset.y,
                                       t_blend.offset.z)

                        # The initial position and rotation of the pov camera is probably creating
                        # the rotation offset should look into it someday but at least background
                        # won't rotate with the camera now.
                    # Putting the map on a plane would not introduce the skysphere distortion and
                    # allow for better image scale matching but also some waay to chose depth and
                    # size of the plane relative to camera.
                    tabWrite("sky_sphere {\n")
                    tabWrite("pigment {\n")
                    tabWrite("image_map{%s \"%s\" %s}\n" % \
                             (imageFormat(texturesBlend), texturesBlend, imgMapBG(t_blend)))
                    tabWrite("}\n")
                    tabWrite("%s\n" % (mappingBlend))
                    # The following layered pigment opacifies to black over the texture for
                    # transmit below 1 or otherwise adds to itself
                    tabWrite("pigment {rgb 0 transmit %s}\n" % (t.texture.intensity))
                    tabWrite("}\n")
                    #tabWrite("scale 2\n")
                    #tabWrite("translate -1\n")

            #For only Background gradient

            if worldTexCount == 0:
                if world.use_sky_blend:
                    tabWrite("sky_sphere {\n")
                    tabWrite("pigment {\n")
                    # maybe Should follow the advice of POV doc about replacing gradient
                    # for skysphere..5.5
                    tabWrite("gradient y\n")
                    tabWrite("color_map {\n")
                    # XXX Does not exists anymore
                    #if render.alpha_mode == 'STRAIGHT':
                        #tabWrite("[0.0 rgbt<%.3g, %.3g, %.3g, 1>]\n" % (world.horizon_color[:]))
                        #tabWrite("[1.0 rgbt<%.3g, %.3g, %.3g, 1>]\n" % (world.zenith_color[:]))
                    if render.alpha_mode == 'TRANSPARENT':
                        tabWrite("[0.0 rgbt<%.3g, %.3g, %.3g, 0.99>]\n" % (world.horizon_color[:]))
                        # aa premult not solved with transmit 1
                        tabWrite("[1.0 rgbt<%.3g, %.3g, %.3g, 0.99>]\n" % (world.zenith_color[:]))
                    else:
                        tabWrite("[0.0 rgbt<%.3g, %.3g, %.3g, 0>]\n" % (world.horizon_color[:]))
                        tabWrite("[1.0 rgbt<%.3g, %.3g, %.3g, 0>]\n" % (world.zenith_color[:]))
                    tabWrite("}\n")
                    tabWrite("}\n")
                    tabWrite("}\n")
                    # Sky_sphere alpha (transmit) is not translating into image alpha the same
                    # way as 'background'

            #if world.light_settings.use_indirect_light:
            #    scene.pov.radio_enable=1

            # Maybe change the above to a funtion copyInternalRenderer settings when
            # user pushes a button, then:
            #scene.pov.radio_enable = world.light_settings.use_indirect_light
            # and other such translations but maybe this would not be allowed either?

        ###############################################################

        mist = world.mist_settings

        if mist.use_mist:
            tabWrite("fog {\n")
            tabWrite("distance %.6f\n" % mist.depth)
            tabWrite("color rgbt<%.3g, %.3g, %.3g, %.3g>\n" % \
                     (*world.horizon_color, 1.0 - mist.intensity))
            #tabWrite("fog_offset %.6f\n" % mist.start)
            #tabWrite("fog_alt 5\n")
            #tabWrite("turbulence 0.2\n")
            #tabWrite("turb_depth 0.3\n")
            tabWrite("fog_type 1\n")
            tabWrite("}\n")
        if scene.pov.media_enable:
            tabWrite("media {\n")
            tabWrite("scattering { 1, rgb <%.4g, %.4g, %.4g>}\n" % scene.pov.media_color[:])
            tabWrite("samples %.d\n" % scene.pov.media_samples)
            tabWrite("}\n")

    def exportGlobalSettings(scene):

        tabWrite("global_settings {\n")
        tabWrite("assumed_gamma 1.0\n")
        tabWrite("max_trace_level %d\n" % scene.pov.max_trace_level)

        if scene.pov.charset != 'ascii':
            file.write("    charset %s\n"%scene.pov.charset)
        if scene.pov.global_settings_advanced:
            if scene.pov.radio_enable == False:
                file.write("    adc_bailout %.6f\n"%scene.pov.adc_bailout)
            file.write("    ambient_light <%.6f,%.6f,%.6f>\n"%scene.pov.ambient_light[:])
            file.write("    irid_wavelength <%.6f,%.6f,%.6f>\n"%scene.pov.irid_wavelength[:])
            file.write("    max_intersections %s\n"%scene.pov.max_intersections)
            file.write("    number_of_waves %s\n"%scene.pov.number_of_waves)
            file.write("    noise_generator %s\n"%scene.pov.noise_generator)
        if scene.pov.radio_enable:
            tabWrite("radiosity {\n")
            tabWrite("adc_bailout %.4g\n" % scene.pov.radio_adc_bailout)
            tabWrite("brightness %.4g\n" % scene.pov.radio_brightness)
            tabWrite("count %d\n" % scene.pov.radio_count)
            tabWrite("error_bound %.4g\n" % scene.pov.radio_error_bound)
            tabWrite("gray_threshold %.4g\n" % scene.pov.radio_gray_threshold)
            tabWrite("low_error_factor %.4g\n" % scene.pov.radio_low_error_factor)
            tabWrite("maximum_reuse %.4g\n" % scene.pov.radio_maximum_reuse)
            tabWrite("minimum_reuse %.4g\n" % scene.pov.radio_minimum_reuse)
            tabWrite("nearest_count %d\n" % scene.pov.radio_nearest_count)
            tabWrite("pretrace_start %.3g\n" % scene.pov.radio_pretrace_start)
            tabWrite("pretrace_end %.3g\n" % scene.pov.radio_pretrace_end)
            tabWrite("recursion_limit %d\n" % scene.pov.radio_recursion_limit)
            tabWrite("always_sample %d\n" % scene.pov.radio_always_sample)
            tabWrite("normal %d\n" % scene.pov.radio_normal)
            tabWrite("media %d\n" % scene.pov.radio_media)
            tabWrite("subsurface %d\n" % scene.pov.radio_subsurface)
            tabWrite("}\n")
        onceSss = 1
        onceAmbient = 1
        oncePhotons = 1
        for material in bpy.data.materials:
            if material.subsurface_scattering.use and onceSss:
                # In pov, the scale has reversed influence compared to blender. these number
                # should correct that
                tabWrite("mm_per_unit %.6f\n" % \
                         (material.subsurface_scattering.scale * 1000.0))
                         # 1000 rather than scale * (-100.0) + 15.0))

                # In POV-Ray, the scale factor for all subsurface shaders needs to be the same

                # formerly sslt_samples were multiplied by 100 instead of 10
                sslt_samples = (11 - material.subsurface_scattering.error_threshold) * 10

                tabWrite("subsurface { samples %d, %d }\n" % (sslt_samples, sslt_samples / 10))
                onceSss = 0

            if world and onceAmbient:
                tabWrite("ambient_light rgb<%.3g, %.3g, %.3g>\n" % world.ambient_color[:])
                onceAmbient = 0

            if scene.pov.photon_enable:
                if (oncePhotons and
                        (material.pov.refraction_type == "2" or
                        material.pov.photons_reflection == True)):
                    tabWrite("photons {\n")
                    tabWrite("spacing %.6f\n" % scene.pov.photon_spacing)
                    tabWrite("max_trace_level %d\n" % scene.pov.photon_max_trace_level)
                    tabWrite("adc_bailout %.3g\n" % scene.pov.photon_adc_bailout)
                    tabWrite("gather %d, %d\n" % (scene.pov.photon_gather_min,
                        scene.pov.photon_gather_max))
                    if scene.pov.photon_map_file_save_load in {'save'}:
                        filePhName = 'Photon_map_file.ph'
                        if scene.pov.photon_map_file != '':
                            filePhName = scene.pov.photon_map_file+'.ph'
                        filePhDir = tempfile.gettempdir()
                        path = bpy.path.abspath(scene.pov.photon_map_dir)
                        if os.path.exists(path):
                            filePhDir = path
                        fullFileName = os.path.join(filePhDir,filePhName)
                        tabWrite('save_file "%s"\n'%fullFileName)
                        scene.pov.photon_map_file = fullFileName
                    if scene.pov.photon_map_file_save_load in {'load'}:
                        fullFileName = bpy.path.abspath(scene.pov.photon_map_file)
                        if os.path.exists(fullFileName):
                            tabWrite('load_file "%s"\n'%fullFileName)                        
                    tabWrite("}\n")
                    oncePhotons = 0

        tabWrite("}\n")

    def exportCustomCode():
        # Write CurrentAnimation Frame for use in Custom POV Code
        file.write("#declare CURFRAMENUM = %d;\n" % bpy.context.scene.frame_current)
        #Change path and uncomment to add an animated include file by hand:
        file.write("//#include \"/home/user/directory/animation_include_file.inc\"\n")
        for txt in bpy.data.texts:
            if txt.pov.custom_code == 'both':
                # Why are the newlines needed?
                file.write("\n")
                file.write(txt.as_string())
                file.write("\n")

    sel = renderable_objects(scene)
    if comments:
        file.write("//----------------------------------------------\n" \
                   "//--Exported with POV-Ray exporter for Blender--\n" \
                   "//----------------------------------------------\n\n")
    file.write("#version 3.7;\n")

    if comments:
        file.write("\n//--Global settings--\n\n")

    exportGlobalSettings(scene)


    if comments:
        file.write("\n//--Custom Code--\n\n")
    exportCustomCode()

    if comments:
        file.write("\n//--Patterns Definitions--\n\n")
    LocalPatternNames = []
    for texture in bpy.data.textures: #ok?
        if texture.users > 0:
            currentPatName = string_strip_hyphen(bpy.path.clean_name(texture.name))
            #string_strip_hyphen(patternNames[texture.name]) #maybe instead of the above
            LocalPatternNames.append(currentPatName)
            #use above list to prevent writing texture instances several times and assign in mats?
            if (texture.type not in {'NONE', 'IMAGE'} and texture.pov.tex_pattern_type == 'emulator')or(texture.type in {'NONE', 'IMAGE'} and texture.pov.tex_pattern_type != 'emulator'):
                file.write("\n#declare PAT_%s = \n" % currentPatName)
                file.write(shading.exportPattern(texture, string_strip_hyphen))
            file.write("\n")
    if comments:
        file.write("\n//--Background--\n\n")

    exportWorld(scene.world)

    if comments:
        file.write("\n//--Cameras--\n\n")

    exportCamera()

    if comments:
        file.write("\n//--Lamps--\n\n")

    exportLamps([L for L in sel if (L.type == 'LAMP' and L.pov.object_as != 'RAINBOW')])

    if comments:
        file.write("\n//--Rainbows--\n\n")
    exportRainbows([L for L in sel if (L.type == 'LAMP' and L.pov.object_as == 'RAINBOW')])


    if comments:
        file.write("\n//--Special Curves--\n\n")
    for c in sel:
        if c.is_modified(scene, 'RENDER'):
            continue #don't export as pov curves objects with modifiers, but as mesh
        elif c.type == 'CURVE' and (c.pov.curveshape in {'lathe','sphere_sweep','loft','birail'}):
            exportCurves(scene,c)


    if comments:
        file.write("\n//--Material Definitions--\n\n")
    # write a default pigment for objects with no material (comment out to show black)
    file.write("#default{ pigment{ color srgb 0.8 }}\n")
    # Convert all materials to strings we can access directly per vertex.
    #exportMaterials()
    shading.writeMaterial(using_uberpov, DEF_MAT_NAME, scene, tabWrite, safety, comments, uniqueName, materialNames, None)  # default material
    for material in bpy.data.materials:
        if material.users > 0:
            if material.pov.material_use_nodes:
                ntree = material.node_tree
                povMatName=string_strip_hyphen(bpy.path.clean_name(material.name))
                if len(ntree.nodes)==0:
                    file.write('#declare %s = texture {%s}\n'%(povMatName,color))
                else:
                    shading.write_nodes(scene,povMatName,ntree,file)

                for node in ntree.nodes:
                    if node:
                        if node.bl_idname == "PovrayOutputNode":
                            if node.inputs["Texture"].is_linked:
                                for link in ntree.links:
                                    if link.to_node.bl_idname == "PovrayOutputNode":
                                        povMatName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                            else:
                                file.write('#declare %s = texture {%s}\n'%(povMatName,color))
            else:
                shading.writeMaterial(using_uberpov, DEF_MAT_NAME, scene, tabWrite, safety, comments, uniqueName, materialNames, material)
            # attributes are all the variables needed by the other python file...
    if comments:
        file.write("\n")

    exportMeta([m for m in sel if m.type == 'META'])

    if comments:
        file.write("//--Mesh objects--\n")

    exportMeshes(scene, sel)

    #What follow used to happen here:
    #exportCamera()
    #exportWorld(scene.world)
    #exportGlobalSettings(scene)
    # MR:..and the order was important for implementing pov 3.7 baking
    #      (mesh camera) comment for the record
    # CR: Baking should be a special case than. If "baking", than we could change the order.

    #print("pov file closed %s" % file.closed)
    file.close()
    #print("pov file closed %s" % file.closed)


def write_pov_ini(scene, filename_ini, filename_log, filename_pov, filename_image):
    feature_set = bpy.context.user_preferences.addons[__package__].preferences.branch_feature_set_povray
    using_uberpov = (feature_set=='uberpov')
    #scene = bpy.data.scenes[0]
    render = scene.render

    x = int(render.resolution_x * render.resolution_percentage * 0.01)
    y = int(render.resolution_y * render.resolution_percentage * 0.01)

    file = open(filename_ini, "w")
    file.write("Version=3.7\n")
    #write povray text stream to temporary file of same name with _log suffix
    #file.write("All_File='%s'\n" % filename_log)
    # DEBUG.OUT log if none specified:
    file.write("All_File=1\n")


    file.write("Input_File_Name='%s'\n" % filename_pov)
    file.write("Output_File_Name='%s'\n" % filename_image)

    file.write("Width=%d\n" % x)
    file.write("Height=%d\n" % y)

    # Border render.
    if render.use_border:
        file.write("Start_Column=%4g\n" % render.border_min_x)
        file.write("End_Column=%4g\n" % (render.border_max_x))

        file.write("Start_Row=%4g\n" % (1.0 - render.border_max_y))
        file.write("End_Row=%4g\n" % (1.0 - render.border_min_y))

    file.write("Bounding_Method=2\n")  # The new automatic BSP is faster in most scenes

    # Activated (turn this back off when better live exchange is done between the two programs
    # (see next comment)
    file.write("Display=1\n")
    file.write("Pause_When_Done=0\n")
    # PNG, with POV-Ray 3.7, can show background color with alpha. In the long run using the
    # POV-Ray interactive preview like bishop 3D could solve the preview for all formats.
    file.write("Output_File_Type=N\n")
    #file.write("Output_File_Type=T\n") # TGA, best progressive loading
    file.write("Output_Alpha=1\n")

    if scene.pov.antialias_enable:
        # method 2 (recursive) with higher max subdiv forced because no mipmapping in POV-Ray
        # needs higher sampling.
        # aa_mapping = {"5": 2, "8": 3, "11": 4, "16": 5}
        if using_uberpov:
            method = {"0": 1, "1": 2, "2": 3}
        else:
            method = {"0": 1, "1": 2, "2": 2}
        file.write("Antialias=on\n")
        file.write("Antialias_Depth=%d\n" % scene.pov.antialias_depth)
        file.write("Antialias_Threshold=%.3g\n" % scene.pov.antialias_threshold)
        if using_uberpov and scene.pov.antialias_method == '2':
            file.write("Sampling_Method=%s\n" % method[scene.pov.antialias_method])
            file.write("Antialias_Confidence=%.3g\n" % scene.pov.antialias_confidence)
        else:
            file.write("Sampling_Method=%s\n" % method[scene.pov.antialias_method])
        file.write("Antialias_Gamma=%.3g\n" % scene.pov.antialias_gamma)
        if scene.pov.jitter_enable:
            file.write("Jitter=on\n")
            file.write("Jitter_Amount=%3g\n" % scene.pov.jitter_amount)
        else:
            file.write("Jitter=off\n")  # prevent animation flicker

    else:
        file.write("Antialias=off\n")
    #print("ini file closed %s" % file.closed)
    file.close()
    #print("ini file closed %s" % file.closed)


class PovrayRender(bpy.types.RenderEngine):
    bl_idname = 'POVRAY_RENDER'
    bl_label = "POV-Ray 3.7"
    DELAY = 0.5

    @staticmethod
    def _locate_binary():
        addon_prefs = bpy.context.user_preferences.addons[__package__].preferences

        # Use the system preference if its set.
        pov_binary = addon_prefs.filepath_povray
        if pov_binary:
            if os.path.exists(pov_binary):
                return pov_binary
            else:
                print("User Preferences path to povray %r NOT FOUND, checking $PATH" % pov_binary)

        # Windows Only
        # assume if there is a 64bit binary that the user has a 64bit capable OS
        if sys.platform[:3] == "win":
            import winreg
            win_reg_key = winreg.OpenKey(winreg.HKEY_CURRENT_USER,
                "Software\\POV-Ray\\v3.7\\Windows")
            win_home = winreg.QueryValueEx(win_reg_key, "Home")[0]

            # First try 64bits UberPOV
            pov_binary = os.path.join(win_home, "bin", "uberpov64.exe")
            if os.path.exists(pov_binary):
                return pov_binary

            # Then try 64bits POV
            pov_binary = os.path.join(win_home, "bin", "pvengine64.exe")
            if os.path.exists(pov_binary):
                return pov_binary

            # Then try 32bits UberPOV
            pov_binary = os.path.join(win_home, "bin", "uberpov32.exe")
            if os.path.exists(pov_binary):
                return pov_binary

            # Then try 32bits POV
            pov_binary = os.path.join(win_home, "bin", "pvengine.exe")
            if os.path.exists(pov_binary):
                return pov_binary

        # search the path all os's
        pov_binary_default = "povray"

        os_path_ls = os.getenv("PATH").split(':') + [""]

        for dir_name in os_path_ls:
            pov_binary = os.path.join(dir_name, pov_binary_default)
            if os.path.exists(pov_binary):
                return pov_binary
        return ""

    def _export(self, scene, povPath, renderImagePath):
        import tempfile

        if scene.pov.tempfiles_enable:
            self._temp_file_in = tempfile.NamedTemporaryFile(suffix=".pov", delete=False).name
            # PNG with POV 3.7, can show the background color with alpha. In the long run using the
            # POV-Ray interactive preview like bishop 3D could solve the preview for all formats.
            self._temp_file_out = tempfile.NamedTemporaryFile(suffix=".png", delete=False).name
            #self._temp_file_out = tempfile.NamedTemporaryFile(suffix=".tga", delete=False).name
            self._temp_file_ini = tempfile.NamedTemporaryFile(suffix=".ini", delete=False).name
            self._temp_file_log = os.path.join(tempfile.gettempdir(), "alltext.out")
        else:
            self._temp_file_in = povPath + ".pov"
            # PNG with POV 3.7, can show the background color with alpha. In the long run using the
            # POV-Ray interactive preview like bishop 3D could solve the preview for all formats.
            self._temp_file_out = renderImagePath + ".png"
            #self._temp_file_out = renderImagePath + ".tga"
            self._temp_file_ini = povPath + ".ini"
            logPath = bpy.path.abspath(scene.pov.scene_path).replace('\\', '/')
            self._temp_file_log = os.path.join(logPath, "alltext.out")
            '''
            self._temp_file_in = "/test.pov"
            # PNG with POV 3.7, can show the background color with alpha. In the long run using the
            # POV-Ray interactive preview like bishop 3D could solve the preview for all formats.
            self._temp_file_out = "/test.png"
            #self._temp_file_out = "/test.tga"
            self._temp_file_ini = "/test.ini"
            '''
        if scene.pov.text_block ==  "":
            def info_callback(txt):
                self.update_stats("", "POV-Ray 3.7: " + txt)

            # os.makedirs(user_dir, exist_ok=True)  # handled with previews
            os.makedirs(preview_dir, exist_ok=True)

            write_pov(self._temp_file_in, scene, info_callback)
        else:
            pass
    def _render(self, scene):
        try:
            os.remove(self._temp_file_out)  # so as not to load the old file
        except OSError:
            pass

        pov_binary = PovrayRender._locate_binary()
        if not pov_binary:
            print("POV-Ray 3.7: could not execute povray, possibly POV-Ray isn't installed")
            return False

        write_pov_ini(scene, self._temp_file_ini, self._temp_file_log, self._temp_file_in, self._temp_file_out)

        print ("***-STARTING-***")

        extra_args = []

        if scene.pov.command_line_switches != "":
            for newArg in scene.pov.command_line_switches.split(" "):
                extra_args.append(newArg)

        self._is_windows = False
        if sys.platform[:3] == "win":
            self._is_windows = True
            if"/EXIT" not in extra_args and not scene.pov.pov_editor:
                extra_args.append("/EXIT")
        else:
            # added -d option to prevent render window popup which leads to segfault on linux
            extra_args.append("-d")

        # Start Rendering!
        try:
            self._process = subprocess.Popen([pov_binary, self._temp_file_ini] + extra_args,
                                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except OSError:
            # TODO, report api
            print("POV-Ray 3.7: could not execute '%s'" % pov_binary)
            import traceback
            traceback.print_exc()
            print ("***-DONE-***")
            return False

        else:
            print("Engine ready!...")
            print("Command line arguments passed: " + str(extra_args))
            return True

        # Now that we have a valid process

    def _cleanup(self):
        for f in (self._temp_file_in, self._temp_file_ini, self._temp_file_out):
            for i in range(5):
                try:
                    os.unlink(f)
                    break
                except OSError:
                    # Wait a bit before retrying file might be still in use by Blender,
                    # and Windows does not know how to delete a file in use!
                    time.sleep(self.DELAY)
        for i in unpacked_images:
            for c in range(5):
                try:
                    os.unlink(i)
                    break
                except OSError:
                    # Wait a bit before retrying file might be still in use by Blender,
                    # and Windows does not know how to delete a file in use!
                    time.sleep(self.DELAY)
    def render(self, scene):
        import tempfile
        r = scene.render
        x = int(r.resolution_x * r.resolution_percentage * 0.01)
        y = int(r.resolution_y * r.resolution_percentage * 0.01)
        print("***INITIALIZING***")
        
        # This makes some tests on the render, returning True if all goes good, and False if
        # it was finished one way or the other.
        # It also pauses the script (time.sleep())
        def _test_wait():
            time.sleep(self.DELAY)

            # User interrupts the rendering
            if self.test_break():
                try:
                    self._process.terminate()
                    print("***POV INTERRUPTED***")
                except OSError:
                    pass
                return False

            poll_result = self._process.poll()
            # POV process is finisehd, one way or the other
            if poll_result is not None:
                if poll_result < 0:
                    print("***POV PROCESS FAILED : %s ***" % poll_result)
                    self.update_stats("", "POV-Ray 3.7: Failed")
                return False

            return True
    
        if scene.pov.text_block !="":
            if scene.pov.tempfiles_enable:
                self._temp_file_in = tempfile.NamedTemporaryFile(suffix=".pov", delete=False).name
                self._temp_file_out = tempfile.NamedTemporaryFile(suffix=".png", delete=False).name
                #self._temp_file_out = tempfile.NamedTemporaryFile(suffix=".tga", delete=False).name
                self._temp_file_ini = tempfile.NamedTemporaryFile(suffix=".ini", delete=False).name
                self._temp_file_log = os.path.join(tempfile.gettempdir(), "alltext.out")
            else:
                povPath = scene.pov.text_block
                renderImagePath = os.path.splitext(povPath)[0]
                self._temp_file_out =os.path.join(preview_dir, renderImagePath )
                self._temp_file_in = os.path.join(preview_dir, povPath)
                self._temp_file_ini = os.path.join(preview_dir, (os.path.splitext(self._temp_file_in)[0]+".INI"))
                self._temp_file_log = os.path.join(preview_dir, "alltext.out")
        
        
            '''
            try:
                os.remove(self._temp_file_in)  # so as not to load the old file
            except OSError:
                pass
            '''    
            print(scene.pov.text_block)
            text = bpy.data.texts[scene.pov.text_block]
            file=open("%s"%self._temp_file_in,"w")
            # Why are the newlines needed?
            file.write("\n")
            file.write(text.as_string())
            file.write("\n")    
            file.close()

            # has to be called to update the frame on exporting animations
            scene.frame_set(scene.frame_current)
            
            pov_binary = PovrayRender._locate_binary()

            if not pov_binary:
                print("POV-Ray 3.7: could not execute povray, possibly POV-Ray isn't installed")
                return False
                
                
            # start ini UI options export
            self.update_stats("", "POV-Ray 3.7: Exporting ini options from Blender")         

            write_pov_ini(scene, self._temp_file_ini, self._temp_file_log, self._temp_file_in, self._temp_file_out)

            print ("***-STARTING-***")

            extra_args = []

            if scene.pov.command_line_switches != "":
                for newArg in scene.pov.command_line_switches.split(" "):
                    extra_args.append(newArg)

            if sys.platform[:3] == "win":
                if"/EXIT" not in extra_args and not scene.pov.pov_editor:
                    extra_args.append("/EXIT")
            else:
                # added -d option to prevent render window popup which leads to segfault on linux
                extra_args.append("-d")

            # Start Rendering!
            try:
                _process = subprocess.Popen([pov_binary, self._temp_file_ini] + extra_args,
                                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            except OSError:
                # TODO, report api
                print("POV-Ray 3.7: could not execute '%s'" % pov_binary)
                import traceback
                traceback.print_exc()
                print ("***-DONE-***")
                return False

            else:
                print("Engine ready!...")
                print("Command line arguments passed: " + str(extra_args))
                #return True
                self.update_stats("", "POV-Ray 3.7: Parsing File")                  
 

                
            # Indented in main function now so repeated here but still not working 
            # to bring back render result to its buffer
   
            if os.path.exists(self._temp_file_out):
                xmin = int(r.border_min_x * x)
                ymin = int(r.border_min_y * y)
                xmax = int(r.border_max_x * x)
                ymax = int(r.border_max_y * y)
                result = self.begin_result(0, 0, x, y) 
                lay = result.layers[0]
                
                time.sleep(self.DELAY)
                try:
                    lay.load_from_file(self._temp_file_out)
                except RuntimeError:
                    print("***POV ERROR WHILE READING OUTPUT FILE***")
                self.end_result(result)
            #print(self._temp_file_log) #bring the pov log to blender console with proper path?
            with open(self._temp_file_log) as f: # The with keyword automatically closes the file when you are done
                print(f.read()) 
                
            self.update_stats("", "")
            
            if scene.pov.tempfiles_enable or scene.pov.deletefiles_enable:
                self._cleanup()            
        else:

    ##WIP output format
    ##        if r.image_settings.file_format == 'OPENEXR':
    ##            fformat = 'EXR'
    ##            render.image_settings.color_mode = 'RGBA'
    ##        else:
    ##            fformat = 'TGA'
    ##            r.image_settings.file_format = 'TARGA'
    ##            r.image_settings.color_mode = 'RGBA'

            blendSceneName = bpy.data.filepath.split(os.path.sep)[-1].split(".")[0]
            povSceneName = ""
            povPath = ""
            renderImagePath = ""

            # has to be called to update the frame on exporting animations
            scene.frame_set(scene.frame_current)

            if not scene.pov.tempfiles_enable:

                # check paths
                povPath = bpy.path.abspath(scene.pov.scene_path).replace('\\', '/')
                if povPath == "":
                    if bpy.data.is_saved:
                        povPath = bpy.path.abspath("//")
                    else:
                        povPath = tempfile.gettempdir()
                elif povPath.endswith("/"):
                    if povPath == "/":
                        povPath = bpy.path.abspath("//")
                    else:
                        povPath = bpy.path.abspath(scene.pov.scene_path)

                if not os.path.exists(povPath):
                    try:
                        os.makedirs(povPath)
                    except:
                        import traceback
                        traceback.print_exc()

                        print("POV-Ray 3.7: Cannot create scenes directory: %r" % povPath)
                        self.update_stats("", "POV-Ray 3.7: Cannot create scenes directory %r" % \
                                          povPath)
                        time.sleep(2.0)
                        #return

                '''
                # Bug in POV-Ray RC3
                renderImagePath = bpy.path.abspath(scene.pov.renderimage_path).replace('\\','/')
                if renderImagePath == "":
                    if bpy.data.is_saved:
                        renderImagePath = bpy.path.abspath("//")
                    else:
                        renderImagePath = tempfile.gettempdir()
                    #print("Path: " + renderImagePath)
                elif path.endswith("/"):
                    if renderImagePath == "/":
                        renderImagePath = bpy.path.abspath("//")
                    else:
                        renderImagePath = bpy.path.abspath(scene.pov.renderimage_path)
                if not os.path.exists(path):
                    print("POV-Ray 3.7: Cannot find render image directory")
                    self.update_stats("", "POV-Ray 3.7: Cannot find render image directory")
                    time.sleep(2.0)
                    return
                '''

                # check name
                if scene.pov.scene_name == "":
                    if blendSceneName != "":
                        povSceneName = blendSceneName
                    else:
                        povSceneName = "untitled"
                else:
                    povSceneName = scene.pov.scene_name
                    if os.path.isfile(povSceneName):
                        povSceneName = os.path.basename(povSceneName)
                    povSceneName = povSceneName.split('/')[-1].split('\\')[-1]
                    if not povSceneName:
                        print("POV-Ray 3.7: Invalid scene name")
                        self.update_stats("", "POV-Ray 3.7: Invalid scene name")
                        time.sleep(2.0)
                        #return
                    povSceneName = os.path.splitext(povSceneName)[0]

                print("Scene name: " + povSceneName)
                print("Export path: " + povPath)
                povPath = os.path.join(povPath, povSceneName)
                povPath = os.path.realpath(povPath)

                # for now this has to be the same like the pov output. Bug in POV-Ray RC3.
                # renderImagePath = renderImagePath + "\\" + povSceneName
                renderImagePath = povPath  # Bugfix for POV-Ray RC3 bug
                # renderImagePath = os.path.realpath(renderImagePath)  # Bugfix for POV-Ray RC3 bug

                #print("Export path: %s" % povPath)
                #print("Render Image path: %s" % renderImagePath)

            # start export
            self.update_stats("", "POV-Ray 3.7: Exporting data from Blender")
            self._export(scene, povPath, renderImagePath)
            self.update_stats("", "POV-Ray 3.7: Parsing File")

            if not self._render(scene):
                self.update_stats("", "POV-Ray 3.7: Not found")
                #return

            #r = scene.render
            # compute resolution
            #x = int(r.resolution_x * r.resolution_percentage * 0.01)
            #y = int(r.resolution_y * r.resolution_percentage * 0.01)


            # Wait for the file to be created
            # XXX This is no more valid, as 3.7 always creates output file once render is finished!
            parsing = re.compile(br"= \[Parsing\.\.\.\] =")
            rendering = re.compile(br"= \[Rendering\.\.\.\] =")
            percent = re.compile(r"\(([0-9]{1,3})%\)")
            # print("***POV WAITING FOR FILE***")

            data = b""
            last_line = ""
            while _test_wait():
                # POV in Windows does not output its stdout/stderr, it displays them in its GUI
                if self._is_windows:
                    self.update_stats("", "POV-Ray 3.7: Rendering File")
                else:
                    t_data = self._process.stdout.read(10000)
                    if not t_data:
                        continue

                    data += t_data
                    # XXX This is working for UNIX, not sure whether it might need adjustments for
                    #     other OSs
                    # First replace is for windows
                    t_data = str(t_data).replace('\\r\\n', '\\n').replace('\\r', '\r')
                    lines = t_data.split('\\n')
                    last_line += lines[0]
                    lines[0] = last_line
                    print('\n'.join(lines), end="")
                    last_line = lines[-1]

                    if rendering.search(data):
                        _pov_rendering = True
                        match = percent.findall(str(data))
                        if match:
                            self.update_stats("", "POV-Ray 3.7: Rendering File (%s%%)" % match[-1])
                        else:
                            self.update_stats("", "POV-Ray 3.7: Rendering File")

                    elif parsing.search(data):
                        self.update_stats("", "POV-Ray 3.7: Parsing File")

            if os.path.exists(self._temp_file_out):
                # print("***POV FILE OK***")
                #self.update_stats("", "POV-Ray 3.7: Rendering")

                # prev_size = -1

                xmin = int(r.border_min_x * x)
                ymin = int(r.border_min_y * y)
                xmax = int(r.border_max_x * x)
                ymax = int(r.border_max_y * y)

                # print("***POV UPDATING IMAGE***")
                result = self.begin_result(0, 0, x, y)
                # XXX, tests for border render.
                #result = self.begin_result(xmin, ymin, xmax - xmin, ymax - ymin)
                #result = self.begin_result(0, 0, xmax - xmin, ymax - ymin)
                lay = result.layers[0]

                # This assumes the file has been fully written We wait a bit, just in case!
                time.sleep(self.DELAY)
                try:
                    lay.load_from_file(self._temp_file_out)
                    # XXX, tests for border render.
                    #lay.load_from_file(self._temp_file_out, xmin, ymin)
                except RuntimeError:
                    print("***POV ERROR WHILE READING OUTPUT FILE***")

                # Not needed right now, might only be useful if we find a way to use temp raw output of
                # pov 3.7 (in which case it might go under _test_wait()).
                '''
                def update_image():
                    # possible the image wont load early on.
                    try:
                        lay.load_from_file(self._temp_file_out)
                        # XXX, tests for border render.
                        #lay.load_from_file(self._temp_file_out, xmin, ymin)
                        #lay.load_from_file(self._temp_file_out, xmin, ymin)
                    except RuntimeError:
                        pass

                # Update while POV-Ray renders
                while True:
                    # print("***POV RENDER LOOP***")

                    # test if POV-Ray exists
                    if self._process.poll() is not None:
                        print("***POV PROCESS FINISHED***")
                        update_image()
                        break

                    # user exit
                    if self.test_break():
                        try:
                            self._process.terminate()
                            print("***POV PROCESS INTERRUPTED***")
                        except OSError:
                            pass

                        break

                    # Would be nice to redirect the output
                    # stdout_value, stderr_value = self._process.communicate() # locks

                    # check if the file updated
                    new_size = os.path.getsize(self._temp_file_out)

                    if new_size != prev_size:
                        update_image()
                        prev_size = new_size

                    time.sleep(self.DELAY)
                '''

                self.end_result(result)

            else:
                print("***POV FILE NOT FOUND***")

            print("***POV FILE FINISHED***")

            #print(filename_log) #bring the pov log to blender console with proper path?
            with open(self._temp_file_log) as f: # The with keyword automatically closes the file when you are done
                print(f.read())

            self.update_stats("", "")

            if scene.pov.tempfiles_enable or scene.pov.deletefiles_enable:
                self._cleanup()


##################################################################################
#################################Operators########################################
##################################################################################
class RenderPovTexturePreview(Operator):
    bl_idname = "tex.preview_update"
    bl_label = "Update preview"
    def execute(self, context):
        tex=bpy.context.object.active_material.active_texture #context.texture
        texPrevName=string_strip_hyphen(bpy.path.clean_name(tex.name))+"_prev"

        ## Make sure Preview directory exists and is empty
        if not os.path.isdir(preview_dir):
            os.mkdir(preview_dir)

        iniPrevFile=os.path.join(preview_dir, "Preview.ini")
        inputPrevFile=os.path.join(preview_dir, "Preview.pov")
        outputPrevFile=os.path.join(preview_dir, texPrevName)
        ##################### ini ##########################################
        fileIni=open("%s"%iniPrevFile,"w")
        fileIni.write('Version=3.7\n')
        fileIni.write('Input_File_Name="%s"\n'%inputPrevFile)
        fileIni.write('Output_File_Name="%s.png"\n'%outputPrevFile)
        fileIni.write('Library_Path="%s"\n' % preview_dir)
        fileIni.write('Width=256\n')
        fileIni.write('Height=256\n')
        fileIni.write('Pause_When_Done=0\n')
        fileIni.write('Output_File_Type=N\n')
        fileIni.write('Output_Alpha=1\n')
        fileIni.write('Antialias=on\n')
        fileIni.write('Sampling_Method=2\n')
        fileIni.write('Antialias_Depth=3\n')
        fileIni.write('-d\n')
        fileIni.close()
        ##################### pov ##########################################
        filePov=open("%s"%inputPrevFile,"w")
        PATname = "PAT_"+string_strip_hyphen(bpy.path.clean_name(tex.name))
        filePov.write("#declare %s = \n"%PATname)
        filePov.write(shading.exportPattern(tex, string_strip_hyphen))

        filePov.write("#declare Plane =\n")
        filePov.write("mesh {\n")
        filePov.write("    triangle {<-2.021,-1.744,2.021>,<-2.021,-1.744,-2.021>,<2.021,-1.744,2.021>}\n")
        filePov.write("    triangle {<-2.021,-1.744,-2.021>,<2.021,-1.744,-2.021>,<2.021,-1.744,2.021>}\n")
        filePov.write("    texture{%s}\n"%PATname)
        filePov.write("}\n")
        filePov.write("object {Plane}\n")
        filePov.write("light_source {\n")
        filePov.write("    <0,4.38,-1.92e-07>\n")
        filePov.write("    color rgb<4, 4, 4>\n")
        filePov.write("    parallel\n")
        filePov.write("    point_at  <0, 0, -1>\n")
        filePov.write("}\n")
        filePov.write("camera {\n")
        filePov.write("    location  <0, 0, 0>\n")
        filePov.write("    look_at  <0, 0, -1>\n")
        filePov.write("    right <-1.0, 0, 0>\n")
        filePov.write("    up <0, 1, 0>\n")
        filePov.write("    angle  96.805211\n")
        filePov.write("    rotate  <-90.000003, -0.000000, 0.000000>\n")
        filePov.write("    translate <0.000000, 0.000000, 0.000000>\n")
        filePov.write("}\n")
        filePov.close()
        ##################### end write ##########################################

        pov_binary = PovrayRender._locate_binary()

        if sys.platform[:3] == "win":
            p1=subprocess.Popen(["%s"%pov_binary,"/EXIT","%s"%iniPrevFile],
                stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        else:
            p1=subprocess.Popen(["%s"%pov_binary,"-d","%s"%iniPrevFile],
                stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        p1.wait()

        tex.use_nodes = True
        tree = tex.node_tree
        links = tree.links
        for n in tree.nodes:
            tree.nodes.remove(n)
        im = tree.nodes.new("TextureNodeImage")
        pathPrev="%s.png"%outputPrevFile
        im.image = bpy.data.images.load(pathPrev)
        name=pathPrev
        name=name.split("/")
        name=name[len(name)-1]
        im.name = name
        im.location = 200,200
        previewer = tree.nodes.new('TextureNodeOutput')
        previewer.label = "Preview"
        previewer.location = 400,400
        links.new(im.outputs[0],previewer.inputs[0])
        #tex.type="IMAGE" # makes clip extend possible
        #tex.extension="CLIP"
        return {'FINISHED'}

class RunPovTextRender(Operator):
    bl_idname = "text.run"
    bl_label = "Run"
    bl_context = "text"
    bl_description = "Run a render with this text only"
        
    def execute(self, context):
        scene = context.scene
        scene.pov.text_block = context.space_data.text.name

            
        bpy.ops.render.render()
 
        #empty text name property engain
        scene.pov.text_block = ""
        return {'FINISHED'}    