
# OSL node shaders

set(osl_sources
	node_add_closure.osl
	node_attribute.osl
	node_background.osl
	node_blend_texture.osl
	node_bump.osl
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
	node_geometry.osl
	node_glass_bsdf.osl
	node_glossy_bsdf.osl
	node_image_texture.osl
	node_light_path.osl
	node_magic_texture.osl
	node_mapping.osl
	node_marble_texture.osl
	node_math.osl
	node_mix.osl
	node_mix_closure.osl
	node_musgrave_texture.osl
	node_blend_weight_texture.osl
	node_noise_texture.osl
	node_output_closure.osl
	node_output_displacement.osl
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
	node_wood_texture.osl)

set(osl_headers
	node_texture.h
	stdosl.h)

set(oso_sources)

foreach(_file ${osl_sources})
	set(osl_file ${CMAKE_CURRENT_SOURCE_DIR}/${_file})
	string(REPLACE ".osl" ".oso" oso_file ${osl_file})
	string(REPLACE ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} oso_file ${oso_file})
	add_custom_command(
		OUTPUT ${oso_file} 
		COMMAND ${OSL_COMPILER} -O2 ${osl_file}
		DEPENDS ${osl_file} ${osl_headers})
	list(APPEND oso_sources ${oso_file})
endforeach()

add_custom_target(shader ALL DEPENDS ${oso_sources} ${osl_headers})

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${oso_sources}" ${CYCLES_INSTALL_PATH}/shader)

