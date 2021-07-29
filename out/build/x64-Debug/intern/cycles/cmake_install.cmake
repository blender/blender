# Install script for directory: D:/Download/Github/blender/code_blender/blender/intern/cycles

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

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/blender/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/bvh/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/device/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/doc/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/graph/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/kernel/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/render/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/subd/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/intern/cycles/util/cmake_install.cmake")

endif()

