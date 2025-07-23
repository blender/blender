# Utility file to help define some vars required for the iOS build
# Defines the following
# IOSDEP_INCLUDES_STRING - String of include paths: "-I <include_path1> -I <include_path2> ..."
# IOSDEP_LIBRARIES_STRING - String of libraries: "<library1> <library2> ..."
# IOSDEP_LIBDIRS_STRING - String of library dirs: "-L <lib_path1> -L <lib_path2> ..."
# IOSDEP_DEFINES - List of defines: "-D <define1> -D <define2> etc."
 
set(ios_get_dependency_env_vars_debug_mode YES)
set(ios_get_dependency_env_vars_run_test NO)

macro(ios_build_string ios_item_list ios_item_string ios_item_prefix)
  foreach (ios_item IN ITEMS ${ios_item_list})
    string(APPEND ${ios_item_string} "${ios_item_prefix} ${ios_item} ")
    if (${ios_get_dependency_env_vars_debug_mode} STREQUAL YES)
      if (NOT EXISTS ${ios_item})
          message(WARNING "Could not find dir ${ios_item}")
      endif()
    endif()
  endforeach()
endmacro()

macro(ios_build_defines ios_item_list ios_item_defines ios_item_prefix)
  foreach (ios_item IN ITEMS ${ios_item_list})
    list(APPEND ${ios_item_defines} "${ios_item_prefix}${ios_item}")
  endforeach()
endmacro()

macro(ios_get_dependency_env_vars)
   if (NOT LIBDIR)
     message(FATAL_ERROR "LIBDIR must be specified when calling ios_get_dependency_env_vars()")
   endif()
   if (${ios_get_dependency_env_vars_debug_mode} STREQUAL YES)
     if (NOT EXISTS ${LIBDIR})
       message(FATAL_ERROR "Could not find directory LIBDIR=${LIBDIR}")
     endif()
   endif()

   foreach (dependency IN ITEMS ${ARGN})
    
    # Boost - Pretty sure this is not required, including for completeness
    if (${dependency} STREQUAL BOOST)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/boost/include/")
      list(APPEND IOSDEP_DEFINES_LIST "=${LIBDIR}/")
      file(GLOB IOSDEP_BOOST_LIBRARIES "${LIBDIR}/boost/lib/*.dylib")
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_BOOST_LIBRARIES})
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/boost/lib")
      unset(IOSDEP_BOOST_LIBRARIES)
    endif()
    
    # Deflate
    if (${dependency} STREQUAL DEFLATE)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/deflate/include/")
      list(APPEND IOSDEP_DEFINES_LIST "libdeflate_DIR=${LIBDIR}/deflate/lib/cmake/libdeflate")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/deflate/lib")
    endif()
    
    # Fmt
    if (${dependency} STREQUAL FMT)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/fmt/include/")
      list(APPEND IOSDEP_DEFINES_LIST "fmt_DIR=${LIBDIR}/fmt/lib/cmake/fmt")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/fmt/lib")
    endif()
    
    # Imath
    if (${dependency} STREQUAL IMATH)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/imath/include/")
      list(APPEND IOSDEP_DEFINES_LIST "Imath_DIR=${LIBDIR}/imath/lib/cmake/Imath")
      list(APPEND IOSDEP_DEFINES_LIST "Imath_ROOT=${LIBDIR}/imath")
      list(APPEND IOSDEP_DEFINES_LIST "Imath_LIBRARY=${LIBDIR}/imath/lib/libImath.dylib")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/imath/lib")
      file(GLOB IOSDEP_IMATH_LIBRARIES_DYLIB "${LIBDIR}/imath/lib/*.dylib")
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_IMATH_LIBRARIES_DYLIB})
      unset(IOSDEP_IMATH_LIBRARIES_DYLIB)
    endif()
    
    # JPEG
    if (${dependency} STREQUAL JPEG)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/jpeg/include/")
      list(APPEND IOSDEP_DEFINES_LIST "libjpeg-turbo_DIR=${LIBDIR}/jpeg/lib/cmake/libjpeg-turbo")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/jpeg/lib")
    endif()

    # LLVM
    # We cannot rely on FindLLVM.cmake as it will invoke llvm-config to set the appropriate env vars
    # llvm-config built for iOS will not run and will fail (silently)
    # If we redirect to the MacOS version the env vars will point at the MacOS version.
    # Therefore we patch the makefile to remove calls to FindLLVM and specify the LLVM env vars manually
    if (${dependency} STREQUAL LLVM)
      if (NOT LLVM_VERSION)
        message(FATAL_ERROR "LLVM_VERSION must be specified when calling ios_get_dependency_env_vars() for LLVM")
      endif()
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/llvm/include/")
      list(APPEND IOSDEP_DEFINES_LIST
        "LLVM_FOUND=YES"
        "LLVM_ROOT=${LIBDIR}/llvm/"
        "LLVM_DIR=${LIBDIR}/llvm/lib/cmake/llvm"
        "LLVM_VERSION=${LLVM_VERSION}"
        "LLVM_INCLUDES=${LIBDIR}/llvm/include/"
      )
      file(GLOB IOSDEP_LLVM_LIBRARIES "${LIBDIR}/llvm/lib/*.a")
      file(GLOB IOSDEP_LLVM_LIBRARIES_DYLIB "${LIBDIR}/llvm/lib/*.dylib")
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_LLVM_LIBRARIES})
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_LLVM_LIBRARIES_DYLIB})
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/llvm/lib")
      unset(IOSDEP_LLVM_LIBRARIES)
      unset(IOSDEP_LLVM_LIBRARIES_DYLIB)
    endif()
    
     # MaterialX
    if (${dependency} STREQUAL MATERIALX)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/materialx/include/")
      list(APPEND IOSDEP_DEFINES_LIST "MaterialX_DIR=${LIBDIR}/materialx/lib/cmake/MaterialX")
      # These two possibly required for USD
      list(APPEND IOSDEP_DEFINES_LIST "MATERIALX_BASE_DIR=${LIBDIR}/materialx/")
      list(APPEND IOSDEP_DEFINES_LIST "MATERIALX_STDLIB_DIR=${LIBDIR}/materialx/lib/")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/materialx/lib")
    endif()

    # OpenColorIO
    if (${dependency} STREQUAL OPENCOLORIO)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/opencolorio/include/")
      list(APPEND IOSDEP_DEFINES_LIST "OpenColorIO_DIR=${LIBDIR}/opencolorio/lib/cmake/OpenColorIO")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/opencolorio/lib")
    endif()
    
    # OpenEXR
    if (${dependency} STREQUAL OPENEXR)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/openexr/include/")
      list(APPEND IOSDEP_DEFINES_LIST "OpenEXR_DIR=${LIBDIR}/openexr/lib/cmake/OpenEXR")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/openexr/lib")
    endif()

    # OpenImageIO
    if (${dependency} STREQUAL OPENIMAGEIO)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/openimageio/include/")
      list(APPEND IOSDEP_DEFINES_LIST "OpenImageIO_DIR=${LIBDIR}/openimageio/lib/cmake/OpenImageIO")
      file(GLOB IOSDEP_OPENIMAGEIO_LIBRARIES "${LIBDIR}/openimageio/lib/*.dylib")
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_OPENIMAGEIO_LIBRARIES})
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/openimageio/lib")
      unset(IOSDEP_OPENIMAGEIO_LIBRARIES)
    endif()
    
    # OpenJPEG
    if (${dependency} STREQUAL OPENJPG)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/openjpeg/include/")
      list(APPEND IOSDEP_DEFINES_LIST "OpenJPEG_DIR=${LIBDIR}/openjpeg/lib/openjpeg-2.5")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/openjpeg/lib")
    endif()

    # OSL
    if (${dependency} STREQUAL OSL)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/osl/include/")
      list(APPEND IOSDEP_DEFINES_LIST "OSL_DIR=${LIBDIR}/osl/lib/cmake/OSL")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/osl/lib")
    endif()
    
    # PNG
    if (${dependency} STREQUAL PNG)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/png/include/")
      list(APPEND IOSDEP_DEFINES_LIST "PNG_DIR=${LIBDIR}/png/lib/libpng")
      file(GLOB IOSDEP_PNG_LIBRARIES "${LIBDIR}/png/lib/*.a")
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_PNG_LIBRARIES})
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/png/lib")
      unset(IOSDEP_PNG_LIBRARIES)
    endif()

     # PugiXML
    if (${dependency} STREQUAL PUGIXML)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/pugixml/include/")
      list(APPEND IOSDEP_DEFINES_LIST "pugixml_DIR=${LIBDIR}/pugixml/lib/cmake/pugixml")
      file(GLOB IOSDEP_PUGIXML_LIBRARIES "${LIBDIR}/pugixml/lib/*.a")
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_PUGIXML_LIBRARIES})
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/pugixml/lib")
      unset(IOSDEP_PUGIXML_LIBRARIES)
    endif()

    # Python
    if (${dependency} STREQUAL PYTHON)
      # Required otherwise python_add_library will not be found
      list(APPEND IOSDEP_DEFINES_LIST "Python3_FOUND=YES")
    endif()

    # Pybind11
    if (${dependency} STREQUAL PYBIND11)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/pybind11/include/")
      list(APPEND IOSDEP_DEFINES_LIST "pybind11_DIR=${LIBDIR}/pybind11/share/cmake/pybind11")
    endif()
    
    # RobinMap
    if (${dependency} STREQUAL ROBINMAP)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/robinmap/include/")
      list(APPEND IOSDEP_DEFINES_LIST "ROBINMAP_DIR=${LIBDIR}/robinmap/share/cmake/tsl-robin-map")
    endif()
    
    # TBB
    if (${dependency} STREQUAL TBB)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/tbb/include/")
      list(APPEND IOSDEP_DEFINES_LIST "TBB_DIR=${LIBDIR}/tbb/lib/cmake/TBB")
      set(IOSDEP_TBB_LIBRARIES
        ${LIBDIR}/tbb/lib/libtbb.dylib
        ${LIBDIR}/tbb/lib/libtbbmalloc_proxy.dylib
        ${LIBDIR}/tbb/lib/libtbbmalloc.dylib)
      list(APPEND IOSDEP_LIBRARIES_LIST ${IOSDEP_TBB_LIBRARIES})
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/tbb/lib")
      unset(IOSDEP_TBB_LIBRARIES)
    endif()
    
    # Webp
    if (${dependency} STREQUAL WEBP)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/webp/include/")
      list(APPEND IOSDEP_DEFINES_LIST "WebP_DIR=${LIBDIR}/webp/share/WebP/cmake")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/webp/lib")
    endif()

    # Blank
    if (${dependency} STREQUAL blank)
      list(APPEND IOSDEP_INCLUDES_LIST "${LIBDIR}/blank/include/")
      list(APPEND IOSDEP_DEFINES_LIST "blank_DIR=${LIBDIR}/")
      list(APPEND IOSDEP_LIBDIRS_LIST "${LIBDIR}/blank/lib")
    endif()

  endforeach()
  unset(IOSDEP_INCLUDES_STRING)
  unset(IOSDEP_LIBRARIES_STRING)
  unset(IOSDEP_LIBDIRS_STRING)
  unset(IOSDEP_DEFINES)
  ios_build_string("${IOSDEP_INCLUDES_LIST}" IOSDEP_INCLUDES_STRING "-I" NO)
  ios_build_string("${IOSDEP_LIBRARIES_LIST}" IOSDEP_LIBRARIES_STRING " " NO)
  ios_build_string("${IOSDEP_LIBDIRS_LIST}" IOSDEP_LIBDIRS_STRING "-L" NO)
  ios_build_defines("${IOSDEP_DEFINES_LIST}" IOSDEP_DEFINES "-D")
  unset(IOSDEP_INCLUDES_LIST)
  unset(IOSDEP_LIBRARIES_LIST)
  unset(IOSDEP_LIBDIRS_LIST)
  unset(IOSDEP_DEFINES_LIST)
endmacro()
    
function(test_ios_get_dependency_env_vars)

  if (NOT LIBDIR)
    set(LIBDIR "test_util_test_dir")
  endif()
  if (NOT LLVM_VERSION)
    set(LLVM_VERSION "1.2.3")
  endif()
  
  ios_get_dependency_env_vars(
    PYTHON
    PYBIND11
    ROBINMAP
    PNG
    PUGIXML
    IMATH
    DEFLATE
    OSL
    OPENIMAGEIO
    OPENCOLORIO
    LLVM
    OPENEXR
  )

  message("IOSDEP_INCLUDES_STRING=${IOSDEP_INCLUDES_STRING}")
  message("IOSDEP_LIBRARIES_STRING=${IOSDEP_LIBRARIES_STRING}")
  message("IOSDEP_LIBDIRS_STRING=${IOSDEP_LIBDIRS_STRING}")
  message("IOSDEP_DEFINES=")
  foreach(define IN ITEMS ${IOSDEP_DEFINES})
    message(${define})
  endforeach()
endfunction()

if (${ios_get_dependency_env_vars_run_test} STREQUAL YES)
  test_ios_get_dependency_env_vars()
endif()
