set(PROJECT_DESCRIPTION  "Blender is a very fast and versatile 3D modeller/renderer.")
set(PROJECT_COPYRIGHT    "Copyright (C) 2001-2012 Blender Foundation")
set(PROJECT_CONTACT      "foundation@blender.org")
set(PROJECT_VENDOR       "Blender Foundation")

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
set(MY_WC_HASH "unknown")
if(EXISTS ${CMAKE_SOURCE_DIR}/.git/)
	include(FindGit)
	if(GIT_FOUND)
		message(STATUS "-- Found Git: ${GIT_EXECUTABLE}")
		execute_process(COMMAND git rev-parse --short @{u}
		                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		                OUTPUT_VARIABLE MY_WC_HASH
		                OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
endif()
set(BUILD_REV ${MY_WC_HASH})
unset(MY_WC_HASH)


# Force Package Name
execute_process(COMMAND date "+%Y%m%d" OUTPUT_VARIABLE CPACK_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}-git${CPACK_DATE}.${BUILD_REV}-${CMAKE_SYSTEM_PROCESSOR})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	# RPM packages
	include(build_files/cmake/RpmBuild.cmake)
	if(RPMBUILD_FOUND)
		set(CPACK_GENERATOR "RPM")
		set(CPACK_RPM_PACKAGE_RELEASE "git${CPACK_DATE}.${BUILD_REV}")
		set(CPACK_SET_DESTDIR "true")
		set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
		set(CPACK_PACKAGE_RELOCATABLE "false")
		set(CPACK_RPM_PACKAGE_LICENSE "GPLv2+ and Apache 2.0")
		set(CPACK_RPM_PACKAGE_GROUP "Amusements/Multimedia")
		set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_SOURCE_DIR}/build_files/package_spec/rpm/blender.spec.in")
	endif()
endif()

# Mac Bundle
if(APPLE)
	set(CPACK_GENERATOR "DragNDrop")

	# Libraries are bundled directly
	set(CPACK_COMPONENT_LIBRARIES_HIDDEN TRUE)
	set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/release/darwin/background.png")
	set(CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/release/darwin/SET_DS_Store")
endif()

if(WIN32)
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "Blender Foundation/Blender")
	set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "Blender Foundation/Blender")

	set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/source/icons/winblender.ico)
	set(CPACK_NSIS_COMPRESSOR "/SOLID lzma")

	set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/release/text/GPL-license.txt)
	set(CPACK_WIX_PRODUCT_ICON ${CMAKE_SOURCE_DIR}/source/icons/winblender.ico)
	set(CPACK_WIX_UPGRADE_GUID "B767E4FD-7DE7-4094-B051-3AE62E13A17A")

	set(CPACK_WIX_UI_BANNER ${LIBDIR}/package/installer_wix/WIX_UI_BANNER.bmp)
	set(CPACK_WIX_UI_DIALOG ${LIBDIR}/package/installer_wix/WIX_UI_DIALOG.bmp)

	#force lzma instead of deflate
	set(CPACK_WIX_LIGHT_EXTRA_FLAGS -dcl:high)
endif()

set(CPACK_PACKAGE_EXECUTABLES "blender" "blender")
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
	unset(build_archive)
	unset(package_output)
endmacro()

if(APPLE)
	add_package_archive(
		"${PROJECT_NAME}-${BLENDER_VERSION}-${BUILD_REV}-OSX-${CMAKE_OSX_ARCHITECTURES}"
		"zip")
elseif(UNIX)
	# platform name could be tweaked, to include glibc, and ensure processor is correct (i386 vs i686)
	string(TOLOWER ${CMAKE_SYSTEM_NAME} PACKAGE_SYSTEM_NAME)

	add_package_archive(
		"${PROJECT_NAME}-${BLENDER_VERSION}-${BUILD_REV}-${PACKAGE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
		"tar.bz2")
endif()

