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

#ifndef APIENTRY
#define APIENTRY
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
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
extern PFNGLPNTRIANGLESIATIPROC glPNTrianglesiATI;
extern PFNGLPNTRIANGLESFATIPROC glPNTrianglesfATI;
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
		_EXT_texture3D(0)
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
}BL_EXTInfo;

extern BL_EXTInfo RAS_EXT_support;


#ifdef GL_EXT_blend_color
typedef void (APIENTRY* PFNGLBLENDCOLOREXTPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
extern PFNGLBLENDCOLOREXTPROC glBlendColorEXT;
#endif

#ifdef GL_ARB_multitexture

extern int max_texture_units;
typedef void (APIENTRY * PFNGLACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (APIENTRY * PFNGLCLIENTACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1DARBPROC) (GLenum target, GLdouble s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1FARBPROC) (GLenum target, GLfloat s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1IARBPROC) (GLenum target, GLint s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1SARBPROC) (GLenum target, GLshort s);
typedef void (APIENTRY * PFNGLMULTITEXCOORD1SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2DARBPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2FARBPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2IARBPROC) (GLenum target, GLint s, GLint t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2SARBPROC) (GLenum target, GLshort s, GLshort t);
typedef void (APIENTRY * PFNGLMULTITEXCOORD2SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3IARBPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (APIENTRY * PFNGLMULTITEXCOORD3SVARBPROC) (GLenum target, const GLshort *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4DVARBPROC) (GLenum target, const GLdouble *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4FVARBPROC) (GLenum target, const GLfloat *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4IARBPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4IVARBPROC) (GLenum target, const GLint *v);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (APIENTRY * PFNGLMULTITEXCOORD4SVARBPROC) (GLenum target, const GLshort *v);


extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB;
extern PFNGLMULTITEXCOORD1DARBPROC glMultiTexCoord1dARB;
extern PFNGLMULTITEXCOORD1DVARBPROC glMultiTexCoord1dvARB;
extern PFNGLMULTITEXCOORD1FARBPROC glMultiTexCoord1fARB;
extern PFNGLMULTITEXCOORD1FVARBPROC glMultiTexCoord1fvARB;
extern PFNGLMULTITEXCOORD1IARBPROC glMultiTexCoord1iARB;
extern PFNGLMULTITEXCOORD1IVARBPROC glMultiTexCoord1ivARB;
extern PFNGLMULTITEXCOORD1SARBPROC glMultiTexCoord1sARB;
extern PFNGLMULTITEXCOORD1SVARBPROC glMultiTexCoord1svARB;
extern PFNGLMULTITEXCOORD2DARBPROC glMultiTexCoord2dARB;
extern PFNGLMULTITEXCOORD2DVARBPROC glMultiTexCoord2dvARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
extern PFNGLMULTITEXCOORD2IARBPROC glMultiTexCoord2iARB;
extern PFNGLMULTITEXCOORD2IVARBPROC glMultiTexCoord2ivARB;
extern PFNGLMULTITEXCOORD2SARBPROC glMultiTexCoord2sARB;
extern PFNGLMULTITEXCOORD2SVARBPROC glMultiTexCoord2svARB;
extern PFNGLMULTITEXCOORD3DARBPROC glMultiTexCoord3dARB;
extern PFNGLMULTITEXCOORD3DVARBPROC glMultiTexCoord3dvARB;
extern PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3fARB;
extern PFNGLMULTITEXCOORD3FVARBPROC glMultiTexCoord3fvARB;
extern PFNGLMULTITEXCOORD3IARBPROC glMultiTexCoord3iARB;
extern PFNGLMULTITEXCOORD3IVARBPROC glMultiTexCoord3ivARB;
extern PFNGLMULTITEXCOORD3SARBPROC glMultiTexCoord3sARB;
extern PFNGLMULTITEXCOORD3SVARBPROC glMultiTexCoord3svARB;
extern PFNGLMULTITEXCOORD4DARBPROC glMultiTexCoord4dARB;
extern PFNGLMULTITEXCOORD4DVARBPROC glMultiTexCoord4dvARB;
extern PFNGLMULTITEXCOORD4FARBPROC glMultiTexCoord4fARB;
extern PFNGLMULTITEXCOORD4FVARBPROC glMultiTexCoord4fvARB;
extern PFNGLMULTITEXCOORD4IARBPROC glMultiTexCoord4iARB;
extern PFNGLMULTITEXCOORD4IVARBPROC glMultiTexCoord4ivARB;
extern PFNGLMULTITEXCOORD4SARBPROC glMultiTexCoord4sARB;
extern PFNGLMULTITEXCOORD4SVARBPROC glMultiTexCoord4svARB;
#endif


#ifdef GL_ARB_shader_objects
typedef char GLcharARB;
typedef unsigned int GLhandleARB;
typedef void (APIENTRY*  PFNGLDELETEOBJECTARBPROC) (GLhandleARB obj);
typedef GLhandleARB (APIENTRY*  PFNGLGETHANDLEARBPROC) (GLenum pname);
typedef void (APIENTRY*  PFNGLDETACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB attachedObj);
typedef GLhandleARB (APIENTRY*  PFNGLCREATESHADEROBJECTARBPROC) (GLenum shaderType);
typedef void (APIENTRY*  PFNGLSHADERSOURCEARBPROC) (GLhandleARB shaderObj, GLsizei count, const GLcharARB* *string, const GLint *length);
typedef void (APIENTRY*  PFNGLCOMPILESHADERARBPROC) (GLhandleARB shaderObj);
typedef GLhandleARB (APIENTRY*  PFNGLCREATEPROGRAMOBJECTARBPROC) (void);
typedef void (APIENTRY*  PFNGLATTACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB obj);
typedef void (APIENTRY*  PFNGLLINKPROGRAMARBPROC) (GLhandleARB programObj);
typedef void (APIENTRY*  PFNGLUSEPROGRAMOBJECTARBPROC) (GLhandleARB programObj);
typedef void (APIENTRY*  PFNGLVALIDATEPROGRAMARBPROC) (GLhandleARB programObj);
typedef void (APIENTRY*  PFNGLUNIFORM1FARBPROC) (GLint location, GLfloat v0);
typedef void (APIENTRY*  PFNGLUNIFORM2FARBPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY*  PFNGLUNIFORM3FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY*  PFNGLUNIFORM4FARBPROC) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (APIENTRY*  PFNGLUNIFORM1IARBPROC) (GLint location, GLint v0);
typedef void (APIENTRY*  PFNGLUNIFORM2IARBPROC) (GLint location, GLint v0, GLint v1);
typedef void (APIENTRY*  PFNGLUNIFORM3IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2);
typedef void (APIENTRY*  PFNGLUNIFORM4IARBPROC) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
typedef void (APIENTRY*  PFNGLUNIFORM1FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY*  PFNGLUNIFORM2FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY*  PFNGLUNIFORM3FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY*  PFNGLUNIFORM4FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRY*  PFNGLUNIFORM1IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRY*  PFNGLUNIFORM2IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRY*  PFNGLUNIFORM3IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRY*  PFNGLUNIFORM4IVARBPROC) (GLint location, GLsizei count, const GLint *value);
typedef void (APIENTRY*  PFNGLUNIFORMMATRIX2FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRY*  PFNGLUNIFORMMATRIX3FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRY*  PFNGLUNIFORMMATRIX4FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRY*  PFNGLGETOBJECTPARAMETERFVARBPROC) (GLhandleARB obj, GLenum pname, GLfloat *params);
typedef void (APIENTRY*  PFNGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint *params);
typedef void (APIENTRY*  PFNGLGETINFOLOGARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
typedef void (APIENTRY*  PFNGLGETATTACHEDOBJECTSARBPROC) (GLhandleARB containerObj, GLsizei maxCount, GLsizei *count, GLhandleARB *obj);
typedef GLint (APIENTRY*  PFNGLGETUNIFORMLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB *name);
typedef void (APIENTRY*  PFNGLGETACTIVEUNIFORMARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
typedef void (APIENTRY*  PFNGLGETUNIFORMFVARBPROC) (GLhandleARB programObj, GLint location, GLfloat *params);
typedef void (APIENTRY*  PFNGLGETUNIFORMIVARBPROC) (GLhandleARB programObj, GLint location, GLint *params);
typedef void (APIENTRY*  PFNGLGETSHADERSOURCEARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *source);

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

#ifdef GL_ARB_vertex_shader
typedef void (APIENTRY*  PFNGLBINDATTRIBLOCATIONARBPROC) (GLhandleARB programObj, GLuint index, const GLcharARB *name);
typedef void (APIENTRY*  PFNGLGETACTIVEATTRIBARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
typedef GLint (APIENTRY*  PFNGLGETATTRIBLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB *name);

extern PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
extern PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
extern PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;
#endif

} /* namespace bgl */


#endif /* __RAS_GLEXTENSIONMANAGER_H__ */
