/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RAS_GLEXTENSIONMANAGER_H__
#define __RAS_GLEXTENSIONMANAGER_H__


#ifdef WIN32
#  include <windows.h>
#  include <GL/gl.h>

#elif defined(__APPLE__)
#  define GL_GLEXT_LEGACY 1
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>

#else /* UNIX */
#  define __glext_h_
#  include <GL/gl.h>
#  include <GL/glx.h>
#  undef GL_ARB_multitexture // (ubuntu)
#  undef __glext_h_
#endif

#ifdef WITH_GLEXT
#ifdef WIN32
#  include <GL/glext.h>
#elif defined(__APPLE__)
#  include "mac_compat_glext.h"
#  include <OpenGL/glext.h>
# else
#  include <GL/glext.h>
# endif
#endif

#ifdef __sgi
#  undef GL_ARB_vertex_program
#endif

#include "EXT_separate_specular_color.h"
#include "ARB_multitexture.h"
namespace bgl
{
	/**
	 *  This is a list of all registered OpenGL extensions.
	 *  It is available from:
	 *  http://oss.sgi.com/projects/ogl-sample/registry/ 
	 */
	typedef enum {
		/* ARB Extensions */
		_GL_ARB_imaging,
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
	
	/**
	 * Checks at runtime whether OpenGL supports the named extension.
	 * Returns true if OpenGL supports the given extension.
	 * 
	 * @param name	The extension name to check.
	 */
	bool QueryExtension(ExtensionName name);
	/**
	 * Checks the OpenGL version.
	 * Returns true if OpenGL is at least the given version.
	 *
	 * @param major	The major version required
	 * @param minor	The minor version required
	 */
	bool QueryVersion(int major, int minor);
	/**
	 * This will dynamically link all runtime supported extensions into
	 * the binary.
	 *
	 * @param debug	Enable debug printing.  This will print debugging info
	 *   when extensions are loaded.
	 */
	void InitExtensions(int debug);

#if defined(PFNGLPNTRIANGLESIATIPROC)
extern PFNGLPNTRIANGLESIATIPROC blPNTrianglesiATI;
extern PFNGLPNTRIANGLESFATIPROC blPNTrianglesfATI;
#endif


// quick runtime checks
typedef struct BL_EXTInfo
{
	BL_EXTInfo():
		_ARB_multitexture(0),
		_ARB_texture_env_combine(0),
		_EXT_blend_color(0),
		_ARB_texture_cube_map(0),
		_ARB_shader_objects(0),
		_ARB_vertex_shader(0),
		_ARB_fragment_shader(0),
		_EXT_texture3D(0),
		_ARB_vertex_program(0),
		_ARB_depth_texture(0),
		_EXT_compiled_vertex_array(0)
	{
		//
	}
	bool _ARB_multitexture;
	bool _ARB_texture_env_combine;
	bool _EXT_blend_color;
	bool _ARB_texture_cube_map;
	bool _ARB_shader_objects;
	bool _ARB_vertex_shader;
	bool _ARB_fragment_shader;
	bool _EXT_texture3D;
	bool _ARB_vertex_program;
	bool _ARB_depth_texture;
	bool _EXT_compiled_vertex_array;
}BL_EXTInfo;

extern BL_EXTInfo RAS_EXT_support;

#ifdef GL_ARB_multitexture // defined in glext.h now...
extern int max_texture_units;
extern PFNGLACTIVETEXTUREARBPROC blActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC blClientActiveTextureARB;
extern PFNGLMULTITEXCOORD1DARBPROC blMultiTexCoord1dARB;
extern PFNGLMULTITEXCOORD1DVARBPROC blMultiTexCoord1dvARB;
extern PFNGLMULTITEXCOORD1FARBPROC blMultiTexCoord1fARB;
extern PFNGLMULTITEXCOORD1FVARBPROC blMultiTexCoord1fvARB;
extern PFNGLMULTITEXCOORD1IARBPROC blMultiTexCoord1iARB;
extern PFNGLMULTITEXCOORD1IVARBPROC blMultiTexCoord1ivARB;
extern PFNGLMULTITEXCOORD1SARBPROC blMultiTexCoord1sARB;
extern PFNGLMULTITEXCOORD1SVARBPROC blMultiTexCoord1svARB;
extern PFNGLMULTITEXCOORD2DARBPROC blMultiTexCoord2dARB;
extern PFNGLMULTITEXCOORD2DVARBPROC blMultiTexCoord2dvARB;
extern PFNGLMULTITEXCOORD2FARBPROC blMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC blMultiTexCoord2fvARB;
extern PFNGLMULTITEXCOORD2IARBPROC blMultiTexCoord2iARB;
extern PFNGLMULTITEXCOORD2IVARBPROC blMultiTexCoord2ivARB;
extern PFNGLMULTITEXCOORD2SARBPROC blMultiTexCoord2sARB;
extern PFNGLMULTITEXCOORD2SVARBPROC blMultiTexCoord2svARB;
extern PFNGLMULTITEXCOORD3DARBPROC blMultiTexCoord3dARB;
extern PFNGLMULTITEXCOORD3DVARBPROC blMultiTexCoord3dvARB;
extern PFNGLMULTITEXCOORD3FARBPROC blMultiTexCoord3fARB;
extern PFNGLMULTITEXCOORD3FVARBPROC blMultiTexCoord3fvARB;
extern PFNGLMULTITEXCOORD3IARBPROC blMultiTexCoord3iARB;
extern PFNGLMULTITEXCOORD3IVARBPROC blMultiTexCoord3ivARB;
extern PFNGLMULTITEXCOORD3SARBPROC blMultiTexCoord3sARB;
extern PFNGLMULTITEXCOORD3SVARBPROC blMultiTexCoord3svARB;
extern PFNGLMULTITEXCOORD4DARBPROC blMultiTexCoord4dARB;
extern PFNGLMULTITEXCOORD4DVARBPROC blMultiTexCoord4dvARB;
extern PFNGLMULTITEXCOORD4FARBPROC blMultiTexCoord4fARB;
extern PFNGLMULTITEXCOORD4FVARBPROC blMultiTexCoord4fvARB;
extern PFNGLMULTITEXCOORD4IARBPROC blMultiTexCoord4iARB;
extern PFNGLMULTITEXCOORD4IVARBPROC blMultiTexCoord4ivARB;
extern PFNGLMULTITEXCOORD4SARBPROC blMultiTexCoord4sARB;
extern PFNGLMULTITEXCOORD4SVARBPROC blMultiTexCoord4svARB;
#endif


#ifdef GL_ARB_shader_objects
extern PFNGLDELETEOBJECTARBPROC blDeleteObjectARB;
extern PFNGLGETHANDLEARBPROC blGetHandleARB;
extern PFNGLDETACHOBJECTARBPROC blDetachObjectARB;
extern PFNGLCREATESHADEROBJECTARBPROC blCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC blShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC blCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC blCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC blAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC blLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC blUseProgramObjectARB;
extern PFNGLVALIDATEPROGRAMARBPROC blValidateProgramARB;
extern PFNGLUNIFORM1FARBPROC blUniform1fARB;
extern PFNGLUNIFORM2FARBPROC blUniform2fARB;
extern PFNGLUNIFORM3FARBPROC blUniform3fARB;
extern PFNGLUNIFORM4FARBPROC blUniform4fARB;
extern PFNGLUNIFORM1IARBPROC blUniform1iARB;
extern PFNGLUNIFORM2IARBPROC blUniform2iARB;
extern PFNGLUNIFORM3IARBPROC blUniform3iARB;
extern PFNGLUNIFORM4IARBPROC blUniform4iARB;
extern PFNGLUNIFORM1FVARBPROC blUniform1fvARB;
extern PFNGLUNIFORM2FVARBPROC blUniform2fvARB;
extern PFNGLUNIFORM3FVARBPROC blUniform3fvARB;
extern PFNGLUNIFORM4FVARBPROC blUniform4fvARB;
extern PFNGLUNIFORM1IVARBPROC blUniform1ivARB;
extern PFNGLUNIFORM2IVARBPROC blUniform2ivARB;
extern PFNGLUNIFORM3IVARBPROC blUniform3ivARB;
extern PFNGLUNIFORM4IVARBPROC blUniform4ivARB;
extern PFNGLUNIFORMMATRIX2FVARBPROC blUniformMatrix2fvARB;
extern PFNGLUNIFORMMATRIX3FVARBPROC blUniformMatrix3fvARB;
extern PFNGLUNIFORMMATRIX4FVARBPROC blUniformMatrix4fvARB;
extern PFNGLGETOBJECTPARAMETERFVARBPROC blGetObjectParameterfvARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC blGetObjectParameterivARB;
extern PFNGLGETINFOLOGARBPROC blGetInfoLogARB;
extern PFNGLGETATTACHEDOBJECTSARBPROC blGetAttachedObjectsARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC blGetUniformLocationARB;
extern PFNGLGETACTIVEUNIFORMARBPROC blGetActiveUniformARB;
extern PFNGLGETUNIFORMFVARBPROC blGetUniformfvARB;
extern PFNGLGETUNIFORMIVARBPROC blGetUniformivARB;
extern PFNGLGETSHADERSOURCEARBPROC blGetShaderSourceARB;
#endif

#ifdef GL_ARB_vertex_shader
extern PFNGLBINDATTRIBLOCATIONARBPROC blBindAttribLocationARB;
extern PFNGLGETACTIVEATTRIBARBPROC blGetActiveAttribARB;
extern PFNGLGETATTRIBLOCATIONARBPROC blGetAttribLocationARB;
#endif

#ifdef GL_ARB_vertex_program
extern PFNGLVERTEXATTRIB1FARBPROC blVertexAttrib1fARB;
extern PFNGLVERTEXATTRIB1FVARBPROC blVertexAttrib1fvARB;
extern PFNGLVERTEXATTRIB2FARBPROC blVertexAttrib2fARB;
extern PFNGLVERTEXATTRIB2FVARBPROC blVertexAttrib2fvARB;
extern PFNGLVERTEXATTRIB3FARBPROC blVertexAttrib3fARB;
extern PFNGLVERTEXATTRIB3FVARBPROC blVertexAttrib3fvARB;
extern PFNGLVERTEXATTRIB4FARBPROC blVertexAttrib4fARB;
extern PFNGLVERTEXATTRIB4FVARBPROC blVertexAttrib4fvARB;
extern PFNGLGETPROGRAMSTRINGARBPROC blGetProgramStringARB;
extern PFNGLGETVERTEXATTRIBDVARBPROC blGetVertexAttribdvARB;
extern PFNGLGETVERTEXATTRIBFVARBPROC blGetVertexAttribfvARB;
extern PFNGLGETVERTEXATTRIBIVARBPROC blGetVertexAttribivARB;
#endif

/*
#ifdef GL_EXT_compiled_vertex_array
extern PFNGLLOCKARRAYSEXTPROC blLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC blUnlockArraysEXT;
#endif
*/
} /* namespace bgl */


#endif /* __RAS_GLEXTENSIONMANAGER_H__ */
