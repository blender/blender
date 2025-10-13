# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../blender/editors/include
  ../blender/io/usd
  ../blender/makesrna
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::imbuf::movie
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::sequencer
  PRIVATE bf::windowmanager
  PRIVATE bf::dependencies::optional::opencolorio
)

if(HAVE_FEENABLEEXCEPT)
  add_definitions(-DHAVE_FEENABLEEXCEPT)
endif()

if(WITH_STRSIZE_DEBUG)
  add_definitions(-DWITH_STRSIZE_DEBUG)
endif()

if(WITH_TBB)
  # Force TBB libraries to be in front of MKL (part of `OpenImageDenoise`), so
  # that it is initialized before MKL and static library initialization order issues are avoided.
  #
  # This isn't fully robust but seems to work.
  list(INSERT LIB 0 ${TBB_LIBRARIES})
  list(INSERT LIB 0 bf_blenkernel)
endif()

if(WIN32)
  list(APPEND INC ../../intern/utfconv)
endif()

if(WITH_LIBMV)
  list(APPEND INC ../../intern/libmv)
  add_definitions(-DWITH_LIBMV)
endif()

if(WITH_CYCLES)
  add_definitions(-DWITH_CYCLES)
  list(APPEND INC ../../intern/cycles/blender)
endif()

if(WITH_OPENGL_BACKEND)
  add_definitions(-DWITH_OPENGL_BACKEND)
endif()

if(WITH_VULKAN_BACKEND)
  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

if(WITH_RENDERDOC)
  add_definitions(-DWITH_RENDERDOC)
endif()

if(WITH_CODEC_FFMPEG)
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_TBB)
  list(APPEND INC ${TBB_INCLUDE_DIRS})
  if(WIN32)
    # For `pragma` that links `tbbmalloc_proxy.lib`.
    link_directories(${LIBDIR}/tbb/lib)
  endif()
endif()

if(WIN32)
  # Windows.h will define min/max macros that will collide with the STL versions.
  add_definitions(-DNOMINMAX)
endif()

if(WITH_USD)
  # USD links libMaterialX, when using pre-compiled libraries
  # ensures `usd_ms` can find `MaterialXRender` and friends.
  #
  # NOTE: This is _only_ needed when linking blender before the install target runs.
  # Once MATERIALX libraries have been copied into `TARGETDIR_LIB` then Blender will link.
  # Don't rely on this though as failing on a fresh build is no good and the library
  # files could get outdated too.
  if(DEFINED LIBDIR)
    link_directories(${LIBDIR}/materialx/lib)
  endif()
endif()

if(WITH_PYTHON)
  list(APPEND INC ../blender/python)
  add_definitions(-DWITH_PYTHON)

  if(WITH_PYTHON_SECURITY)
    add_definitions(-DWITH_PYTHON_SECURITY)
  endif()
endif()

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
endif()

if(WITH_SDL)
  add_definitions(-DWITH_SDL)
endif()

if(WITH_BINRELOC)
  list(APPEND INC ${BINRELOC_INCLUDE_DIRS})
  add_definitions(-DWITH_BINRELOC)
endif()

if(WITH_FREESTYLE)
  list(APPEND INC ../blender/freestyle)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

if(WITH_GMP)
  list(APPEND INC ${GMP_INCLUDE_DIRS})
  add_definitions(-DWITH_GMP)
endif()

# Setup the EXE sources and `buildinfo`.
set(SRC
  creator.cc
  creator_args.cc
  creator_signals.cc

  creator_intern.h
)

if(CMAKE_GENERATOR MATCHES "^Visual Studio.+")
  # This helps visual studio find the debugger visualizers
  list(APPEND SRC ${CMAKE_SOURCE_DIR}/tools/utils_ide/natvis/Blender.natvis)
endif()

# MSVC 2010 gives linking errors with the manifest.
if(WIN32 AND NOT UNIX)
  add_definitions(
    -DBLEN_VER_RC_STR="${BLENDER_VERSION}"
    -DBLEN_VER_RC_1=${BLENDER_VERSION_MAJOR}
    -DBLEN_VER_RC_2=${BLENDER_VERSION_MINOR}
    -DBLEN_VER_RC_3=${BLENDER_VERSION_PATCH}
    -DBLEN_VER_RC_4=0
  )

  list(APPEND SRC
    ${CMAKE_SOURCE_DIR}/release/windows/icons/winblender.rc
  )

  if(NOT WITH_WINDOWS_EXTERNAL_MANIFEST)
    list(APPEND SRC
      ${CMAKE_BINARY_DIR}/blender.exe.manifest
    )
  endif()
endif()

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
  # --------------------------------------------------------------------------
  # These defines could all be moved into the header below

  # Write strings into a separate header since we can escape C-strings
  # in a way that's not practical when passing defines.
  set(BUILD_PLATFORM "${CMAKE_SYSTEM_NAME}")
  set(BUILD_TYPE "${CMAKE_BUILD_TYPE}")
  set(BUILD_CFLAGS "${CMAKE_C_FLAGS}")
  set(BUILD_CXXFLAGS "${CMAKE_CXX_FLAGS}")
  set(BUILD_LINKFLAGS "${PLATFORM_LINKFLAGS}")
  set(BUILD_SYSTEM "CMake")

  if(WITH_COMPILER_SHORT_FILE_MACRO)
    # It's not necessary to include path information
    # about the system building Blender in the executable.
    string(REPLACE "${PLATFORM_CFLAGS_FMACRO_PREFIX_MAP}" " " BUILD_CFLAGS "${BUILD_CFLAGS}")
    string(REPLACE "${PLATFORM_CFLAGS_FMACRO_PREFIX_MAP}" " " BUILD_CXXFLAGS "${BUILD_CXXFLAGS}")
  endif()

  # Use `configure_file` instead of definitions since properly
  # escaping the multiple command line arguments which themselves
  # contain strings and spaces becomes overly error-prone & complicated.
  configure_file(
    "${CMAKE_SOURCE_DIR}/build_files/cmake/buildinfo_static.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/buildinfo_static.h"
    ESCAPE_QUOTES
    @ONLY
  )

  unset(BUILD_PLATFORM)
  unset(BUILD_TYPE)
  unset(BUILD_CFLAGS)
  unset(BUILD_CXXFLAGS)
  unset(BUILD_LINKFLAGS)
  unset(BUILD_SYSTEM)

  # --------------------------------------------------------------------------
  # Write header for values that change each build
  #
  # NOTE: generated file is in build directory `source/creator`
  # except when used as an include path.

  add_definitions(-DWITH_BUILDINFO_HEADER)

  # Include the output directory, where the `buildinfo.h` file is generated.
  include_directories(${CMAKE_CURRENT_BINARY_DIR})


  # XXX: `${buildinfo_h_fake}` is used here,
  # because we rely on that file being detected as missing
  # every build so that the real header `buildinfo.h` is updated.
  #
  # Keep this until we find a better way to resolve!

  set(buildinfo_h_real "${CMAKE_CURRENT_BINARY_DIR}/buildinfo.h")
  set(buildinfo_h_fake "${CMAKE_CURRENT_BINARY_DIR}/buildinfo.h_fake")

  if(EXISTS ${buildinfo_h_fake})
    message(
      FATAL_ERROR
      "File \"${buildinfo_h_fake}\" found, this should never be created, remove!"
    )
  endif()

  # From the CMAKE documentation "If the output of the custom command is not actually created as a
  # file on disk it should be marked with the SYMBOLIC source file property."
  #
  # Not doing this leads to build warnings for the not generated file on
  # MS-Windows when using `msbuild`.
  set_source_files_properties(${buildinfo_h_fake} PROPERTIES SYMBOLIC TRUE)

  # a custom target that is always built
  add_custom_target(
    buildinfo ALL
    DEPENDS ${buildinfo_h_fake}
  )

  # Creates `buildinfo.h` using CMAKE script.
  add_custom_command(
    OUTPUT
      ${buildinfo_h_fake}  # ensure we always run
      ${buildinfo_h_real}
    COMMAND
      ${CMAKE_COMMAND}
      -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
      # Overrides only used when non-empty strings.
      -DBUILD_DATE=${BUILDINFO_OVERRIDE_DATE}
      -DBUILD_TIME=${BUILDINFO_OVERRIDE_TIME}
      -P ${CMAKE_SOURCE_DIR}/build_files/cmake/buildinfo.cmake
  )

  # `buildinfo.h` is a generated file.
  set_source_files_properties(
    ${buildinfo_h_real}
    PROPERTIES GENERATED TRUE
    HEADER_FILE_ONLY TRUE)

  unset(buildinfo_h_real)
  unset(buildinfo_h_fake)

  # Add dependencies below, after adding Blender
  # -------------- done with header values.

  list(APPEND SRC
    buildinfo.c
  )

  # make an object library so can load with it in tests
  add_library(buildinfoobj OBJECT buildinfo.c)
  add_dependencies(buildinfoobj buildinfo)
endif()

add_cc_flags_custom_test(blender)

# message(STATUS "Configuring blender")
if(WITH_PYTHON_MODULE)
  add_definitions(-DWITH_PYTHON_MODULE)

  # Creates `./bpy/__init__.so` which can be imported as a Python module.
  #
  # Note that 'SHARED' works on Linux and Windows, but not MACOS which _must_ be 'MODULE'.
  add_library(blender MODULE ${SRC})


  get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(GENERATOR_IS_MULTI_CONFIG)
    set(BPY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/bpy)
  else()
    set(BPY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/bpy)
  endif()

  set_target_properties(
    blender
    PROPERTIES
      PREFIX ""
      OUTPUT_NAME __init__
      LIBRARY_OUTPUT_DIRECTORY ${BPY_OUTPUT_DIRECTORY}
      RUNTIME_OUTPUT_DIRECTORY ${BPY_OUTPUT_DIRECTORY}
  )
  unset(BPY_OUTPUT_DIRECTORY)

  if(APPLE)
    set_target_properties(blender PROPERTIES MACOSX_BUNDLE TRUE)
    if(WITH_BLENDER_THUMBNAILER)
      set_target_properties(blender-thumbnailer PROPERTIES MACOSX_BUNDLE TRUE)
    endif()
  endif()

  if(WIN32)
    # Python modules use this.
    set_target_properties(
      blender
      PROPERTIES
      SUFFIX ".pyd"
    )
  endif()

else()
  add_executable(blender ${EXETYPE} ${SRC})
  if(WITH_CPU_CHECK)
    # blender_cpu_check *NEEDS* to be linked first, there can be no exceptions
    # to this, this is to ensure this will be the first code to run once the
    # blender binary has been loaded by the OS.
    target_link_libraries(blender PRIVATE blender_cpu_check)
  endif()
  if(WIN32)
    add_executable(blender-launcher WIN32
      blender_launcher_win32.c
      ${CMAKE_SOURCE_DIR}/release/windows/icons/winblender.rc
    )
    if(NOT WITH_WINDOWS_EXTERNAL_MANIFEST)
      target_sources(blender-launcher PRIVATE
        ${CMAKE_BINARY_DIR}/blender.exe.manifest
      )
    endif()
    target_compile_definitions(blender-launcher PRIVATE -D_UNICODE -DUNICODE)
    target_link_libraries(blender-launcher Pathcch.lib)
  endif()
endif()

if(WITH_BUILDINFO)
  # Explicitly say that the executable depends on the `buildinfo`.
  add_dependencies(blender buildinfo)
endif()


set(BLENDER_TEXT_FILES
  # Generate this file:
  # `${CMAKE_SOURCE_DIR}/release/text/readme.html`
)

if(WITH_INSTALL_COPYRIGHT)
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_SOURCE_DIR}/release/text/copyright.txt
  )
endif()

# -----------------------------------------------------------------------------
# Platform specific target destinations
#
# Setup version directory, libraries, `bpy` & text files.

if(UNIX AND NOT APPLE)
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_BPY "bpy")
      set(TARGETDIR_VER "bpy/${BLENDER_VERSION}")
      set(TARGETDIR_LIB "bpy/lib")
    else()
      set(TARGETDIR_BPY ${PYTHON_SITE_PACKAGES}/bpy)
      set(TARGETDIR_VER ${PYTHON_SITE_PACKAGES}/bpy/${BLENDER_VERSION})
      set(TARGETDIR_LIB ${PYTHON_SITE_PACKAGES}/bpy/lib)
    endif()
  else()
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_VER "${BLENDER_VERSION}")
      set(TARGETDIR_TEXT ".")
      set(TARGETDIR_LIB "lib")
    else()
      set(TARGETDIR_VER "share/blender/${BLENDER_VERSION}")
      set(TARGETDIR_TEXT "share/doc/blender")
    endif()
  endif()
  set(TARGETDIR_SITE_PACKAGES "${TARGETDIR_VER}/python/lib/python${PYTHON_VERSION}/site-packages")
elseif(WIN32)
  if(WITH_PYTHON_MODULE)
    set(TARGETDIR_BPY ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy)
    set(TARGETDIR_VER ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy/${BLENDER_VERSION})
    # Important the DLL's are next to `__init__.pyd` otherwise it won't load.
    set(TARGETDIR_LIB ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy)
    set(TARGETDIR_EXE ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy)
  else()
    set(TARGETDIR_VER "${BLENDER_VERSION}")
    set(TARGETDIR_TEXT ".")
    set(TARGETDIR_LIB "blender.shared")
    set(TARGETDIR_EXE ".")
  endif()
  set(TARGETDIR_SITE_PACKAGES "${TARGETDIR_VER}/python/lib/site-packages")
elseif(APPLE)
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_BPY "bpy")
      set(TARGETDIR_VER "bpy/${BLENDER_VERSION}")
      set(TARGETDIR_LIB "bpy/lib")
    else()
      # Paths defined in terms of `site-packages` since the `site-packages`
      # directory can be a symbolic-link (brew for example).
      set(TARGETDIR_BPY ${PYTHON_SITE_PACKAGES}/bpy)
      set(TARGETDIR_VER ${PYTHON_SITE_PACKAGES}/bpy/${BLENDER_VERSION})
      set(TARGETDIR_LIB ${PYTHON_SITE_PACKAGES}/bpy/lib)
    endif()
  else()
    set(TARGETDIR_VER "Blender.app/Contents/Resources/${BLENDER_VERSION}")
    set(TARGETDIR_LIB "Blender.app/Contents/Resources/lib")
    set(TARGETDIR_TEXT "Blender.app/Contents/Resources/text")
  endif()
  set(TARGETDIR_SITE_PACKAGES "${TARGETDIR_VER}/python/lib/python${PYTHON_VERSION}/site-packages")
  # Skip re-linking on CPACK / install.
  set_target_properties(blender PROPERTIES BUILD_WITH_INSTALL_RPATH true)
  if(WITH_BLENDER_THUMBNAILER)
    set_target_properties(blender-thumbnailer PROPERTIES BUILD_WITH_INSTALL_RPATH true)
  endif()
endif()


# -----------------------------------------------------------------------------
# Install Targets (Generic, All Platforms)

if(WITH_PYTHON)
  # install(CODE "message(\"copying blender scripts...\")")

  # do not install freestyle dir if disabled
  if(NOT WITH_FREESTYLE)
    set(FREESTYLE_EXCLUDE_CONDITIONAL "freestyle/*")
  else()
    set(FREESTYLE_EXCLUDE_CONDITIONAL "_freestyle/*")  # Dummy, won't do anything.
  endif()

  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/scripts
    DESTINATION ${TARGETDIR_VER}
    PATTERN ".git" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
    PATTERN ".gitea" EXCLUDE
    PATTERN ".github" EXCLUDE
    PATTERN ".arcconfig" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "site" EXCLUDE
    PATTERN "${FREESTYLE_EXCLUDE_CONDITIONAL}" EXCLUDE

    # Exclude extensions development files.
    PATTERN "addons_core/bl_pkg/Makefile" EXCLUDE
    PATTERN "addons_core/bl_pkg/tests" EXCLUDE
    PATTERN "addons_core/bl_pkg/example_extension" EXCLUDE
  )

  if(WITH_PYTHON_MODULE)
    install(
      FILES ${CMAKE_SOURCE_DIR}/scripts/site/sitecustomize.py
      DESTINATION ${TARGETDIR_VER}/scripts/startup
      # Rename to avoid conflict with system `sitecustomize.py`.
      RENAME bpy_site_customize.py
    )
  elseif(WITH_PYTHON_INSTALL)
    install(
      FILES ${CMAKE_SOURCE_DIR}/scripts/site/sitecustomize.py
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
    )
  endif()
  unset(FREESTYLE_EXCLUDE_CONDITIONAL)

  if(WITH_DRACO)
    install(
      PROGRAMS $<TARGET_FILE:extern_draco>
      DESTINATION ${TARGETDIR_VER}/scripts/addons_core/io_scene_gltf2
    )
  endif()

endif()

# fonts
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/release/datafiles/fonts
  DESTINATION ${TARGETDIR_VER}/datafiles
)

# localization
if(WITH_INTERNATIONAL)
  set(_locale_dir "${CMAKE_SOURCE_DIR}/locale")
  set(_locale_target_dir ${TARGETDIR_VER}/datafiles/locale)

  file(GLOB _po_files "${_locale_dir}/po/*.po")
  foreach(_po_file ${_po_files})
    msgfmt_simple(${_po_file} _all_mo_files)
  endforeach()

  # Create a custom target which will compile all `*.po` to `*.mo`.
  add_custom_target(
    locales
    DEPENDS ${_all_mo_files}
  )
  add_dependencies(blender locales)

  # Generate INSTALL rules.
  install(
    FILES ${_locale_dir}/languages
    DESTINATION ${_locale_target_dir}
  )

  foreach(_mo_file ${_all_mo_files})
    get_filename_component(_locale_name ${_mo_file} NAME_WE)
    install(
      FILES ${_mo_file}
      DESTINATION ${_locale_target_dir}/${_locale_name}/LC_MESSAGES
      RENAME blender.mo
    )
    unset(_locale_name)
  endforeach()

  unset(_all_mo_files)
  unset(_po_files)
  unset(_po_file)
  unset(_mo_file)
  unset(_locale_target_dir)

  unset(_locale_dir)
endif()

# Color management.
if(WITH_OPENCOLORIO)
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/release/datafiles/colormanagement
    DESTINATION ${TARGETDIR_VER}/datafiles
  )
endif()
if(WIN32)
  if(EXISTS ${LIBDIR}/osl/lib/python${PYTHON_VERSION}/site-packages/oslquery) # 4.4+
    install(
      DIRECTORY ${LIBDIR}/osl/lib/python${PYTHON_VERSION}/site-packages/oslquery
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
      CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
    )
    install(
      DIRECTORY ${LIBDIR}/osl/lib/python${PYTHON_VERSION}_debug/site-packages/oslquery
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
      CONFIGURATIONS Debug
    )
  endif()
  if(EXISTS ${LIBDIR}/osl/bin/oslquery.dll) # 4.1+
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/osl/bin/oslquery.dll
        ${LIBDIR}/osl/bin/oslcomp.dll
        ${LIBDIR}/osl/bin/oslexec.dll
        ${LIBDIR}/osl/bin/oslnoise.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/osl/bin/oslquery_d.dll
        ${LIBDIR}/osl/bin/oslcomp_d.dll
        ${LIBDIR}/osl/bin/oslexec_d.dll
        ${LIBDIR}/osl/bin/oslnoise_d.dll
      DEBUG
    )
  endif()
  if(EXISTS ${LIBDIR}/opencolorio/bin/opencolorio_2_4.dll) # 4.4
    windows_install_shared_manifest(
      FILES ${LIBDIR}/opencolorio/bin/opencolorio_2_4.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES ${LIBDIR}/opencolorio/bin/opencolorio_d_2_4.dll
      DEBUG
    )
  endif()
  if(EXISTS ${LIBDIR}/opencolorio/bin/opencolorio_2_3.dll) # 4.1
    windows_install_shared_manifest(
      FILES ${LIBDIR}/opencolorio/bin/opencolorio_2_3.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES ${LIBDIR}/opencolorio/bin/opencolorio_d_2_3.dll
      DEBUG
    )
  endif()
  install(
    DIRECTORY ${LIBDIR}/opencolorio/lib/site-packages-debug/PyOpenColorIO
    DESTINATION ${TARGETDIR_SITE_PACKAGES}
    CONFIGURATIONS Debug
  )
  install(
    DIRECTORY ${LIBDIR}/opencolorio/lib/site-packages/PyOpenColorIO
    DESTINATION ${TARGETDIR_SITE_PACKAGES}
    CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
  )
  if(EXISTS ${LIBDIR}/OpenImageDenoise/bin/openimagedenoise.dll) # 4.0
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise.dll
        ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_core.dll
        ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_cpu.dll
    )
  endif()
  # Platforms that have SyCL support.
  if(EXISTS ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_sycl.dll)
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_sycl.dll
    )
  endif()
  if(EXISTS ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_hip.dll) # 4.1
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_hip.dll
    )
  endif()
  if(EXISTS ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_cuda.dll) # 4.1
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/OpenImageDenoise/bin/OpenImageDenoise_device_cuda.dll
    )
  endif()
endif()

# Show helpful tip.
set(_install_cmd "")
if("${CMAKE_GENERATOR}" MATCHES ".*Makefiles.*")
  set(_install_cmd "make install")
elseif("${CMAKE_GENERATOR}" MATCHES "Ninja")
  set(_install_cmd "ninja install")
endif()
if(NOT ("${_install_cmd}" STREQUAL ""))
  # Message to display after building.
  get_filename_component(_install_dst ${TARGETDIR_VER} ABSOLUTE BASE_DIR ${CMAKE_INSTALL_PREFIX})
  add_custom_command(
    TARGET blender POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E echo
      "Run: \\\"${_install_cmd}\\\" to copy runtime files and scripts to: ${_install_dst}"
  )
  unset(_install_dst)
endif()
unset(_install_cmd)

# macro to help install files without dragging in unnecessary data.
macro(install_dir from to)
  install(
    DIRECTORY ${from}
    DESTINATION ${to}
    # Irrelevant files and caches.
    PATTERN ".git" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
    PATTERN ".gitea" EXCLUDE
    PATTERN ".github" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
    PATTERN "*.pyo" EXCLUDE
    PATTERN "*.orig" EXCLUDE
    PATTERN "*.rej" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "__MACOSX" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE
    # Unneeded Python files.
    PATTERN "config-${PYTHON_VERSION}/*.a" EXCLUDE  # static lib
    PATTERN "lib2to3" EXCLUDE                   # ./lib2to3
    PATTERN "tkinter" EXCLUDE                   # ./tkinter
    PATTERN "lib-dynload/_tkinter.*" EXCLUDE    # ./lib-dynload/_tkinter.co
    PATTERN "idlelib" EXCLUDE                   # ./idlelib
    PATTERN "test" EXCLUDE                      # ./test
    PATTERN "turtledemo" EXCLUDE                # ./turtledemo
    PATTERN "turtle.py" EXCLUDE                 # ./turtle.py
    PATTERN "wininst*.exe" EXCLUDE              # from distutils, avoid malware false positive
  )
endmacro()

# -----------------------------------------------------------------------------
# Install Targets (Platform Specific)

if(UNIX AND NOT APPLE)

  if(PLATFORM_BUNDLED_LIBRARIES AND TARGETDIR_LIB)
    install(
      FILES ${PLATFORM_BUNDLED_LIBRARIES}
      DESTINATION ${TARGETDIR_LIB}
    )
  endif()

  # There are a few differences between portable and system install.
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      install(
        TARGETS blender
        DESTINATION ${TARGETDIR_BPY}
      )
    else()
      install(
        TARGETS blender
        LIBRARY DESTINATION ${TARGETDIR_BPY}
      )
    endif()

    # none of the other files are needed currently
  elseif(WITH_INSTALL_PORTABLE)
    set(BLENDER_BIN "blender")
    install(
      TARGETS blender
      DESTINATION "."
    )

    if(WITH_CPU_CHECK)
      install(
        TARGETS blender_cpu_check
        DESTINATION "./lib"
      )
    endif()

    install(
      FILES
        ${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
        ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
        ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/symbolic/apps/blender-symbolic.svg
      DESTINATION "."
    )

    if(WITH_BLENDER_THUMBNAILER)
      install(
        TARGETS blender-thumbnailer
        DESTINATION "."
      )
    endif()

    # NOTE: there is a bug in CMake 3.25.1 where `LIBDIR` is reported as undefined.
    if(NOT DEFINED LIBDIR)
      # Pass.
    elseif(EXISTS ${LIBDIR}/mesa)
      install(
        # Trailing slash is needed to install contents instead of directory itself.
        DIRECTORY ${LIBDIR}/mesa/lib/
        DESTINATION "./lib/mesa"
      )

      install(
        PROGRAMS
        ${CMAKE_SOURCE_DIR}/release/bin/blender-launcher
        ${CMAKE_SOURCE_DIR}/release/bin/blender-softwaregl
        DESTINATION "."
      )

      # Remove from old location, so existing builds don't start with software
      # OpenGL now that the `./lib/` directory is used for other libraries.
      install(
        CODE "\
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libGL.so)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libGL.so.1)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libGL.so.1.5.0)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libGLU.so)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libGLU.so.1)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libGLU.so.1.3.1)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libglapi.so)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libglapi.so.0)\n
file(REMOVE ${CMAKE_BINARY_DIR}/bin/lib/libglapi.so.0.0.0)\n
"
      )
    endif()
  else()
    # main blender binary
    set(BLENDER_BIN "bin/blender")
    install(
      TARGETS blender
      DESTINATION "./bin"
    )
    # Misc files.
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
      DESTINATION "./share/applications"
    )
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/org.blender.Blender.metainfo.xml
      DESTINATION "./share/metainfo"
    )
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
      DESTINATION "./share/icons/hicolor/scalable/apps"
    )
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/symbolic/apps/blender-symbolic.svg
      DESTINATION "./share/icons/hicolor/symbolic/apps"
    )
    if(WITH_BLENDER_THUMBNAILER)
      install(
        TARGETS blender-thumbnailer
        DESTINATION "./bin"
      )
    endif()
  endif()

  if(WITH_PYTHON AND WITH_PYTHON_INSTALL)
    # Install executable
    install(
      PROGRAMS ${PYTHON_EXECUTABLE}
      DESTINATION ${TARGETDIR_VER}/python/bin
    )

    if(DEFINED LIBDIR)
      # Pre-compiled libraries, copy over complete lib directory.
      install_dir(
        ${PYTHON_LIBPATH}
        ${TARGETDIR_VER}/python
      )
    else()
      # System libraries.
      install(
        PROGRAMS ${PYTHON_EXECUTABLE}
        DESTINATION ${TARGETDIR_VER}/python/bin
      )

      # On some platforms (like openSUSE) Python is linked to be used from `lib64` directory.
      # determine this from Python's libraries path.
      # Ugh, its possible `lib64` is just a symbolic-link to `lib`
      # which causes incorrect use of `lib64`.
      get_filename_component(_pypath_real ${PYTHON_LIBPATH} REALPATH)
      if(${_pypath_real} MATCHES "lib64$")
        set(_target_LIB "lib64")
      else()
        set(_target_LIB "lib")
      endif()
      unset(_pypath_real)

      # Copy the systems python into the install directory:
      # install(CODE "message(\"copying a subset of the systems python...\")")
      install(
        DIRECTORY ${PYTHON_LIBPATH}/python${PYTHON_VERSION}
        DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}
        PATTERN "__pycache__" EXCLUDE               # * any cache *
        PATTERN "config-${PYTHON_VERSION}/*.a" EXCLUDE  # static lib
        PATTERN "lib2to3" EXCLUDE                   # ./lib2to3
        PATTERN "site-packages/*" EXCLUDE           # ./site-packages/*
        PATTERN "tkinter" EXCLUDE                   # ./tkinter
        PATTERN "lib-dynload/_tkinter.*" EXCLUDE    # ./lib-dynload/_tkinter.co
        PATTERN "idlelib" EXCLUDE                   # ./idlelib
        PATTERN "test" EXCLUDE                      # ./test
        PATTERN "turtledemo" EXCLUDE                # ./turtledemo
        PATTERN "turtle.py" EXCLUDE                 # ./turtle.py
        PATTERN "wininst*.exe" EXCLUDE              # from distutils, avoid malware false positive
      )

      # Needed for `distutils/pip`.
      # Get the last part of the include dir, will be `python{version}{abiflag}`.
      get_filename_component(_py_inc_suffix ${PYTHON_INCLUDE_DIR} NAME)
      install(
        FILES ${PYTHON_INCLUDE_DIR}/pyconfig.h
        DESTINATION ${TARGETDIR_VER}/python/include/${_py_inc_suffix}
      )
      unset(_py_inc_suffix)

      if(WITH_PYTHON_INSTALL_NUMPY)
        # Install to the same directory as the source, so debian-like
        # distributions are happy with their policy.
        set(_suffix "site-packages")
        if(${PYTHON_NUMPY_PATH} MATCHES "dist-packages")
          set(_suffix "dist-packages")
        endif()
        install(
          DIRECTORY ${PYTHON_NUMPY_PATH}/numpy
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
          PATTERN "oldnumeric" EXCLUDE            # ./oldnumeric
          PATTERN "doc" EXCLUDE                   # ./doc
          PATTERN "tests" EXCLUDE                 # ./tests
          PATTERN "f2py" EXCLUDE                  # ./f2py - fortran/python interface code, not for blender.
          PATTERN "include" EXCLUDE               # include dirs all over, we won't use NumPy/CAPI
          PATTERN "*.h" EXCLUDE                   # some includes are not in include dirs
          PATTERN "*.a" EXCLUDE                   # ./core/lib/libnpymath.a - for linking, we don't need.
        )
        install(
          DIRECTORY ${PYTHON_NUMPY_PATH}/Cython
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
        install(
          FILES ${PYTHON_NUMPY_PATH}/cython.py
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
        )
        unset(_suffix)
      endif()

      if(WITH_USD)
        # Install to the same directory as the source, so debian-like
        # distributions are happy with their policy.
        set(_suffix "site-packages")
        if(0) # TODO: `PYTHON_USD_PATH` isn't defined anywhere.
          if(${PYTHON_USD_PATH} MATCHES "dist-packages")
            set(_suffix "dist-packages")
          endif()
        endif()
        install(
          DIRECTORY ${USD_LIBRARY_DIR}/python/
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
        unset(_suffix)
      endif()

      if(WITH_PYTHON_INSTALL_ZSTANDARD)
        # Install to the same directory as the source, so debian-like
        # distributions are happy with their policy.
        set(_suffix "site-packages")
        if(${PYTHON_ZSTANDARD_PATH} MATCHES "dist-packages")
          set(_suffix "dist-packages")
        endif()
        install(
          DIRECTORY ${PYTHON_ZSTANDARD_PATH}/zstandard
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
        unset(_suffix)
      endif()

      # Copy requests, we need to generalize site-packages.
      if(WITH_PYTHON_INSTALL_REQUESTS)
        set(_suffix "site-packages")
        if(${PYTHON_REQUESTS_PATH} MATCHES "dist-packages")
          set(_suffix "dist-packages")
        endif()
        install(
          DIRECTORY ${PYTHON_REQUESTS_PATH}/requests
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
        # On some platforms requests does have extra dependencies.
        #
        # Either `chardet` or `charset_normalizer` is used, depending on the version of Python.
        # The code below silently skips the one that's not available, so we can list both here.
        set(_requests_deps "certifi" "chardet" "charset_normalizer" "idna" "urllib3")
        foreach(_requests_dep ${_requests_deps})
          if(EXISTS ${PYTHON_REQUESTS_PATH}/${_requests_dep})
            install(
              DIRECTORY ${PYTHON_REQUESTS_PATH}/${_requests_dep}
              DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
              PATTERN "__pycache__" EXCLUDE           # * any cache *
              PATTERN "*.pyc" EXCLUDE                 # * any cache *
              PATTERN "*.pyo" EXCLUDE                 # * any cache *
            )
          endif()
        endforeach()
        if(EXISTS ${PYTHON_REQUESTS_PATH}/six.py)
          install(
            FILES ${PYTHON_REQUESTS_PATH}/six.py
            DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          )
        endif()
        unset(_requests_dep)
        unset(_requests_deps)
        unset(_suffix)
      endif()
      unset(_target_LIB)
    endif()

    if(WITH_INSTALL_PORTABLE)
      # The script uses the version of Python installed with Blender,
      # so it must only be inside installed if `WITH_PYTHON AND WITH_PYTHON_INSTALL`
      # are active.
      get_filename_component(PYTHON_EXECUTABLE_NAME_ONLY ${PYTHON_EXECUTABLE} NAME)
      configure_file(
        ${CMAKE_SOURCE_DIR}/release/freedesktop/scripts/blender-system-info.sh.in
        ${CMAKE_BINARY_DIR}/release/freedesktop/scripts/blender-system-info.sh
        @ONLY
      )
      unset(PYTHON_EXECUTABLE_NAME_ONLY)
      install(
        PROGRAMS ${CMAKE_BINARY_DIR}/release/freedesktop/scripts/blender-system-info.sh
        DESTINATION "."
      )
    endif()
  endif()

elseif(WIN32)
  if(WITH_WINDOWS_EXTERNAL_MANIFEST)
    install(
      FILES ${CMAKE_BINARY_DIR}/blender.exe.manifest
      DESTINATION "."
    )
    install(
      FILES ${CMAKE_BINARY_DIR}/blender.exe.manifest
      DESTINATION "."
      RENAME blender-launcher.exe.manifest
    )
  endif()
  windows_install_shared_manifest(
    FILES ${LIBDIR}/epoxy/bin/epoxy-0.dll
    ALL
  )

  if(WITH_VULKAN_BACKEND)
    windows_install_shared_manifest(
      FILES ${LIBDIR}/vulkan/bin/vulkan-1.dll
      ALL
    )
  endif()

  # 4.1 FFTW libs need to be installed, in 4.2 FFTW got turned into a static lib
  # and the files below no longer exist.
  if(EXISTS ${LIBDIR}/fftw3/lib/fftw3.dll)
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/fftw3/lib/fftw3.dll
        ${LIBDIR}/fftw3/lib/fftw3f.dll
      ALL
    )
  endif()
  if(MSVC_ASAN)
    # The ASAN DLL's can be found in the same directory as the compiler,
    # this is the easiest way to find these.
    string(
      REPLACE "cl.exe" "clang_rt.asan_dynamic-x86_64.dll"
      ASAN_DLL ${CMAKE_C_COMPILER})
    string(
      REPLACE "cl.exe" "clang_rt.asan_dbg_dynamic-x86_64.dll"
      ASAN_DEBUG_DLL ${CMAKE_C_COMPILER}
    )
    if(NOT EXISTS "${ASAN_DLL}")
      message(
        FATAL_ERROR
        "ASAN is enabled, but the ASAN runtime is not detected, "
        "this is an optional component during the MSVC install, please install it"
      )
    endif()
    windows_install_shared_manifest(
      FILES ${ASAN_DLL}
      RELEASE
    )
    windows_install_shared_manifest(
      FILES ${ASAN_DEBUG_DLL}
      DEBUG
    )
    unset(ASAN_DLL)
    unset(ASAN_DEBUG_DLL)
  endif()
  if(EXISTS ${LIBDIR}/openexr/bin/Iex.dll)
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/openexr/bin/Iex.dll
        ${LIBDIR}/openexr/bin/IlmThread.dll
        ${LIBDIR}/openexr/bin/OpenEXRCore.dll
        ${LIBDIR}/openexr/bin/OpenEXRUtil.dll
        ${LIBDIR}/openexr/bin/OpenEXR.dll
        ${LIBDIR}/imath/bin/imath.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/openexr/bin/Iex_d.dll
        ${LIBDIR}/openexr/bin/IlmThread_d.dll
        ${LIBDIR}/openexr/bin/OpenEXRCore_d.dll
        ${LIBDIR}/openexr/bin/OpenEXRUtil_d.dll
        ${LIBDIR}/openexr/bin/OpenEXR_d.dll
        ${LIBDIR}/imath/bin/imath_d.dll
      DEBUG
    )
  endif()
  if(EXISTS ${LIBDIR}/openimageio/bin/openimageio.dll)
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/openimageio/bin/openimageio.dll
        ${LIBDIR}/openimageio/bin/openimageio_util.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/openimageio/bin/openimageio_d.dll
        ${LIBDIR}/openimageio/bin/openimageio_util_d.dll
      DEBUG
    )
  endif()

  if(EXISTS ${LIBDIR}/gmp/lib/gmp-10.dll)
    set(GMP_DLL ${LIBDIR}/gmp/lib/gmp-10.dll)
  else()
    set(GMP_DLL ${LIBDIR}/gmp/lib/libgmp-10.dll)
  endif()

  windows_install_shared_manifest(
    FILES ${GMP_DLL}
    ALL
  )
  unset(GMP_DLL)

  windows_install_shared_manifest(
    FILES ${LIBDIR}/gmp/lib/libgmpxx.dll
    RELEASE
  )
  windows_install_shared_manifest(
    FILES ${LIBDIR}/gmp/lib/libgmpxx_d.dll
    DEBUG
  )

  if(WITH_WINDOWS_RELEASE_PDB)
    # Skip install of stripped PDB if compiling with clang since there doesn't seem
    # to be a PDB-stripped version for `clang-cl`.
    if(WITH_WINDOWS_RELEASE_STRIPPED_PDB AND NOT MSVC_CLANG)
      # Icky hack for older CMAKE from https://stackoverflow.com/a/21198501
      # `$<CONFIG>` will work in newer CMAKE but the version currently (3.12)
      # on the build-bot does not support this endeavor.
      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/blender_public.pdb
        DESTINATION "."
        RENAME blender.pdb
        CONFIGURATIONS Release
      )
    else()
      install(
        FILES $<TARGET_PDB_FILE:blender>
        DESTINATION "."
        RENAME blender.pdb
        CONFIGURATIONS Release
      )
    endif()
  endif()

  windows_install_shared_manifest(
    FILES ${LIBDIR}/openvdb/bin/openvdb.dll
    RELEASE
  )
  windows_install_shared_manifest(
    FILES ${LIBDIR}/openvdb/bin/openvdb_d.dll
    DEBUG
  )

  windows_install_shared_manifest(
    FILES
      ${LIBDIR}/materialx/bin/MaterialXCore.dll
      ${LIBDIR}/materialx/bin/MaterialXFormat.dll
      ${LIBDIR}/materialx/bin/MaterialXGenGlsl.dll
      ${LIBDIR}/materialx/bin/MaterialXGenMdl.dll
      ${LIBDIR}/materialx/bin/MaterialXGenOsl.dll
      ${LIBDIR}/materialx/bin/MaterialXGenShader.dll
    RELEASE
  )
  if(EXISTS ${LIBDIR}/materialx/bin/MaterialXRender.dll) # 3.6+
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/materialx/bin/MaterialXRender.dll
        ${LIBDIR}/materialx/bin/MaterialXRenderGlsl.dll
        ${LIBDIR}/materialx/bin/MaterialXRenderHw.dll
        ${LIBDIR}/materialx/bin/MaterialXRenderOsl.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/materialx/bin/MaterialXRender_d.dll
        ${LIBDIR}/materialx/bin/MaterialXRenderGlsl_d.dll
        ${LIBDIR}/materialx/bin/MaterialXRenderHw_d.dll
        ${LIBDIR}/materialx/bin/MaterialXRenderOsl_d.dll
      DEBUG
    )
  endif()
  if(EXISTS ${LIBDIR}/materialx/bin/MaterialXGenMsl.dll) # 4.1+
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/materialx/bin/MaterialXGenMsl.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/materialx/bin/MaterialXGenMsl_d.dll
      DEBUG
    )
  endif()
  windows_install_shared_manifest(
    FILES
      ${LIBDIR}/materialx/bin/MaterialXCore_d.dll
      ${LIBDIR}/materialx/bin/MaterialXFormat_d.dll
      ${LIBDIR}/materialx/bin/MaterialXGenGlsl_d.dll
      ${LIBDIR}/materialx/bin/MaterialXGenMdl_d.dll
      ${LIBDIR}/materialx/bin/MaterialXGenOsl_d.dll
      ${LIBDIR}/materialx/bin/MaterialXGenShader_d.dll
    DEBUG
  )

  if(WITH_PYTHON)
    string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

    if(NOT WITH_PYTHON_MODULE)
      if(NOT CMAKE_COMPILER_IS_GNUCC)
        install(
          FILES
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python${_PYTHON_VERSION_NO_DOTS}.dll
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python3.dll
          DESTINATION ${TARGETDIR_EXE}
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )

        install(
          FILES
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python${_PYTHON_VERSION_NO_DOTS}_d.dll
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python3_d.dll
          DESTINATION ${TARGETDIR_EXE}
          CONFIGURATIONS Debug
        )
      endif()
    endif()

    # VFX libs are bundled with both Blender executable and Python module.
    if(WITH_PYTHON_INSTALL OR WITH_PYTHON_MODULE)
      # NOTE: as far as python is concerned `RelWithDebInfo`
      # is not debug since its without debug flags.

      install(DIRECTORY DESTINATION ${TARGETDIR_VER}/python)
      install(DIRECTORY DESTINATION ${TARGETDIR_VER}/python/lib)

      install(
        DIRECTORY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/lib
        DESTINATION ${BLENDER_VERSION}/python/
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        PATTERN "*_d.*" EXCLUDE                 # * debug libraries *
        PATTERN "__pycache__" EXCLUDE           # * any cache *
        PATTERN "*.pyc" EXCLUDE                 # * any cache *
        PATTERN "*.pyo" EXCLUDE                 # * any cache *
      )

      install(
        DIRECTORY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/lib
        DESTINATION ${BLENDER_VERSION}/python/
        CONFIGURATIONS Debug
        PATTERN "__pycache__" EXCLUDE           # * any cache *
        PATTERN "*.pyc" EXCLUDE                 # * any cache *
        PATTERN "*.pyo" EXCLUDE                 # * any cache *
      )

      install(
        DIRECTORY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/DLLs
        DESTINATION ${BLENDER_VERSION}/python
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        PATTERN "*.pdb" EXCLUDE
        PATTERN "*_d.*" EXCLUDE
      )

      install(
        DIRECTORY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/DLLs
        DESTINATION ${BLENDER_VERSION}/python
        CONFIGURATIONS Debug
      )

      install(
        FILES
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python${_PYTHON_VERSION_NO_DOTS}.dll
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python3.dll
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python.exe
        DESTINATION ${BLENDER_VERSION}/python/bin
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
      )
      install(
        FILES
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python${_PYTHON_VERSION_NO_DOTS}_d.dll
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python3_d.dll
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python_d.exe
        DESTINATION ${BLENDER_VERSION}/python/bin
        CONFIGURATIONS Debug
      )

      # This will only exist for 3.5+.
      if(EXISTS ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}/site-packages)
        install(
          DIRECTORY ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}/site-packages/
          DESTINATION ${TARGETDIR_SITE_PACKAGES}/
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()
      if(EXISTS ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}_debug/site-packages)
        install(
          DIRECTORY ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}_debug/site-packages/
          DESTINATION ${TARGETDIR_SITE_PACKAGES}/
          CONFIGURATIONS Debug
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()

      # This will not exist for 3.4 and earlier `./lib` directory
      # to ease the transition, support both 3.4 and 3.5 `./lib` directories.
      if(EXISTS ${USD_LIBRARY_DIR}/python/)
        install(
          DIRECTORY ${USD_LIBRARY_DIR}/python/
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()
      if(EXISTS ${USD_LIBRARY_DIR}/debug/python/)
        install(
          DIRECTORY ${USD_LIBRARY_DIR}/debug/python/
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Debug
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()

      # This will not exist for 3.4 and earlier `./lib` directories
      # to ease the transition, support both 3.4 and 3.5 `./lib` directories.
      if(EXISTS ${LIBDIR}/openvdb/python/pyopenvdb_d.pyd)
        install(
          FILES ${LIBDIR}/openvdb/python/pyopenvdb_d.pyd
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Debug
        )
        install(
          FILES ${LIBDIR}/openvdb/python/pyopenvdb.pyd
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )
      endif()

      # This will exist for 4.1 `./lib` directories.
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "ARM64")
        set(_openvdb_arch arm64)
      else()
        set(_openvdb_arch amd64)
      endif()

      # 4.3
      if(EXISTS ${LIBDIR}/openvdb/python/pyopenvdb_d.cp${_PYTHON_VERSION_NO_DOTS}-win_${_openvdb_arch}.pyd)
        install(
          FILES ${LIBDIR}/openvdb/python/pyopenvdb_d.cp${_PYTHON_VERSION_NO_DOTS}-win_${_openvdb_arch}.pyd
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Debug
        )
        install(
          FILES ${LIBDIR}/openvdb/python/pyopenvdb.cp${_PYTHON_VERSION_NO_DOTS}-win_${_openvdb_arch}.pyd
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )
      endif()
      # 4.4
      if(EXISTS ${LIBDIR}/openvdb/python/openvdb_d.cp${_PYTHON_VERSION_NO_DOTS}-win_${_openvdb_arch}.pyd)
        install(
          FILES ${LIBDIR}/openvdb/python/openvdb_d.cp${_PYTHON_VERSION_NO_DOTS}-win_${_openvdb_arch}.pyd
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Debug
        )
        install(
          FILES ${LIBDIR}/openvdb/python/openvdb.cp${_PYTHON_VERSION_NO_DOTS}-win_${_openvdb_arch}.pyd
          DESTINATION ${TARGETDIR_SITE_PACKAGES}
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )
      endif()
      # MaterialX python bindings.
      # Check they exist so building against non `LIBDIR` Python works as expected.
      if(EXISTS ${LIBDIR}/materialx/python/Release/MaterialX)
        install(
          DIRECTORY ${LIBDIR}/materialx/python/Release/MaterialX
          DESTINATION ${TARGETDIR_SITE_PACKAGES}/
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()
      if(EXISTS ${LIBDIR}/materialx/python/Debug/MaterialX)
        install(
          DIRECTORY ${LIBDIR}/materialx/python/Debug/MaterialX
          DESTINATION ${TARGETDIR_SITE_PACKAGES}/
          CONFIGURATIONS Debug
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()
    endif()

    if(WITH_PYTHON_INSTALL)
      if(WINDOWS_PYTHON_DEBUG)
        install(
          FILES
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/libs/python${_PYTHON_VERSION_NO_DOTS}.pdb
          DESTINATION "."
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )

        install(
          FILES
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/libs/python${_PYTHON_VERSION_NO_DOTS}_d.pdb
          DESTINATION "."
          CONFIGURATIONS Debug
        )
      endif()
    endif()

  endif()

  # Filenames change slightly between FFMPEG versions check both 6.0 and fallback to 5.0
  # to ease the transition between versions.
  if(EXISTS "${LIBDIR}/ffmpeg/lib/avfilter-10.dll")
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/ffmpeg/lib/avfilter-10.dll
      ALL
    )
  endif()
  if(EXISTS "${LIBDIR}/ffmpeg/lib/avcodec-61.dll")
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/ffmpeg/lib/avcodec-61.dll
        ${LIBDIR}/ffmpeg/lib/avformat-61.dll
        ${LIBDIR}/ffmpeg/lib/avdevice-61.dll
        ${LIBDIR}/ffmpeg/lib/avutil-59.dll
        ${LIBDIR}/ffmpeg/lib/swscale-8.dll
        ${LIBDIR}/ffmpeg/lib/swresample-5.dll
      ALL
    )
  elseif(EXISTS "${LIBDIR}/ffmpeg/lib/avcodec-60.dll")
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/ffmpeg/lib/avcodec-60.dll
        ${LIBDIR}/ffmpeg/lib/avformat-60.dll
        ${LIBDIR}/ffmpeg/lib/avdevice-60.dll
        ${LIBDIR}/ffmpeg/lib/avutil-58.dll
        ${LIBDIR}/ffmpeg/lib/swscale-7.dll
        ${LIBDIR}/ffmpeg/lib/swresample-4.dll
      ALL
    )
  else()
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/ffmpeg/lib/avcodec-59.dll
        ${LIBDIR}/ffmpeg/lib/avformat-59.dll
        ${LIBDIR}/ffmpeg/lib/avdevice-59.dll
        ${LIBDIR}/ffmpeg/lib/avutil-57.dll
        ${LIBDIR}/ffmpeg/lib/swscale-6.dll
        ${LIBDIR}/ffmpeg/lib/swresample-4.dll
      ALL
    )
  endif()
  if(EXISTS ${LIBDIR}/tbb/bin/tbb12.dll) # 4.4
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/tbb/bin/tbb12.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/tbb/bin/tbb12_debug.dll
      DEBUG
    )
  else()
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/tbb/bin/tbb.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/tbb/bin/tbb_debug.dll
      DEBUG
    )
  endif()
  if(WITH_TBB_MALLOC_PROXY)
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/tbb/bin/tbbmalloc.dll
        ${LIBDIR}/tbb/bin/tbbmalloc_proxy.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/tbb/bin/tbbmalloc_debug.dll
        ${LIBDIR}/tbb/bin/tbbmalloc_proxy_debug.dll
      DEBUG
    )
    list(APPEND LIB ${TBB_MALLOC_LIBRARIES})
  endif()

  if(EXISTS ${LIBDIR}/sndfile/lib/sndfile.dll)
    set(SNDFILE_DLL ${LIBDIR}/sndfile/lib/sndfile.dll)
  else()
    set(SNDFILE_DLL ${LIBDIR}/sndfile/lib/libsndfile-1.dll)
  endif()

  windows_install_shared_manifest(
    FILES ${SNDFILE_DLL}
    ALL
  )
  unset(SNDFILE_DLL)

  windows_install_shared_manifest(
    FILES ${LIBDIR}/shaderc/bin/shaderc_shared.dll
    ALL
  )

  windows_install_shared_manifest(
    FILES
      ${LIBDIR}/openal/lib/OpenAL32.dll
    ALL
  )

  windows_install_shared_manifest(
    FILES ${LIBDIR}/sdl/lib/SDL2.dll
    ALL
  )

  if(WITH_SYSTEM_AUDASPACE)
    install(
      FILES
        ${LIBDIR}/audaspace/lib/audaspace.dll
        ${LIBDIR}/audaspace/lib/audaspace-c.dll
        ${LIBDIR}/audaspace/lib/audaspace-py.dll
      DESTINATION "."
    )
  endif()


  if(NOT WITH_PYTHON_MODULE)
    get_filename_component(PYTHON_EXECUTABLE_NAME_ONLY ${PYTHON_EXECUTABLE} NAME)
    # Configure then generate file. Note that this copies the literal generator
    # for the Python executable name. So the file needs to be generated afterwards
    # to get the correct name.
    configure_file(
      ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_system_info.cmd.in
      ${CMAKE_BINARY_DIR}/release/windows/batch/blender_system_info.with_vars.cmd.in
      @ONLY
    )
    unset(PYTHON_EXECUTABLE_NAME_ONLY)
    # Replace Python executable generator with actual executable name.
    file(GENERATE
      OUTPUT ${CMAKE_BINARY_DIR}/release/windows/batch/blender_system_info_$<CONFIG>.cmd
      INPUT ${CMAKE_BINARY_DIR}/release/windows/batch/blender_system_info.with_vars.cmd.in
    )
    install(
      FILES
      ${CMAKE_BINARY_DIR}/release/windows/batch/blender_system_info_$<CONFIG>.cmd
      DESTINATION ${TARGETDIR_EXE}
      RENAME blender_system_info.cmd
    )
    if(WITH_CYCLES)
      install(
        FILES
          ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_cycles.cmd
        DESTINATION ${TARGETDIR_EXE}
      )
    endif()
    install(
      FILES
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_gpu.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_gpu_glitchworkaround.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_log.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_factory_startup.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_factory_startup_vulkan.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_startup_opengl.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_startup_vulkan.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_oculus.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/oculus.json
      DESTINATION ${TARGETDIR_EXE}
    )
  endif()

  if(WITH_BLENDER_THUMBNAILER)
    install(
      TARGETS BlendThumb
      DESTINATION "."
    )
  endif()

  if(WITH_PYTHON_MODULE AND TARGETDIR_BPY)
    install(
      TARGETS blender
      LIBRARY DESTINATION ${TARGETDIR_BPY}
    )
  endif()

  if(PLATFORM_BUNDLED_LIBRARIES)
    windows_process_platform_bundled_libraries("${PLATFORM_BUNDLED_LIBRARIES}")
  endif()
elseif(APPLE)
  if(NOT WITH_PYTHON_MODULE)
    # Uppercase name for app bundle.
    set_target_properties(blender PROPERTIES OUTPUT_NAME Blender)
  endif()

  set(OSX_APP_SOURCEDIR ${CMAKE_SOURCE_DIR}/release/darwin/Blender.app)

  # Setup `Info.plist`.
  execute_process(
    COMMAND date "+%Y-%m-%d"
    OUTPUT_VARIABLE BLENDER_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  set_target_properties(blender PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${OSX_APP_SOURCEDIR}/Contents/Info.plist
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${BLENDER_VERSION}.${BLENDER_VERSION_PATCH}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${BLENDER_VERSION}.${BLENDER_VERSION_PATCH} ${BLENDER_DATE}"
  )

  # Xcode Archiving support for distributing the application (Testflight, App Store Connect, etc...)
  set_target_properties(blender PROPERTIES
    XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
    XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
  )

  if(WITH_BLENDER_THUMBNAILER)
    set(OSX_THUMBNAILER_SOURCEDIR ${OSX_APP_SOURCEDIR}/Contents/PlugIns/blender-thumbnailer.appex)
    set_target_properties(blender-thumbnailer PROPERTIES
      BUNDLE_EXTENSION appex
      MACOSX_BUNDLE_INFO_PLIST ${OSX_THUMBNAILER_SOURCEDIR}/Contents/Info.plist
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${BLENDER_VERSION}.${BLENDER_VERSION_PATCH}"
      MACOSX_BUNDLE_LONG_VERSION_STRING "${BLENDER_VERSION}.${BLENDER_VERSION_PATCH} ${BLENDER_DATE}"
    )
  endif()

  # Gather the date in finder-style.
  execute_process(
    COMMAND date "+%m/%d/%Y/%H:%M"
    OUTPUT_VARIABLE SETFILE_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  # Give the bundle actual creation/modification date.
  #
  # Note that the directory might not yet exist, which happens when CMAKE is first run.
  if(NOT EXISTS ${EXECUTABLE_OUTPUT_PATH}/Blender.app)
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/Blender.app)
  endif()
  execute_process(
    COMMAND SetFile -d ${SETFILE_DATE} -m ${SETFILE_DATE} ${EXECUTABLE_OUTPUT_PATH}/Blender.app
  )

  set(BLENDER_BIN "bin/blender")
  install(
    TARGETS blender
    DESTINATION "."
  )

  install(
    FILES ${OSX_APP_SOURCEDIR}/Contents/PkgInfo
    DESTINATION "Blender.app/Contents"
  )

  install_dir(
    ${OSX_APP_SOURCEDIR}/Contents/Resources
    "Blender.app/Contents"
  )

  if(WITH_BLENDER_THUMBNAILER)
    install(
      TARGETS blender-thumbnailer
      DESTINATION "./Blender.app/Contents/PlugIns"
    )
  endif()

  if(PLATFORM_BUNDLED_LIBRARIES AND TARGETDIR_LIB)
    install(
      FILES ${PLATFORM_BUNDLED_LIBRARIES}
      DESTINATION ${TARGETDIR_LIB}
    )
  endif()

  if(WITH_VULKAN_BACKEND)
    install(
      FILES ${VULKAN_LIBRARY}
      DESTINATION ${TARGETDIR_LIB}
    )
  endif()

  # Python.
  if(WITH_PYTHON AND NOT WITH_PYTHON_MODULE AND NOT WITH_PYTHON_FRAMEWORK)
    # Copy the python libraries into the install directory.
    install_dir(
      ${PYTHON_LIBPATH}/python${PYTHON_VERSION}
      ${TARGETDIR_VER}/python/lib
    )

    # Install Python executable.
    install(
      PROGRAMS ${PYTHON_EXECUTABLE}
      DESTINATION ${TARGETDIR_VER}/python/bin
    )

    # Needed for `distutils/pip`.
    # Get the last part of the include dir, will be `python{version}{abiflag}`.
    get_filename_component(_py_inc_suffix ${PYTHON_INCLUDE_DIR} NAME)
    install(
      FILES ${PYTHON_INCLUDE_DIR}/pyconfig.h
      DESTINATION ${TARGETDIR_VER}/python/include/${_py_inc_suffix}
    )
    unset(_py_inc_suffix)
  endif()

  if(WITH_PYTHON_MODULE AND TARGETDIR_BPY)
    install(
      TARGETS blender
      LIBRARY DESTINATION ${TARGETDIR_BPY}
    )
  endif()

  if(NOT WITH_PYTHON_MODULE AND WITH_PYTHON AND WITH_PYTHON_INSTALL)
    get_filename_component(PYTHON_EXECUTABLE_NAME_ONLY ${PYTHON_EXECUTABLE} NAME)
    configure_file(
      ${CMAKE_SOURCE_DIR}/release/darwin/scripts/blender-system-info.sh.in
      ${CMAKE_BINARY_DIR}/release/darwin/scripts/blender-system-info.sh
      @ONLY
    )
    unset(PYTHON_EXECUTABLE_NAME_ONLY)
    install(
      PROGRAMS ${CMAKE_BINARY_DIR}/release/darwin/scripts/blender-system-info.sh
      DESTINATION "Blender.app/Contents/Resources"
    )
  endif()

endif()

# Windows already installs these, for Unix we need to pick just the VFX libs from
# the site-packages directory.
if(WITH_PYTHON_MODULE AND LIBDIR AND NOT WIN32)
  # It's possible for a build using LIBDIR to reference a non-standard Python installation.
  path_is_prefix(LIBDIR PYTHON_INCLUDE_DIR _is_python_in_libdir)
  if(_is_python_in_libdir)
    install(
      DIRECTORY ${LIBDIR}/python/lib/python${PYTHON_VERSION}/site-packages/MaterialX
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
    )
    install(
      DIRECTORY ${LIBDIR}/python/lib/python${PYTHON_VERSION}/site-packages/OpenImageIO
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
    )
    install(
      DIRECTORY ${LIBDIR}/python/lib/python${PYTHON_VERSION}/site-packages/PyOpenColorIO
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
    )
    install(
      DIRECTORY ${LIBDIR}/python/lib/python${PYTHON_VERSION}/site-packages/oslquery
      DESTINATION ${TARGETDIR_SITE_PACKAGES} OPTIONAL
    )
    install(
      DIRECTORY ${LIBDIR}/python/lib/python${PYTHON_VERSION}/site-packages/pxr
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
    )
    if(APPLE)
      set(_openvdb_filename openvdb.cpython-${PYTHON_VERSION_NO_DOTS}-darwin.so)
    else()
      set(_openvdb_filename openvdb.cpython-${PYTHON_VERSION_NO_DOTS}-${CMAKE_SYSTEM_PROCESSOR}-linux-gnu.so)
    endif()
    install(
      FILES ${LIBDIR}/python/lib/python${PYTHON_VERSION}/site-packages/${_openvdb_filename}
      DESTINATION ${TARGETDIR_SITE_PACKAGES}
    )
  endif()
  unset(_is_python_in_libdir)
endif()

# -----------------------------------------------------------------------------
# Generic Install, for all targets

if(DEFINED TARGETDIR_TEXT)

  configure_file(
    ${CMAKE_SOURCE_DIR}/release/text/readme.html
    ${CMAKE_BINARY_DIR}/release/text/readme.html
    @ONLY
  )
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_BINARY_DIR}/release/text/readme.html
  )

  install(
    FILES ${BLENDER_TEXT_FILES}
    DESTINATION "${TARGETDIR_TEXT}"
  )

  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/release/license
    DESTINATION "${TARGETDIR_TEXT}"
  )
endif()

# Create a system extensions directory (users or administrators may populate this).
# This only contains a `readme.txt` explaining it's purpose.
install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/release/extensions
  DESTINATION ${TARGETDIR_VER}
)

# Install more files specified elsewhere.
delayed_do_install(${TARGETDIR_VER})

unset(BLENDER_TEXT_FILES)
unset(TARGETDIR_TEXT)


# -----------------------------------------------------------------------------
# Geometry Icons

# Geometry icons.
get_property(_icon_names GLOBAL PROPERTY ICON_GEOM_NAMES)
set(_icon_files)
foreach(_f ${_icon_names})
  list(APPEND _icon_files
    "${CMAKE_SOURCE_DIR}/release/datafiles/icons/${_f}.dat"
  )
endforeach()
install(
  FILES ${_icon_files}
  DESTINATION ${TARGETDIR_VER}/datafiles/icons
)

unset(_icon_names)
unset(_icon_files)
unset(_f)


# -----------------------------------------------------------------------------
# Studio Lights

install(
  DIRECTORY ${CMAKE_SOURCE_DIR}/release/datafiles/studiolights
  DESTINATION ${TARGETDIR_VER}/datafiles
)


# -----------------------------------------------------------------------------
# Bundle assets

set(ASSET_BUNDLE_DIR ${CMAKE_SOURCE_DIR}/assets/)

if(EXISTS "${ASSET_BUNDLE_DIR}")
  install(
    DIRECTORY ${ASSET_BUNDLE_DIR}
    DESTINATION ${TARGETDIR_VER}/datafiles/assets
  )
endif()


# -----------------------------------------------------------------------------
# Setup link libraries

add_dependencies(blender makesdna)
target_link_libraries(blender PRIVATE ${LIB})
unset(LIB)

setup_platform_linker_flags(blender)
setup_platform_linker_libs(blender)

if(DEFINED PLATFORM_SYMBOLS_MAP)
  set_target_properties(blender PROPERTIES LINK_DEPENDS ${PLATFORM_SYMBOLS_MAP})
endif()

blender_target_include_dirs(blender ${INC})

# -----------------------------------------------------------------------------
# USD registry.

# USD requires a set of JSON files that define the standard schemas.
# These files are required at runtime.
if(WITH_USD)
  add_definitions(-DWITH_USD)
  absolute_include_dirs(../blender/io/usd)
endif()

# Always install USD shared library and `datafiles` regardless if Blender
# itself uses them, the bundled Python module still needs it.
if((DEFINED LIBDIR) AND TARGETDIR_LIB)
  # On windows the USD library sits in `./blender.shared` copy the files
  # relative to the location of the USD DLL, if the DLL does not exist
  # assume we are linking against the static 3.5 lib.
  if(WITH_USD)
    if(WIN32 AND
        (
          EXISTS ${LIBDIR}/usd/lib/usd_usd_ms.dll OR  # USD 22.03
          EXISTS ${LIBDIR}/usd/lib/usd_ms.dll         # USD 22.11
        )
      )
      install(DIRECTORY
        ${USD_LIBRARY_DIR}/usd
        DESTINATION ${TARGETDIR_LIB}
      )
      install(DIRECTORY
        ${LIBDIR}/usd/plugin/usd/hdStorm
        ${LIBDIR}/usd/plugin/usd/usdShaders
        ${LIBDIR}/usd/plugin/usd/hioOiio
        DESTINATION "blender.shared/usd"
      )
    elseif(USD_PYTHON_SUPPORT)
      install(DIRECTORY
        ${USD_LIBRARY_DIR}/usd
        DESTINATION ${TARGETDIR_LIB}
      )
      install(DIRECTORY
        ${LIBDIR}/usd/plugin/usd/hdStorm
        ${LIBDIR}/usd/plugin/usd/usdShaders
        DESTINATION ${TARGETDIR_LIB}/usd
      )
    else()
      install(DIRECTORY
        ${USD_LIBRARY_DIR}/usd
        DESTINATION "${TARGETDIR_VER}/datafiles"
      )
      install(DIRECTORY
        ${LIBDIR}/usd/plugin/usd/hdStorm
        ${LIBDIR}/usd/plugin/usd/usdShaders
        DESTINATION "${TARGETDIR_VER}/datafiles/usd"
      )
    endif()
  endif()
  if(WIN32)
    # If this file exists we are building against a 3.5 22.03 library directory
    # that needs these DLL's installed.
    if(EXISTS ${LIBDIR}/usd/lib/usd_usd_ms.dll)
      windows_install_shared_manifest(FILES
        ${LIBDIR}/usd/lib/usd_usd_ms.dll
        RELEASE
      )
      windows_install_shared_manifest(FILES
        ${LIBDIR}/usd/lib/usd_usd_ms_d.dll
        DEBUG
      )
    endif()
    # If this file exists we are building against a 3.5 22.11 library directory
    # that needs these DLL's installed.
    if(EXISTS ${LIBDIR}/usd/lib/usd_ms.dll)
      windows_install_shared_manifest(FILES
        ${LIBDIR}/usd/lib/usd_ms.dll
        RELEASE
      )
      windows_install_shared_manifest(FILES
        ${LIBDIR}/usd/lib/usd_ms_d.dll
        DEBUG
      )
    endif()
  endif()
endif()

# Always install MaterialX files regardless if Blender itself uses them, the
# bundled Python module still needs it.
if((DEFINED LIBDIR) AND TARGETDIR_LIB AND WITH_MATERIALX)
  install(
    DIRECTORY ${LIBDIR}/materialx/libraries
    DESTINATION "${TARGETDIR_LIB}/materialx"
  )
endif()

if(WIN32 AND DEFINED BOOST_LIBPATH)
  if(EXISTS ${BOOST_LIBPATH})
    set(BOOST_COMPONENTS atomic chrono date_time filesystem
      iostreams locale program_options regex
      serialization system thread wave wserialization
      python${_PYTHON_VERSION_NO_DOTS} numpy${_PYTHON_VERSION_NO_DOTS}
    )
    foreach(component ${BOOST_COMPONENTS})
      if(EXISTS ${BOOST_LIBPATH}/${BOOST_PREFIX}boost_${component}-${BOOST_POSTFIX}.dll)
        windows_install_shared_manifest(
          FILES ${BOOST_LIBPATH}/${BOOST_PREFIX}boost_${component}-${BOOST_POSTFIX}.dll
          RELEASE
        )
        windows_install_shared_manifest(
          FILES ${BOOST_LIBPATH}/${BOOST_PREFIX}boost_${component}-${BOOST_DEBUG_POSTFIX}.dll
          DEBUG
        )
      endif()
    endforeach()
  endif()
endif()

if(WIN32)
  if(WITH_CYCLES_DEVICE_HIPRT)
    if(EXISTS ${LIBDIR}/hiprt/bin/hiprt64.dll)
      install(
        FILES ${LIBDIR}/hiprt/bin/hiprt64.dll
        DESTINATION "./"
      )
    endif()
  endif()
endif()

# `vcpkg` substitutes our libraries with theirs, which will cause issues when you run
# these builds on other systems due to missing DLL's. So we opt out the use of `vcpkg`.
if(WIN32)
  set_target_properties(blender PROPERTIES VS_GLOBAL_VcpkgEnabled "false")
  set_target_properties(blender PROPERTIES
    PDB_NAME "blender_private"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
  )
  # If compiling with clang-cl we skip PDBSTRIPPED.
  # There's doesn't seem to be a switch for it currently
  if(WITH_WINDOWS_RELEASE_PDB AND WITH_WINDOWS_RELEASE_STRIPPED_PDB AND NOT MSVC_CLANG)
    # This is slightly messy, but single target generators like ninja will not have the
    # `CMAKE_CFG_INTDIR` variable and multi-target generators like `msbuild` will not have
    # `CMAKE_BUILD_TYPE`. This can be simplified by `target_link_options` and the `$<CONFIG>`
    # generator expression in newer CMAKE (2.13+) but until that time this fill have suffice.
    if(CMAKE_BUILD_TYPE)
      set_property(
        TARGET blender APPEND_STRING PROPERTY LINK_FLAGS_RELEASE
        " /PDBSTRIPPED:${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/blender_public.pdb"
      )
    else()
      set_property(
        TARGET blender APPEND_STRING PROPERTY LINK_FLAGS_RELEASE
        " /PDBSTRIPPED:${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/blender_public.pdb"
      )
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Setup launcher

if(WIN32 AND NOT WITH_PYTHON_MODULE)
  set(BLENDER_BIN "blender.exe")
  install(
    TARGETS blender blender-launcher
    COMPONENT Blender
    DESTINATION "."
  )
  if(WITH_CPU_CHECK)
    install(
      TARGETS blender_cpu_check
      COMPONENT Blender
      DESTINATION "."
    )
  endif()
  set_target_properties(
    blender
    PROPERTIES
      VS_USER_PROPS "blender.Cpp.user.props"
  )
endif()

# -----------------------------------------------------------------------------
# Windows shared library manifest
if(WIN32)
  windows_generate_shared_manifest()
endif()

# -----------------------------------------------------------------------------
# Steps that Run Blender
#
# As executing Blender is needed - it's important this operation runs after the shared
# libraries have been installed to their destination.

if(UNIX AND NOT APPLE)
  if(NOT WITH_PYTHON_MODULE)
    if(WITH_DOC_MANPAGE)
      # Only run the command to generate the man-page when it may be outdated.
      # The `IS_NEWER_THAN` checks always run when files are missing.

      # NOTE: While `${EXECUTABLE_OUTPUT_PATH}/blender` may work in some cases
      # Blender's requirement of libraries mean the installation path must be used.
      install(
        CODE "\
set(BLENDER_BIN \"${CMAKE_INSTALL_PREFIX}/${BLENDER_BIN}\")\n\
if(DEFINED ENV\{DESTDIR\})\n\
  set(BLENDER_BIN \"$ENV\{DESTDIR\}/$\{BLENDER_BIN\}\")\n\
endif()\n\
set(PYTHON_EXECUTABLE \"${PYTHON_EXECUTABLE}\")\n\
set(MANPAGE_GEN \"${CMAKE_SOURCE_DIR}/doc/manpage/blender.1.py\")\n\
set(MANPAGE_OUT \"${CMAKE_CURRENT_BINARY_DIR}/blender.1\")\n\
if(\n\
  ($\{BLENDER_BIN\} IS_NEWER_THAN $\{MANPAGE_OUT\}) OR\n\
  ($\{MANPAGE_GEN\} IS_NEWER_THAN $\{MANPAGE_OUT\})\n\
)\n\
  set(ENV{ASAN_OPTIONS} \"$ENV{ASAN_OPTIONS}:\
exitcode=0:\
check_initialization_order=0:\
strict_init_order=0:\
detect_leaks=0\"\n\
  )\n\
  execute_process(\n\
    COMMAND\n\
    $\{PYTHON_EXECUTABLE\} $\{MANPAGE_GEN\}\n\
    --blender $\{BLENDER_BIN\}\n\
    --output $\{MANPAGE_OUT\}\n\
  )\n\
endif()\n\
"
        DEPENDS blender
      )

      if(WITH_INSTALL_PORTABLE)
        install(
          FILES ${CMAKE_CURRENT_BINARY_DIR}/blender.1
          DESTINATION "."
        )
      else()
        # Manual page (only with `blender` binary).
        install(
          FILES ${CMAKE_CURRENT_BINARY_DIR}/blender.1
          DESTINATION "./share/man/man1"
        )
      endif()
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Post-install script

if(POSTINSTALL_SCRIPT)
  install(SCRIPT ${POSTINSTALL_SCRIPT})
endif()
