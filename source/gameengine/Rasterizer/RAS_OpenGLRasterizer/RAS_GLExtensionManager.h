/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef __RAS_GLEXTENSIONMANAGER_H__
#define __RAS_GLEXTENSIONMANAGER_H__

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

class RAS_GLExtensionManager
{
public:
	/* http://oss.sgi.com/projects/ogl-sample/registry/ */
	typedef enum {
		/* ARB Extensions */
		_GL_ARB_multitexture ,
		_GLX_ARB_get_proc_address ,
		_GL_ARB_transpose_matrix ,
		_WGL_ARB_buffer_region ,
		_GL_ARB_multisample ,
		_GL_ARB_texture_env_add ,
		_GL_ARB_texture_cube_map ,
		_WGL_ARB_extensions_string ,
		_WGL_ARB_pixel_format ,
		_WGL_ARB_make_current_read ,
		_WGL_ARB_pbuffer ,
		_GL_ARB_texture_compression ,
		_GL_ARB_texture_border_clamp ,
		_GL_ARB_point_parameters ,
		_GL_ARB_vertex_blend ,
		_GL_ARB_matrix_palette ,
		_GL_ARB_texture_env_combine ,
		_GL_ARB_texture_env_crossbar ,
		_GL_ARB_texture_env_dot3 ,
		_WGL_ARB_render_texture ,
		_GL_ARB_texture_mirrored_repeat ,
		_GL_ARB_depth_texture ,
		_GL_ARB_shadow ,
		_GL_ARB_shadow_ambient ,
		_GL_ARB_window_pos ,
		_GL_ARB_vertex_program ,
		_GL_ARB_fragment_program ,
		_GL_ARB_vertex_buffer_object ,
		_GL_ARB_occlusion_query ,
		_GL_ARB_shader_objects ,
		_GL_ARB_vertex_shader ,
		_GL_ARB_fragment_shader ,
		_GL_ARB_shading_language_100 ,
		_GL_ARB_texture_non_power_of_two ,
		_GL_ARB_point_sprite ,
		_GL_ARB_fragment_program_shadow ,
		
		/* Non ARB Extensions */
		_GL_EXT_abgr ,
		_GL_EXT_blend_color ,
		_GL_EXT_polygon_offset ,
		_GL_EXT_texture ,
		_GL_EXT_texture3D ,
		_GL_SGIS_texture_filter4 ,
		_GL_EXT_subtexture ,
		_GL_EXT_copy_texture ,
		_GL_EXT_histogram ,
		_GL_EXT_convolution ,
		_GL_SGI_color_matrix ,
		_GL_SGI_color_table ,
		_GL_SGIS_pixel_texture ,
		_GL_SGIS_texture4D ,
		_GL_SGI_texture_color_table ,
		_GL_EXT_cmyka ,
		_GL_EXT_texture_object ,
		_GL_SGIS_detail_texture ,
		_GL_SGIS_sharpen_texture ,
		_GL_EXT_packed_pixels ,
		_GL_SGIS_texture_lod ,
		_GL_SGIS_multisample ,
		_GL_EXT_rescale_normal ,
		_GLX_EXT_visual_info ,
		_GL_EXT_vertex_array ,
		_GL_EXT_misc_attribute ,
		_GL_SGIS_generate_mipmap ,
		_GL_SGIX_clipmap ,
		_GL_SGIX_shadow ,
		_GL_SGIS_texture_edge_clamp ,
		_GL_SGIS_texture_border_clamp ,
		_GL_EXT_blend_minmax ,
		_GL_EXT_blend_subtract ,
		_GL_EXT_blend_logic_op ,
		_GLX_SGI_swap_control ,
		_GLX_SGI_video_sync ,
		_GLX_SGI_make_current_read ,
		_GLX_SGIX_video_source ,
		_GLX_EXT_visual_rating ,
		_GL_SGIX_interlace ,
		_GLX_EXT_import_context ,
		_GLX_SGIX_fbconfig ,
		_GLX_SGIX_pbuffer ,
		_GL_SGIS_texture_select ,
		_GL_SGIX_sprite ,
		_GL_SGIX_texture_multi_buffer ,
		_GL_EXT_point_parameters ,
		_GL_SGIX_instruments ,
		_GL_SGIX_texture_scale_bias ,
		_GL_SGIX_framezoom ,
		_GL_SGIX_tag_sample_buffer ,
		_GL_SGIX_reference_plane ,
		_GL_SGIX_flush_raster ,
		_GLX_SGI_cushion ,
		_GL_SGIX_depth_texture ,
		_GL_SGIS_fog_function ,
		_GL_SGIX_fog_offset ,
		_GL_HP_image_transform ,
		_GL_HP_convolution_border_modes ,
		_GL_SGIX_texture_add_env ,
		_GL_EXT_color_subtable ,
		_GLU_EXT_object_space_tess ,
		_GL_PGI_vertex_hints ,
		_GL_PGI_misc_hints ,
		_GL_EXT_paletted_texture ,
		_GL_EXT_clip_volume_hint ,
		_GL_SGIX_list_priority ,
		_GL_SGIX_ir_instrument1 ,
		_GLX_SGIX_video_resize ,
		_GL_SGIX_texture_lod_bias ,
		_GLU_SGI_filter4_parameters ,
		_GLX_SGIX_dm_buffer ,
		_GL_SGIX_shadow_ambient ,
		_GLX_SGIX_swap_group ,
		_GLX_SGIX_swap_barrier ,
		_GL_EXT_index_texture ,
		_GL_EXT_index_material ,
		_GL_EXT_index_func ,
		_GL_EXT_index_array_formats ,
		_GL_EXT_compiled_vertex_array ,
		_GL_EXT_cull_vertex ,
		_GLU_EXT_nurbs_tessellator ,
		_GL_SGIX_ycrcb ,
		_GL_EXT_fragment_lighting ,
		_GL_IBM_rasterpos_clip ,
		_GL_HP_texture_lighting ,
		_GL_EXT_draw_range_elements ,
		_GL_WIN_phong_shading ,
		_GL_WIN_specular_fog ,
		_GLX_SGIS_color_range ,
		_GL_EXT_light_texture ,
		_GL_SGIX_blend_alpha_minmax ,
		_GL_EXT_scene_marker ,
		_GL_SGIX_pixel_texture_bits ,
		_GL_EXT_bgra ,
		_GL_SGIX_async ,
		_GL_SGIX_async_pixel ,
		_GL_SGIX_async_histogram ,
		_GL_INTEL_texture_scissor ,
		_GL_INTEL_parallel_arrays ,
		_GL_HP_occlusion_test ,
		_GL_EXT_pixel_transform ,
		_GL_EXT_pixel_transform_color_table ,
		_GL_EXT_shared_texture_palette ,
		_GLX_SGIS_blended_overlay ,
		_GL_EXT_separate_specular_color ,
		_GL_EXT_secondary_color ,
		_GL_EXT_texture_env ,
		_GL_EXT_texture_perturb_normal ,
		_GL_EXT_multi_draw_arrays ,
		_GL_EXT_fog_coord ,
		_GL_REND_screen_coordinates ,
		_GL_EXT_coordinate_frame ,
		_GL_EXT_texture_env_combine ,
		_GL_APPLE_specular_vector ,
		_GL_SGIX_pixel_texture ,
		_GL_APPLE_transform_hint ,
		_GL_SUNX_constant_data ,
		_GL_SUN_global_alpha ,
		_GL_SUN_triangle_list ,
		_GL_SUN_vertex ,
		_WGL_EXT_display_color_table ,
		_WGL_EXT_extensions_string ,
		_WGL_EXT_make_current_read ,
		_WGL_EXT_pixel_format ,
		_WGL_EXT_pbuffer ,
		_WGL_EXT_swap_control ,
		_GL_EXT_blend_func_separate ,
		_GL_INGR_color_clamp ,
		_GL_INGR_interlace_read ,
		_GL_EXT_stencil_wrap ,
		_WGL_EXT_depth_float ,
		_GL_EXT_422_pixels ,
		_GL_NV_texgen_reflection ,
		_GL_SGIX_texture_range ,
		_GL_SUN_convolution_border_modes ,
		_GLX_SUN_get_transparent_index ,
		_GL_EXT_texture_env_add ,
		_GL_EXT_texture_lod_bias ,
		_GL_EXT_texture_filter_anisotropic ,
		_GL_EXT_vertex_weighting ,
		_GL_NV_light_max_exponent ,
		_GL_NV_vertex_array_range ,
		_GL_NV_register_combiners ,
		_GL_NV_fog_distance ,
		_GL_NV_texgen_emboss ,
		_GL_NV_blend_square ,
		_GL_NV_texture_env_combine4 ,
		_GL_MESA_resize_buffers ,
		_GL_MESA_window_pos ,
		_GL_EXT_texture_compression_s3tc ,
		_GL_IBM_cull_vertex ,
		_GL_IBM_multimode_draw_arrays ,
		_GL_IBM_vertex_array_lists ,
		_GL_3DFX_texture_compression_FXT1 ,
		_GL_3DFX_multisample ,
		_GL_3DFX_tbuffer ,
		_WGL_EXT_multisample ,
		_GL_SGIX_vertex_preclip ,
		_GL_SGIX_resample ,
		_GL_SGIS_texture_color_mask ,
		_GLX_MESA_copy_sub_buffer ,
		_GLX_MESA_pixmap_colormap ,
		_GLX_MESA_release_buffers ,
		_GLX_MESA_set_3dfx_mode ,
		_GL_EXT_texture_env_dot3 ,
		_GL_ATI_texture_mirror_once ,
		_GL_NV_fence ,
		_GL_IBM_static_data ,
		_GL_IBM_texture_mirrored_repeat ,
		_GL_NV_evaluators ,
		_GL_NV_packed_depth_stencil ,
		_GL_NV_register_combiners2 ,
		_GL_NV_texture_compression_vtc ,
		_GL_NV_texture_rectangle ,
		_GL_NV_texture_shader ,
		_GL_NV_texture_shader2 ,
		_GL_NV_vertex_array_range2 ,
		_GL_NV_vertex_program ,
		_GLX_SGIX_visual_select_group ,
		_GL_SGIX_texture_coordinate_clamp ,
		_GLX_OML_swap_method ,
		_GLX_OML_sync_control ,
		_GL_OML_interlace ,
		_GL_OML_subsample ,
		_GL_OML_resample ,
		_WGL_OML_sync_control ,
		_GL_NV_copy_depth_to_color ,
		_GL_ATI_envmap_bumpmap ,
		_GL_ATI_fragment_shader ,
		_GL_ATI_pn_triangles ,
		_GL_ATI_vertex_array_object ,
		_GL_EXT_vertex_shader ,
		_GL_ATI_vertex_streams ,
		_WGL_I3D_digital_video_control ,
		_WGL_I3D_gamma ,
		_WGL_I3D_genlock ,
		_WGL_I3D_image_buffer ,
		_WGL_I3D_swap_frame_lock ,
		_WGL_I3D_swap_frame_usage ,
		_GL_ATI_element_array ,
		_GL_SUN_mesh_array ,
		_GL_SUN_slice_accum ,
		_GL_NV_multisample_filter_hint ,
		_GL_NV_depth_clamp ,
		_GL_NV_occlusion_query ,
		_GL_NV_point_sprite ,
		_WGL_NV_render_depth_texture ,
		_WGL_NV_render_texture_rectangle ,
		_GL_NV_texture_shader3 ,
		_GL_NV_vertex_program1_1 ,
		_GL_EXT_shadow_funcs ,
		_GL_EXT_stencil_two_side ,
		_GL_ATI_text_fragment_shader ,
		_GL_APPLE_client_storage ,
		_GL_APPLE_element_array ,
		_GL_APPLE_fence ,
		_GL_APPLE_vertex_array_object ,
		_GL_APPLE_vertex_array_range ,
		_GL_APPLE_ycbcr_422 ,
		_GL_S3_s3tc ,
		_GL_ATI_draw_buffers ,
		_WGL_ATI_pixel_format_float ,
		_GL_ATI_texture_env_combine3 ,
		_GL_ATI_texture_float ,
		_GL_NV_float_buffer ,
		_GL_NV_fragment_program ,
		_GL_NV_half_float ,
		_GL_NV_pixel_data_range ,
		_GL_NV_primitive_restart ,
		_GL_NV_texture_expand_normal ,
		_GL_NV_vertex_program2 ,
		_GL_ATI_map_object_buffer ,
		_GL_ATI_separate_stencil ,
		_GL_ATI_vertex_attrib_array_object ,
		_GL_OES_byte_coordinates ,
		_GL_OES_fixed_point ,
		_GL_OES_single_precision ,
		_GL_OES_compressed_paletted_texture ,
		_GL_OES_read_format ,
		_GL_OES_query_matrix ,
		_GL_EXT_depth_bounds_test ,
		_GL_EXT_texture_mirror_clamp ,
		_GL_EXT_blend_equation_separate ,
		_GL_MESA_pack_invert ,
		_GL_MESA_ycbcr_texture,
		
		/* Finished */
		_BGL_TEST,
		NUM_EXTENSIONS
	} ExtensionName;
	
	bool QueryExtension(ExtensionName name);
	bool QueryVersion(int major, int minor);

	void LinkExtensions();
	
	RAS_GLExtensionManager(int debug = 0);
	// FIXME: GLX only
	//RAS_GLExtensionManager(Display *dpy, int screen);
	~RAS_GLExtensionManager();
	
private:
	std::vector<STR_String> extensions;
	/* Bit array of available extensions */
	unsigned int enabled_extensions[(NUM_EXTENSIONS + 8*sizeof(unsigned int) - 1)/(8*sizeof(unsigned int))];
	int m_debug;
	
	bool QueryExtension(STR_String extension_name);
	void EnableExtension(ExtensionName name);

};

namespace RAS_GL {

/* Begin mkglext.h */

/* GL_EXT_compiled_vertex_array */
/* Always safe to call: will default to noop */
#ifdef GL_EXT_compiled_vertex_array
extern PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
#else
static inline void glLockArraysEXT(GLint x, GLsizei y) {}
static inline void glUnlockArraysEXT(void) {}
#endif

#if defined(GL_ARB_transpose_matrix)
extern PFNGLLOADTRANSPOSEMATRIXFARBPROC glLoadTransposeMatrixfARB;
extern PFNGLLOADTRANSPOSEMATRIXDARBPROC glLoadTransposeMatrixdARB;
extern PFNGLMULTTRANSPOSEMATRIXFARBPROC glMultTransposeMatrixfARB;
extern PFNGLMULTTRANSPOSEMATRIXDARBPROC glMultTransposeMatrixdARB;
#endif

#if defined(GL_ARB_multisample)
extern PFNGLSAMPLECOVERAGEARBPROC glSampleCoverageARB;
#endif

#if defined(GL_ARB_texture_env_add)
#endif

#if defined(GL_ARB_texture_cube_map)
#endif

#if defined(GL_ARB_texture_compression)
extern PFNGLCOMPRESSEDTEXIMAGE3DARBPROC glCompressedTexImage3DARB;
extern PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB;
extern PFNGLCOMPRESSEDTEXIMAGE1DARBPROC glCompressedTexImage1DARB;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC glCompressedTexSubImage3DARB;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC glCompressedTexSubImage2DARB;
extern PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC glCompressedTexSubImage1DARB;
extern PFNGLGETCOMPRESSEDTEXIMAGEARBPROC glGetCompressedTexImageARB;
#endif

#if defined(GL_ARB_texture_border_clamp)
#endif

#if defined(GL_ARB_point_parameters)
extern PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB;
extern PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB;
#endif

#if defined(GL_ARB_vertex_blend)
extern PFNGLWEIGHTBVARBPROC glWeightbvARB;
extern PFNGLWEIGHTSVARBPROC glWeightsvARB;
extern PFNGLWEIGHTIVARBPROC glWeightivARB;
extern PFNGLWEIGHTFVARBPROC glWeightfvARB;
extern PFNGLWEIGHTDVARBPROC glWeightdvARB;
extern PFNGLWEIGHTUBVARBPROC glWeightubvARB;
extern PFNGLWEIGHTUSVARBPROC glWeightusvARB;
extern PFNGLWEIGHTUIVARBPROC glWeightuivARB;
extern PFNGLWEIGHTPOINTERARBPROC glWeightPointerARB;
extern PFNGLVERTEXBLENDARBPROC glVertexBlendARB;
#endif

#if defined(GL_ARB_matrix_palette)
extern PFNGLCURRENTPALETTEMATRIXARBPROC glCurrentPaletteMatrixARB;
extern PFNGLMATRIXINDEXUBVARBPROC glMatrixIndexubvARB;
extern PFNGLMATRIXINDEXUSVARBPROC glMatrixIndexusvARB;
extern PFNGLMATRIXINDEXUIVARBPROC glMatrixIndexuivARB;
extern PFNGLMATRIXINDEXPOINTERARBPROC glMatrixIndexPointerARB;
#endif

#if defined(GL_ARB_texture_env_combine)
#endif

#if defined(GL_ARB_texture_env_crossbar)
#endif

#if defined(GL_ARB_texture_env_dot3)
#endif

#if defined(GL_ARB_texture_mirrored_repeat)
#endif

#if defined(GL_ARB_depth_texture)
#endif

#if defined(GL_ARB_shadow)
#endif

#if defined(GL_ARB_shadow_ambient)
#endif

#if defined(GL_ARB_window_pos)
extern PFNGLWINDOWPOS2DARBPROC glWindowPos2dARB;
extern PFNGLWINDOWPOS2DVARBPROC glWindowPos2dvARB;
extern PFNGLWINDOWPOS2FARBPROC glWindowPos2fARB;
extern PFNGLWINDOWPOS2FVARBPROC glWindowPos2fvARB;
extern PFNGLWINDOWPOS2IARBPROC glWindowPos2iARB;
extern PFNGLWINDOWPOS2IVARBPROC glWindowPos2ivARB;
extern PFNGLWINDOWPOS2SARBPROC glWindowPos2sARB;
extern PFNGLWINDOWPOS2SVARBPROC glWindowPos2svARB;
extern PFNGLWINDOWPOS3DARBPROC glWindowPos3dARB;
extern PFNGLWINDOWPOS3DVARBPROC glWindowPos3dvARB;
extern PFNGLWINDOWPOS3FARBPROC glWindowPos3fARB;
extern PFNGLWINDOWPOS3FVARBPROC glWindowPos3fvARB;
extern PFNGLWINDOWPOS3IARBPROC glWindowPos3iARB;
extern PFNGLWINDOWPOS3IVARBPROC glWindowPos3ivARB;
extern PFNGLWINDOWPOS3SARBPROC glWindowPos3sARB;
extern PFNGLWINDOWPOS3SVARBPROC glWindowPos3svARB;
#endif

#if defined(GL_ARB_vertex_program)
extern PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB;
extern PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB;
extern PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB;
extern PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB;
extern PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB;
extern PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB;
extern PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB;
extern PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB;
extern PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB;
extern PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB;
extern PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB;
extern PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB;
extern PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB;
extern PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB;
extern PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB;
extern PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB;
extern PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB;
extern PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB;
extern PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4NbvARB;
extern PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4NivARB;
extern PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4NsvARB;
extern PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4NubARB;
extern PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4NubvARB;
extern PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4NuivARB;
extern PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4NusvARB;
extern PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB;
extern PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB;
extern PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB;
extern PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB;
extern PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB;
extern PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB;
extern PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB;
extern PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB;
extern PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB;
extern PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB;
extern PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB;
extern PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;
extern PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
extern PFNGLBINDPROGRAMARBPROC glBindProgramARB;
extern PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
extern PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
extern PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB;
extern PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB;
extern PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB;
extern PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB;
extern PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
extern PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;
extern PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB;
extern PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB;
extern PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB;
extern PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;
extern PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB;
extern PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB;
extern PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB;
extern PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB;
extern PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB;
extern PFNGLISPROGRAMARBPROC glIsProgramARB;
#endif

#if defined(GL_ARB_fragment_program)
#endif

#if defined(GL_ARB_vertex_buffer_object)
extern PFNGLBINDBUFFERARBPROC glBindBufferARB;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
extern PFNGLGENBUFFERSARBPROC glGenBuffersARB;
extern PFNGLISBUFFERARBPROC glIsBufferARB;
extern PFNGLBUFFERDATAARBPROC glBufferDataARB;
extern PFNGLBUFFERSUBDATAARBPROC glBufferSubDataARB;
extern PFNGLGETBUFFERSUBDATAARBPROC glGetBufferSubDataARB;
extern PFNGLMAPBUFFERARBPROC glMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB;
extern PFNGLGETBUFFERPARAMETERIVARBPROC glGetBufferParameterivARB;
extern PFNGLGETBUFFERPOINTERVARBPROC glGetBufferPointervARB;
#endif

#if defined(GL_ARB_occlusion_query)
extern PFNGLGENQUERIESARBPROC glGenQueriesARB;
extern PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB;
extern PFNGLISQUERYARBPROC glIsQueryARB;
extern PFNGLBEGINQUERYARBPROC glBeginQueryARB;
extern PFNGLENDQUERYARBPROC glEndQueryARB;
extern PFNGLGETQUERYIVARBPROC glGetQueryivARB;
extern PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB;
extern PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB;
#endif

#if defined(GL_ARB_shader_objects)
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLGETHANDLEARBPROC glGetHandleARB;
extern PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB;
extern PFNGLUNIFORM1FARBPROC glUniform1fARB;
extern PFNGLUNIFORM2FARBPROC glUniform2fARB;
extern PFNGLUNIFORM3FARBPROC glUniform3fARB;
extern PFNGLUNIFORM4FARBPROC glUniform4fARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLUNIFORM2IARBPROC glUniform2iARB;
extern PFNGLUNIFORM3IARBPROC glUniform3iARB;
extern PFNGLUNIFORM4IARBPROC glUniform4iARB;
extern PFNGLUNIFORM1FVARBPROC glUniform1fvARB;
extern PFNGLUNIFORM2FVARBPROC glUniform2fvARB;
extern PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
extern PFNGLUNIFORM4FVARBPROC glUniform4fvARB;
extern PFNGLUNIFORM1IVARBPROC glUniform1ivARB;
extern PFNGLUNIFORM2IVARBPROC glUniform2ivARB;
extern PFNGLUNIFORM3IVARBPROC glUniform3ivARB;
extern PFNGLUNIFORM4IVARBPROC glUniform4ivARB;
extern PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB;
extern PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB;
extern PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB;
extern PFNGLGETOBJECTPARAMETERFVARBPROC glGetObjectParameterfvARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB;
extern PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB;
extern PFNGLGETUNIFORMIVARBPROC glGetUniformivARB;
extern PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB;
#endif

#if defined(GL_ARB_vertex_shader)
extern PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
extern PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
extern PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;
#endif

#if defined(GL_ARB_fragment_shader)
#endif

#if defined(GL_ARB_shading_language_100)
#endif

#if defined(GL_ARB_texture_non_power_of_two)
#endif

#if defined(GL_ARB_point_sprite)
#endif

#if defined(GL_ARB_fragment_program_shadow)
#endif

#if defined(GL_EXT_abgr)
#endif

#if defined(GL_EXT_texture3D)
extern PFNGLTEXIMAGE3DEXTPROC glTexImage3DEXT;
extern PFNGLTEXSUBIMAGE3DEXTPROC glTexSubImage3DEXT;
#endif

#if defined(GL_SGIS_texture_filter4)
extern PFNGLGETTEXFILTERFUNCSGISPROC glGetTexFilterFuncSGIS;
extern PFNGLTEXFILTERFUNCSGISPROC glTexFilterFuncSGIS;
#endif

#if defined(GL_EXT_histogram)
extern PFNGLGETHISTOGRAMEXTPROC glGetHistogramEXT;
extern PFNGLGETHISTOGRAMPARAMETERFVEXTPROC glGetHistogramParameterfvEXT;
extern PFNGLGETHISTOGRAMPARAMETERIVEXTPROC glGetHistogramParameterivEXT;
extern PFNGLGETMINMAXEXTPROC glGetMinmaxEXT;
extern PFNGLGETMINMAXPARAMETERFVEXTPROC glGetMinmaxParameterfvEXT;
extern PFNGLGETMINMAXPARAMETERIVEXTPROC glGetMinmaxParameterivEXT;
extern PFNGLHISTOGRAMEXTPROC glHistogramEXT;
extern PFNGLMINMAXEXTPROC glMinmaxEXT;
extern PFNGLRESETHISTOGRAMEXTPROC glResetHistogramEXT;
extern PFNGLRESETMINMAXEXTPROC glResetMinmaxEXT;
#endif

#if defined(GL_EXT_convolution)
extern PFNGLCONVOLUTIONFILTER1DEXTPROC glConvolutionFilter1DEXT;
extern PFNGLCONVOLUTIONFILTER2DEXTPROC glConvolutionFilter2DEXT;
extern PFNGLCONVOLUTIONPARAMETERFEXTPROC glConvolutionParameterfEXT;
extern PFNGLCONVOLUTIONPARAMETERFVEXTPROC glConvolutionParameterfvEXT;
extern PFNGLCONVOLUTIONPARAMETERIEXTPROC glConvolutionParameteriEXT;
extern PFNGLCONVOLUTIONPARAMETERIVEXTPROC glConvolutionParameterivEXT;
extern PFNGLCOPYCONVOLUTIONFILTER1DEXTPROC glCopyConvolutionFilter1DEXT;
extern PFNGLCOPYCONVOLUTIONFILTER2DEXTPROC glCopyConvolutionFilter2DEXT;
extern PFNGLGETCONVOLUTIONFILTEREXTPROC glGetConvolutionFilterEXT;
extern PFNGLGETCONVOLUTIONPARAMETERFVEXTPROC glGetConvolutionParameterfvEXT;
extern PFNGLGETCONVOLUTIONPARAMETERIVEXTPROC glGetConvolutionParameterivEXT;
extern PFNGLGETSEPARABLEFILTEREXTPROC glGetSeparableFilterEXT;
extern PFNGLSEPARABLEFILTER2DEXTPROC glSeparableFilter2DEXT;
#endif

#if defined(GL_SGI_color_table)
extern PFNGLCOLORTABLESGIPROC glColorTableSGI;
extern PFNGLCOLORTABLEPARAMETERFVSGIPROC glColorTableParameterfvSGI;
extern PFNGLCOLORTABLEPARAMETERIVSGIPROC glColorTableParameterivSGI;
extern PFNGLCOPYCOLORTABLESGIPROC glCopyColorTableSGI;
extern PFNGLGETCOLORTABLESGIPROC glGetColorTableSGI;
extern PFNGLGETCOLORTABLEPARAMETERFVSGIPROC glGetColorTableParameterfvSGI;
extern PFNGLGETCOLORTABLEPARAMETERIVSGIPROC glGetColorTableParameterivSGI;
#endif

#if defined(GL_SGIX_pixel_texture)
extern PFNGLPIXELTEXGENSGIXPROC glPixelTexGenSGIX;
#endif

#if defined(GL_SGIS_pixel_texture)
extern PFNGLPIXELTEXGENPARAMETERISGISPROC glPixelTexGenParameteriSGIS;
extern PFNGLPIXELTEXGENPARAMETERIVSGISPROC glPixelTexGenParameterivSGIS;
extern PFNGLPIXELTEXGENPARAMETERFSGISPROC glPixelTexGenParameterfSGIS;
extern PFNGLPIXELTEXGENPARAMETERFVSGISPROC glPixelTexGenParameterfvSGIS;
extern PFNGLGETPIXELTEXGENPARAMETERIVSGISPROC glGetPixelTexGenParameterivSGIS;
extern PFNGLGETPIXELTEXGENPARAMETERFVSGISPROC glGetPixelTexGenParameterfvSGIS;
#endif

#if defined(GL_SGIS_texture4D)
extern PFNGLTEXIMAGE4DSGISPROC glTexImage4DSGIS;
extern PFNGLTEXSUBIMAGE4DSGISPROC glTexSubImage4DSGIS;
#endif

#if defined(GL_SGI_texture_color_table)
#endif

#if defined(GL_EXT_cmyka)
#endif

#if defined(GL_SGIS_detail_texture)
extern PFNGLDETAILTEXFUNCSGISPROC glDetailTexFuncSGIS;
extern PFNGLGETDETAILTEXFUNCSGISPROC glGetDetailTexFuncSGIS;
#endif

#if defined(GL_SGIS_sharpen_texture)
extern PFNGLSHARPENTEXFUNCSGISPROC glSharpenTexFuncSGIS;
extern PFNGLGETSHARPENTEXFUNCSGISPROC glGetSharpenTexFuncSGIS;
#endif

#if defined(GL_EXT_packed_pixels)
#endif

#if defined(GL_SGIS_texture_lod)
#endif

#if defined(GL_SGIS_multisample)
extern PFNGLSAMPLEMASKSGISPROC glSampleMaskSGIS;
extern PFNGLSAMPLEPATTERNSGISPROC glSamplePatternSGIS;
#endif

#if defined(GL_EXT_rescale_normal)
#endif

#if defined(GL_EXT_misc_attribute)
#endif

#if defined(GL_SGIS_generate_mipmap)
#endif

#if defined(GL_SGIX_clipmap)
#endif

#if defined(GL_SGIX_shadow)
#endif

#if defined(GL_SGIS_texture_edge_clamp)
#endif

#if defined(GL_SGIS_texture_border_clamp)
#endif

#if defined(GL_EXT_blend_minmax)
extern PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
#endif

#if defined(GL_EXT_blend_subtract)
#endif

#if defined(GL_EXT_blend_logic_op)
#endif

#if defined(GL_SGIX_interlace)
#endif

#if defined(GL_SGIX_sprite)
extern PFNGLSPRITEPARAMETERFSGIXPROC glSpriteParameterfSGIX;
extern PFNGLSPRITEPARAMETERFVSGIXPROC glSpriteParameterfvSGIX;
extern PFNGLSPRITEPARAMETERISGIXPROC glSpriteParameteriSGIX;
extern PFNGLSPRITEPARAMETERIVSGIXPROC glSpriteParameterivSGIX;
#endif

#if defined(GL_SGIX_texture_multi_buffer)
#endif

#if defined(GL_SGIX_instruments)
extern PFNGLGETINSTRUMENTSSGIXPROC glGetInstrumentsSGIX;
extern PFNGLINSTRUMENTSBUFFERSGIXPROC glInstrumentsBufferSGIX;
extern PFNGLPOLLINSTRUMENTSSGIXPROC glPollInstrumentsSGIX;
extern PFNGLREADINSTRUMENTSSGIXPROC glReadInstrumentsSGIX;
extern PFNGLSTARTINSTRUMENTSSGIXPROC glStartInstrumentsSGIX;
extern PFNGLSTOPINSTRUMENTSSGIXPROC glStopInstrumentsSGIX;
#endif

#if defined(GL_SGIX_texture_scale_bias)
#endif

#if defined(GL_SGIX_framezoom)
extern PFNGLFRAMEZOOMSGIXPROC glFrameZoomSGIX;
#endif

#if defined(GL_SGIX_tag_sample_buffer)
extern PFNGLTAGSAMPLEBUFFERSGIXPROC glTagSampleBufferSGIX;
#endif

#if defined(GL_SGIX_reference_plane)
extern PFNGLREFERENCEPLANESGIXPROC glReferencePlaneSGIX;
#endif

#if defined(GL_SGIX_flush_raster)
extern PFNGLFLUSHRASTERSGIXPROC glFlushRasterSGIX;
#endif

#if defined(GL_SGIX_depth_texture)
#endif

#if defined(GL_SGIS_fog_function)
extern PFNGLFOGFUNCSGISPROC glFogFuncSGIS;
extern PFNGLGETFOGFUNCSGISPROC glGetFogFuncSGIS;
#endif

#if defined(GL_SGIX_fog_offset)
#endif

#if defined(GL_HP_image_transform)
extern PFNGLIMAGETRANSFORMPARAMETERIHPPROC glImageTransformParameteriHP;
extern PFNGLIMAGETRANSFORMPARAMETERFHPPROC glImageTransformParameterfHP;
extern PFNGLIMAGETRANSFORMPARAMETERIVHPPROC glImageTransformParameterivHP;
extern PFNGLIMAGETRANSFORMPARAMETERFVHPPROC glImageTransformParameterfvHP;
extern PFNGLGETIMAGETRANSFORMPARAMETERIVHPPROC glGetImageTransformParameterivHP;
extern PFNGLGETIMAGETRANSFORMPARAMETERFVHPPROC glGetImageTransformParameterfvHP;
#endif

#if defined(GL_HP_convolution_border_modes)
#endif

#if defined(GL_SGIX_texture_add_env)
#endif

#if defined(GL_EXT_color_subtable)
extern PFNGLCOLORSUBTABLEEXTPROC glColorSubTableEXT;
extern PFNGLCOPYCOLORSUBTABLEEXTPROC glCopyColorSubTableEXT;
#endif

#if defined(GL_PGI_vertex_hints)
#endif

#if defined(GL_PGI_misc_hints)
extern PFNGLHINTPGIPROC glHintPGI;
#endif

#if defined(GL_EXT_paletted_texture)
extern PFNGLCOLORTABLEEXTPROC glColorTableEXT;
extern PFNGLGETCOLORTABLEEXTPROC glGetColorTableEXT;
extern PFNGLGETCOLORTABLEPARAMETERIVEXTPROC glGetColorTableParameterivEXT;
extern PFNGLGETCOLORTABLEPARAMETERFVEXTPROC glGetColorTableParameterfvEXT;
#endif

#if defined(GL_EXT_clip_volume_hint)
#endif

#if defined(GL_SGIX_list_priority)
extern PFNGLGETLISTPARAMETERFVSGIXPROC glGetListParameterfvSGIX;
extern PFNGLGETLISTPARAMETERIVSGIXPROC glGetListParameterivSGIX;
extern PFNGLLISTPARAMETERFSGIXPROC glListParameterfSGIX;
extern PFNGLLISTPARAMETERFVSGIXPROC glListParameterfvSGIX;
extern PFNGLLISTPARAMETERISGIXPROC glListParameteriSGIX;
extern PFNGLLISTPARAMETERIVSGIXPROC glListParameterivSGIX;
#endif

#if defined(GL_SGIX_ir_instrument1)
#endif

#if defined(GL_SGIX_texture_lod_bias)
#endif

#if defined(GL_SGIX_shadow_ambient)
#endif

#if defined(GL_EXT_index_texture)
#endif

#if defined(GL_EXT_index_material)
extern PFNGLINDEXMATERIALEXTPROC glIndexMaterialEXT;
#endif

#if defined(GL_EXT_index_func)
extern PFNGLINDEXFUNCEXTPROC glIndexFuncEXT;
#endif

#if defined(GL_EXT_index_array_formats)
#endif

#if defined(GL_EXT_cull_vertex)
extern PFNGLCULLPARAMETERDVEXTPROC glCullParameterdvEXT;
extern PFNGLCULLPARAMETERFVEXTPROC glCullParameterfvEXT;
#endif

#if defined(GL_SGIX_ycrcb)
#endif

#if defined(GL_IBM_rasterpos_clip)
#endif

#if defined(GL_HP_texture_lighting)
#endif

#if defined(GL_EXT_draw_range_elements)
extern PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElementsEXT;
#endif

#if defined(GL_WIN_phong_shading)
#endif

#if defined(GL_WIN_specular_fog)
#endif

#if defined(GL_EXT_light_texture)
extern PFNGLAPPLYTEXTUREEXTPROC glApplyTextureEXT;
extern PFNGLTEXTURELIGHTEXTPROC glTextureLightEXT;
extern PFNGLTEXTUREMATERIALEXTPROC glTextureMaterialEXT;
#endif

#if defined(GL_SGIX_blend_alpha_minmax)
#endif

#if defined(GL_EXT_bgra)
#endif

#if defined(GL_SGIX_async)
extern PFNGLASYNCMARKERSGIXPROC glAsyncMarkerSGIX;
extern PFNGLFINISHASYNCSGIXPROC glFinishAsyncSGIX;
extern PFNGLPOLLASYNCSGIXPROC glPollAsyncSGIX;
extern PFNGLGENASYNCMARKERSSGIXPROC glGenAsyncMarkersSGIX;
extern PFNGLDELETEASYNCMARKERSSGIXPROC glDeleteAsyncMarkersSGIX;
extern PFNGLISASYNCMARKERSGIXPROC glIsAsyncMarkerSGIX;
#endif

#if defined(GL_SGIX_async_pixel)
#endif

#if defined(GL_SGIX_async_histogram)
#endif

#if defined(GL_INTEL_parallel_arrays)
extern PFNGLVERTEXPOINTERVINTELPROC glVertexPointervINTEL;
extern PFNGLNORMALPOINTERVINTELPROC glNormalPointervINTEL;
extern PFNGLCOLORPOINTERVINTELPROC glColorPointervINTEL;
extern PFNGLTEXCOORDPOINTERVINTELPROC glTexCoordPointervINTEL;
#endif

#if defined(GL_HP_occlusion_test)
#endif

#if defined(GL_EXT_pixel_transform)
extern PFNGLPIXELTRANSFORMPARAMETERIEXTPROC glPixelTransformParameteriEXT;
extern PFNGLPIXELTRANSFORMPARAMETERFEXTPROC glPixelTransformParameterfEXT;
extern PFNGLPIXELTRANSFORMPARAMETERIVEXTPROC glPixelTransformParameterivEXT;
extern PFNGLPIXELTRANSFORMPARAMETERFVEXTPROC glPixelTransformParameterfvEXT;
#endif

#if defined(GL_EXT_pixel_transform_color_table)
#endif

#if defined(GL_EXT_shared_texture_palette)
#endif

#if defined(GL_EXT_separate_specular_color)
#endif

#if defined(GL_EXT_secondary_color)
extern PFNGLSECONDARYCOLOR3BEXTPROC glSecondaryColor3bEXT;
extern PFNGLSECONDARYCOLOR3BVEXTPROC glSecondaryColor3bvEXT;
extern PFNGLSECONDARYCOLOR3DEXTPROC glSecondaryColor3dEXT;
extern PFNGLSECONDARYCOLOR3DVEXTPROC glSecondaryColor3dvEXT;
extern PFNGLSECONDARYCOLOR3FEXTPROC glSecondaryColor3fEXT;
extern PFNGLSECONDARYCOLOR3FVEXTPROC glSecondaryColor3fvEXT;
extern PFNGLSECONDARYCOLOR3IEXTPROC glSecondaryColor3iEXT;
extern PFNGLSECONDARYCOLOR3IVEXTPROC glSecondaryColor3ivEXT;
extern PFNGLSECONDARYCOLOR3SEXTPROC glSecondaryColor3sEXT;
extern PFNGLSECONDARYCOLOR3SVEXTPROC glSecondaryColor3svEXT;
extern PFNGLSECONDARYCOLOR3UBEXTPROC glSecondaryColor3ubEXT;
extern PFNGLSECONDARYCOLOR3UBVEXTPROC glSecondaryColor3ubvEXT;
extern PFNGLSECONDARYCOLOR3UIEXTPROC glSecondaryColor3uiEXT;
extern PFNGLSECONDARYCOLOR3UIVEXTPROC glSecondaryColor3uivEXT;
extern PFNGLSECONDARYCOLOR3USEXTPROC glSecondaryColor3usEXT;
extern PFNGLSECONDARYCOLOR3USVEXTPROC glSecondaryColor3usvEXT;
extern PFNGLSECONDARYCOLORPOINTEREXTPROC glSecondaryColorPointerEXT;
#endif

#if defined(GL_EXT_texture_perturb_normal)
extern PFNGLTEXTURENORMALEXTPROC glTextureNormalEXT;
#endif

#if defined(GL_EXT_multi_draw_arrays)
extern PFNGLMULTIDRAWARRAYSEXTPROC glMultiDrawArraysEXT;
extern PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElementsEXT;
#endif

#if defined(GL_EXT_fog_coord)
extern PFNGLFOGCOORDFEXTPROC glFogCoordfEXT;
extern PFNGLFOGCOORDFVEXTPROC glFogCoordfvEXT;
extern PFNGLFOGCOORDDEXTPROC glFogCoorddEXT;
extern PFNGLFOGCOORDDVEXTPROC glFogCoorddvEXT;
extern PFNGLFOGCOORDPOINTEREXTPROC glFogCoordPointerEXT;
#endif

#if defined(GL_REND_screen_coordinates)
#endif

#if defined(GL_EXT_coordinate_frame)
extern PFNGLTANGENT3BEXTPROC glTangent3bEXT;
extern PFNGLTANGENT3BVEXTPROC glTangent3bvEXT;
extern PFNGLTANGENT3DEXTPROC glTangent3dEXT;
extern PFNGLTANGENT3DVEXTPROC glTangent3dvEXT;
extern PFNGLTANGENT3FEXTPROC glTangent3fEXT;
extern PFNGLTANGENT3FVEXTPROC glTangent3fvEXT;
extern PFNGLTANGENT3IEXTPROC glTangent3iEXT;
extern PFNGLTANGENT3IVEXTPROC glTangent3ivEXT;
extern PFNGLTANGENT3SEXTPROC glTangent3sEXT;
extern PFNGLTANGENT3SVEXTPROC glTangent3svEXT;
extern PFNGLBINORMAL3BEXTPROC glBinormal3bEXT;
extern PFNGLBINORMAL3BVEXTPROC glBinormal3bvEXT;
extern PFNGLBINORMAL3DEXTPROC glBinormal3dEXT;
extern PFNGLBINORMAL3DVEXTPROC glBinormal3dvEXT;
extern PFNGLBINORMAL3FEXTPROC glBinormal3fEXT;
extern PFNGLBINORMAL3FVEXTPROC glBinormal3fvEXT;
extern PFNGLBINORMAL3IEXTPROC glBinormal3iEXT;
extern PFNGLBINORMAL3IVEXTPROC glBinormal3ivEXT;
extern PFNGLBINORMAL3SEXTPROC glBinormal3sEXT;
extern PFNGLBINORMAL3SVEXTPROC glBinormal3svEXT;
extern PFNGLTANGENTPOINTEREXTPROC glTangentPointerEXT;
extern PFNGLBINORMALPOINTEREXTPROC glBinormalPointerEXT;
#endif

#if defined(GL_EXT_texture_env_combine)
#endif

#if defined(GL_APPLE_specular_vector)
#endif

#if defined(GL_APPLE_transform_hint)
#endif

#if defined(GL_SUNX_constant_data)
extern PFNGLFINISHTEXTURESUNXPROC glFinishTextureSUNX;
#endif

#if defined(GL_SUN_global_alpha)
extern PFNGLGLOBALALPHAFACTORBSUNPROC glGlobalAlphaFactorbSUN;
extern PFNGLGLOBALALPHAFACTORSSUNPROC glGlobalAlphaFactorsSUN;
extern PFNGLGLOBALALPHAFACTORISUNPROC glGlobalAlphaFactoriSUN;
extern PFNGLGLOBALALPHAFACTORFSUNPROC glGlobalAlphaFactorfSUN;
extern PFNGLGLOBALALPHAFACTORDSUNPROC glGlobalAlphaFactordSUN;
extern PFNGLGLOBALALPHAFACTORUBSUNPROC glGlobalAlphaFactorubSUN;
extern PFNGLGLOBALALPHAFACTORUSSUNPROC glGlobalAlphaFactorusSUN;
extern PFNGLGLOBALALPHAFACTORUISUNPROC glGlobalAlphaFactoruiSUN;
#endif

#if defined(GL_SUN_triangle_list)
extern PFNGLREPLACEMENTCODEUISUNPROC glReplacementCodeuiSUN;
extern PFNGLREPLACEMENTCODEUSSUNPROC glReplacementCodeusSUN;
extern PFNGLREPLACEMENTCODEUBSUNPROC glReplacementCodeubSUN;
extern PFNGLREPLACEMENTCODEUIVSUNPROC glReplacementCodeuivSUN;
extern PFNGLREPLACEMENTCODEUSVSUNPROC glReplacementCodeusvSUN;
extern PFNGLREPLACEMENTCODEUBVSUNPROC glReplacementCodeubvSUN;
extern PFNGLREPLACEMENTCODEPOINTERSUNPROC glReplacementCodePointerSUN;
#endif

#if defined(GL_SUN_vertex)
extern PFNGLCOLOR4UBVERTEX2FSUNPROC glColor4ubVertex2fSUN;
extern PFNGLCOLOR4UBVERTEX2FVSUNPROC glColor4ubVertex2fvSUN;
extern PFNGLCOLOR4UBVERTEX3FSUNPROC glColor4ubVertex3fSUN;
extern PFNGLCOLOR4UBVERTEX3FVSUNPROC glColor4ubVertex3fvSUN;
extern PFNGLCOLOR3FVERTEX3FSUNPROC glColor3fVertex3fSUN;
extern PFNGLCOLOR3FVERTEX3FVSUNPROC glColor3fVertex3fvSUN;
extern PFNGLNORMAL3FVERTEX3FSUNPROC glNormal3fVertex3fSUN;
extern PFNGLNORMAL3FVERTEX3FVSUNPROC glNormal3fVertex3fvSUN;
extern PFNGLCOLOR4FNORMAL3FVERTEX3FSUNPROC glColor4fNormal3fVertex3fSUN;
extern PFNGLCOLOR4FNORMAL3FVERTEX3FVSUNPROC glColor4fNormal3fVertex3fvSUN;
extern PFNGLTEXCOORD2FVERTEX3FSUNPROC glTexCoord2fVertex3fSUN;
extern PFNGLTEXCOORD2FVERTEX3FVSUNPROC glTexCoord2fVertex3fvSUN;
extern PFNGLTEXCOORD4FVERTEX4FSUNPROC glTexCoord4fVertex4fSUN;
extern PFNGLTEXCOORD4FVERTEX4FVSUNPROC glTexCoord4fVertex4fvSUN;
extern PFNGLTEXCOORD2FCOLOR4UBVERTEX3FSUNPROC glTexCoord2fColor4ubVertex3fSUN;
extern PFNGLTEXCOORD2FCOLOR4UBVERTEX3FVSUNPROC glTexCoord2fColor4ubVertex3fvSUN;
extern PFNGLTEXCOORD2FCOLOR3FVERTEX3FSUNPROC glTexCoord2fColor3fVertex3fSUN;
extern PFNGLTEXCOORD2FCOLOR3FVERTEX3FVSUNPROC glTexCoord2fColor3fVertex3fvSUN;
extern PFNGLTEXCOORD2FNORMAL3FVERTEX3FSUNPROC glTexCoord2fNormal3fVertex3fSUN;
extern PFNGLTEXCOORD2FNORMAL3FVERTEX3FVSUNPROC glTexCoord2fNormal3fVertex3fvSUN;
extern PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC glTexCoord2fColor4fNormal3fVertex3fSUN;
extern PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC glTexCoord2fColor4fNormal3fVertex3fvSUN;
extern PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FSUNPROC glTexCoord4fColor4fNormal3fVertex4fSUN;
extern PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FVSUNPROC glTexCoord4fColor4fNormal3fVertex4fvSUN;
extern PFNGLREPLACEMENTCODEUIVERTEX3FSUNPROC glReplacementCodeuiVertex3fSUN;
extern PFNGLREPLACEMENTCODEUIVERTEX3FVSUNPROC glReplacementCodeuiVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FSUNPROC glReplacementCodeuiColor4ubVertex3fSUN;
extern PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FVSUNPROC glReplacementCodeuiColor4ubVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FSUNPROC glReplacementCodeuiColor3fVertex3fSUN;
extern PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FVSUNPROC glReplacementCodeuiColor3fVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FSUNPROC glReplacementCodeuiNormal3fVertex3fSUN;
extern PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiNormal3fVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FSUNPROC glReplacementCodeuiColor4fNormal3fVertex3fSUN;
extern PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiColor4fNormal3fVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FSUNPROC glReplacementCodeuiTexCoord2fVertex3fSUN;
extern PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FVSUNPROC glReplacementCodeuiTexCoord2fVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FSUNPROC glReplacementCodeuiTexCoord2fNormal3fVertex3fSUN;
extern PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiTexCoord2fNormal3fVertex3fvSUN;
extern PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fSUN;
extern PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC glReplacementCodeuiTexCoord2fColor4fNormal3fVertex3fvSUN;
#endif

#if defined(GL_EXT_blend_func_separate)
extern PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT;
#endif

#if defined(GL_INGR_color_clamp)
#endif

#if defined(GL_INGR_interlace_read)
#endif

#if defined(GL_EXT_stencil_wrap)
#endif

#if defined(GL_EXT_422_pixels)
#endif

#if defined(GL_NV_texgen_reflection)
#endif

#if defined(GL_SUN_convolution_border_modes)
#endif

#if defined(GL_EXT_texture_env_add)
#endif

#if defined(GL_EXT_texture_lod_bias)
#endif

#if defined(GL_EXT_texture_filter_anisotropic)
#endif

#if defined(GL_EXT_vertex_weighting)
extern PFNGLVERTEXWEIGHTFEXTPROC glVertexWeightfEXT;
extern PFNGLVERTEXWEIGHTFVEXTPROC glVertexWeightfvEXT;
extern PFNGLVERTEXWEIGHTPOINTEREXTPROC glVertexWeightPointerEXT;
#endif

#if defined(GL_NV_light_max_exponent)
#endif

#if defined(GL_NV_vertex_array_range)
extern PFNGLFLUSHVERTEXARRAYRANGENVPROC glFlushVertexArrayRangeNV;
extern PFNGLVERTEXARRAYRANGENVPROC glVertexArrayRangeNV;
#endif

#if defined(GL_NV_register_combiners)
extern PFNGLCOMBINERPARAMETERFVNVPROC glCombinerParameterfvNV;
extern PFNGLCOMBINERPARAMETERFNVPROC glCombinerParameterfNV;
extern PFNGLCOMBINERPARAMETERIVNVPROC glCombinerParameterivNV;
extern PFNGLCOMBINERPARAMETERINVPROC glCombinerParameteriNV;
extern PFNGLCOMBINERINPUTNVPROC glCombinerInputNV;
extern PFNGLCOMBINEROUTPUTNVPROC glCombinerOutputNV;
extern PFNGLFINALCOMBINERINPUTNVPROC glFinalCombinerInputNV;
extern PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC glGetCombinerInputParameterfvNV;
extern PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC glGetCombinerInputParameterivNV;
extern PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC glGetCombinerOutputParameterfvNV;
extern PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC glGetCombinerOutputParameterivNV;
extern PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC glGetFinalCombinerInputParameterfvNV;
extern PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC glGetFinalCombinerInputParameterivNV;
#endif

#if defined(GL_NV_fog_distance)
#endif

#if defined(GL_NV_texgen_emboss)
#endif

#if defined(GL_NV_blend_square)
#endif

#if defined(GL_NV_texture_env_combine4)
#endif

#if defined(GL_MESA_resize_buffers)
extern PFNGLRESIZEBUFFERSMESAPROC glResizeBuffersMESA;
#endif

#if defined(GL_MESA_window_pos)
extern PFNGLWINDOWPOS2DMESAPROC glWindowPos2dMESA;
extern PFNGLWINDOWPOS2DVMESAPROC glWindowPos2dvMESA;
extern PFNGLWINDOWPOS2FMESAPROC glWindowPos2fMESA;
extern PFNGLWINDOWPOS2FVMESAPROC glWindowPos2fvMESA;
extern PFNGLWINDOWPOS2IMESAPROC glWindowPos2iMESA;
extern PFNGLWINDOWPOS2IVMESAPROC glWindowPos2ivMESA;
extern PFNGLWINDOWPOS2SMESAPROC glWindowPos2sMESA;
extern PFNGLWINDOWPOS2SVMESAPROC glWindowPos2svMESA;
extern PFNGLWINDOWPOS3DMESAPROC glWindowPos3dMESA;
extern PFNGLWINDOWPOS3DVMESAPROC glWindowPos3dvMESA;
extern PFNGLWINDOWPOS3FMESAPROC glWindowPos3fMESA;
extern PFNGLWINDOWPOS3FVMESAPROC glWindowPos3fvMESA;
extern PFNGLWINDOWPOS3IMESAPROC glWindowPos3iMESA;
extern PFNGLWINDOWPOS3IVMESAPROC glWindowPos3ivMESA;
extern PFNGLWINDOWPOS3SMESAPROC glWindowPos3sMESA;
extern PFNGLWINDOWPOS3SVMESAPROC glWindowPos3svMESA;
extern PFNGLWINDOWPOS4DMESAPROC glWindowPos4dMESA;
extern PFNGLWINDOWPOS4DVMESAPROC glWindowPos4dvMESA;
extern PFNGLWINDOWPOS4FMESAPROC glWindowPos4fMESA;
extern PFNGLWINDOWPOS4FVMESAPROC glWindowPos4fvMESA;
extern PFNGLWINDOWPOS4IMESAPROC glWindowPos4iMESA;
extern PFNGLWINDOWPOS4IVMESAPROC glWindowPos4ivMESA;
extern PFNGLWINDOWPOS4SMESAPROC glWindowPos4sMESA;
extern PFNGLWINDOWPOS4SVMESAPROC glWindowPos4svMESA;
#endif

#if defined(GL_IBM_cull_vertex)
#endif

#if defined(GL_IBM_multimode_draw_arrays)
extern PFNGLMULTIMODEDRAWARRAYSIBMPROC glMultiModeDrawArraysIBM;
extern PFNGLMULTIMODEDRAWELEMENTSIBMPROC glMultiModeDrawElementsIBM;
#endif

#if defined(GL_IBM_vertex_array_lists)
extern PFNGLCOLORPOINTERLISTIBMPROC glColorPointerListIBM;
extern PFNGLSECONDARYCOLORPOINTERLISTIBMPROC glSecondaryColorPointerListIBM;
extern PFNGLEDGEFLAGPOINTERLISTIBMPROC glEdgeFlagPointerListIBM;
extern PFNGLFOGCOORDPOINTERLISTIBMPROC glFogCoordPointerListIBM;
extern PFNGLINDEXPOINTERLISTIBMPROC glIndexPointerListIBM;
extern PFNGLNORMALPOINTERLISTIBMPROC glNormalPointerListIBM;
extern PFNGLTEXCOORDPOINTERLISTIBMPROC glTexCoordPointerListIBM;
extern PFNGLVERTEXPOINTERLISTIBMPROC glVertexPointerListIBM;
#endif

#if defined(GL_3DFX_texture_compression_FXT1)
#endif

#if defined(GL_3DFX_multisample)
#endif

#if defined(GL_3DFX_tbuffer)
extern PFNGLTBUFFERMASK3DFXPROC glTbufferMask3DFX;
#endif

#if defined(GL_SGIX_vertex_preclip)
#endif

#if defined(GL_SGIX_resample)
#endif

#if defined(GL_SGIS_texture_color_mask)
extern PFNGLTEXTURECOLORMASKSGISPROC glTextureColorMaskSGIS;
#endif

#if defined(GL_EXT_texture_env_dot3)
#endif

#if defined(GL_ATI_texture_mirror_once)
#endif

#if defined(GL_NV_fence)
extern PFNGLDELETEFENCESNVPROC glDeleteFencesNV;
extern PFNGLGENFENCESNVPROC glGenFencesNV;
extern PFNGLISFENCENVPROC glIsFenceNV;
extern PFNGLTESTFENCENVPROC glTestFenceNV;
extern PFNGLGETFENCEIVNVPROC glGetFenceivNV;
extern PFNGLFINISHFENCENVPROC glFinishFenceNV;
extern PFNGLSETFENCENVPROC glSetFenceNV;
#endif

#if defined(GL_NV_evaluators)
extern PFNGLMAPCONTROLPOINTSNVPROC glMapControlPointsNV;
extern PFNGLMAPPARAMETERIVNVPROC glMapParameterivNV;
extern PFNGLMAPPARAMETERFVNVPROC glMapParameterfvNV;
extern PFNGLGETMAPCONTROLPOINTSNVPROC glGetMapControlPointsNV;
extern PFNGLGETMAPPARAMETERIVNVPROC glGetMapParameterivNV;
extern PFNGLGETMAPPARAMETERFVNVPROC glGetMapParameterfvNV;
extern PFNGLGETMAPATTRIBPARAMETERIVNVPROC glGetMapAttribParameterivNV;
extern PFNGLGETMAPATTRIBPARAMETERFVNVPROC glGetMapAttribParameterfvNV;
extern PFNGLEVALMAPSNVPROC glEvalMapsNV;
#endif

#if defined(GL_NV_packed_depth_stencil)
#endif

#if defined(GL_NV_register_combiners2)
extern PFNGLCOMBINERSTAGEPARAMETERFVNVPROC glCombinerStageParameterfvNV;
extern PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC glGetCombinerStageParameterfvNV;
#endif

#if defined(GL_NV_texture_compression_vtc)
#endif

#if defined(GL_NV_texture_rectangle)
#endif

#if defined(GL_NV_texture_shader)
#endif

#if defined(GL_NV_texture_shader2)
#endif

#if defined(GL_NV_vertex_array_range2)
#endif

#if defined(GL_NV_vertex_program)
extern PFNGLAREPROGRAMSRESIDENTNVPROC glAreProgramsResidentNV;
extern PFNGLBINDPROGRAMNVPROC glBindProgramNV;
extern PFNGLDELETEPROGRAMSNVPROC glDeleteProgramsNV;
extern PFNGLEXECUTEPROGRAMNVPROC glExecuteProgramNV;
extern PFNGLGENPROGRAMSNVPROC glGenProgramsNV;
extern PFNGLGETPROGRAMPARAMETERDVNVPROC glGetProgramParameterdvNV;
extern PFNGLGETPROGRAMPARAMETERFVNVPROC glGetProgramParameterfvNV;
extern PFNGLGETPROGRAMIVNVPROC glGetProgramivNV;
extern PFNGLGETPROGRAMSTRINGNVPROC glGetProgramStringNV;
extern PFNGLGETTRACKMATRIXIVNVPROC glGetTrackMatrixivNV;
extern PFNGLGETVERTEXATTRIBDVNVPROC glGetVertexAttribdvNV;
extern PFNGLGETVERTEXATTRIBFVNVPROC glGetVertexAttribfvNV;
extern PFNGLGETVERTEXATTRIBIVNVPROC glGetVertexAttribivNV;
extern PFNGLGETVERTEXATTRIBPOINTERVNVPROC glGetVertexAttribPointervNV;
extern PFNGLISPROGRAMNVPROC glIsProgramNV;
extern PFNGLLOADPROGRAMNVPROC glLoadProgramNV;
extern PFNGLPROGRAMPARAMETER4DNVPROC glProgramParameter4dNV;
extern PFNGLPROGRAMPARAMETER4DVNVPROC glProgramParameter4dvNV;
extern PFNGLPROGRAMPARAMETER4FNVPROC glProgramParameter4fNV;
extern PFNGLPROGRAMPARAMETER4FVNVPROC glProgramParameter4fvNV;
extern PFNGLPROGRAMPARAMETERS4DVNVPROC glProgramParameters4dvNV;
extern PFNGLPROGRAMPARAMETERS4FVNVPROC glProgramParameters4fvNV;
extern PFNGLREQUESTRESIDENTPROGRAMSNVPROC glRequestResidentProgramsNV;
extern PFNGLTRACKMATRIXNVPROC glTrackMatrixNV;
extern PFNGLVERTEXATTRIBPOINTERNVPROC glVertexAttribPointerNV;
extern PFNGLVERTEXATTRIB1DNVPROC glVertexAttrib1dNV;
extern PFNGLVERTEXATTRIB1DVNVPROC glVertexAttrib1dvNV;
extern PFNGLVERTEXATTRIB1FNVPROC glVertexAttrib1fNV;
extern PFNGLVERTEXATTRIB1FVNVPROC glVertexAttrib1fvNV;
extern PFNGLVERTEXATTRIB1SNVPROC glVertexAttrib1sNV;
extern PFNGLVERTEXATTRIB1SVNVPROC glVertexAttrib1svNV;
extern PFNGLVERTEXATTRIB2DNVPROC glVertexAttrib2dNV;
extern PFNGLVERTEXATTRIB2DVNVPROC glVertexAttrib2dvNV;
extern PFNGLVERTEXATTRIB2FNVPROC glVertexAttrib2fNV;
extern PFNGLVERTEXATTRIB2FVNVPROC glVertexAttrib2fvNV;
extern PFNGLVERTEXATTRIB2SNVPROC glVertexAttrib2sNV;
extern PFNGLVERTEXATTRIB2SVNVPROC glVertexAttrib2svNV;
extern PFNGLVERTEXATTRIB3DNVPROC glVertexAttrib3dNV;
extern PFNGLVERTEXATTRIB3DVNVPROC glVertexAttrib3dvNV;
extern PFNGLVERTEXATTRIB3FNVPROC glVertexAttrib3fNV;
extern PFNGLVERTEXATTRIB3FVNVPROC glVertexAttrib3fvNV;
extern PFNGLVERTEXATTRIB3SNVPROC glVertexAttrib3sNV;
extern PFNGLVERTEXATTRIB3SVNVPROC glVertexAttrib3svNV;
extern PFNGLVERTEXATTRIB4DNVPROC glVertexAttrib4dNV;
extern PFNGLVERTEXATTRIB4DVNVPROC glVertexAttrib4dvNV;
extern PFNGLVERTEXATTRIB4FNVPROC glVertexAttrib4fNV;
extern PFNGLVERTEXATTRIB4FVNVPROC glVertexAttrib4fvNV;
extern PFNGLVERTEXATTRIB4SNVPROC glVertexAttrib4sNV;
extern PFNGLVERTEXATTRIB4SVNVPROC glVertexAttrib4svNV;
extern PFNGLVERTEXATTRIB4UBNVPROC glVertexAttrib4ubNV;
extern PFNGLVERTEXATTRIB4UBVNVPROC glVertexAttrib4ubvNV;
extern PFNGLVERTEXATTRIBS1DVNVPROC glVertexAttribs1dvNV;
extern PFNGLVERTEXATTRIBS1FVNVPROC glVertexAttribs1fvNV;
extern PFNGLVERTEXATTRIBS1SVNVPROC glVertexAttribs1svNV;
extern PFNGLVERTEXATTRIBS2DVNVPROC glVertexAttribs2dvNV;
extern PFNGLVERTEXATTRIBS2FVNVPROC glVertexAttribs2fvNV;
extern PFNGLVERTEXATTRIBS2SVNVPROC glVertexAttribs2svNV;
extern PFNGLVERTEXATTRIBS3DVNVPROC glVertexAttribs3dvNV;
extern PFNGLVERTEXATTRIBS3FVNVPROC glVertexAttribs3fvNV;
extern PFNGLVERTEXATTRIBS3SVNVPROC glVertexAttribs3svNV;
extern PFNGLVERTEXATTRIBS4DVNVPROC glVertexAttribs4dvNV;
extern PFNGLVERTEXATTRIBS4FVNVPROC glVertexAttribs4fvNV;
extern PFNGLVERTEXATTRIBS4SVNVPROC glVertexAttribs4svNV;
extern PFNGLVERTEXATTRIBS4UBVNVPROC glVertexAttribs4ubvNV;
#endif

#if defined(GL_SGIX_texture_coordinate_clamp)
#endif

#if defined(GL_OML_interlace)
#endif

#if defined(GL_OML_subsample)
#endif

#if defined(GL_OML_resample)
#endif

#if defined(GL_NV_copy_depth_to_color)
#endif

#if defined(GL_ATI_envmap_bumpmap)
extern PFNGLTEXBUMPPARAMETERIVATIPROC glTexBumpParameterivATI;
extern PFNGLTEXBUMPPARAMETERFVATIPROC glTexBumpParameterfvATI;
extern PFNGLGETTEXBUMPPARAMETERIVATIPROC glGetTexBumpParameterivATI;
extern PFNGLGETTEXBUMPPARAMETERFVATIPROC glGetTexBumpParameterfvATI;
#endif

#if defined(GL_ATI_fragment_shader)
extern PFNGLGENFRAGMENTSHADERSATIPROC glGenFragmentShadersATI;
extern PFNGLBINDFRAGMENTSHADERATIPROC glBindFragmentShaderATI;
extern PFNGLDELETEFRAGMENTSHADERATIPROC glDeleteFragmentShaderATI;
extern PFNGLBEGINFRAGMENTSHADERATIPROC glBeginFragmentShaderATI;
extern PFNGLENDFRAGMENTSHADERATIPROC glEndFragmentShaderATI;
extern PFNGLPASSTEXCOORDATIPROC glPassTexCoordATI;
extern PFNGLSAMPLEMAPATIPROC glSampleMapATI;
extern PFNGLCOLORFRAGMENTOP1ATIPROC glColorFragmentOp1ATI;
extern PFNGLCOLORFRAGMENTOP2ATIPROC glColorFragmentOp2ATI;
extern PFNGLCOLORFRAGMENTOP3ATIPROC glColorFragmentOp3ATI;
extern PFNGLALPHAFRAGMENTOP1ATIPROC glAlphaFragmentOp1ATI;
extern PFNGLALPHAFRAGMENTOP2ATIPROC glAlphaFragmentOp2ATI;
extern PFNGLALPHAFRAGMENTOP3ATIPROC glAlphaFragmentOp3ATI;
extern PFNGLSETFRAGMENTSHADERCONSTANTATIPROC glSetFragmentShaderConstantATI;
#endif

#if defined(GL_ATI_pn_triangles)
#endif

#if defined(GL_ATI_vertex_array_object) && 0
extern PFNGLNEWOBJECTBUFFERATIPROC glNewObjectBufferATI;
extern PFNGLISOBJECTBUFFERATIPROC glIsObjectBufferATI;
extern PFNGLUPDATEOBJECTBUFFERATIPROC glUpdateObjectBufferATI;
extern PFNGLGETOBJECTBUFFERFVATIPROC glGetObjectBufferfvATI;
extern PFNGLGETOBJECTBUFFERIVATIPROC glGetObjectBufferivATI;
extern PFNGLFREEOBJECTBUFFERATIPROC glFreeObjectBufferATI;
extern PFNGLARRAYOBJECTATIPROC glArrayObjectATI;
extern PFNGLGETARRAYOBJECTFVATIPROC glGetArrayObjectfvATI;
extern PFNGLGETARRAYOBJECTIVATIPROC glGetArrayObjectivATI;
extern PFNGLVARIANTARRAYOBJECTATIPROC glVariantArrayObjectATI;
extern PFNGLGETVARIANTARRAYOBJECTFVATIPROC glGetVariantArrayObjectfvATI;
extern PFNGLGETVARIANTARRAYOBJECTIVATIPROC glGetVariantArrayObjectivATI;
#endif

#if defined(GL_EXT_vertex_shader)
extern PFNGLBEGINVERTEXSHADEREXTPROC glBeginVertexShaderEXT;
extern PFNGLENDVERTEXSHADEREXTPROC glEndVertexShaderEXT;
extern PFNGLBINDVERTEXSHADEREXTPROC glBindVertexShaderEXT;
extern PFNGLGENVERTEXSHADERSEXTPROC glGenVertexShadersEXT;
extern PFNGLDELETEVERTEXSHADEREXTPROC glDeleteVertexShaderEXT;
extern PFNGLSHADEROP1EXTPROC glShaderOp1EXT;
extern PFNGLSHADEROP2EXTPROC glShaderOp2EXT;
extern PFNGLSHADEROP3EXTPROC glShaderOp3EXT;
extern PFNGLSWIZZLEEXTPROC glSwizzleEXT;
extern PFNGLWRITEMASKEXTPROC glWriteMaskEXT;
extern PFNGLINSERTCOMPONENTEXTPROC glInsertComponentEXT;
extern PFNGLEXTRACTCOMPONENTEXTPROC glExtractComponentEXT;
extern PFNGLGENSYMBOLSEXTPROC glGenSymbolsEXT;
extern PFNGLSETINVARIANTEXTPROC glSetInvariantEXT;
extern PFNGLSETLOCALCONSTANTEXTPROC glSetLocalConstantEXT;
extern PFNGLVARIANTBVEXTPROC glVariantbvEXT;
extern PFNGLVARIANTSVEXTPROC glVariantsvEXT;
extern PFNGLVARIANTIVEXTPROC glVariantivEXT;
extern PFNGLVARIANTFVEXTPROC glVariantfvEXT;
extern PFNGLVARIANTDVEXTPROC glVariantdvEXT;
extern PFNGLVARIANTUBVEXTPROC glVariantubvEXT;
extern PFNGLVARIANTUSVEXTPROC glVariantusvEXT;
extern PFNGLVARIANTUIVEXTPROC glVariantuivEXT;
extern PFNGLVARIANTPOINTEREXTPROC glVariantPointerEXT;
extern PFNGLENABLEVARIANTCLIENTSTATEEXTPROC glEnableVariantClientStateEXT;
extern PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC glDisableVariantClientStateEXT;
extern PFNGLBINDLIGHTPARAMETEREXTPROC glBindLightParameterEXT;
extern PFNGLBINDMATERIALPARAMETEREXTPROC glBindMaterialParameterEXT;
extern PFNGLBINDTEXGENPARAMETEREXTPROC glBindTexGenParameterEXT;
extern PFNGLBINDTEXTUREUNITPARAMETEREXTPROC glBindTextureUnitParameterEXT;
extern PFNGLBINDPARAMETEREXTPROC glBindParameterEXT;
extern PFNGLISVARIANTENABLEDEXTPROC glIsVariantEnabledEXT;
extern PFNGLGETVARIANTBOOLEANVEXTPROC glGetVariantBooleanvEXT;
extern PFNGLGETVARIANTINTEGERVEXTPROC glGetVariantIntegervEXT;
extern PFNGLGETVARIANTFLOATVEXTPROC glGetVariantFloatvEXT;
extern PFNGLGETVARIANTPOINTERVEXTPROC glGetVariantPointervEXT;
extern PFNGLGETINVARIANTBOOLEANVEXTPROC glGetInvariantBooleanvEXT;
extern PFNGLGETINVARIANTINTEGERVEXTPROC glGetInvariantIntegervEXT;
extern PFNGLGETINVARIANTFLOATVEXTPROC glGetInvariantFloatvEXT;
extern PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC glGetLocalConstantBooleanvEXT;
extern PFNGLGETLOCALCONSTANTINTEGERVEXTPROC glGetLocalConstantIntegervEXT;
extern PFNGLGETLOCALCONSTANTFLOATVEXTPROC glGetLocalConstantFloatvEXT;
#endif

#if defined(GL_ATI_vertex_streams)
extern PFNGLVERTEXSTREAM1SATIPROC glVertexStream1sATI;
extern PFNGLVERTEXSTREAM1SVATIPROC glVertexStream1svATI;
extern PFNGLVERTEXSTREAM1IATIPROC glVertexStream1iATI;
extern PFNGLVERTEXSTREAM1IVATIPROC glVertexStream1ivATI;
extern PFNGLVERTEXSTREAM1FATIPROC glVertexStream1fATI;
extern PFNGLVERTEXSTREAM1FVATIPROC glVertexStream1fvATI;
extern PFNGLVERTEXSTREAM1DATIPROC glVertexStream1dATI;
extern PFNGLVERTEXSTREAM1DVATIPROC glVertexStream1dvATI;
extern PFNGLVERTEXSTREAM2SATIPROC glVertexStream2sATI;
extern PFNGLVERTEXSTREAM2SVATIPROC glVertexStream2svATI;
extern PFNGLVERTEXSTREAM2IATIPROC glVertexStream2iATI;
extern PFNGLVERTEXSTREAM2IVATIPROC glVertexStream2ivATI;
extern PFNGLVERTEXSTREAM2FATIPROC glVertexStream2fATI;
extern PFNGLVERTEXSTREAM2FVATIPROC glVertexStream2fvATI;
extern PFNGLVERTEXSTREAM2DATIPROC glVertexStream2dATI;
extern PFNGLVERTEXSTREAM2DVATIPROC glVertexStream2dvATI;
extern PFNGLVERTEXSTREAM3SATIPROC glVertexStream3sATI;
extern PFNGLVERTEXSTREAM3SVATIPROC glVertexStream3svATI;
extern PFNGLVERTEXSTREAM3IATIPROC glVertexStream3iATI;
extern PFNGLVERTEXSTREAM3IVATIPROC glVertexStream3ivATI;
extern PFNGLVERTEXSTREAM3FATIPROC glVertexStream3fATI;
extern PFNGLVERTEXSTREAM3FVATIPROC glVertexStream3fvATI;
extern PFNGLVERTEXSTREAM3DATIPROC glVertexStream3dATI;
extern PFNGLVERTEXSTREAM3DVATIPROC glVertexStream3dvATI;
extern PFNGLVERTEXSTREAM4SATIPROC glVertexStream4sATI;
extern PFNGLVERTEXSTREAM4SVATIPROC glVertexStream4svATI;
extern PFNGLVERTEXSTREAM4IATIPROC glVertexStream4iATI;
extern PFNGLVERTEXSTREAM4IVATIPROC glVertexStream4ivATI;
extern PFNGLVERTEXSTREAM4FATIPROC glVertexStream4fATI;
extern PFNGLVERTEXSTREAM4FVATIPROC glVertexStream4fvATI;
extern PFNGLVERTEXSTREAM4DATIPROC glVertexStream4dATI;
extern PFNGLVERTEXSTREAM4DVATIPROC glVertexStream4dvATI;
extern PFNGLNORMALSTREAM3BATIPROC glNormalStream3bATI;
extern PFNGLNORMALSTREAM3BVATIPROC glNormalStream3bvATI;
extern PFNGLNORMALSTREAM3SATIPROC glNormalStream3sATI;
extern PFNGLNORMALSTREAM3SVATIPROC glNormalStream3svATI;
extern PFNGLNORMALSTREAM3IATIPROC glNormalStream3iATI;
extern PFNGLNORMALSTREAM3IVATIPROC glNormalStream3ivATI;
extern PFNGLNORMALSTREAM3FATIPROC glNormalStream3fATI;
extern PFNGLNORMALSTREAM3FVATIPROC glNormalStream3fvATI;
extern PFNGLNORMALSTREAM3DATIPROC glNormalStream3dATI;
extern PFNGLNORMALSTREAM3DVATIPROC glNormalStream3dvATI;
extern PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC glClientActiveVertexStreamATI;
extern PFNGLVERTEXBLENDENVIATIPROC glVertexBlendEnviATI;
extern PFNGLVERTEXBLENDENVFATIPROC glVertexBlendEnvfATI;
#endif

#if defined(GL_ATI_element_array)
extern PFNGLELEMENTPOINTERATIPROC glElementPointerATI;
extern PFNGLDRAWELEMENTARRAYATIPROC glDrawElementArrayATI;
extern PFNGLDRAWRANGEELEMENTARRAYATIPROC glDrawRangeElementArrayATI;
#endif

#if defined(GL_SUN_mesh_array)
extern PFNGLDRAWMESHARRAYSSUNPROC glDrawMeshArraysSUN;
#endif

#if defined(GL_SUN_slice_accum)
#endif

#if defined(GL_NV_multisample_filter_hint)
#endif

#if defined(GL_NV_depth_clamp)
#endif

#if defined(GL_NV_occlusion_query)
extern PFNGLGENOCCLUSIONQUERIESNVPROC glGenOcclusionQueriesNV;
extern PFNGLDELETEOCCLUSIONQUERIESNVPROC glDeleteOcclusionQueriesNV;
extern PFNGLISOCCLUSIONQUERYNVPROC glIsOcclusionQueryNV;
extern PFNGLBEGINOCCLUSIONQUERYNVPROC glBeginOcclusionQueryNV;
extern PFNGLENDOCCLUSIONQUERYNVPROC glEndOcclusionQueryNV;
extern PFNGLGETOCCLUSIONQUERYIVNVPROC glGetOcclusionQueryivNV;
extern PFNGLGETOCCLUSIONQUERYUIVNVPROC glGetOcclusionQueryuivNV;
#endif

#if defined(GL_NV_point_sprite)
extern PFNGLPOINTPARAMETERINVPROC glPointParameteriNV;
extern PFNGLPOINTPARAMETERIVNVPROC glPointParameterivNV;
#endif

#if defined(GL_NV_texture_shader3)
#endif

#if defined(GL_NV_vertex_program1_1)
#endif

#if defined(GL_EXT_shadow_funcs)
#endif

#if defined(GL_EXT_stencil_two_side)
extern PFNGLACTIVESTENCILFACEEXTPROC glActiveStencilFaceEXT;
#endif

#if defined(GL_ATI_text_fragment_shader)
#endif

#if defined(GL_APPLE_client_storage)
#endif

#if defined(GL_APPLE_element_array)
extern PFNGLELEMENTPOINTERAPPLEPROC glElementPointerAPPLE;
extern PFNGLDRAWELEMENTARRAYAPPLEPROC glDrawElementArrayAPPLE;
extern PFNGLDRAWRANGEELEMENTARRAYAPPLEPROC glDrawRangeElementArrayAPPLE;
extern PFNGLMULTIDRAWELEMENTARRAYAPPLEPROC glMultiDrawElementArrayAPPLE;
extern PFNGLMULTIDRAWRANGEELEMENTARRAYAPPLEPROC glMultiDrawRangeElementArrayAPPLE;
#endif

#if defined(GL_APPLE_fence)
extern PFNGLGENFENCESAPPLEPROC glGenFencesAPPLE;
extern PFNGLDELETEFENCESAPPLEPROC glDeleteFencesAPPLE;
extern PFNGLSETFENCEAPPLEPROC glSetFenceAPPLE;
extern PFNGLISFENCEAPPLEPROC glIsFenceAPPLE;
extern PFNGLTESTFENCEAPPLEPROC glTestFenceAPPLE;
extern PFNGLFINISHFENCEAPPLEPROC glFinishFenceAPPLE;
extern PFNGLTESTOBJECTAPPLEPROC glTestObjectAPPLE;
extern PFNGLFINISHOBJECTAPPLEPROC glFinishObjectAPPLE;
#endif

#if defined(GL_APPLE_vertex_array_object)
extern PFNGLBINDVERTEXARRAYAPPLEPROC glBindVertexArrayAPPLE;
extern PFNGLDELETEVERTEXARRAYSAPPLEPROC glDeleteVertexArraysAPPLE;
extern PFNGLGENVERTEXARRAYSAPPLEPROC glGenVertexArraysAPPLE;
extern PFNGLISVERTEXARRAYAPPLEPROC glIsVertexArrayAPPLE;
#endif

#if defined(GL_APPLE_vertex_array_range)
extern PFNGLVERTEXARRAYRANGEAPPLEPROC glVertexArrayRangeAPPLE;
extern PFNGLFLUSHVERTEXARRAYRANGEAPPLEPROC glFlushVertexArrayRangeAPPLE;
extern PFNGLVERTEXARRAYPARAMETERIAPPLEPROC glVertexArrayParameteriAPPLE;
#endif

#if defined(GL_APPLE_ycbcr_422)
#endif

#if defined(GL_S3_s3tc)
#endif

#if defined(GL_ATI_draw_buffers)
extern PFNGLDRAWBUFFERSATIPROC glDrawBuffersATI;
#endif

#if defined(GL_ATI_texture_env_combine3)
#endif

#if defined(GL_ATI_texture_float)
#endif

#if defined(GL_NV_float_buffer)
#endif

#if defined(GL_NV_fragment_program)
extern PFNGLPROGRAMNAMEDPARAMETER4FNVPROC glProgramNamedParameter4fNV;
extern PFNGLPROGRAMNAMEDPARAMETER4DNVPROC glProgramNamedParameter4dNV;
extern PFNGLPROGRAMNAMEDPARAMETER4FVNVPROC glProgramNamedParameter4fvNV;
extern PFNGLPROGRAMNAMEDPARAMETER4DVNVPROC glProgramNamedParameter4dvNV;
extern PFNGLGETPROGRAMNAMEDPARAMETERFVNVPROC glGetProgramNamedParameterfvNV;
extern PFNGLGETPROGRAMNAMEDPARAMETERDVNVPROC glGetProgramNamedParameterdvNV;
#endif

#if defined(GL_NV_half_float)
extern PFNGLVERTEX2HNVPROC glVertex2hNV;
extern PFNGLVERTEX2HVNVPROC glVertex2hvNV;
extern PFNGLVERTEX3HNVPROC glVertex3hNV;
extern PFNGLVERTEX3HVNVPROC glVertex3hvNV;
extern PFNGLVERTEX4HNVPROC glVertex4hNV;
extern PFNGLVERTEX4HVNVPROC glVertex4hvNV;
extern PFNGLNORMAL3HNVPROC glNormal3hNV;
extern PFNGLNORMAL3HVNVPROC glNormal3hvNV;
extern PFNGLCOLOR3HNVPROC glColor3hNV;
extern PFNGLCOLOR3HVNVPROC glColor3hvNV;
extern PFNGLCOLOR4HNVPROC glColor4hNV;
extern PFNGLCOLOR4HVNVPROC glColor4hvNV;
extern PFNGLTEXCOORD1HNVPROC glTexCoord1hNV;
extern PFNGLTEXCOORD1HVNVPROC glTexCoord1hvNV;
extern PFNGLTEXCOORD2HNVPROC glTexCoord2hNV;
extern PFNGLTEXCOORD2HVNVPROC glTexCoord2hvNV;
extern PFNGLTEXCOORD3HNVPROC glTexCoord3hNV;
extern PFNGLTEXCOORD3HVNVPROC glTexCoord3hvNV;
extern PFNGLTEXCOORD4HNVPROC glTexCoord4hNV;
extern PFNGLTEXCOORD4HVNVPROC glTexCoord4hvNV;
extern PFNGLMULTITEXCOORD1HNVPROC glMultiTexCoord1hNV;
extern PFNGLMULTITEXCOORD1HVNVPROC glMultiTexCoord1hvNV;
extern PFNGLMULTITEXCOORD2HNVPROC glMultiTexCoord2hNV;
extern PFNGLMULTITEXCOORD2HVNVPROC glMultiTexCoord2hvNV;
extern PFNGLMULTITEXCOORD3HNVPROC glMultiTexCoord3hNV;
extern PFNGLMULTITEXCOORD3HVNVPROC glMultiTexCoord3hvNV;
extern PFNGLMULTITEXCOORD4HNVPROC glMultiTexCoord4hNV;
extern PFNGLMULTITEXCOORD4HVNVPROC glMultiTexCoord4hvNV;
extern PFNGLFOGCOORDHNVPROC glFogCoordhNV;
extern PFNGLFOGCOORDHVNVPROC glFogCoordhvNV;
extern PFNGLSECONDARYCOLOR3HNVPROC glSecondaryColor3hNV;
extern PFNGLSECONDARYCOLOR3HVNVPROC glSecondaryColor3hvNV;
extern PFNGLVERTEXWEIGHTHNVPROC glVertexWeighthNV;
extern PFNGLVERTEXWEIGHTHVNVPROC glVertexWeighthvNV;
extern PFNGLVERTEXATTRIB1HNVPROC glVertexAttrib1hNV;
extern PFNGLVERTEXATTRIB1HVNVPROC glVertexAttrib1hvNV;
extern PFNGLVERTEXATTRIB2HNVPROC glVertexAttrib2hNV;
extern PFNGLVERTEXATTRIB2HVNVPROC glVertexAttrib2hvNV;
extern PFNGLVERTEXATTRIB3HNVPROC glVertexAttrib3hNV;
extern PFNGLVERTEXATTRIB3HVNVPROC glVertexAttrib3hvNV;
extern PFNGLVERTEXATTRIB4HNVPROC glVertexAttrib4hNV;
extern PFNGLVERTEXATTRIB4HVNVPROC glVertexAttrib4hvNV;
extern PFNGLVERTEXATTRIBS1HVNVPROC glVertexAttribs1hvNV;
extern PFNGLVERTEXATTRIBS2HVNVPROC glVertexAttribs2hvNV;
extern PFNGLVERTEXATTRIBS3HVNVPROC glVertexAttribs3hvNV;
extern PFNGLVERTEXATTRIBS4HVNVPROC glVertexAttribs4hvNV;
#endif

#if defined(GL_NV_pixel_data_range)
extern PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV;
extern PFNGLFLUSHPIXELDATARANGENVPROC glFlushPixelDataRangeNV;
#endif

#if defined(GL_NV_primitive_restart)
extern PFNGLPRIMITIVERESTARTNVPROC glPrimitiveRestartNV;
extern PFNGLPRIMITIVERESTARTINDEXNVPROC glPrimitiveRestartIndexNV;
#endif

#if defined(GL_NV_texture_expand_normal)
#endif

#if defined(GL_NV_vertex_program2)
#endif

#if defined(GL_ATI_map_object_buffer)
extern PFNGLMAPOBJECTBUFFERATIPROC glMapObjectBufferATI;
extern PFNGLUNMAPOBJECTBUFFERATIPROC glUnmapObjectBufferATI;
#endif

#if defined(GL_ATI_separate_stencil)
extern PFNGLSTENCILOPSEPARATEATIPROC glStencilOpSeparateATI;
extern PFNGLSTENCILFUNCSEPARATEATIPROC glStencilFuncSeparateATI;
#endif

#if defined(GL_ATI_vertex_attrib_array_object)
extern PFNGLVERTEXATTRIBARRAYOBJECTATIPROC glVertexAttribArrayObjectATI;
extern PFNGLGETVERTEXATTRIBARRAYOBJECTFVATIPROC glGetVertexAttribArrayObjectfvATI;
extern PFNGLGETVERTEXATTRIBARRAYOBJECTIVATIPROC glGetVertexAttribArrayObjectivATI;
#endif

#if defined(GL_EXT_depth_bounds_test)
extern PFNGLDEPTHBOUNDSEXTPROC glDepthBoundsEXT;
#endif

#if defined(GL_EXT_texture_mirror_clamp)
#endif

#if defined(GL_EXT_blend_equation_separate)
extern PFNGLBLENDEQUATIONSEPARATEEXTPROC glBlendEquationSeparateEXT;
#endif

#if defined(GL_MESA_pack_invert)
#endif

#if defined(GL_MESA_ycbcr_texture)
#endif

/* end mkglext.py */

} /* namespace RAS_GL */

#endif /* __RAS_GLEXTENSIONMANAGER_H__ */
