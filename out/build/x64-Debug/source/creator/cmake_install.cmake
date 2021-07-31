# Install script for directory: D:/Download/Github/blender/code_blender/blender/source/creator

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/Download/Github/blender/code_blender/blender/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(REMOVE_RECURSE 2.79)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79" TYPE DIRECTORY FILES "D:/Download/Github/blender/code_blender/blender/release/scripts" REGEX "/\\.git$" EXCLUDE REGEX "/\\.gitignore$" EXCLUDE REGEX "/\\.arcconfig$" EXCLUDE REGEX "/\\_\\_pycache\\_\\_$" EXCLUDE REGEX "/addons\\_contrib\\/[^/]*$" EXCLUDE REGEX "/\\_freestyle\\/[^/]*$" EXCLUDE)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/datafiles" TYPE DIRECTORY FILES "D:/Download/Github/blender/code_blender/blender/release/datafiles/colormanagement")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee]|[Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo]|[Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/python/lib/python35.dll")
  endif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee]|[Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo]|[Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/python/lib/python35_d.dll")
  endif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/python" TYPE DIRECTORY FILES "")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/python/lib" TYPE DIRECTORY FILES "")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
				message(STATUS "Extracting Python to: ${CMAKE_INSTALL_PREFIX}/2.79/python")
				if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
					set(PYTHON_ZIP "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/release/python35_d.tar.gz")
				else()
					set(PYTHON_ZIP "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/release/python35.tar.gz")
				endif()

				execute_process(
					COMMAND ${CMAKE_COMMAND} -E make_directory
					        "${CMAKE_INSTALL_PREFIX}/2.79/python"
					COMMAND ${CMAKE_COMMAND} -E
					        chdir "${CMAKE_INSTALL_PREFIX}/2.79/python"
					        ${CMAKE_COMMAND} -E
					        tar xzfv "${PYTHON_ZIP}"
				)
				unset(PYTHON_ZIP)
				
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/python/lib" TYPE DIRECTORY FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/release/site-packages" REGEX "/\\.svn$" EXCLUDE REGEX "/\\_\\_pycache\\_\\_$" EXCLUDE REGEX "/[^/]*\\.pyc$" EXCLUDE REGEX "/[^/]*\\.pyo$" EXCLUDE)
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/python/lib/site-packages" TYPE DIRECTORY FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/source/creator/2.79/python/lib/site-packages/numpy")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee]|[Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo]|[Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/python/bin" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/python/lib/python35.dll")
  endif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee]|[Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo]|[Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/python/bin" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/python/lib/python35_d.dll")
  endif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/pthreads/lib/pthreadVC2.dll")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES
    "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/ffmpeg/lib/avcodec-57.dll"
    "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/ffmpeg/lib/avformat-57.dll"
    "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/ffmpeg/lib/avdevice-57.dll"
    "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/ffmpeg/lib/avutil-55.dll"
    "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/ffmpeg/lib/swscale-4.dll"
    "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/ffmpeg/lib/swresample-2.dll"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/openal/lib/OpenAL32.dll")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/sdl/lib/SDL2.dll")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/thumbhandler/lib/BlendThumb64.dll")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/../lib/win64_vc12/opencolorio/bin/OpenColorIO.dll")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  
		file(READ "D:/Download/Github/blender/code_blender/blender/release/text/readme.html" DATA_SRC)
		string(REGEX REPLACE "BLENDER_VERSION" "2.79" DATA_DST "${DATA_SRC}")
		file(WRITE "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/release/text/readme.html" "${DATA_DST}")
		unset(DATA_SRC)
		unset(DATA_DST)
		
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE FILE FILES
    "D:/Download/Github/blender/code_blender/blender/release/text/GPL-license.txt"
    "D:/Download/Github/blender/code_blender/blender/release/text/GPL3-license.txt"
    "D:/Download/Github/blender/code_blender/blender/release/text/Python-license.txt"
    "D:/Download/Github/blender/code_blender/blender/release/text/copyright.txt"
    "D:/Download/Github/blender/code_blender/blender/release/text/jemalloc-license.txt"
    "D:/Download/Github/blender/code_blender/blender/release/text/ocio-license.txt"
    "D:/Download/Github/blender/code_blender/blender/release/datafiles/LICENSE-bfont.ttf.txt"
    "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/release/text/readme.html"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/__init__.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/engine.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/osl.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/presets.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/properties.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/ui.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/blender/addon/version_update.py")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/license" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/doc/license/Apache_2.0.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/license" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/doc/license/ILM.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/license" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/doc/license/NVidia.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/license" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/doc/license/OSL.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/license" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/doc/license/Sobol.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/license" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/doc/license/readme.txt")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_add_closure.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_ambient_occlusion.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_anisotropic_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_attribute.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_background.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_brick_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_brightness.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_bump.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_camera.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_checker_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_combine_rgb.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_combine_hsv.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_combine_xyz.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_convert_from_color.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_convert_from_float.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_convert_from_int.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_convert_from_normal.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_convert_from_point.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_convert_from_vector.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_diffuse_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_emission.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_environment_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_fresnel.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_gamma.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_geometry.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_glass_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_glossy_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_gradient_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_hair_info.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_scatter_volume.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_absorption_volume.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_holdout.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_hsv.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_image_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_invert.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_layer_weight.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_light_falloff.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_light_path.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_magic_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_mapping.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_math.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_mix.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_mix_closure.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_musgrave_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_noise_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_normal.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_normal_map.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_object_info.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_output_displacement.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_output_surface.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_output_volume.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_particle_info.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_refraction_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_rgb_curves.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_rgb_ramp.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_separate_rgb.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_separate_hsv.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_separate_xyz.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_set_normal.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_sky_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_subsurface_scattering.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_tangent.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_texture_coordinate.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_toon_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_translucent_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_transparent_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_value.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_vector_curves.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_vector_math.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_vector_transform.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_velvet_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_voronoi_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_voxel_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_wavelength.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_blackbody.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_wave_texture.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_wireframe.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_hair_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_uv_map.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_principled_bsdf.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/shaders/node_rgb_to_bw.oso")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/shaders/node_color.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/shaders/node_fresnel.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/shaders/node_ramp_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/shaders/node_texture.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/shaders/stdosl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/shader" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/shaders/oslutil.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_state_buffer_size.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_split.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_data_init.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_path_init.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_queue_enqueue.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_scene_intersect.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_lamp_emission.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_do_volume.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_indirect_background.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_shader_setup.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_shader_sort.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_shader_eval.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_holdout_emission_blurring_pathtermination_ao.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_subsurface_scatter.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_direct_lighting.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_shadow_blocked_ao.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_shadow_blocked_dl.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_enqueue_inactive.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_next_iteration_setup.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_indirect_subsurface.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_buffer_update.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/kernel_split_function.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/opencl" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/opencl/filter.cl")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/cuda" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/cuda/kernel.cu")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/cuda" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/cuda/kernel_split.cu")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/cuda" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/cuda/filter.cu")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_accumulate.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_bake.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_camera.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_compat_cpu.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_compat_cuda.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_compat_opencl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_debug.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_differential.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_emission.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_film.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_globals.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_image_opencl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_jitter.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_light.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_math.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_montecarlo.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_passes.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path_branched.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path_common.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path_state.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path_surface.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path_subsurface.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_path_volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_projection.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_queues.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_random.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_shader.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_shadow.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_subsurface.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_textures.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_types.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernel_work_stealing.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/kernels/cuda" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/kernels/cuda/kernel_config.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_nodes.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_shadow_all.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_subsurface.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_traversal.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_types.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/bvh_volume_all.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/qbvh_nodes.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/qbvh_shadow_all.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/qbvh_subsurface.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/qbvh_traversal.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/qbvh_volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/bvh" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/bvh/qbvh_volume_all.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/alloc.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_ashikhmin_velvet.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_diffuse.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_diffuse_ramp.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_microfacet.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_microfacet_multi.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_microfacet_multi_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_oren_nayar.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_phong_ramp.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_reflection.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_refraction.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_toon.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_transparent.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_ashikhmin_shirley.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_hair.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bssrdf.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/emissive.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_principled_diffuse.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/closure" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/closure/bsdf_principled_sheen.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_defines.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_features.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_features_sse.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_kernel.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_nlm_cpu.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_nlm_gpu.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_prefilter.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_reconstruction.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_transform.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_transform_gpu.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/filter" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/filter/filter_transform_sse.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_attribute.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_blackbody.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_bump.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_camera.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_closure.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_convert.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_checker.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_color_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_brick.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_displace.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_fresnel.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_wireframe.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_wavelength.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_gamma.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_brightness.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_geometry.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_gradient.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_hsv.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_image.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_invert.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_light_path.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_magic.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_mapping.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_math.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_math_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_mix.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_musgrave.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_noise.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_noisetex.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_normal.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_ramp.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_ramp_util.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_sepcomb_hsv.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_sepcomb_vector.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_sky.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_tex_coord.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_texture.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_types.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_value.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_vector_transform.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_voronoi.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_voxel.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/svm" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/svm/svm_wave.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_attribute.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_curve.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_motion_curve.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_motion_triangle.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_motion_triangle_intersect.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_motion_triangle_shader.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_object.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_patch.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_primitive.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_subd_triangle.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_triangle.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_triangle_intersect.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/geom" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/geom/geom_volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_atomic.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_color.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_half.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_hash.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_fast.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_intersect.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_float2.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_float3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_float4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_int2.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_int3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_int4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_math_matrix.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_static_assert.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_transform.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_texture.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_float2.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_float2_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_float3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_float3_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_float4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_float4_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_int2.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_int2_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_int3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_int3_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_int4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_int4_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uchar2.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uchar2_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uchar3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uchar3_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uchar4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uchar4_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uint2.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uint2_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uint3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uint3_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uint4.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_uint4_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_vector3.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/util" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/../util/util_types_vector3_impl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_branched.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_buffer_update.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_data_init.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_direct_lighting.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_do_volume.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_enqueue_inactive.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_holdout_emission_blurring_pathtermination_ao.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_indirect_background.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_indirect_subsurface.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_lamp_emission.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_next_iteration_setup.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_path_init.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_queue_enqueue.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_scene_intersect.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_shader_setup.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_shader_sort.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_shader_eval.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_shadow_blocked_ao.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_shadow_blocked_dl.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_split_common.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_split_data.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_split_data_types.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/2.79/scripts/addons/cycles/source/kernel/split" TYPE FILE FILES "D:/Download/Github/blender/code_blender/blender/intern/cycles/kernel/split/kernel_subsurface_scatter.h")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xBlenderx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/." TYPE EXECUTABLE FILES "D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/bin/blender.exe")
endif()

