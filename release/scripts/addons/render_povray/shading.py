# For BI > POV shaders emulation
import bpy

def writeMaterial(using_uberpov, DEF_MAT_NAME, scene, tabWrite, safety, comments, uniqueName, materialNames, material):
    # Assumes only called once on each material
    if material:
        name_orig = material.name
        name = materialNames[name_orig] = uniqueName(bpy.path.clean_name(name_orig), materialNames)
    else:
        name = name_orig = DEF_MAT_NAME


    if material:
        # If saturation(.s) is not zero, then color is not grey, and has a tint
        colored_specular_found = ((material.specular_color.s > 0.0) and (material.diffuse_shader != 'MINNAERT'))

    ##################
    # Several versions of the finish: Level conditions are variations for specular/Mirror
    # texture channel map with alternative finish of 0 specular and no mirror reflection.
    # Level=1 Means No specular nor Mirror reflection
    # Level=2 Means translation of spec and mir levels for when no map influences them
    # Level=3 Means Maximum Spec and Mirror

    def povHasnoSpecularMaps(Level):
        if Level == 1:
            if comments:
                tabWrite("//--No specular nor Mirror reflection--\n")
            else:
                tabWrite("\n")
            tabWrite("#declare %s = finish {\n" % safety(name, Level=1))

        elif Level == 2:
            if comments:
                tabWrite("//--translation of spec and mir levels for when no map " \
                           "influences them--\n")
            else:
                tabWrite("\n")
            tabWrite("#declare %s = finish {\n" % safety(name, Level=2))

        elif Level == 3:
            if comments:
                tabWrite("//--Maximum Spec and Mirror--\n")
            else:
                tabWrite("\n")
            tabWrite("#declare %s = finish {\n" % safety(name, Level=3))
        if material:
            # POV-Ray 3.7 now uses two diffuse values respectively for front and back shading
            # (the back diffuse is like blender translucency)
            frontDiffuse = material.diffuse_intensity
            backDiffuse = material.translucency

            if material.pov.conserve_energy:

                #Total should not go above one
                if (frontDiffuse + backDiffuse) <= 1.0:
                    pass
                elif frontDiffuse == backDiffuse:
                    # Try to respect the user's 'intention' by comparing the two values but
                    # bringing the total back to one.
                    frontDiffuse = backDiffuse = 0.5
                # Let the highest value stay the highest value.
                elif frontDiffuse > backDiffuse:
                    # clamps the sum below 1
                    backDiffuse = min(backDiffuse, (1.0 - frontDiffuse))
                else:
                    frontDiffuse = min(frontDiffuse, (1.0 - backDiffuse))

            # map hardness between 0.0 and 1.0
            roughness = ((1.0 - ((material.specular_hardness - 1.0) / 510.0)))
            ## scale from 0.0 to 0.1
            roughness *= 0.1
            # add a small value because 0.0 is invalid.
            roughness += (1.0 / 511.0)

            ################################Diffuse Shader######################################
            # Not used for Full spec (Level=3) of the shader.
            if material.diffuse_shader == 'OREN_NAYAR' and Level != 3:
                # Blender roughness is what is generally called oren nayar Sigma,
                # and brilliance in POV-Ray.
                tabWrite("brilliance %.3g\n" % (0.9 + material.roughness))

            if material.diffuse_shader == 'TOON' and Level != 3:
                tabWrite("brilliance %.3g\n" % (0.01 + material.diffuse_toon_smooth * 0.25))
                # Lower diffuse and increase specular for toon effect seems to look better
                # in POV-Ray.
                frontDiffuse *= 0.5

            if material.diffuse_shader == 'MINNAERT' and Level != 3:
                #tabWrite("aoi %.3g\n" % material.darkness)
                pass  # let's keep things simple for now
            if material.diffuse_shader == 'FRESNEL' and Level != 3:
                #tabWrite("aoi %.3g\n" % material.diffuse_fresnel_factor)
                pass  # let's keep things simple for now
            if material.diffuse_shader == 'LAMBERT' and Level != 3:
                # trying to best match lambert attenuation by that constant brilliance value
                tabWrite("brilliance 1\n")

            if Level == 2:
                ###########################Specular Shader######################################
                # No difference between phong and cook torrence in blender HaHa!
                if (material.specular_shader == 'COOKTORR' or
                    material.specular_shader == 'PHONG'):
                    tabWrite("phong %.3g\n" % (material.specular_intensity))
                    tabWrite("phong_size %.3g\n" % (material.specular_hardness /3.14))

                # POV-Ray 'specular' keyword corresponds to a Blinn model, without the ior.
                elif material.specular_shader == 'BLINN':
                    # Use blender Blinn's IOR just as some factor for spec intensity
                    tabWrite("specular %.3g\n" % (material.specular_intensity *
                                                  (material.specular_ior / 4.0)))
                    tabWrite("roughness %.3g\n" % roughness)
                    #Could use brilliance 2(or varying around 2 depending on ior or factor) too.

                elif material.specular_shader == 'TOON':
                    tabWrite("phong %.3g\n" % (material.specular_intensity * 2.0))
                    # use extreme phong_size
                    tabWrite("phong_size %.3g\n" % (0.1 + material.specular_toon_smooth / 2.0))

                elif material.specular_shader == 'WARDISO':
                    # find best suited default constant for brilliance Use both phong and
                    # specular for some values.
                    tabWrite("specular %.3g\n" % (material.specular_intensity /
                                                  (material.specular_slope + 0.0005)))
                    # find best suited default constant for brilliance Use both phong and
                    # specular for some values.
                    tabWrite("roughness %.4g\n" % (0.0005 + material.specular_slope / 10.0))
                    # find best suited default constant for brilliance Use both phong and
                    # specular for some values.
                    tabWrite("brilliance %.4g\n" % (1.8 - material.specular_slope * 1.8))

            ####################################################################################
            elif Level == 1:
                if (material.specular_shader == 'COOKTORR' or
                    material.specular_shader == 'PHONG'):
                    tabWrite("phong %.3g\n" % (material.specular_intensity/5))
                    tabWrite("phong_size %.3g\n" % (material.specular_hardness /3.14))

                # POV-Ray 'specular' keyword corresponds to a Blinn model, without the ior.
                elif material.specular_shader == 'BLINN':
                    # Use blender Blinn's IOR just as some factor for spec intensity
                    tabWrite("specular %.3g\n" % (material.specular_intensity *
                                                  (material.specular_ior / 4.0)))
                    tabWrite("roughness %.3g\n" % roughness)
                    #Could use brilliance 2(or varying around 2 depending on ior or factor) too.

                elif material.specular_shader == 'TOON':
                    tabWrite("phong %.3g\n" % (material.specular_intensity * 2.0))
                    # use extreme phong_size
                    tabWrite("phong_size %.3g\n" % (0.1 + material.specular_toon_smooth / 2.0))

                elif material.specular_shader == 'WARDISO':
                    # find best suited default constant for brilliance Use both phong and
                    # specular for some values.
                    tabWrite("specular %.3g\n" % (material.specular_intensity /
                                                  (material.specular_slope + 0.0005)))
                    # find best suited default constant for brilliance Use both phong and
                    # specular for some values.
                    tabWrite("roughness %.4g\n" % (0.0005 + material.specular_slope / 10.0))
                    # find best suited default constant for brilliance Use both phong and
                    # specular for some values.
                    tabWrite("brilliance %.4g\n" % (1.8 - material.specular_slope * 1.8))
            elif Level == 3:
                tabWrite("specular %.3g\n" % ((material.specular_intensity*material.specular_color.v)*5))
                tabWrite("roughness %.3g\n" % (1.1/material.specular_hardness))
            tabWrite("diffuse %.3g %.3g\n" % (frontDiffuse, backDiffuse))

            tabWrite("ambient %.3g\n" % material.ambient)
            # POV-Ray blends the global value
            #tabWrite("ambient rgb <%.3g, %.3g, %.3g>\n" % \
            #         tuple([c*material.ambient for c in world.ambient_color]))
            tabWrite("emission %.3g\n" % material.emit)  # New in POV-Ray 3.7

            #POV-Ray just ignores roughness if there's no specular keyword
            #tabWrite("roughness %.3g\n" % roughness)

            if material.pov.conserve_energy:
                # added for more realistic shading. Needs some checking to see if it
                # really works. --Maurice.
                tabWrite("conserve_energy\n")

            if colored_specular_found == True:
                 tabWrite("metallic\n")

            # 'phong 70.0 '
            if Level != 1:
                if material.raytrace_mirror.use:
                    raytrace_mirror = material.raytrace_mirror
                    if raytrace_mirror.reflect_factor:
                        tabWrite("reflection {\n")
                        tabWrite("rgb <%.3g, %.3g, %.3g>\n" % material.mirror_color[:])
                        if material.pov.mirror_metallic:
                            tabWrite("metallic %.3g\n" % (raytrace_mirror.reflect_factor))
                        # Blurry reflections for UberPOV
                        if using_uberpov and raytrace_mirror.gloss_factor < 1.0:
                            #tabWrite("#ifdef(unofficial) #if(unofficial = \"patch\") #if(patch(\"upov-reflection-roughness\") > 0)\n")
                            tabWrite("roughness %.6f\n" % \
                                     (0.000001/raytrace_mirror.gloss_factor))
                            #tabWrite("#end #end #end\n") # This and previous comment for backward compatibility, messier pov code
                        if material.pov.mirror_use_IOR:  # WORKING ?
                            # Removed from the line below: gives a more physically correct
                            # material but needs proper IOR. --Maurice
                            tabWrite("fresnel 1 ")
                        tabWrite("falloff %.3g exponent %.3g} " % \
                                 (raytrace_mirror.fresnel, raytrace_mirror.fresnel_factor))

            if material.subsurface_scattering.use:
                subsurface_scattering = material.subsurface_scattering
                tabWrite("subsurface { translucency <%.3g, %.3g, %.3g> }\n" % (
                         (subsurface_scattering.radius[0]),
                         (subsurface_scattering.radius[1]),
                         (subsurface_scattering.radius[2]),
                         )
                        )

            if material.pov.irid_enable:
                tabWrite("irid { %.4g thickness %.4g turbulence %.4g }" % \
                         (material.pov.irid_amount, material.pov.irid_thickness,
                          material.pov.irid_turbulence))

        else:
            tabWrite("diffuse 0.8\n")
            tabWrite("phong 70.0\n")

            #tabWrite("specular 0.2\n")

        # This is written into the object
        '''
        if material and material.transparency_method=='RAYTRACE':
            'interior { ior %.3g} ' % material.raytrace_transparency.ior
        '''

        #tabWrite("crand 1.0\n") # Sand granyness
        #tabWrite("metallic %.6f\n" % material.spec)
        #tabWrite("phong %.6f\n" % material.spec)
        #tabWrite("phong_size %.6f\n" % material.spec)
        #tabWrite("brilliance %.6f " % (material.specular_hardness/256.0) # Like hardness

        tabWrite("}\n\n")

    # Level=2 Means translation of spec and mir levels for when no map influences them
    povHasnoSpecularMaps(Level=2)

    if material:
        special_texture_found = False
        for t in material.texture_slots:
            if t and t.use and t.texture is not None:
                if (t.texture.type == 'IMAGE' and t.texture.image) or t.texture.type != 'IMAGE':
                    validPath=True
            else:
                validPath=False
            if(t and t.use and validPath and
               (t.use_map_specular or t.use_map_raymir or t.use_map_normal or t.use_map_alpha)):
                special_texture_found = True
                continue  # Some texture found

        if special_texture_found or colored_specular_found:
            # Level=1 Means No specular nor Mirror reflection
            povHasnoSpecularMaps(Level=1)

            # Level=3 Means Maximum Spec and Mirror
            povHasnoSpecularMaps(Level=3)

def exportPattern(texture, string_strip_hyphen):
    tex=texture
    pat = tex.pov
    PATname = "PAT_%s"%string_strip_hyphen(bpy.path.clean_name(tex.name))
    mappingDif = ("translate <%.4g,%.4g,%.4g> scale <%.4g,%.4g,%.4g>" % \
          (pat.tex_mov_x, pat.tex_mov_y, pat.tex_mov_z,
           1.0 / pat.tex_scale_x, 1.0 / pat.tex_scale_y, 1.0 / pat.tex_scale_z))
    texStrg=""
    def exportColorRamp(texture):
        tex=texture
        pat = tex.pov
        colRampStrg="color_map {\n"
        numColor=0
        for el in tex.color_ramp.elements:
            numColor+=1
            pos = el.position
            col=el.color
            colR,colG,colB,colA = col[0],col[1],col[2],1-col[3]
            if pat.tex_pattern_type not in {'checker', 'hexagon', 'square', 'triangular', 'brick'} :
                colRampStrg+="[%.4g color rgbf<%.4g,%.4g,%.4g,%.4g>] \n"%(pos,colR,colG,colB,colA)
            if pat.tex_pattern_type in {'brick','checker'} and numColor < 3:
                colRampStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
            if pat.tex_pattern_type == 'hexagon' and numColor < 4 :
                colRampStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
            if pat.tex_pattern_type == 'square' and numColor < 5 :
                colRampStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
            if pat.tex_pattern_type == 'triangular' and numColor < 7 :
                colRampStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)

        colRampStrg+="} \n"
        #end color map
        return colRampStrg
    #much work to be done here only defaults translated for now:
    #pov noise_generator 3 means perlin noise
    if tex.type not in {'NONE', 'IMAGE'} and pat.tex_pattern_type == 'emulator':
        texStrg+="pigment {\n"
        ####################### EMULATE BLENDER VORONOI TEXTURE ####################
        if tex.type == 'VORONOI':
            texStrg+="crackle\n"
            texStrg+="    offset %.4g\n"%tex.nabla
            texStrg+="    form <%.4g,%.4g,%.4g>\n"%(tex.weight_1, tex.weight_2, tex.weight_3)
            if tex.distance_metric == 'DISTANCE':
                texStrg+="    metric 2.5\n"
            if tex.distance_metric == 'DISTANCE_SQUARED':
                texStrg+="    metric 2.5\n"
                texStrg+="    poly_wave 2\n"
            if tex.distance_metric == 'MINKOVSKY':
                texStrg+="    metric %s\n"%tex.minkovsky_exponent
            if tex.distance_metric == 'MINKOVSKY_FOUR':
                texStrg+="    metric 4\n"
            if tex.distance_metric == 'MINKOVSKY_HALF':
                texStrg+="    metric 0.5\n"
            if tex.distance_metric == 'CHEBYCHEV':
                texStrg+="    metric 10\n"
            if tex.distance_metric == 'MANHATTAN':
                texStrg+="    metric 1\n"

            if tex.color_mode == 'POSITION':
                texStrg+="solid\n"
            texStrg+="scale 0.25\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0 color rgbt<0,0,0,1>]\n"
                texStrg+="[1 color rgbt<1,1,1,0>]\n"
                texStrg+="}\n"

        ####################### EMULATE BLENDER CLOUDS TEXTURE ####################
        if tex.type == 'CLOUDS':
            if tex.noise_type == 'SOFT_NOISE':
                texStrg+="wrinkles\n"
                texStrg+="scale 0.25\n"
            else:
                texStrg+="granite\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0 color rgbt<0,0,0,1>]\n"
                texStrg+="[1 color rgbt<1,1,1,0>]\n"
                texStrg+="}\n"

        ####################### EMULATE BLENDER WOOD TEXTURE ####################
        if tex.type == 'WOOD':
            if tex.wood_type == 'RINGS':
                texStrg+="wood\n"
                texStrg+="scale 0.25\n"
            if tex.wood_type == 'RINGNOISE':
                texStrg+="wood\n"
                texStrg+="scale 0.25\n"
                texStrg+="turbulence %.4g\n"%(tex.turbulence/100)
            if tex.wood_type == 'BANDS':
                texStrg+="marble\n"
                texStrg+="scale 0.25\n"
                texStrg+="rotate <45,-45,45>\n"
            if tex.wood_type == 'BANDNOISE':
                texStrg+="marble\n"
                texStrg+="scale 0.25\n"
                texStrg+="rotate <45,-45,45>\n"
                texStrg+="turbulence %.4g\n"%(tex.turbulence/10)

            if tex.noise_basis_2 == 'SIN':
                texStrg+="sine_wave\n"
            if tex.noise_basis_2 == 'TRI':
                texStrg+="triangle_wave\n"
            if tex.noise_basis_2 == 'SAW':
                texStrg+="ramp_wave\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0 color rgbt<0,0,0,0>]\n"
                texStrg+="[1 color rgbt<1,1,1,0>]\n"
                texStrg+="}\n"

        ####################### EMULATE BLENDER STUCCI TEXTURE ####################
        if tex.type == 'STUCCI':
            texStrg+="bozo\n"
            texStrg+="scale 0.25\n"
            if tex.noise_type == 'HARD_NOISE':
                texStrg+="triangle_wave\n"
                if tex.use_color_ramp == True:
                    texStrg+=exportColorRamp(tex)
                else:
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbf<1,1,1,0>]\n"
                    texStrg+="[1 color rgbt<0,0,0,1>]\n"
                    texStrg+="}\n"
            else:
                if tex.use_color_ramp == True:
                    texStrg+=exportColorRamp(tex)
                else:
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbf<0,0,0,1>]\n"
                    texStrg+="[1 color rgbt<1,1,1,0>]\n"
                    texStrg+="}\n"

        ####################### EMULATE BLENDER MAGIC TEXTURE ####################
        if tex.type == 'MAGIC':
            texStrg+="leopard\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0 color rgbt<1,1,1,0.5>]\n"
                texStrg+="[0.25 color rgbf<0,1,0,0.75>]\n"
                texStrg+="[0.5 color rgbf<0,0,1,0.75>]\n"
                texStrg+="[0.75 color rgbf<1,0,1,0.75>]\n"
                texStrg+="[1 color rgbf<0,1,0,0.75>]\n"
                texStrg+="}\n"
            texStrg+="scale 0.1\n"

        ####################### EMULATE BLENDER MARBLE TEXTURE ####################
        if tex.type == 'MARBLE':
            texStrg+="marble\n"
            texStrg+="turbulence 0.5\n"
            texStrg+="noise_generator 3\n"
            texStrg+="scale 0.75\n"
            texStrg+="rotate <45,-45,45>\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                if tex.marble_type == 'SOFT':
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbt<0,0,0,0>]\n"
                    texStrg+="[0.05 color rgbt<0,0,0,0>]\n"
                    texStrg+="[1 color rgbt<0.9,0.9,0.9,0>]\n"
                    texStrg+="}\n"
                elif tex.marble_type == 'SHARP':
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbt<0,0,0,0>]\n"
                    texStrg+="[0.025 color rgbt<0,0,0,0>]\n"
                    texStrg+="[1 color rgbt<0.9,0.9,0.9,0>]\n"
                    texStrg+="}\n"
                else:
                    texStrg+="[0 color rgbt<0,0,0,0>]\n"
                    texStrg+="[1 color rgbt<1,1,1,0>]\n"
                    texStrg+="}\n"
            if tex.noise_basis_2 == 'SIN':
                texStrg+="sine_wave\n"
            if tex.noise_basis_2 == 'TRI':
                texStrg+="triangle_wave\n"
            if tex.noise_basis_2 == 'SAW':
                texStrg+="ramp_wave\n"

        ####################### EMULATE BLENDER BLEND TEXTURE ####################
        if tex.type == 'BLEND':
            if tex.progression=='RADIAL':
                texStrg+="radial\n"
                if tex.use_flip_axis=='HORIZONTAL':
                    texStrg+="rotate x*90\n"
                else:
                    texStrg+="rotate <-90,0,90>\n"
                texStrg+="ramp_wave\n"
            elif tex.progression=='SPHERICAL':
                texStrg+="spherical\n"
                texStrg+="scale 3\n"
                texStrg+="poly_wave 1\n"
            elif tex.progression=='QUADRATIC_SPHERE':
                texStrg+="spherical\n"
                texStrg+="scale 3\n"
                texStrg+="    poly_wave 2\n"
            elif tex.progression=='DIAGONAL':
                texStrg+="gradient <1,1,0>\n"
                texStrg+="scale 3\n"
            elif tex.use_flip_axis=='HORIZONTAL':
                texStrg+="gradient x\n"
                texStrg+="scale 2.01\n"
            elif tex.use_flip_axis=='VERTICAL':
                texStrg+="gradient y\n"
                texStrg+="scale 2.01\n"
            #texStrg+="ramp_wave\n"
            #texStrg+="frequency 0.5\n"
            texStrg+="phase 0.5\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0 color rgbt<1,1,1,0>]\n"
                texStrg+="[1 color rgbf<0,0,0,1>]\n"
                texStrg+="}\n"
            if tex.progression == 'LINEAR':
                texStrg+="    poly_wave 1\n"
            if tex.progression == 'QUADRATIC':
                texStrg+="    poly_wave 2\n"
            if tex.progression == 'EASING':
                texStrg+="    poly_wave 1.5\n"


        ####################### EMULATE BLENDER MUSGRAVE TEXTURE ####################
        # if tex.type == 'MUSGRAVE':
            # texStrg+="function{ f_ridged_mf( x, y, 0, 1, 2, 9, -0.5, 3,3 )*0.5}\n"
            # texStrg+="color_map {\n"
            # texStrg+="[0 color rgbf<0,0,0,1>]\n"
            # texStrg+="[1 color rgbf<1,1,1,0>]\n"
            # texStrg+="}\n"
        # simplified for now:

        if tex.type == 'MUSGRAVE':
            texStrg+="bozo scale 0.25 \n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {[0.5 color rgbf<0,0,0,1>][1 color rgbt<1,1,1,0>]}ramp_wave \n"

        ####################### EMULATE BLENDER DISTORTED NOISE TEXTURE ####################
        if tex.type == 'DISTORTED_NOISE':
            texStrg+="average\n"
            texStrg+="  pigment_map {\n"
            texStrg+="  [1 bozo scale 0.25 turbulence %.4g\n" %tex.distortion
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0 color rgbt<1,1,1,0>]\n"
                texStrg+="[1 color rgbf<0,0,0,1>]\n"
                texStrg+="}\n"
            texStrg+="]\n"

            if tex.noise_distortion == 'CELL_NOISE':
                texStrg+="  [1 cells scale 0.1\n"
                if tex.use_color_ramp == True:
                    texStrg+=exportColorRamp(tex)
                else:
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbt<1,1,1,0>]\n"
                    texStrg+="[1 color rgbf<0,0,0,1>]\n"
                    texStrg+="}\n"
                texStrg+="]\n"
            if tex.noise_distortion=='VORONOI_CRACKLE':
                texStrg+="  [1 crackle scale 0.25\n"
                if tex.use_color_ramp == True:
                    texStrg+=exportColorRamp(tex)
                else:
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbt<1,1,1,0>]\n"
                    texStrg+="[1 color rgbf<0,0,0,1>]\n"
                    texStrg+="}\n"
                texStrg+="]\n"
            if tex.noise_distortion in ['VORONOI_F1','VORONOI_F2','VORONOI_F3','VORONOI_F4','VORONOI_F2_F1']:
                texStrg+="  [1 crackle metric 2.5 scale 0.25 turbulence %.4g\n" %(tex.distortion/2)
                if tex.use_color_ramp == True:
                    texStrg+=exportColorRamp(tex)
                else:
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbt<1,1,1,0>]\n"
                    texStrg+="[1 color rgbf<0,0,0,1>]\n"
                    texStrg+="}\n"
                texStrg+="]\n"
            else:
                texStrg+="  [1 wrinkles scale 0.25\n"
                if tex.use_color_ramp == True:
                    texStrg+=exportColorRamp(tex)
                else:
                    texStrg+="color_map {\n"
                    texStrg+="[0 color rgbt<1,1,1,0>]\n"
                    texStrg+="[1 color rgbf<0,0,0,1>]\n"
                    texStrg+="}\n"
                texStrg+="]\n"
            texStrg+="  }\n"

        ####################### EMULATE BLENDER NOISE TEXTURE ####################
        if tex.type == 'NOISE':
            texStrg+="cells\n"
            texStrg+="turbulence 3\n"
            texStrg+="omega 3\n"
            if tex.use_color_ramp == True:
                texStrg+=exportColorRamp(tex)
            else:
                texStrg+="color_map {\n"
                texStrg+="[0.75 color rgb<0,0,0,>]\n"
                texStrg+="[1 color rgb<1,1,1,>]\n"
                texStrg+="}\n"

        ####################### IGNORE OTHER BLENDER TEXTURE ####################
        else: #non translated textures
            pass
        texStrg+="}\n\n"

        texStrg+="#declare f%s=\n"%PATname
        texStrg+="function{pigment{%s}}\n"%PATname
        texStrg+="\n"

    elif pat.tex_pattern_type != 'emulator':
        texStrg+="pigment {\n"
        texStrg+="%s\n"%pat.tex_pattern_type
        if pat.tex_pattern_type == 'agate':
            texStrg+="agate_turb %.4g\n"%pat.modifier_turbulence
        if pat.tex_pattern_type in {'spiral1', 'spiral2', 'tiling'}:
            texStrg+="%s\n"%pat.modifier_numbers
        if pat.tex_pattern_type == 'quilted':
            texStrg+="control0 %s control1 %s\n"%(pat.modifier_control0, pat.modifier_control1)
        if pat.tex_pattern_type == 'mandel':
            texStrg+="%s exponent %s \n"%(pat.f_iter, pat.f_exponent)
        if pat.tex_pattern_type == 'julia':
            texStrg+="<%.4g, %.4g> %s exponent %s \n"%(pat.julia_complex_1, pat.julia_complex_2, pat.f_iter, pat.f_exponent)
        if pat.tex_pattern_type == 'magnet' and pat.magnet_style == 'mandel':
            texStrg+="%s mandel %s \n"%(pat.magnet_type, pat.f_iter)
        if pat.tex_pattern_type == 'magnet' and pat.magnet_style == 'julia':
            texStrg+="%s julia <%.4g, %.4g> %s\n"%(pat.magnet_type, pat.julia_complex_1, pat.julia_complex_2, pat.f_iter)
        if pat.tex_pattern_type in {'mandel', 'julia', 'magnet'}:
            texStrg+="interior %s, %.4g\n"%(pat.f_ior, pat.f_ior_fac)
            texStrg+="exterior %s, %.4g\n"%(pat.f_eor, pat.f_eor_fac)
        if pat.tex_pattern_type == 'gradient':
            texStrg+="<%s, %s, %s> \n"%(pat.grad_orient_x, pat.grad_orient_y, pat.grad_orient_z)
        if pat.tex_pattern_type == 'pavement':
            numTiles=pat.pave_tiles
            numPattern=1
            if pat.pave_sides == '4' and pat.pave_tiles == 3:
                 numPattern = pat.pave_pat_2
            if pat.pave_sides == '6' and pat.pave_tiles == 3:
                numPattern = pat.pave_pat_3
            if pat.pave_sides == '3' and pat.pave_tiles == 4:
                numPattern = pat.pave_pat_3
            if pat.pave_sides == '3' and pat.pave_tiles == 5:
                numPattern = pat.pave_pat_4
            if pat.pave_sides == '4' and pat.pave_tiles == 4:
                numPattern = pat.pave_pat_5
            if pat.pave_sides == '6' and pat.pave_tiles == 4:
                numPattern = pat.pave_pat_7
            if pat.pave_sides == '4' and pat.pave_tiles == 5:
                numPattern = pat.pave_pat_12
            if pat.pave_sides == '3' and pat.pave_tiles == 6:
                numPattern = pat.pave_pat_12
            if pat.pave_sides == '6' and pat.pave_tiles == 5:
                numPattern = pat.pave_pat_22
            if pat.pave_sides == '4' and pat.pave_tiles == 6:
                numPattern = pat.pave_pat_35
            if pat.pave_sides == '6' and pat.pave_tiles == 6:
                numTiles = 5
            texStrg+="number_of_sides %s number_of_tiles %s pattern %s form %s \n"%(pat.pave_sides, numTiles, numPattern, pat.pave_form)
        ################ functions #####################################################################################################
        if pat.tex_pattern_type == 'function':
            texStrg+="{ %s"%pat.func_list
            texStrg+="(x"
            if pat.func_plus_x != "NONE":
                if pat.func_plus_x =='increase':
                    texStrg+="*"
                if pat.func_plus_x =='plus':
                    texStrg+="+"
                texStrg+="%.4g"%pat.func_x
            texStrg+=",y"
            if pat.func_plus_y != "NONE":
                if pat.func_plus_y =='increase':
                    texStrg+="*"
                if pat.func_plus_y =='plus':
                    texStrg+="+"
                texStrg+="%.4g"%pat.func_y
            texStrg+=",z"
            if pat.func_plus_z != "NONE":
                if pat.func_plus_z =='increase':
                    texStrg+="*"
                if pat.func_plus_z =='plus':
                    texStrg+="+"
                texStrg+="%.4g"%pat.func_z
            sort = -1
            if pat.func_list in {"f_comma","f_crossed_trough","f_cubic_saddle","f_cushion","f_devils_curve",
                                 "f_enneper","f_glob","f_heart","f_hex_x","f_hex_y","f_hunt_surface",
                                 "f_klein_bottle","f_kummer_surface_v1","f_lemniscate_of_gerono","f_mitre",
                                 "f_nodal_cubic","f_noise_generator","f_odd","f_paraboloid","f_pillow",
                                 "f_piriform","f_quantum","f_quartic_paraboloid","f_quartic_saddle",
                                 "f_sphere","f_steiners_roman","f_torus_gumdrop","f_umbrella"}:
                sort = 0
            if pat.func_list in {"f_bicorn","f_bifolia","f_boy_surface","f_superellipsoid","f_torus"}:
                sort = 1
            if pat.func_list in {"f_ellipsoid","f_folium_surface","f_hyperbolic_torus",
                                 "f_kampyle_of_eudoxus","f_parabolic_torus","f_quartic_cylinder","f_torus2"}:
                sort = 2
            if pat.func_list in {"f_blob2","f_cross_ellipsoids","f_flange_cover","f_isect_ellipsoids",
                                 "f_kummer_surface_v2","f_ovals_of_cassini","f_rounded_box","f_spikes_2d","f_strophoid"}:
                sort = 3
            if pat.func_list in {"f_algbr_cyl1","f_algbr_cyl2","f_algbr_cyl3","f_algbr_cyl4","f_blob","f_mesh1","f_poly4","f_spikes"}:
                sort = 4
            if pat.func_list in {"f_devils_curve_2d","f_dupin_cyclid","f_folium_surface_2d","f_hetero_mf","f_kampyle_of_eudoxus_2d",
                                 "f_lemniscate_of_gerono_2d","f_polytubes","f_ridge","f_ridged_mf","f_spiral","f_witch_of_agnesi"}:
                sort = 5
            if pat.func_list in {"f_helix1","f_helix2","f_piriform_2d","f_strophoid_2d"}:
                sort = 6
            if pat.func_list == "f_helical_torus":
                sort = 7
            if sort > -1:
                texStrg+=",%.4g"%pat.func_P0
            if sort > 0:
                texStrg+=",%.4g"%pat.func_P1
            if sort > 1:
                texStrg+=",%.4g"%pat.func_P2
            if sort > 2:
                texStrg+=",%.4g"%pat.func_P3
            if sort > 3:
                texStrg+=",%.4g"%pat.func_P4
            if sort > 4:
                texStrg+=",%.4g"%pat.func_P5
            if sort > 5:
                texStrg+=",%.4g"%pat.func_P6
            if sort > 6:
                texStrg+=",%.4g"%pat.func_P7
                texStrg+=",%.4g"%pat.func_P8
                texStrg+=",%.4g"%pat.func_P9
            texStrg+=")}\n"
        ############## end functions ###############################################################
        if pat.tex_pattern_type not in {'checker', 'hexagon', 'square', 'triangular', 'brick'}:
            texStrg+="color_map {\n"
        numColor=0
        if tex.use_color_ramp == True:
            for el in tex.color_ramp.elements:
                numColor+=1
                pos = el.position
                col=el.color
                colR,colG,colB,colA = col[0],col[1],col[2],1-col[3]
                if pat.tex_pattern_type not in {'checker', 'hexagon', 'square', 'triangular', 'brick'} :
                    texStrg+="[%.4g color rgbf<%.4g,%.4g,%.4g,%.4g>] \n"%(pos,colR,colG,colB,colA)
                if pat.tex_pattern_type in {'brick','checker'} and numColor < 3:
                    texStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
                if pat.tex_pattern_type == 'hexagon' and numColor < 4 :
                    texStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
                if pat.tex_pattern_type == 'square' and numColor < 5 :
                    texStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
                if pat.tex_pattern_type == 'triangular' and numColor < 7 :
                    texStrg+="color rgbf<%.4g,%.4g,%.4g,%.4g> \n"%(colR,colG,colB,colA)
        else:
            texStrg+="[0 color rgbf<0,0,0,1>]\n"
            texStrg+="[1 color rgbf<1,1,1,0>]\n"
        if pat.tex_pattern_type not in {'checker', 'hexagon', 'square', 'triangular', 'brick'} :
            texStrg+="} \n"
        if pat.tex_pattern_type == 'brick':
            texStrg+="brick_size <%.4g, %.4g, %.4g> mortar %.4g \n"%(pat.brick_size_x, pat.brick_size_y, pat.brick_size_z, pat.brick_mortar)
        texStrg+="%s \n"%mappingDif
        texStrg+="rotate <%.4g,%.4g,%.4g> \n"%(pat.tex_rot_x, pat.tex_rot_y, pat.tex_rot_z)
        texStrg+="turbulence <%.4g,%.4g,%.4g> \n"%(pat.warp_turbulence_x, pat.warp_turbulence_y, pat.warp_turbulence_z)
        texStrg+="octaves %s \n"%pat.modifier_octaves
        texStrg+="lambda %.4g \n"%pat.modifier_lambda
        texStrg+="omega %.4g \n"%pat.modifier_omega
        texStrg+="frequency %.4g \n"%pat.modifier_frequency
        texStrg+="phase %.4g \n"%pat.modifier_phase
        texStrg+="}\n\n"
        texStrg+="#declare f%s=\n"%PATname
        texStrg+="function{pigment{%s}}\n"%PATname
        texStrg+="\n"
    return(texStrg)


def writeTextureInfluence(mater, materialNames, LocalMaterialNames, path_image, lampCount,
                            imageFormat, imgMap, imgMapTransforms, tabWrite, comments,
                            string_strip_hyphen, safety, col, os, preview_dir, unpacked_images):
    material_finish = materialNames[mater.name]
    if mater.use_transparency:
        trans = 1.0 - mater.alpha
    else:
        trans = 0.0
    if ((mater.specular_color.s == 0.0) or (mater.diffuse_shader == 'MINNAERT')):
    # No layered texture because of aoi pattern used for minnaert and pov can't layer patterned
        colored_specular_found = False
    else:
        colored_specular_found = True

    if mater.use_transparency and mater.transparency_method == 'RAYTRACE':
        povFilter = mater.raytrace_transparency.filter * (1.0 - mater.alpha)
        trans = (1.0 - mater.alpha) - povFilter
    else:
        povFilter = 0.0

    ##############SF
    texturesDif = ""
    texturesSpec = ""
    texturesNorm = ""
    texturesAlpha = ""
    #proceduralFlag=False
    for t in mater.texture_slots:
        if t and (t.use and (t.texture is not None)):
            # 'NONE' ('NONE' type texture is different from no texture covered above)
            if (t.texture.type == 'NONE' and t.texture.pov.tex_pattern_type == 'emulator'):
                continue # move to next slot
            # PROCEDURAL
            elif (t.texture.type != 'IMAGE' and t.texture.type != 'NONE'):
                proceduralFlag=True
                image_filename = "PAT_%s"%string_strip_hyphen(bpy.path.clean_name(t.texture.name))
                if image_filename:
                    if t.use_map_color_diffuse:
                        texturesDif = image_filename
                        # colvalue = t.default_value  # UNUSED
                        t_dif = t
                        if t_dif.texture.pov.tex_gamma_enable:
                            imgGamma = (" gamma %.3g " % t_dif.texture.pov.tex_gamma_value)
                    if t.use_map_specular or t.use_map_raymir:
                        texturesSpec = image_filename
                        # colvalue = t.default_value  # UNUSED
                        t_spec = t
                    if t.use_map_normal:
                        texturesNorm = image_filename
                        # colvalue = t.normal_factor/10 # UNUSED
                        #textNormName=t.texture.image.name + ".normal"
                        #was the above used? --MR
                        t_nor = t
                    if t.use_map_alpha:
                        texturesAlpha = image_filename
                        # colvalue = t.alpha_factor * 10.0  # UNUSED
                        #textDispName=t.texture.image.name + ".displ"
                        #was the above used? --MR
                        t_alpha = t

            # RASTER IMAGE
            elif (t.texture.type == 'IMAGE' and t.texture.image and t.texture.pov.tex_pattern_type == 'emulator'):
                proceduralFlag=False
                #PACKED
                if t.texture.image.packed_file:
                    orig_image_filename=t.texture.image.filepath_raw
                    unpackedfilename= os.path.join(preview_dir,("unpacked_img_"+(string_strip_hyphen(bpy.path.clean_name(t.texture.name)))))
                    if not os.path.exists(unpackedfilename):
                        # record which images that were newly copied and can be safely
                        # cleaned up
                        unpacked_images.append(unpackedfilename)
                    t.texture.image.filepath_raw=unpackedfilename
                    t.texture.image.save()
                    image_filename = unpackedfilename.replace("\\","/")
                    # .replace("\\","/") to get only forward slashes as it's what POV prefers,
                    # even on windows
                    t.texture.image.filepath_raw=orig_image_filename
                #FILE
                else:
                    image_filename = path_image(t.texture.image)
                # IMAGE SEQUENCE BEGINS
                if image_filename:
                    if bpy.data.images[t.texture.image.name].source == 'SEQUENCE':
                        korvaa = "." + str(t.texture.image_user.frame_offset + 1).zfill(3) + "."
                        image_filename = image_filename.replace(".001.", korvaa)
                        print(" seq debug ")
                        print(image_filename)
                # IMAGE SEQUENCE ENDS
                imgGamma = ""
                if image_filename:
                    if t.use_map_color_diffuse:
                        texturesDif = image_filename
                        # colvalue = t.default_value  # UNUSED
                        t_dif = t
                        if t_dif.texture.pov.tex_gamma_enable:
                            imgGamma = (" gamma %.3g " % t_dif.texture.pov.tex_gamma_value)
                    if t.use_map_specular or t.use_map_raymir:
                        texturesSpec = image_filename
                        # colvalue = t.default_value  # UNUSED
                        t_spec = t
                    if t.use_map_normal:
                        texturesNorm = image_filename
                        # colvalue = t.normal_factor/10  # UNUSED
                        #textNormName=t.texture.image.name + ".normal"
                        #was the above used? --MR
                        t_nor = t
                    if t.use_map_alpha:
                        texturesAlpha = image_filename
                        # colvalue = t.alpha_factor * 10.0  # UNUSED
                        #textDispName=t.texture.image.name + ".displ"
                        #was the above used? --MR
                        t_alpha = t

    ####################################################################################


    tabWrite("\n")
    # THIS AREA NEEDS TO LEAVE THE TEXTURE OPEN UNTIL ALL MAPS ARE WRITTEN DOWN.
    # --MR
    currentMatName = string_strip_hyphen(materialNames[mater.name])
    LocalMaterialNames.append(currentMatName)
    tabWrite("\n#declare MAT_%s = \ntexture{\n" % currentMatName)
    ################################################################################

    if mater.pov.replacement_text != "":
        tabWrite("%s\n" % mater.pov.replacement_text)
    #################################################################################
    if mater.diffuse_shader == 'MINNAERT':
        tabWrite("\n")
        tabWrite("aoi\n")
        tabWrite("texture_map {\n")
        tabWrite("[%.3g finish {diffuse %.3g}]\n" % \
                 (mater.darkness / 2.0, 2.0 - mater.darkness))
        tabWrite("[%.3g\n" % (1.0 - (mater.darkness / 2.0)))

    if mater.diffuse_shader == 'FRESNEL':
        # For FRESNEL diffuse in POV, we'll layer slope patterned textures
        # with lamp vector as the slope vector and nest one slope per lamp
        # into each texture map's entry.

        c = 1
        while (c <= lampCount):
            tabWrite("slope { lampTarget%s }\n" % (c))
            tabWrite("texture_map {\n")
            # Diffuse Fresnel value and factor go up to five,
            # other kind of values needed: used the number 5 below to remap
            tabWrite("[%.3g finish {diffuse %.3g}]\n" % \
                     ((5.0 - mater.diffuse_fresnel) / 5,
                      (mater.diffuse_intensity *
                       ((5.0 - mater.diffuse_fresnel_factor) / 5))))
            tabWrite("[%.3g\n" % ((mater.diffuse_fresnel_factor / 5) *
                                  (mater.diffuse_fresnel / 5.0)))
            c += 1

    # if shader is a 'FRESNEL' or 'MINNAERT': slope pigment pattern or aoi
    # and texture map above, the rest below as one of its entry

    if texturesSpec != "" or texturesAlpha != "":
        if texturesSpec != "":
            # tabWrite("\n")
            tabWrite("pigment_pattern {\n")

            mappingSpec =imgMapTransforms(t_spec)
            if texturesSpec and texturesSpec.startswith("PAT_"):
                tabWrite("function{f%s(x,y,z).grey}\n" %texturesSpec)
                tabWrite("%s\n" % mappingSpec)
            else:

                tabWrite("uv_mapping image_map{%s \"%s\" %s}\n" % \
                         (imageFormat(texturesSpec), texturesSpec, imgMap(t_spec)))
                tabWrite("%s\n" % mappingSpec)
            tabWrite("}\n")
            tabWrite("texture_map {\n")
            tabWrite("[0 \n")

        if texturesDif == "":
            if texturesAlpha != "":
                tabWrite("\n")

                mappingAlpha = imgMapTransforms(t_alpha)

                if texturesAlpha and texturesAlpha.startswith("PAT_"):
                    tabWrite("function{f%s(x,y,z).transmit}%s\n" %(texturesAlpha, mappingAlpha))
                else:

                    tabWrite("pigment {pigment_pattern {uv_mapping image_map" \
                             "{%s \"%s\" %s}%s" % \
                             (imageFormat(texturesAlpha), texturesAlpha,
                              imgMap(t_alpha), mappingAlpha))
                tabWrite("}\n")
                tabWrite("pigment_map {\n")
                tabWrite("[0 color rgbft<0,0,0,1,1>]\n")
                tabWrite("[1 color rgbft<%.3g, %.3g, %.3g, %.3g, %.3g>]\n" % \
                         (col[0], col[1], col[2], povFilter, trans))
                tabWrite("}\n")
                tabWrite("}\n")

            else:

                tabWrite("pigment {rgbft<%.3g, %.3g, %.3g, %.3g, %.3g>}\n" % \
                         (col[0], col[1], col[2], povFilter, trans))

            if texturesSpec != "":
                # Level 1 is no specular
                tabWrite("finish {%s}\n" % (safety(material_finish, Level=1)))

            else:
                # Level 2 is translated spec
                tabWrite("finish {%s}\n" % (safety(material_finish, Level=2)))

        else:
            mappingDif = imgMapTransforms(t_dif)

            if texturesAlpha != "":
                mappingAlpha = imgMapTransforms(t_alpha)

                tabWrite("pigment {\n")
                tabWrite("pigment_pattern {\n")
                if texturesAlpha and texturesAlpha.startswith("PAT_"):
                    tabWrite("function{f%s(x,y,z).transmit}%s\n" %(texturesAlpha, mappingAlpha))
                else:
                    tabWrite("uv_mapping image_map{%s \"%s\" %s}%s}\n" % \
                             (imageFormat(texturesAlpha), texturesAlpha,
                              imgMap(t_alpha), mappingAlpha))
                tabWrite("pigment_map {\n")
                tabWrite("[0 color rgbft<0,0,0,1,1>]\n")
                #if texturesAlpha and texturesAlpha.startswith("PAT_"):
                    #tabWrite("[1 pigment{%s}]\n" %texturesDif)
                if texturesDif and not texturesDif.startswith("PAT_"):
                    tabWrite("[1 uv_mapping image_map {%s \"%s\" %s} %s]\n" % \
                             (imageFormat(texturesDif), texturesDif,
                              (imgGamma + imgMap(t_dif)), mappingDif))
                elif texturesDif and texturesDif.startswith("PAT_"):
                    tabWrite("[1 %s]\n" %texturesDif)
                tabWrite("}\n")
                tabWrite("}\n")
                if texturesAlpha and texturesAlpha.startswith("PAT_"):
                    tabWrite("}\n")

            else:
                if texturesDif and texturesDif.startswith("PAT_"):
                    tabWrite("pigment{%s}\n" %texturesDif)
                else:
                    tabWrite("pigment {uv_mapping image_map {%s \"%s\" %s}%s}\n" % \
                             (imageFormat(texturesDif), texturesDif,
                              (imgGamma + imgMap(t_dif)), mappingDif))

            if texturesSpec != "":
                # Level 1 is no specular
                tabWrite("finish {%s}\n" % (safety(material_finish, Level=1)))

            else:
                # Level 2 is translated specular
                tabWrite("finish {%s}\n" % (safety(material_finish, Level=2)))

            ## scale 1 rotate y*0
            #imageMap = ("{image_map {%s \"%s\" %s }\n" % \
            #            (imageFormat(textures),textures,imgMap(t_dif)))
            #tabWrite("uv_mapping pigment %s} %s finish {%s}\n" % \
            #         (imageMap,mapping,safety(material_finish)))
            #tabWrite("pigment {uv_mapping image_map {%s \"%s\" %s}%s} " \
            #         "finish {%s}\n" % \
            #         (imageFormat(texturesDif), texturesDif, imgMap(t_dif),
            #          mappingDif, safety(material_finish)))
        if texturesNorm != "":
            ## scale 1 rotate y*0

            mappingNor =imgMapTransforms(t_nor)

            if texturesNorm and texturesNorm.startswith("PAT_"):
                tabWrite("normal{function{f%s(x,y,z).grey} bump_size %.4g %s}\n" %(texturesNorm, t_nor.normal_factor, mappingNor))
            else:
                tabWrite("normal {uv_mapping bump_map " \
                         "{%s \"%s\" %s  bump_size %.4g }%s}\n" % \
                         (imageFormat(texturesNorm), texturesNorm, imgMap(t_nor),
                          t_nor.normal_factor, mappingNor))
        if texturesSpec != "":
            tabWrite("]\n")
        ##################Second index for mapping specular max value###############
            tabWrite("[1 \n")

    if texturesDif == "" and mater.pov.replacement_text == "":
        if texturesAlpha != "":
            mappingAlpha = imgMapTransforms(t_alpha)

            if texturesAlpha and texturesAlpha.startswith("PAT_"):
                tabWrite("function{f%s(x,y,z).transmit %s}\n" %(texturesAlpha, mappingAlpha))
            else:
                tabWrite("pigment {pigment_pattern {uv_mapping image_map" \
                         "{%s \"%s\" %s}%s}\n" % \
                         (imageFormat(texturesAlpha), texturesAlpha, imgMap(t_alpha),
                          mappingAlpha))
            tabWrite("pigment_map {\n")
            tabWrite("[0 color rgbft<0,0,0,1,1>]\n")
            tabWrite("[1 color rgbft<%.3g, %.3g, %.3g, %.3g, %.3g>]\n" % \
                     (col[0], col[1], col[2], povFilter, trans))
            tabWrite("}\n")
            tabWrite("}\n")

        else:
            tabWrite("pigment {rgbft<%.3g, %.3g, %.3g, %.3g, %.3g>}\n" % \
                     (col[0], col[1], col[2], povFilter, trans))


        if texturesSpec != "":
            # Level 3 is full specular
            tabWrite("finish {%s}\n" % (safety(material_finish, Level=3)))

        elif colored_specular_found:
            # Level 1 is no specular
            tabWrite("finish {%s}\n" % (safety(material_finish, Level=1)))

        else:
            # Level 2 is translated specular
            tabWrite("finish {%s}\n" % (safety(material_finish, Level=2)))

    elif mater.pov.replacement_text == "":
        mappingDif = imgMapTransforms(t_dif)

        if texturesAlpha != "":

            mappingAlpha = imgMapTransforms(t_alpha)

            if texturesAlpha and texturesAlpha.startswith("PAT_"):
                tabWrite("pigment{pigment_pattern {function{f%s(x,y,z).transmit}%s}\n" %(texturesAlpha, mappingAlpha))
            else:
                tabWrite("pigment {pigment_pattern {uv_mapping image_map" \
                         "{%s \"%s\" %s}%s}\n" % \
                         (imageFormat(texturesAlpha), texturesAlpha, imgMap(t_alpha),
                          mappingAlpha))
            tabWrite("pigment_map {\n")
            tabWrite("[0 color rgbft<0,0,0,1,1>]\n")
            if texturesAlpha and texturesAlpha.startswith("PAT_"):
                tabWrite("[1 function{f%s(x,y,z).transmit}%s]\n" %(texturesAlpha, mappingAlpha))
            elif texturesDif and not texturesDif.startswith("PAT_"):
                tabWrite("[1 uv_mapping image_map {%s \"%s\" %s} %s]\n" % \
                         (imageFormat(texturesDif), texturesDif,
                          (imgMap(t_dif) + imgGamma), mappingDif))
            elif texturesDif and texturesDif.startswith("PAT_"):
                tabWrite("[1 %s %s]\n" %(texturesDif, mappingDif))
            tabWrite("}\n")
            tabWrite("}\n")

        else:
            if texturesDif and texturesDif.startswith("PAT_"):
                tabWrite("pigment{%s %s}\n" %(texturesDif, mappingDif))
                print('XXXMEEEERDE!')
            else:
                tabWrite("pigment {\n")
                tabWrite("uv_mapping image_map {\n")
                #tabWrite("%s \"%s\" %s}%s\n" % \
                #         (imageFormat(texturesDif), texturesDif,
                #         (imgGamma + imgMap(t_dif)),mappingDif))
                tabWrite("%s \"%s\" \n" % (imageFormat(texturesDif), texturesDif))
                tabWrite("%s\n" % (imgGamma + imgMap(t_dif)))
                tabWrite("}\n")
                tabWrite("%s\n" % mappingDif)
                tabWrite("}\n")

        if texturesSpec != "":
            # Level 3 is full specular
            tabWrite("finish {%s}\n" % (safety(material_finish, Level=3)))
        else:
            # Level 2 is translated specular
            tabWrite("finish {%s}\n" % (safety(material_finish, Level=2)))

        ## scale 1 rotate y*0
        #imageMap = ("{image_map {%s \"%s\" %s }" % \
        #            (imageFormat(textures), textures,imgMap(t_dif)))
        #tabWrite("\n\t\t\tuv_mapping pigment %s} %s finish {%s}" % \
        #           (imageMap, mapping, safety(material_finish)))
        #tabWrite("\n\t\t\tpigment {uv_mapping image_map " \
        #           "{%s \"%s\" %s}%s} finish {%s}" % \
        #           (imageFormat(texturesDif), texturesDif,imgMap(t_dif),
        #            mappingDif, safety(material_finish)))
    if texturesNorm != "" and mater.pov.replacement_text == "":


        mappingNor =imgMapTransforms(t_nor)

        if texturesNorm and texturesNorm.startswith("PAT_"):
            tabWrite("normal{function{f%s(x,y,z).grey} bump_size %.4g %s}\n" %(texturesNorm, t_nor.normal_factor, mappingNor))
        else:
            tabWrite("normal {uv_mapping bump_map {%s \"%s\" %s  bump_size %.4g }%s}\n" % \
                     (imageFormat(texturesNorm), texturesNorm, imgMap(t_nor),
                      t_nor.normal_factor, mappingNor))
    if texturesSpec != "" and mater.pov.replacement_text == "":
        tabWrite("]\n")

        tabWrite("}\n")

    #End of slope/ior texture_map
    if mater.diffuse_shader == 'MINNAERT' and mater.pov.replacement_text == "":
        tabWrite("]\n")
        tabWrite("}\n")
    if mater.diffuse_shader == 'FRESNEL' and mater.pov.replacement_text == "":
        c = 1
        while (c <= lampCount):
            tabWrite("]\n")
            tabWrite("}\n")
            c += 1



    # Close first layer of POV "texture" (Blender material)
    tabWrite("}\n")

    if ((mater.specular_color.s > 0.0) and (mater.diffuse_shader != 'MINNAERT')):

        colored_specular_found = True
    else:
        colored_specular_found = False

    # Write another layered texture using invisible diffuse and metallic trick
    # to emulate colored specular highlights
    special_texture_found = False
    for t in mater.texture_slots:
        if(t and t.use and ((t.texture.type == 'IMAGE' and t.texture.image) or t.texture.type != 'IMAGE') and
           (t.use_map_specular or t.use_map_raymir)):
            # Specular mapped textures would conflict with colored specular
            # because POV can't layer over or under pigment patterned textures
            special_texture_found = True

    if colored_specular_found and not special_texture_found:
        if comments:
            tabWrite("  // colored highlights with a stransparent metallic layer\n")
        else:
            tabWrite("\n")

        tabWrite("texture {\n")
        tabWrite("pigment {rgbft<%.3g, %.3g, %.3g, 0, 1>}\n" % \
                         (mater.specular_color[0], mater.specular_color[1], mater.specular_color[2]))
        tabWrite("finish {%s}\n" % (safety(material_finish, Level=2))) # Level 2 is translated spec

        texturesNorm = ""
        for t in mater.texture_slots:

            if t and t.texture.pov.tex_pattern_type != 'emulator':
                proceduralFlag=True
                image_filename = string_strip_hyphen(bpy.path.clean_name(t.texture.name))
            if (t and t.texture.type == 'IMAGE' and
                    t.use and t.texture.image and
                    t.texture.pov.tex_pattern_type == 'emulator'):
                proceduralFlag=False
                image_filename = path_image(t.texture.image)
                imgGamma = ""
                if image_filename:
                    if t.use_map_normal:
                        texturesNorm = image_filename
                        # colvalue = t.normal_factor/10  # UNUSED
                        #textNormName=t.texture.image.name + ".normal"
                        #was the above used? --MR
                        t_nor = t
                        if proceduralFlag:
                            tabWrite("normal{function" \
                                     "{f%s(x,y,z).grey} bump_size %.4g}\n" % \
                                     (texturesNorm,
                                     t_nor.normal_factor))
                        else:
                            tabWrite("normal {uv_mapping bump_map " \
                                     "{%s \"%s\" %s  bump_size %.4g }%s}\n" % \
                                     (imageFormat(texturesNorm),
                                     texturesNorm, imgMap(t_nor),
                                     t_nor.normal_factor,
                                     mappingNor))

        tabWrite("}\n") # THEN IT CAN CLOSE LAST LAYER OF TEXTURE

def string_strip_hyphen(name):
    return name.replace("-", "")
# WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
def write_nodes(scene,povMatName,ntree,file):
    declareNodes=[]
    scene=bpy.context.scene
    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayFinishNode" and node.outputs["Finish"].is_linked:
            file.write('#declare %s = finish {\n'%povNodeName)
            emission=node.inputs["Emission"].default_value
            if node.inputs["Emission"].is_linked:
                pass
            file.write('    emission %.4g\n'%emission)
            for link in ntree.links:
                if link.to_node == node:

                    if link.from_node.bl_idname == 'PovrayDiffuseNode':
                        intensity=0
                        albedo=""
                        brilliance=0
                        crand=0
                        if link.from_node.inputs["Intensity"].is_linked:
                            pass
                        else:
                            intensity=link.from_node.inputs["Intensity"].default_value
                        if link.from_node.inputs["Albedo"].is_linked:
                            pass
                        else:
                            if link.from_node.inputs["Albedo"].default_value == True:
                                albedo = "albedo"
                        file.write('    diffuse %s %.4g\n'%(albedo,intensity))
                        if link.from_node.inputs["Brilliance"].is_linked:
                            pass
                        else:
                            brilliance=link.from_node.inputs["Brilliance"].default_value
                        file.write('    brilliance %.4g\n'%brilliance)
                        if link.from_node.inputs["Crand"].is_linked:
                            pass
                        else:
                            crand=link.from_node.inputs["Crand"].default_value
                        if crand > 0:
                            file.write('    crand %.4g\n'%crand)


                    if link.from_node.bl_idname == 'PovraySubsurfaceNode':
                        if scene.povray.sslt_enable:
                            energy = 0
                            r = g = b = 0
                            if link.from_node.inputs["Translucency"].is_linked:
                                pass
                            else:
                                r,g,b,a=link.from_node.inputs["Translucency"].default_value[:]
                            if link.from_node.inputs["Energy"].is_linked:
                                pass
                            else:
                                energy=link.from_node.inputs["Energy"].default_value
                            file.write('    subsurface { translucency <%.4g,%.4g,%.4g>*%s }\n'%(r,g,b,energy))



                    if link.from_node.bl_idname in {'PovraySpecularNode','PovrayPhongNode'}:
                        intensity=0
                        albedo=""
                        roughness=0
                        metallic=0
                        phong_size=0
                        highlight="specular"
                        if link.from_node.inputs["Intensity"].is_linked:
                            pass
                        else:
                            intensity=link.from_node.inputs["Intensity"].default_value

                        if link.from_node.inputs["Albedo"].is_linked:
                            pass
                        else:
                            if link.from_node.inputs["Albedo"].default_value == True:
                                albedo = "albedo"
                        if link.from_node.bl_idname in {'PovrayPhongNode'}:
                            highlight="phong"
                        file.write('    %s %s %.4g\n'%(highlight,albedo,intensity))

                        if link.from_node.bl_idname in {'PovraySpecularNode'}:
                            if link.from_node.inputs["Roughness"].is_linked:
                                pass
                            else:
                                roughness=link.from_node.inputs["Roughness"].default_value
                            file.write('    roughness %.6g\n'%roughness)

                        if link.from_node.bl_idname in {'PovrayPhongNode'}:
                            if link.from_node.inputs["Size"].is_linked:
                                pass
                            else:
                                phong_size=link.from_node.inputs["Size"].default_value
                            file.write('    phong_size %s\n'%phong_size)

                        if link.from_node.inputs["Metallic"].is_linked:
                            pass
                        else:
                            metallic=link.from_node.inputs["Metallic"].default_value
                        file.write('    metallic %.4g\n'%metallic)

                    if link.from_node.bl_idname in {'PovrayMirrorNode'}:
                        file.write('    reflection {\n')
                        color=None
                        exponent=0
                        metallic=0
                        falloff=0
                        fresnel=""
                        conserve=""
                        if link.from_node.inputs["Color"].is_linked:
                            pass
                        else:
                            color=link.from_node.inputs["Color"].default_value[:]
                        file.write('    <%.4g,%.4g,%.4g>\n'%color)

                        if link.from_node.inputs["Exponent"].is_linked:
                            pass
                        else:
                            exponent=link.from_node.inputs["Exponent"].default_value
                        file.write('    exponent %.4g\n'%exponent)

                        if link.from_node.inputs["Falloff"].is_linked:
                            pass
                        else:
                            falloff=link.from_node.inputs["Falloff"].default_value
                        file.write('    falloff %.4g\n'%falloff)

                        if link.from_node.inputs["Metallic"].is_linked:
                            pass
                        else:
                            metallic=link.from_node.inputs["Metallic"].default_value
                        file.write('    metallic %.4g'%metallic)

                        if link.from_node.inputs["Fresnel"].is_linked:
                            pass
                        else:
                            if link.from_node.inputs["Fresnel"].default_value==True:
                                fresnel="fresnel"

                        if link.from_node.inputs["Conserve energy"].is_linked:
                            pass
                        else:
                            if link.from_node.inputs["Conserve energy"].default_value==True:
                                conserve="conserve_energy"

                        file.write('    %s}\n    %s\n'%(fresnel,conserve))

                    if link.from_node.bl_idname == 'PovrayAmbientNode':
                        ambient=(0,0,0)
                        if link.from_node.inputs["Ambient"].is_linked:
                            pass
                        else:
                            ambient=link.from_node.inputs["Ambient"].default_value[:]
                        file.write('    ambient <%.4g,%.4g,%.4g>\n'%ambient)

                    if link.from_node.bl_idname in {'PovrayIridescenceNode'}:
                        file.write('    irid {\n')
                        amount=0
                        thickness=0
                        turbulence=0
                        if link.from_node.inputs["Amount"].is_linked:
                            pass
                        else:
                            amount=link.from_node.inputs["Amount"].default_value
                        file.write('    %.4g\n'%amount)

                        if link.from_node.inputs["Thickness"].is_linked:
                            pass
                        else:
                            exponent=link.from_node.inputs["Thickness"].default_value
                        file.write('    thickness %.4g\n'%thickness)

                        if link.from_node.inputs["Turbulence"].is_linked:
                            pass
                        else:
                            falloff=link.from_node.inputs["Turbulence"].default_value
                        file.write('    turbulence %.4g}\n'%turbulence)

            file.write('}\n')

    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayTransformNode" and node.outputs["Transform"].is_linked:
            tx=node.inputs["Translate x"].default_value
            ty=node.inputs["Translate y"].default_value
            tz=node.inputs["Translate z"].default_value
            rx=node.inputs["Rotate x"].default_value
            ry=node.inputs["Rotate y"].default_value
            rz=node.inputs["Rotate z"].default_value
            sx=node.inputs["Scale x"].default_value
            sy=node.inputs["Scale y"].default_value
            sz=node.inputs["Scale z"].default_value
            file.write('#declare %s = transform {\n    translate<%.4g,%.4g,%.4g>\n    rotate<%.4g,%.4g,%.4g>\n    scale<%.4g,%.4g,%.4g>}\n'%(povNodeName,tx,ty,tz,rx,ry,rz,sx,sy,sz))

    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayColorImageNode" and node.outputs["Pigment"].is_linked:
            declareNodes.append(node.name)
            if node.image == "":
                file.write('#declare %s = pigment { color rgb 0.8}\n'%(povNodeName))
            else:
                im=bpy.data.images[node.image]
                if im.filepath and os.path.exists(bpy.path.abspath(im.filepath)):
                    transform = ""
                    for link in ntree.links:
                        if link.from_node.bl_idname=='PovrayTransformNode' and link.to_node==node:
                            povTransName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                            transform="transform {%s}"%povTransName
                    uv=""
                    if node.map_type=="uv_mapping":
                        uv="uv_mapping"
                    filepath=bpy.path.abspath(im.filepath)
                    file.write('#declare %s = pigment {%s image_map {\n'%(povNodeName,uv))
                    premul="off"
                    if node.premultiplied:
                        premul="on"
                    once=""
                    if node.once:
                        once="once"
                    file.write('    "%s"\n    gamma %.6g\n    premultiplied %s\n'%(filepath,node.inputs["Gamma"].default_value,premul))
                    file.write('    %s\n'%once)
                    if node.map_type!="uv_mapping":
                        file.write('    map_type %s\n'%(node.map_type))
                    file.write('    interpolate %s\n    filter all %.4g\n    transmit all %.4g\n'%
                        (node.interpolate,node.inputs["Filter"].default_value,node.inputs["Transmit"].default_value))
                    file.write('    }\n')
                    file.write('    %s\n'%transform)
                    file.write('    }\n')

    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayImagePatternNode" and node.outputs["Pattern"].is_linked:
            declareNodes.append(node.name)
            if node.image != "":
                im=bpy.data.images[node.image]
                if im.filepath and os.path.exists(bpy.path.abspath(im.filepath)):
                    transform = ""
                    for link in ntree.links:
                        if link.from_node.bl_idname=='PovrayTransformNode' and link.to_node==node:
                            povTransName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                            transform="transform {%s}"%povTransName
                    uv=""
                    if node.map_type=="uv_mapping":
                        uv="uv_mapping"
                    filepath=bpy.path.abspath(im.filepath)
                    file.write('#macro %s() %s image_pattern {\n'%(povNodeName,uv))
                    premul="off"
                    if node.premultiplied:
                        premul="on"
                    once=""
                    if node.once:
                        once="once"
                    file.write('    "%s"\n    gamma %.6g\n    premultiplied %s\n'%(filepath,node.inputs["Gamma"].default_value,premul))
                    file.write('    %s\n'%once)
                    if node.map_type!="uv_mapping":
                        file.write('    map_type %s\n'%(node.map_type))
                    file.write('    interpolate %s\n'%node.interpolate)
                    file.write('    }\n')
                    file.write('    %s\n'%transform)
                    file.write('#end\n')

    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayBumpMapNode" and node.outputs["Normal"].is_linked:
            if node.image != "":
                im=bpy.data.images[node.image]
                if im.filepath and os.path.exists(bpy.path.abspath(im.filepath)):
                    transform = ""
                    for link in ntree.links:
                        if link.from_node.bl_idname=='PovrayTransformNode' and link.to_node==node:
                            povTransName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                            transform="transform {%s}"%povTransName
                    uv=""
                    if node.map_type=="uv_mapping":
                        uv="uv_mapping"
                    filepath=bpy.path.abspath(im.filepath)
                    file.write('#declare %s = normal {%s bump_map {\n'%(povNodeName,uv))
                    once=""
                    if node.once:
                        once="once"
                    file.write('    "%s"\n'%filepath)
                    file.write('    %s\n'%once)
                    if node.map_type!="uv_mapping":
                        file.write('    map_type %s\n'%(node.map_type))
                    bump_size=node.inputs["Normal"].default_value
                    if node.inputs["Normal"].is_linked:
                        pass
                    file.write('    interpolate %s\n    bump_size %.4g\n'%(node.interpolate,bump_size))
                    file.write('    }\n')
                    file.write('    %s\n'%transform)
                    file.write('    }\n')
                    declareNodes.append(node.name)



    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayPigmentNode" and node.outputs["Pigment"].is_linked:
            declareNodes.append(node.name)
            r,g,b=node.inputs["Color"].default_value[:]
            f=node.inputs["Filter"].default_value
            t=node.inputs["Transmit"].default_value
            if node.inputs["Color"].is_linked:
                pass
            file.write('#declare %s = pigment{color srgbft <%.4g,%.4g,%.4g,%.4g,%.4g>}\n'%(povNodeName,r,g,b,f,t))


    for node in ntree.nodes:
        povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
        if node.bl_idname == "PovrayTextureNode" and node.outputs["Texture"].is_linked:
            declareNodes.append(node.name)
            r,g,b=node.inputs["Pigment"].default_value[:]
            povColName="color rgb <%.4g,%.4g,%.4g>"%(r,g,b)
            if node.inputs["Pigment"].is_linked:
                for link in ntree.links:
                    if link.to_node==node and link.to_socket.name=="Pigment":
                        povColName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
            file.write('#declare %s = texture{\n    pigment{%s}\n'%(povNodeName,povColName))
            if node.inputs["Normal"].is_linked:
                for link in ntree.links:
                    if link.to_node==node and link.to_socket.name=="Normal" and link.from_node.name in declareNodes:
                        povNorName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                        file.write('    normal{%s}\n'%povNorName)
            if node.inputs["Finish"].is_linked:
                for link in ntree.links:
                    if link.to_node==node and link.to_socket.name=="Finish":
                        povFinName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                        file.write('    finish{%s}\n'%povFinName)
            file.write('}\n')
            declareNodes.append(node.name)

    for i in range(0,len(ntree.nodes)):
        for node in ntree.nodes:
            if node.bl_idname in {"ShaderNodeGroup","ShaderTextureMapNode"}:
                for output in node.outputs:
                    if output.name=="Texture" and output.is_linked and (node.name not in declareNodes):
                        declare=True
                        for link in ntree.links:
                            if link.to_node==node and link.to_socket.name not in {"","Color ramp","Mapping","Transform","Modifier"}:
                                if link.from_node.name not in declareNodes:
                                    declare=False
                        if declare:
                            povNodeName=string_strip_hyphen(bpy.path.clean_name(node.name))+"_%s"%povMatName
                            uv=""
                            warp=""
                            for link in ntree.links:
                                if link.to_node==node and link.from_node.bl_idname=='PovrayMappingNode' and link.from_node.warp_type!="NONE":
                                    w_type = link.from_node.warp_type
                                    if w_type=="uv_mapping":
                                        uv="uv_mapping"
                                    else:
                                        tor=""
                                        if w_type=="toroidal":
                                            tor="major_radius %.4g"%link.from_node.warp_tor_major_radius
                                        orient=link.from_node.warp_orientation
                                        exp=link.from_node.warp_dist_exp
                                        warp="warp{%s orientation %s dist_exp %.4g %s}"%(w_type,orient,exp,tor)
                                        if link.from_node.warp_type=="planar":
                                            warp="warp{%s %s %.4g}"%(w_type,orient,exp)
                                        if link.from_node.warp_type=="cubic":
                                            warp="warp{%s}"%w_type
                            file.write('#declare %s = texture {%s\n'%(povNodeName,uv))
                            pattern=node.inputs[0].default_value
                            advanced=""
                            if node.inputs[0].is_linked:
                                for link in ntree.links:
                                    if link.to_node==node and link.from_node.bl_idname=='ShaderPatternNode':
                                        ########### advanced ###############################################
                                        lfn=link.from_node
                                        pattern=lfn.pattern
                                        if pattern == 'agate':
                                            advanced = 'agate_turb %.4g'%lfn.agate_turb
                                        if pattern == 'crackle':
                                            advanced="form <%.4g,%.4g,%.4g>"%(lfn.crackle_form_x,lfn.crackle_form_y,lfn.crackle_form_z)
                                            advanced+=" metric %.4g"%lfn.crackle_metric
                                            if lfn.crackle_solid:
                                                advanced+=" solid"
                                        if pattern in {'spiral1', 'spiral2'}:
                                            advanced='%.4g'%lfn.spiral_arms
                                        if pattern in {'tiling'}:
                                            advanced='%.4g'%lfn.tiling_number
                                        if pattern in {'gradient'}:
                                            advanced='%s'%lfn.gradient_orient
                                    if link.to_node==node and link.from_node.bl_idname=='PovrayImagePatternNode':
                                        povMacroName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                                        pattern = "%s()"%povMacroName
                            file.write('    %s %s %s\n'%(pattern,advanced,warp))

                            repeat=""
                            for link in ntree.links:
                                if link.to_node==node and link.from_node.bl_idname=='PovrayMultiplyNode':
                                    if link.from_node.amount_x > 1:
                                        repeat+="warp{repeat %.4g * x}"%link.from_node.amount_x
                                    if link.from_node.amount_y > 1:
                                        repeat+=" warp{repeat %.4g * y}"%link.from_node.amount_y
                                    if link.from_node.amount_z > 1:
                                        repeat+=" warp{repeat %.4g * z}"%link.from_node.amount_z

                            transform=""
                            for link in ntree.links:
                                if link.to_node==node and link.from_node.bl_idname=='PovrayTransformNode':
                                    povTransName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
                                    transform="transform {%s}"%povTransName
                            x=0
                            y=0
                            z=0
                            d=0
                            e=0
                            f=0
                            g=0
                            h=0
                            modifier=False
                            for link in ntree.links:
                                if link.to_node==node and link.from_node.bl_idname=='PovrayModifierNode':
                                    modifier=True
                                    if link.from_node.inputs["Turb X"].is_linked:
                                        pass
                                    else:
                                        x = link.from_node.inputs["Turb X"].default_value

                                    if link.from_node.inputs["Turb Y"].is_linked:
                                        pass
                                    else:
                                        y = link.from_node.inputs["Turb Y"].default_value

                                    if link.from_node.inputs["Turb Z"].is_linked:
                                        pass
                                    else:
                                        z = link.from_node.inputs["Turb Z"].default_value

                                    if link.from_node.inputs["Octaves"].is_linked:
                                        pass
                                    else:
                                        d = link.from_node.inputs["Octaves"].default_value

                                    if link.from_node.inputs["Lambda"].is_linked:
                                        pass
                                    else:
                                        e = link.from_node.inputs["Lambda"].default_value

                                    if link.from_node.inputs["Omega"].is_linked:
                                        pass
                                    else:
                                        f = link.from_node.inputs["Omega"].default_value

                                    if link.from_node.inputs["Frequency"].is_linked:
                                        pass
                                    else:
                                        g = link.from_node.inputs["Frequency"].default_value

                                    if link.from_node.inputs["Phase"].is_linked:
                                        pass
                                    else:
                                        h = link.from_node.inputs["Phase"].default_value

                            turb = "turbulence <%.4g,%.4g,%.4g>"%(x,y,z)
                            octv = "octaves %s"%d
                            lmbd = "lambda %.4g"%e
                            omg = "omega %.4g"%f
                            freq = "frequency %.4g"%g
                            pha = "phase %.4g"%h


                            file.write('\n')
                            if pattern not in {'checker', 'hexagon', 'square', 'triangular', 'brick'}:
                                file.write('    texture_map {\n')
                            if node.inputs["Color ramp"].is_linked:
                                for link in ntree.links:
                                    if link.to_node==node and link.from_node.bl_idname=="ShaderNodeValToRGB":
                                        els = link.from_node.color_ramp.elements
                                        n=-1
                                        for el in els:
                                            n+=1
                                            povInMatName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s_%s"%(n,povMatName)
                                            default=True
                                            for ilink in ntree.links:
                                                if ilink.to_node==node and ilink.to_socket.name == str(n):
                                                    default=False
                                                    povInMatName=string_strip_hyphen(bpy.path.clean_name(ilink.from_node.name))+"_%s"%povMatName
                                            if default:
                                                r,g,b,a=el.color[:]
                                                file.write('    #declare %s = texture{pigment{color srgbt <%.4g,%.4g,%.4g,%.4g>}};\n'%(povInMatName,r,g,b,1-a))
                                            file.write('    [%s %s]\n'%(el.position,povInMatName))
                            else:
                                els=[[0,0,0,0],[1,1,1,1]]
                                for i in range(0,2):
                                    povInMatName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s_%s"%(i,povMatName)
                                    default=True
                                    for ilink in ntree.links:
                                        if ilink.to_node==node and ilink.to_socket.name == str(i):
                                            default=False
                                            povInMatName=string_strip_hyphen(bpy.path.clean_name(ilink.from_node.name))+"_%s"%povMatName
                                    if default:
                                        r,g,b=els[i][1],els[i][2],els[i][3]
                                        if pattern not in {'checker', 'hexagon', 'square', 'triangular', 'brick'}:
                                            file.write('    #declare %s = texture{pigment{color rgb <%.4g,%.4g,%.4g>}};\n'%(povInMatName,r,g,b))
                                        else:
                                            file.write('    texture{pigment{color rgb <%.4g,%.4g,%.4g>}}\n'%(r,g,b))
                                    if pattern not in {'checker', 'hexagon', 'square', 'triangular', 'brick'}:
                                        file.write('    [%s %s]\n'%(els[i][0],povInMatName))
                                    else:
                                        if default==False:
                                            file.write('    texture{%s}\n'%povInMatName)
                            if pattern not in {'checker', 'hexagon', 'square', 'triangular', 'brick'}:
                                file.write('}\n')
                            if pattern == 'brick':
                                file.write("brick_size <%.4g, %.4g, %.4g> mortar %.4g \n"%(node.brick_size_x,
                                                        node.brick_size_y, node.brick_size_z, node.brick_mortar))
                            file.write('    %s %s'%(repeat,transform))
                            if modifier:
                                file.write(' %s %s %s %s %s %s'%(turb,octv,lmbd,omg,freq,pha))
                            file.write('}\n')
                            declareNodes.append(node.name)

    for link in ntree.links:
        if link.to_node.bl_idname == "PovrayOutputNode" and link.from_node.name in declareNodes:
            povMatNodeName=string_strip_hyphen(bpy.path.clean_name(link.from_node.name))+"_%s"%povMatName
            file.write('#declare %s = %s\n'%(povMatName,povMatNodeName))
