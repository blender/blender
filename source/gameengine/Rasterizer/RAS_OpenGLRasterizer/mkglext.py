#!/usr/bin/python
#
# $Id$
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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): none yet.
#
# ***** END GPL LICENSE BLOCK *****

#
# mkglext.py generates code for linking extensions.
#
# It reads the glext.h header from stdin and writes code to stdout.
#
# Usage: mkglext.py < glext.h > tmp
# Code can be copied & pasted from tmp to GL_ExtensionManager.cpp.
#
# glext.h is available here: http://oss.sgi.com/projects/ogl-sample/ABI/glext.h
#

from sys import stdin
import string, re

glext_h = string.split(stdin.read(), '\n')

# These extensions have been incorporated into the core GL or been superceded.
# Code will not be generated for these extensions
blacklist = [
	"GL_EXT_multisample", 
	"GL_INGR_blend_func_separate", 
	"GL_SGIX_fragment_lighting", 
	"GL_SGIX_polynomial_ffd", 
	"GL_SGIS_point_parameters", 
	"GL_EXT_texture_object", 
	"GL_EXT_subtexture",
	"GL_EXT_copy_texture", 
	"GL_EXT_vertex_array",
	"GL_EXT_point_parameters",
	"GL_EXT_blend_color",
	"GL_EXT_polygon_offset",
	"GL_EXT_texture"]

# Only code for these extensions will be generated.  Extensions on both the 
# blacklist & whitelist will not have code generated.
# This list is from http://oss.sgi.com/projects/ogl-sample/registry/ at 14-Mar-04
whitelist = [
	# ARB Extensions 
	"GL_ARB_multitexture",
	"GLX_ARB_get_proc_address",
	"GL_ARB_transpose_matrix",
	"WGL_ARB_buffer_region",
	"GL_ARB_multisample",
	"GL_ARB_texture_env_add",
	"GL_ARB_texture_cube_map",
	"WGL_ARB_extensions_string",
	"WGL_ARB_pixel_format",
	"WGL_ARB_make_current_read",
	"WGL_ARB_pbuffer",
	"GL_ARB_texture_compression",
	"GL_ARB_texture_border_clamp",
	"GL_ARB_point_parameters",
	"GL_ARB_vertex_blend",
	"GL_ARB_matrix_palette",
	"GL_ARB_texture_env_combine",
	"GL_ARB_texture_env_crossbar",
	"GL_ARB_texture_env_dot3",
	"WGL_ARB_render_texture",
	"GL_ARB_texture_mirrored_repeat",
	"GL_ARB_depth_texture",
	"GL_ARB_shadow",
	"GL_ARB_shadow_ambient",
	"GL_ARB_window_pos",
	"GL_ARB_vertex_program",
	"GL_ARB_fragment_program",
	"GL_ARB_vertex_buffer_object",
	"GL_ARB_occlusion_query",
	"GL_ARB_shader_objects",
	"GL_ARB_vertex_shader",
	"GL_ARB_fragment_shader",
	"GL_ARB_shading_language_100",
	"GL_ARB_texture_non_power_of_two",
	"GL_ARB_point_sprite",
	"GL_ARB_fragment_program_shadow",
	
	# Non ARB Extensions
	"GL_EXT_abgr",
	"GL_EXT_blend_color",
	"GL_EXT_polygon_offset",
	"GL_EXT_texture",
	"GL_EXT_texture3D",
	"GL_SGIS_texture_filter4",
	"GL_EXT_subtexture",
	"GL_EXT_copy_texture",
	"GL_EXT_histogram",
	"GL_EXT_convolution",
	"GL_SGI_color_matrix",
	"GL_SGI_color_table",
	"GL_SGIS_pixel_texture",
	"GL_SGIS_texture4D",
	"GL_SGI_texture_color_table",
	"GL_EXT_cmyka",
	"GL_EXT_texture_object",
	"GL_SGIS_detail_texture",
	"GL_SGIS_sharpen_texture",
	"GL_EXT_packed_pixels",
	"GL_SGIS_texture_lod",
	"GL_SGIS_multisample",
	"GL_EXT_rescale_normal",
	"GLX_EXT_visual_info",
	"GL_EXT_vertex_array",
	"GL_EXT_misc_attribute",
	"GL_SGIS_generate_mipmap",
	"GL_SGIX_clipmap",
	"GL_SGIX_shadow",
	"GL_SGIS_texture_edge_clamp",
	"GL_SGIS_texture_border_clamp",
	"GL_EXT_blend_minmax",
	"GL_EXT_blend_subtract",
	"GL_EXT_blend_logic_op",
	"GLX_SGI_swap_control",
	"GLX_SGI_video_sync",
	"GLX_SGI_make_current_read",
	"GLX_SGIX_video_source",
	"GLX_EXT_visual_rating",
	"GL_SGIX_interlace",
	"GLX_EXT_import_context",
	"GLX_SGIX_fbconfig",
	"GLX_SGIX_pbuffer",
	"GL_SGIS_texture_select",
	"GL_SGIX_sprite",
	"GL_SGIX_texture_multi_buffer",
	"GL_EXT_point_parameters",
	"GL_SGIX_instruments",
	"GL_SGIX_texture_scale_bias",
	"GL_SGIX_framezoom",
	"GL_SGIX_tag_sample_buffer",
	"GL_SGIX_reference_plane",
	"GL_SGIX_flush_raster",
	"GLX_SGI_cushion",
	"GL_SGIX_depth_texture",
	"GL_SGIS_fog_function",
	"GL_SGIX_fog_offset",
	"GL_HP_image_transform",
	"GL_HP_convolution_border_modes",
	"GL_SGIX_texture_add_env",
	"GL_EXT_color_subtable",
	"GLU_EXT_object_space_tess",
	"GL_PGI_vertex_hints",
	"GL_PGI_misc_hints",
	"GL_EXT_paletted_texture",
	"GL_EXT_clip_volume_hint",
	"GL_SGIX_list_priority",
	"GL_SGIX_ir_instrument1",
	"GLX_SGIX_video_resize",
	"GL_SGIX_texture_lod_bias",
	"GLU_SGI_filter4_parameters",
	"GLX_SGIX_dm_buffer",
	"GL_SGIX_shadow_ambient",
	"GLX_SGIX_swap_group",
	"GLX_SGIX_swap_barrier",
	"GL_EXT_index_texture",
	"GL_EXT_index_material",
	"GL_EXT_index_func",
	"GL_EXT_index_array_formats",
	"GL_EXT_compiled_vertex_array",
	"GL_EXT_cull_vertex",
	"GLU_EXT_nurbs_tessellator",
	"GL_SGIX_ycrcb",
	"GL_EXT_fragment_lighting",
	"GL_IBM_rasterpos_clip",
	"GL_HP_texture_lighting",
	"GL_EXT_draw_range_elements",
	"GL_WIN_phong_shading",
	"GL_WIN_specular_fog",
	"GLX_SGIS_color_range",
	"GL_EXT_light_texture",
	"GL_SGIX_blend_alpha_minmax",
	"GL_EXT_scene_marker",
	"GL_SGIX_pixel_texture_bits",
	"GL_EXT_bgra",
	"GL_SGIX_async",
	"GL_SGIX_async_pixel",
	"GL_SGIX_async_histogram",
	"GL_INTEL_texture_scissor",
	"GL_INTEL_parallel_arrays",
	"GL_HP_occlusion_test",
	"GL_EXT_pixel_transform",
	"GL_EXT_pixel_transform_color_table",
	"GL_EXT_shared_texture_palette",
	"GLX_SGIS_blended_overlay",
	"GL_EXT_separate_specular_color",
	"GL_EXT_secondary_color",
	"GL_EXT_texture_env",
	"GL_EXT_texture_perturb_normal",
	"GL_EXT_multi_draw_arrays",
	"GL_EXT_fog_coord",
	"GL_REND_screen_coordinates",
	"GL_EXT_coordinate_frame",
	"GL_EXT_texture_env_combine",
	"GL_APPLE_specular_vector",
	"GL_SGIX_pixel_texture",
	"GL_APPLE_transform_hint",
	"GL_SUNX_constant_data",
	"GL_SUN_global_alpha",
	"GL_SUN_triangle_list",
	"GL_SUN_vertex",
	"WGL_EXT_display_color_table",
	"WGL_EXT_extensions_string",
	"WGL_EXT_make_current_read",
	"WGL_EXT_pixel_format",
	"WGL_EXT_pbuffer",
	"WGL_EXT_swap_control",
	"GL_EXT_blend_func_separate",
	"GL_INGR_color_clamp",
	"GL_INGR_interlace_read",
	"GL_EXT_stencil_wrap",
	"WGL_EXT_depth_float",
	"GL_EXT_422_pixels",
	"GL_NV_texgen_reflection",
	"GL_SGIX_texture_range",
	"GL_SUN_convolution_border_modes",
	"GLX_SUN_get_transparent_index",
	"GL_EXT_texture_env_add",
	"GL_EXT_texture_lod_bias",
	"GL_EXT_texture_filter_anisotropic",
	"GL_EXT_vertex_weighting",
	"GL_NV_light_max_exponent",
	"GL_NV_vertex_array_range",
	"GL_NV_register_combiners",
	"GL_NV_fog_distance",
	"GL_NV_texgen_emboss",
	"GL_NV_blend_square",
	"GL_NV_texture_env_combine4",
	"GL_MESA_resize_buffers",
	"GL_MESA_window_pos",
	"GL_EXT_texture_compression_s3tc",
	"GL_IBM_cull_vertex",
	"GL_IBM_multimode_draw_arrays",
	"GL_IBM_vertex_array_lists",
	"GL_3DFX_texture_compression_FXT1",
	"GL_3DFX_multisample",
	"GL_3DFX_tbuffer",
	"WGL_EXT_multisample",
	"GL_SGIX_vertex_preclip",
	"GL_SGIX_resample",
	"GL_SGIS_texture_color_mask",
	"GLX_MESA_copy_sub_buffer",
	"GLX_MESA_pixmap_colormap",
	"GLX_MESA_release_buffers",
	"GLX_MESA_set_3dfx_mode",
	"GL_EXT_texture_env_dot3",
	"GL_ATI_texture_mirror_once",
	"GL_NV_fence",
	"GL_IBM_static_data",
	"GL_IBM_texture_mirrored_repeat",
	"GL_NV_evaluators",
	"GL_NV_packed_depth_stencil",
	"GL_NV_register_combiners2",
	"GL_NV_texture_compression_vtc",
	"GL_NV_texture_rectangle",
	"GL_NV_texture_shader",
	"GL_NV_texture_shader2",
	"GL_NV_vertex_array_range2",
	"GL_NV_vertex_program",
	"GLX_SGIX_visual_select_group",
	"GL_SGIX_texture_coordinate_clamp",
	"GLX_OML_swap_method",
	"GLX_OML_sync_control",
	"GL_OML_interlace",
	"GL_OML_subsample",
	"GL_OML_resample",
	"WGL_OML_sync_control",
	"GL_NV_copy_depth_to_color",
	"GL_ATI_envmap_bumpmap",
	"GL_ATI_fragment_shader",
	"GL_ATI_pn_triangles",
	"GL_ATI_vertex_array_object",
	"GL_EXT_vertex_shader",
	"GL_ATI_vertex_streams",
	"WGL_I3D_digital_video_control",
	"WGL_I3D_gamma",
	"WGL_I3D_genlock",
	"WGL_I3D_image_buffer",
	"WGL_I3D_swap_frame_lock",
	"WGL_I3D_swap_frame_usage",
	"GL_ATI_element_array",
	"GL_SUN_mesh_array",
	"GL_SUN_slice_accum",
	"GL_NV_multisample_filter_hint",
	"GL_NV_depth_clamp",
	"GL_NV_occlusion_query",
	"GL_NV_point_sprite",
	"WGL_NV_render_depth_texture",
	"WGL_NV_render_texture_rectangle",
	"GL_NV_texture_shader3",
	"GL_NV_vertex_program1_1",
	"GL_EXT_shadow_funcs",
	"GL_EXT_stencil_two_side",
	"GL_ATI_text_fragment_shader",
	"GL_APPLE_client_storage",
	"GL_APPLE_element_array",
	"GL_APPLE_fence",
	"GL_APPLE_vertex_array_object",
	"GL_APPLE_vertex_array_range",
	"GL_APPLE_ycbcr_422",
	"GL_S3_s3tc",
	"GL_ATI_draw_buffers",
	"WGL_ATI_pixel_format_float",
	"GL_ATI_texture_env_combine3",
	"GL_ATI_texture_float",
	"GL_NV_float_buffer",
	"GL_NV_fragment_program",
	"GL_NV_half_float",
	"GL_NV_pixel_data_range",
	"GL_NV_primitive_restart",
	"GL_NV_texture_expand_normal",
	"GL_NV_vertex_program2",
	"GL_ATI_map_object_buffer",
	"GL_ATI_separate_stencil",
	"GL_ATI_vertex_attrib_array_object",
	"GL_OES_byte_coordinates",
	"GL_OES_fixed_point",
	"GL_OES_single_precision",
	"GL_OES_compressed_paletted_texture",
	"GL_OES_read_format",
	"GL_OES_query_matrix",
	"GL_EXT_depth_bounds_test",
	"GL_EXT_texture_mirror_clamp",
	"GL_EXT_blend_equation_separate",
	"GL_MESA_pack_invert",
	"GL_MESA_ycbcr_texture"]

"""
Example code output:
#ifdef GL_EXT_compiled_vertex_array
	if (QueryExtension("GL_EXT_compiled_vertex_array"))
	{
		glUnlockArraysEXT = reinterpret_cast<PFNGLUNLOCKARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glUnlockArraysEXT"));
		glLockArraysEXT = reinterpret_cast<PFNGLLOCKARRAYSEXTPROC>(bglGetProcAddress((const GLubyte *) "glLockArraysEXT"));
		if (glUnlockArraysEXT && glLockArraysEXT)
		{
			EnableExtension(_GL_EXT_compiled_vertex_array);
			if (doDebugMessages)
				std::cout << "Detected GL_EXT_compiled_vertex_array" << std::endl;
		} else {
			std::cout << "ERROR: GL_EXT_compiled_vertex_array implementation is broken!" << std::endl;
		}
	}
#endif
"""
def writeext(ext, fnlist):
	if (find(blacklist, ext)):
		return
	if (len(fnlist) == 0):
		# This extension has no functions to detect - don't need to wrap in
		# #ifdef GL_extension names
		print "\tif (QueryExtension(\"" + ext + "\"))"
		print "\t{"
		print "\t\tEnableExtension(_" + ext + ");"
		print "\t\tif (doDebugMessages)"
		print "\t\t\tstd::cout << \"Detected " + ext + "\" << std::endl;"
		print "\t}"
		print
		return
	print "#if defined(" + ext + ")"
	print "\tif (QueryExtension(\"" + ext + "\"))"
	print "\t{"
	for fn in fnlist:
		print "\t\t" + fn[0] + " = reinterpret_cast<" + fn[1] + ">(bglGetProcAddress((const GLubyte *) \"" + fn[0] + "\"));"
	errcheck = ""
	for fn in fnlist:
		if (errcheck == ""):
			errcheck = fn[0]
		else:
			errcheck = errcheck + " && " + fn[0]
	print "\t\tif (" + errcheck + ") {"
	print "\t\t\tEnableExtension(_" + ext + ");"
	print "\t\t\tif (doDebugMessages)"
	print "\t\t\t\tstd::cout << \"Enabled " + ext + "\" << std::endl;"
	print "\t\t} else {"
	print "\t\t\tstd::cout << \"ERROR: " + ext + " implementation is broken!\" << std::endl;"
	print "\t\t}"
	print "\t}"
	print "#endif"
	print
	
"""
Example Output:
#if defined(GL_EXT_compiled_vertex_array)
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
#endif
"""
def writeproto(ext, fnlist):
	if (find(blacklist, ext) or not find(whitelist, ext)):
		return
	print "#if defined(" + ext + ")"
	for fn in fnlist:
		print fn[1] + " " + fn[0] + ";"
	print "#endif"
	print

"""
#ifdef GL_EXT_compiled_vertex_array
extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
#endif
"""
def writeheader(ext, fnlisti):
	if (find(blacklist, ext) or not find(whitelist, ext)):
		return
	print "#if defined(" + ext + ")"
	for fn in fnlist:
		print "extern " + fn[1] + " " + fn[0] + ";"
	print "#endif"
	print

def find(l, x):
	for i in l:
		if (i == x):
			return 1
	return 0


# Write Prototypes
ext = ""
fns = []
fnlist = []
ifdef = 0
for i in glext_h:
	line = re.search('^#ifn?def', i)
	if (line):
		ifdef = ifdef + 1
	
	line = re.search('^#ifndef (GL_.*)', i)
	if (line):
		if (not re.search('GL_VERSION.*', line.group(1)) and find(whitelist, line.group(1))):
			ext = line.group(1)
	
	line = re.search('^#endif', i)
	if (line):
		ifdef = ifdef - 1
		if (ifdef == 0 and ext != ""):
			writeproto(ext, fnlist)
			ext = ""
			fns = []
			fnlist = []
	if (ext != ""):
		line = re.search('.* (gl.*) \(.*\);', i)
		if (line):
			fns += [line.group(1)]
		line = re.search('.*PFN(.*)PROC.*', i)
		if (line):
			for j in fns:
				if (string.lower(line.group(1)) == string.lower(j)):
					fnlist += [(j, "PFN" + line.group(1) + "PROC")]

# Write link code
ext = ""
fns = []
fnlist = []
ifdef = 0
for i in glext_h:
	line = re.search('^#ifn?def', i)
	if (line):
		ifdef = ifdef + 1
	
	line = re.search('^#ifndef (GL_.*)', i)
	if (line):
		if (not re.search('GL_VERSION.*', line.group(1)) and find(whitelist, line.group(1))):
			ext = line.group(1)
	
	line = re.search('^#endif', i)
	if (line):
		ifdef = ifdef - 1
		if (ifdef == 0 and ext != ""):
			writeext(ext, fnlist)
			ext = ""
			fns = []
			fnlist = []
	if (ext != ""):
		line = re.search('.* (gl.*) \(.*\);', i)
		if (line):
			fns += [line.group(1)]
		line = re.search('.*PFN(.*)PROC.*', i)
		if (line):
			for j in fns:
				if (string.lower(line.group(1)) == string.lower(j)):
					fnlist += [(j, "PFN" + line.group(1) + "PROC")]

# Write header code
ext = ""
fns = []
fnlist = []
ifdef = 0
for i in glext_h:
	line = re.search('^#ifn?def', i)
	if (line):
		ifdef = ifdef + 1
	
	line = re.search('^#ifndef (GL_.*)', i)
	if (line):
		if (not re.search('GL_VERSION.*', line.group(1)) and find(whitelist, line.group(1))):
			ext = line.group(1)
	
	line = re.search('^#endif', i)
	if (line):
		ifdef = ifdef - 1
		if (ifdef == 0 and ext != ""):
			writeheader(ext, fnlist)
			ext = ""
			fns = []
			fnlist = []
	if (ext != ""):
		line = re.search('.* (gl.*) \(.*\);', i)
		if (line):
			fns += [line.group(1)]
		line = re.search('.*PFN(.*)PROC.*', i)
		if (line):
			for j in fns:
				if (string.lower(line.group(1)) == string.lower(j)):
					fnlist += [(j, "PFN" + line.group(1) + "PROC")]

# Write Python link code
ext = ""
extensions = []
fns = []
defines = []
ifdef = 0
for i in glext_h:
	line = re.search('^#ifn?def', i)
	if (line):
		ifdef = ifdef + 1
	
	line = re.search('^#ifndef (GL_.*)', i)
	if (line):
		if (not re.search('GL_VERSION.*', line.group(1)) and find(whitelist, line.group(1))):
			ext = line.group(1)
	
	line = re.search('^#endif', i)
	if (line):
		ifdef = ifdef - 1
		if (ifdef == 0 and ext != ""):
			done = 0
			for e in range(len(extensions)):
				if extensions[e][0] == ext:
					extensions[e] = (ext, defines + extensions[e][1], fns + extensions[e][2])
					done = 1
			if not done:
				extensions = extensions + [(ext, defines, fns)]
			ext = ""
			fns = []
			defines = []
	if (ext != ""):
		line = re.search('#define +(GL.*) +(0x.*)', i) # #define GL_ONE_MINUS_CONSTANT_COLOR       0x8002
		if (line):
			defines += [(line.group(1), line.group(2))]
		
		line = re.search('(.* )(gl.*)(\(.*\));', i) # GLAPI void APIENTRY glMultiTexCoord2f (GLenum, GLfloat, GLfloat);
		if (line):
			fns += [(line.group(1), line.group(2), line.group(3))] 

for ext in extensions:
	if (find(blacklist, ext[0]) or not find(whitelist, ext[0])):
		continue
	print "#if defined(" + ext[0] + ")"
	for fn in ext[2]:
		line = re.search('gl(.*)', fn[1])
		# BGL_Wrap(2, RasterPos2f,      void,     (GLfloat, GLfloat))
		rtype = ""
		for r in string.split(fn[0]):
			if r != "GLAPI" and r != "APIENTRY":
				rtype = rtype + " " + r
		params = ""
		for p in string.split(fn[2], ','):
			pline = re.search('(.*) \*', p)
			if (pline):
				p = pline.group(1) + "P"
			if params == "":
				params = p
			else:
				params = params + "," + p
		if not params[-1] == ")":
			params = params + ")"
		print "BGL_Wrap(" + str(len(string.split(fn[2], ','))) + ", " + line.group(1) + ",\t" + rtype + ",\t" + params + ")"
	print "#endif"
	print

for ext in extensions:
	if (find(blacklist, ext[0]) or not find(whitelist, ext[0])):
		continue
	print 'PyDict_SetItemString(dict, "' + ext[0] + '", PyInt_FromLong(_' + ext[0] + '))'
	print "#if defined(" + ext[0] + ")"
	print "if (bglQueryExtension(_" + ext[0] + ")) {"
	if len(ext[2]) > 0:
		for fn in ext[2]:
			line = re.search('gl(.*)', fn[1])
			# MethodDef(Vertex3iv),
			print "  BGL_AddMethod(" + line.group(1) + ");"
		print
	
	for define in ext[1]:
		print "  BGL_AddConst(" + define[0] + ");"
	print
	
	print "}"
	print "#endif"
	print
