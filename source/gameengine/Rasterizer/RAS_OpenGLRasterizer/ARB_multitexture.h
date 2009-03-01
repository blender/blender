#ifndef __ARB_MULTITEXTURE_H__
#define __ARB_MULTITEXTURE_H__

/* 
*/

/* ----------------------------------------------------------------------------
	GL_ARB_multitexture
---------------------------------------------------------------------------- */
#ifdef GL_ARB_multitexture
	#define GL_TEXTURE0_ARB                   0x84C0
	#define GL_TEXTURE1_ARB                   0x84C1
	#define GL_TEXTURE2_ARB                   0x84C2
	#define GL_TEXTURE3_ARB                   0x84C3
	#define GL_TEXTURE4_ARB                   0x84C4
	#define GL_TEXTURE5_ARB                   0x84C5
	#define GL_TEXTURE6_ARB                   0x84C6
	#define GL_TEXTURE7_ARB                   0x84C7
	#define GL_TEXTURE8_ARB                   0x84C8
	#define GL_TEXTURE9_ARB                   0x84C9
	#define GL_TEXTURE10_ARB                  0x84CA
	#define GL_ACTIVE_TEXTURE_ARB             0x84E0
	#define GL_CLIENT_ACTIVE_TEXTURE_ARB      0x84E1
	#define GL_MAX_TEXTURE_UNITS_ARB          0x84E2
#endif


/* ----------------------------------------------------------------------------
	GL_ARB_texture_env_combine
---------------------------------------------------------------------------- */
#ifdef GL_ARB_texture_env_combine
	#define GL_COMBINE_ARB                    0x8570
	#define GL_COMBINE_RGB_ARB                0x8571
	#define GL_COMBINE_ALPHA_ARB              0x8572
	#define GL_SOURCE0_RGB_ARB                0x8580
	#define GL_SOURCE1_RGB_ARB                0x8581
	#define GL_SOURCE2_RGB_ARB                0x8582
	#define GL_SOURCE0_ALPHA_ARB              0x8588
	#define GL_SOURCE1_ALPHA_ARB              0x8589
	#define GL_SOURCE2_ALPHA_ARB              0x858A
	#define GL_OPERAND0_RGB_ARB               0x8590
	#define GL_OPERAND1_RGB_ARB               0x8591
	#define GL_OPERAND2_RGB_ARB               0x8592
	#define GL_OPERAND0_ALPHA_ARB             0x8598
	#define GL_OPERAND1_ALPHA_ARB             0x8599
	#define GL_OPERAND2_ALPHA_ARB             0x859A
	#define GL_RGB_SCALE_ARB                  0x8573
	#define GL_ADD_SIGNED_ARB                 0x8574
	#define GL_INTERPOLATE_ARB                0x8575
	#define GL_SUBTRACT_ARB                   0x84E7
	#define GL_CONSTANT_ARB                   0x8576
	#define GL_PRIMARY_COLOR_ARB              0x8577
	#define GL_PREVIOUS_ARB                   0x8578
#endif

/* ----------------------------------------------------------------------------
	GL_ARB_texture_cube_map
---------------------------------------------------------------------------- */
#ifdef GL_ARB_texture_cube_map
	#define GL_NORMAL_MAP_ARB                 0x8511
	#define GL_REFLECTION_MAP_ARB             0x8512
	#define GL_TEXTURE_CUBE_MAP_ARB           0x8513
	#define GL_TEXTURE_BINDING_CUBE_MAP_ARB   0x8514
	#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 0x8515
	#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB 0x8516
	#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB 0x8517
	#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB 0x8518
	#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB 0x8519
	#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB 0x851A
	#define GL_PROXY_TEXTURE_CUBE_MAP_ARB     0x851B
	#define GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB  0x851C
#endif

/* ----------------------------------------------------------------------------
	GL_ARB_shader_objects
---------------------------------------------------------------------------- */
#ifdef GL_ARB_shader_objects
	#define GL_PROGRAM_OBJECT_ARB             0x8B40
	#define GL_SHADER_OBJECT_ARB              0x8B48
	#define GL_OBJECT_TYPE_ARB                0x8B4E
	#define GL_OBJECT_SUBTYPE_ARB             0x8B4F
	#define GL_FLOAT_VEC2_ARB                 0x8B50
	#define GL_FLOAT_VEC3_ARB                 0x8B51
	#define GL_FLOAT_VEC4_ARB                 0x8B52
	#define GL_INT_VEC2_ARB                   0x8B53
	#define GL_INT_VEC3_ARB                   0x8B54
	#define GL_INT_VEC4_ARB                   0x8B55
	#define GL_BOOL_ARB                       0x8B56
	#define GL_BOOL_VEC2_ARB                  0x8B57
	#define GL_BOOL_VEC3_ARB                  0x8B58
	#define GL_BOOL_VEC4_ARB                  0x8B59
	#define GL_FLOAT_MAT2_ARB                 0x8B5A
	#define GL_FLOAT_MAT3_ARB                 0x8B5B
	#define GL_FLOAT_MAT4_ARB                 0x8B5C
	#define GL_SAMPLER_1D_ARB                 0x8B5D
	#define GL_SAMPLER_2D_ARB                 0x8B5E
	#define GL_SAMPLER_3D_ARB                 0x8B5F
	#define GL_SAMPLER_CUBE_ARB               0x8B60
	#define GL_SAMPLER_1D_SHADOW_ARB          0x8B61
	#define GL_SAMPLER_2D_SHADOW_ARB          0x8B62
	#define GL_SAMPLER_2D_RECT_ARB            0x8B63
	#define GL_SAMPLER_2D_RECT_SHADOW_ARB     0x8B64
	#define GL_OBJECT_DELETE_STATUS_ARB       0x8B80
	#define GL_OBJECT_COMPILE_STATUS_ARB      0x8B81
	#define GL_OBJECT_LINK_STATUS_ARB         0x8B82
	#define GL_OBJECT_VALIDATE_STATUS_ARB     0x8B83
	#define GL_OBJECT_INFO_LOG_LENGTH_ARB     0x8B84
	#define GL_OBJECT_ATTACHED_OBJECTS_ARB    0x8B85
	#define GL_OBJECT_ACTIVE_UNIFORMS_ARB     0x8B86
	#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB 0x8B87
	#define GL_OBJECT_SHADER_SOURCE_LENGTH_ARB 0x8B88
#endif

/* ----------------------------------------------------------------------------
	GL_ARB_vertex_shader
---------------------------------------------------------------------------- */
#ifdef GL_ARB_vertex_shader
	#define GL_VERTEX_SHADER_ARB              0x8B31
	#define GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB 0x8B4A
	#define GL_MAX_VARYING_FLOATS_ARB         0x8B4B
	#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS_ARB 0x8B4C
	#define GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB 0x8B4D
	#define GL_OBJECT_ACTIVE_ATTRIBUTES_ARB   0x8B89
	#define GL_OBJECT_ACTIVE_ATTRIBUTE_MAX_LENGTH_ARB 0x8B8A
#endif


/* ----------------------------------------------------------------------------
	GL_ARB_fragment_shader
---------------------------------------------------------------------------- */
#ifdef GL_ARB_fragment_shader
	#define GL_FRAGMENT_SHADER_ARB            0x8B30
	#define GL_MAX_FRAGMENT_UNIFORM_COMPONENTS_ARB 0x8B49
	#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT_ARB 0x8B8B
#endif


/* ----------------------------------------------------------------------------
	GL_ARB_depth_texture
---------------------------------------------------------------------------- */
#ifndef GL_ARB_depth_texture
	#define GL_DEPTH_COMPONENT16_ARB          0x81A5
	#define GL_DEPTH_COMPONENT24_ARB          0x81A6
	#define GL_DEPTH_COMPONENT32_ARB          0x81A7
	#define GL_TEXTURE_DEPTH_SIZE_ARB         0x884A
	#define GL_DEPTH_TEXTURE_MODE_ARB         0x884B
#endif


#endif//__ARB_MULTITEXTURE_H__
