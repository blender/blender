
INSTALL(DIRECTORY license DESTINATION ${CYCLES_INSTALL_PATH}/cycles PATTERN ".svn" EXCLUDE)

SET(doc_sources
	index.html

	reference/camera.html
	reference/curve.html
	reference/devices.html
	reference/film.html
	reference/index.html
	reference/integrator.html
	reference/interactive.html
	reference/lamp.html
	reference/mesh.html
	reference/motion_blur.html
	reference/particle.html
	reference/subdivision.html
	reference/world.html

	reference/material/displacement.html
	reference/material/index.html
	reference/material/surface.html
	reference/material/volume.html

	reference/shader/background.html
	reference/shader/bsdf.html
	reference/shader/color_operations.html
	reference/shader/custom.html
	reference/shader/emission.html
	reference/shader/image_textures.html
	reference/shader/index.html
	reference/shader/input.html
	reference/shader/output.html
	reference/shader/procedural_textures.html
	reference/shader/vector_operations.html
	reference/shader/volume.html
	reference/shader/volume_textures.html)

SET(doc_extra
	reference/camera_ortho.svg
	reference/camera_persp.svg
	reference/material/material.svg
	reference/shader/bsdf.svg
	style/style.css)

SET(doc_templates
	templates/footer.html
	templates/header.html
	templates/nodes.html
	templates/reference.html)

IF(WITH_CYCLES_DOCS)
	MACRO(install_doc_file source_file html_file)
		GET_FILENAME_COMPONENT(subdir ${source_file} PATH)
		INSTALL(
			FILES ${html_file}
			DESTINATION ${CYCLES_INSTALL_PATH}/cycles/doc/${subdir})
	ENDMACRO()

	FOREACH(_file ${doc_sources})
		SET(source_file ${_file})
		SET(html_file ${CMAKE_CURRENT_BINARY_DIR}/${_file})
		ADD_CUSTOM_COMMAND(
			OUTPUT ${html_file} 
			COMMAND python generate.py ${source_file} ${html_file} ${CYCLES_VERSION}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			DEPENDS ${source_file} ${doc_templates} generate.py)
		LIST(APPEND html_files ${html_file})
		install_doc_file(${source_file} ${html_file})
	ENDFOREACH()

	FOREACH(_file ${doc_extra})
		install_doc_file(${_file} ${_file})
	ENDFOREACH()

	ADD_CUSTOM_TARGET(cycles_doc ALL DEPENDS ${html_files})
ENDIF()

