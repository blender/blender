# Install script for directory: D:/Download/Github/blender/code_blender/blender/extern

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
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/curve_fit_nd/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/rangetree/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/wcwidth/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/bullet2/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/glew/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/recastnavigation/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/libopenjpeg/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/lzo/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/lzma/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/clew/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/cuew/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/carve/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/ceres/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/gflags/cmake_install.cmake")
  include("D:/Download/Github/blender/code_blender/blender/out/build/x64-Debug/extern/glog/cmake_install.cmake")

endif()

