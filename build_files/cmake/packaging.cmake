set(PROJECT_DESCRIPTION  "Blender is a very fast and versatile 3D modeller/renderer.")
set(PROJECT_COPYRIGHT    "Copyright (C) 2001-2012 Blender Foundation")
set(PROJECT_CONTACT      "foundation@blender.org")
set(PROJECT_VENDOR       "Blender Foundation")
set(ORG_WEBSITE          "www.blender.org")

set(MAJOR_VERSION ${BLENDER_VERSION_MAJOR})
set(MINOR_VERSION ${BLENDER_VERSION_MINOR})
set(PATCH_VERSION ${BLENDER_VERSION_CHAR_INDEX})

set(CPACK_SYSTEM_NAME ${CMAKE_SYSTEM_NAME})
set(CPACK_PACKAGE_DESCRIPTION ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR ${PROJECT_VENDOR})
set(CPACK_PACKAGE_CONTACT ${PROJECT_CONTACT})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
SET(CPACK_PACKAGE_VERSION_MAJOR "${MAJOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_MINOR "${MINOR_VERSION}")
SET(CPACK_PACKAGE_VERSION_PATCH "${PATCH_VERSION}")


# Get the build revision, note that this can get out-of-sync, so for packaging run cmake first.
include(FindSubversion)
set(MY_WC_REVISION "unknown")
if(EXISTS ${CMAKE_SOURCE_DIR}/.svn/)
	if(Subversion_FOUND)
		Subversion_WC_INFO(${CMAKE_SOURCE_DIR} MY)
	endif()
endif()
set(BUILD_REV ${MY_WC_REVISION})


# Force Package Name
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}-1.r${BUILD_REV}-${CMAKE_SYSTEM_PROCESSOR})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	# RPM packages
	include(build_files/cmake/RpmBuild.cmake)
	if(RPMBUILD_FOUND AND NOT WIN32)
		set(CPACK_GENERATOR "RPM")
		set(CPACK_RPM_PACKAGE_RELEASE "1.r${BUILD_REV}")
		set(CPACK_SET_DESTDIR "true")
		set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
		set(CPACK_PACKAGE_RELOCATABLE "false")
		set(CPACK_RPM_PACKAGE_LICENSE "GPLv2")
		set(CPACK_RPM_PACKAGE_GROUP "Amusements/Multimedia")
		set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_SOURCE_DIR}/build_files/package_spec/rpm/blender.spec.in")
	endif()
endif()

# Mac Bundle
if(APPLE)
	set(CPACK_GENERATOR "DragNDrop")

	# Libraries are bundled directly
	set(CPACK_COMPONENT_LIBRARIES_HIDDEN TRUE)
endif()

set(CPACK_PACKAGE_EXECUTABLES "blender")
include(CPack)

# Target for build_archive.py script, to automatically pass along
# version, revision, platform, build directory
macro(add_package_archive packagename extension)
	set(build_archive python ${CMAKE_SOURCE_DIR}/build_files/package_spec/build_archive.py)
	set(package_output ${CMAKE_BINARY_DIR}/release/${packagename}.${extension})

	add_custom_target(package_archive DEPENDS ${package_output})

	add_custom_command(
		OUTPUT ${package_output}
		COMMAND ${build_archive} ${packagename} ${extension} bin release
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endmacro()

if(APPLE)
	add_package_archive(
		"${PROJECT_NAME}-${BLENDER_VERSION}-r${BUILD_REV}-OSX-${CMAKE_OSX_ARCHITECTURES}"
		"zip")
elseif(UNIX)
	# platform name could be tweaked, to include glibc, and ensure processor is correct (i386 vs i686)
	string(TOLOWER ${CMAKE_SYSTEM_NAME} PACKAGE_SYSTEM_NAME)

	add_package_archive(
		"${PROJECT_NAME}-${BLENDER_VERSION}-r${BUILD_REV}-${PACKAGE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
		"tar.bz2")
endif()

