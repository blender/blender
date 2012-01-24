
# OSL node shaders

set(SRC_OSL
	node_add_closure.osl
	node_attribute.osl
	node_background.osl
	node_blend_texture.osl
	node_bump.osl
	node_camera.osl
	node_clouds_texture.osl
	node_convert_from_color.osl
	node_convert_from_float.osl
	node_convert_from_normal.osl
	node_convert_from_point.osl
	node_convert_from_vector.osl
	node_diffuse_bsdf.osl
	node_distorted_noise_texture.osl
	node_emission.osl
	node_environment_texture.osl
	node_fresnel.osl
	node_gamma.osl
	node_brightness.osl
	node_geometry.osl
	node_glass_bsdf.osl
	node_glossy_bsdf.osl
	node_hsv.osl
	node_image_texture.osl
	node_invert.osl
	node_light_path.osl
	node_magic_texture.osl
	node_mapping.osl
	node_marble_texture.osl
	node_math.osl
	node_mix.osl
	node_mix_closure.osl
	node_musgrave_texture.osl
	node_normal.osl
	node_blend_weight_texture.osl
	node_noise_texture.osl
	node_output_displacement.osl
	node_output_surface.osl
	node_output_volume.osl
	node_sepcomb_rgb.osl
	node_sky_texture.osl
	node_stucci_texture.osl
	node_texture_coordinate.osl
	node_translucent_bsdf.osl
	node_transparent_bsdf.osl
	node_value.osl
	node_vector_math.osl
	node_velvet_bsdf.osl
	node_voronoi_texture.osl
	node_ward_bsdf.osl
	node_wood_texture.osl
)

set(SRC_OSL_HEADERS
	node_texture.h
	node_color.h
	node_fresnel.h
	stdosl.h
)

set(SRC_OSO

)

# TODO, add a module to compile OSL
foreach(_file ${SRC_OSL})
	set(_OSL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${_file})
	string(REPLACE ".osl" ".oso" _OSO_FILE ${_OSL_FILE})  # TODO, replace extension only
	string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} _OSO_FILE ${_OSO_FILE})
	add_custom_command(
		OUTPUT ${_OSO_FILE} 
		COMMAND ${OSL_COMPILER} -O2 ${_OSL_FILE}
		DEPENDS ${_OSL_FILE} ${SRC_OSL_HEADERS})
	list(APPEND SRC_OSO
		${_OSO_FILE}
	)
	
	unset(_OSL_FILE)
	unset(_OSO_FILE)
endforeach()

add_custom_target(shader ALL DEPENDS ${SRC_OSO} ${SRC_OSL_HEADERS})

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${SRC_OSO}" ${CYCLES_INSTALL_PATH}/shader)
