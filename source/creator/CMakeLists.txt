# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  ../../intern/clog
  ../../intern/guardedalloc
  ../blender/blenkernel
  ../blender/blenlib
  ../blender/blenloader
  ../blender/depsgraph
  ../blender/editors/include
  ../blender/gpu
  ../blender/imbuf
  ../blender/io/usd
  ../blender/makesdna
  ../blender/makesrna
  ../blender/render
  ../blender/windowmanager
)

set(LIB
  bf_windowmanager
)

if(HAVE_FEENABLEEXCEPT)
  add_definitions(-DHAVE_FEENABLEEXCEPT)
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
  if(WITH_CYCLES_LOGGING)
    list(APPEND INC ../../intern/cycles/blender)
    add_definitions(-DWITH_CYCLES_LOGGING)
  endif()
endif()

if(WITH_VULKAN_BACKEND)
  add_definitions(-DWITH_VULKAN_BACKEND)
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
  if(WITH_SDL_DYNLOAD)
    list(APPEND INC ../../extern/sdlew/include)
    add_definitions(-DWITH_SDL_DYNLOAD)
  endif()
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

if(WITH_OPENCOLORIO)
  add_definitions(-DWITH_OCIO)
endif()

# Setup the EXE sources and `buildinfo`.
set(SRC
  creator.c
  creator_args.c
  creator_signals.c

  creator_intern.h
)

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
    ${CMAKE_BINARY_DIR}/blender.exe.manifest
  )
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
  if(WIN32)
    add_executable(blender-launcher WIN32
      blender_launcher_win32.c
      ${CMAKE_SOURCE_DIR}/release/windows/icons/winblender.rc
      ${CMAKE_BINARY_DIR}/blender.exe.manifest
    )
    target_compile_definitions (blender-launcher PRIVATE -D_UNICODE -DUNICODE)
    target_link_libraries(blender-launcher Pathcch.lib)
  endif()
endif()

if(WITH_BUILDINFO)
  # Explicitly say that the executable depends on the `buildinfo`.
  add_dependencies(blender buildinfo)
endif()


set(BLENDER_TEXT_FILES
  ${CMAKE_SOURCE_DIR}/release/text/copyright.txt
  # Generate this file:
  # `${CMAKE_SOURCE_DIR}/release/text/readme.html`
)


# -----------------------------------------------------------------------------
# Platform specific target destinations
#
# Setup version directory, libraries, `bpy` & text files.

if(UNIX AND NOT APPLE)
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_BPY bpy)
      set(TARGETDIR_VER bpy/${BLENDER_VERSION})
      set(TARGETDIR_LIB bpy/lib)
    else()
      set(TARGETDIR_BPY ${PYTHON_SITE_PACKAGES}/bpy)
      set(TARGETDIR_VER ${PYTHON_SITE_PACKAGES}/bpy/${BLENDER_VERSION})
      set(TARGETDIR_LIB ${PYTHON_SITE_PACKAGES}/bpy/lib)
    endif()
  else()
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_VER ${BLENDER_VERSION})
      set(TARGETDIR_TEXT .)
      set(TARGETDIR_LIB lib)
    else()
      set(TARGETDIR_VER share/blender/${BLENDER_VERSION})
      set(TARGETDIR_TEXT share/doc/blender)
    endif()
  endif()

elseif(WIN32)
  if(WITH_PYTHON_MODULE)
    set(TARGETDIR_BPY ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy)
    set(TARGETDIR_VER ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy/${BLENDER_VERSION})
    # Important the DLL's are next to `__init__.pyd` otherwise it won't load.
    set(TARGETDIR_LIB ${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/bpy)
  else()
    set(TARGETDIR_VER ${BLENDER_VERSION})
    set(TARGETDIR_TEXT .)
    set(TARGETDIR_LIB .)
  endif()
elseif(APPLE)
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_BPY bpy)
      set(TARGETDIR_VER bpy/${BLENDER_VERSION})
      set(TARGETDIR_LIB bpy/lib)
    else()
      # Paths defined in terms of site-packages since the site-packages
      # directory can be a symlink (brew for example).
      set(TARGETDIR_BPY ${PYTHON_SITE_PACKAGES}/bpy)
      set(TARGETDIR_VER ${PYTHON_SITE_PACKAGES}/bpy/${BLENDER_VERSION})
      set(TARGETDIR_LIB ${PYTHON_SITE_PACKAGES}/bpy/lib)
    endif()
  else()
    set(TARGETDIR_VER Blender.app/Contents/Resources/${BLENDER_VERSION})
    set(TARGETDIR_LIB Blender.app/Contents/Resources/lib)
    set(TARGETDIR_TEXT Blender.app/Contents/Resources/text)
  endif()
  # Skip re-linking on CPACK / install.
  set_target_properties(blender PROPERTIES BUILD_WITH_INSTALL_RPATH true)
endif()


# -----------------------------------------------------------------------------
# Install Targets (Generic, All Platforms)


# Important to make a clean install each time, else old scripts get loaded.
install(
  CODE
  "file(REMOVE_RECURSE ${TARGETDIR_VER})"
)

if(WITH_PYTHON)
  # install(CODE "message(\"copying blender scripts...\")")

  # exclude addons_contrib if release branch
  if("${BLENDER_VERSION_CYCLE}" STREQUAL "release" OR
     "${BLENDER_VERSION_CYCLE}" STREQUAL "rc" OR
     "${BLENDER_VERSION_CYCLE}" STREQUAL "beta")
    set(ADDON_EXCLUDE_CONDITIONAL "addons_contrib/*")
  else()
    set(ADDON_EXCLUDE_CONDITIONAL "_addons_contrib/*")  # Dummy, won't do anything.
  endif()

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
    PATTERN ".github" EXCLUDE
    PATTERN ".arcconfig" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "site" EXCLUDE
    PATTERN "${ADDON_EXCLUDE_CONDITIONAL}" EXCLUDE
    PATTERN "${FREESTYLE_EXCLUDE_CONDITIONAL}" EXCLUDE
  )

  if(WITH_PYTHON_INSTALL)
    if(WIN32)
      install(
        FILES ${CMAKE_SOURCE_DIR}/scripts/site/sitecustomize.py
        DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
      )
    else()
      install(
        FILES ${CMAKE_SOURCE_DIR}/scripts/site/sitecustomize.py
        DESTINATION ${TARGETDIR_VER}/python/lib/python${PYTHON_VERSION}/site-packages
      )
    endif()
  endif()
  unset(ADDON_EXCLUDE_CONDITIONAL)
  unset(FREESTYLE_EXCLUDE_CONDITIONAL)
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
  if(EXISTS ${LIBDIR}/opencolorio/bin/opencolorio_2_2.dll) # 3.5
    windows_install_shared_manifest(
      FILES ${LIBDIR}/opencolorio/bin/opencolorio_2_2.dll
      RELEASE
    )
    windows_install_shared_manifest(
      FILES ${LIBDIR}/opencolorio/bin/opencolorio_d_2_2.dll
      DEBUG
    )
    install(
      FILES ${LIBDIR}/opencolorio/lib/site-packages-debug/PyOpenColorIO_d.pyd
      DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
      CONFIGURATIONS Debug
    )
    install(
      FILES ${LIBDIR}/opencolorio/lib/site-packages/PyOpenColorIO.pyd
      DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
      CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
    )
  endif()
endif()

# Helpful tip when using make.
if("${CMAKE_GENERATOR}" MATCHES ".*Makefiles.*")
  # Message to display after building.
  add_custom_command(
    TARGET blender POST_BUILD MAIN_DEPENDENCY blender
    COMMAND
      ${CMAKE_COMMAND} -E
      echo 'now run: \"make install\" to copy runtime files and scripts to ${TARGETDIR_VER}'
  )
endif()

# macro to help install files without dragging in unnecessary data.
macro(install_dir from to)
  install(
    DIRECTORY ${from}
    DESTINATION ${to}
    # Irrelevant files and caches.
    PATTERN ".git" EXCLUDE
    PATTERN ".svn" EXCLUDE
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
    install(
      TARGETS blender
      DESTINATION "."
    )

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
      install(DIRECTORY ${LIBDIR}/mesa/lib/ DESTINATION "lib/mesa/")

      install(
        PROGRAMS
        ${CMAKE_SOURCE_DIR}/release/bin/blender-softwaregl
        DESTINATION "."
      )

      # Remove from old location, so existing builds don't start with software
      # OpenGL now that the lib/ folder is used for other libraries.
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
    install(
      TARGETS blender
      DESTINATION bin
    )

    # Misc files.
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
      DESTINATION share/applications
    )
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
      DESTINATION share/icons/hicolor/scalable/apps
    )
    install(
      FILES ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/symbolic/apps/blender-symbolic.svg
      DESTINATION share/icons/hicolor/symbolic/apps
    )
    if(WITH_BLENDER_THUMBNAILER)
      install(
        TARGETS blender-thumbnailer
        DESTINATION bin
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
      # Precompiled libraries, copy over complete lib directory.
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
      # Ugh, its possible `lib64` is just a symlink to 'lib' which causes incorrect use of `lib64`.
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
          PATTERN ".svn" EXCLUDE
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
          PATTERN ".svn" EXCLUDE
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
        # distros are happy with their policy.
        set(_suffix "site-packages")
        if(${PYTHON_USD_PATH} MATCHES "dist-packages")
          set(_suffix "dist-packages")
        endif()
        install(
          DIRECTORY ${USD_LIBRARY_DIR}/python/
          DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}/python${PYTHON_VERSION}/${_suffix}
          PATTERN ".svn" EXCLUDE
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
          PATTERN ".svn" EXCLUDE
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
          PATTERN ".svn" EXCLUDE
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
              PATTERN ".svn" EXCLUDE
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
  endif()

  if(WITH_DRACO)
    install(
      PROGRAMS $<TARGET_FILE:extern_draco>
      DESTINATION ${TARGETDIR_VER}/python/lib/python${PYTHON_VERSION}/site-packages
    )
  endif()
elseif(WIN32)
  windows_install_shared_manifest(
    FILES ${LIBDIR}/epoxy/bin/epoxy-0.dll
    ALL
  )

  if(WITH_OPENMP AND MSVC_CLANG)
    windows_install_shared_manifest(
      FILES ${CLANG_OPENMP_DLL}
      ALL
    )
  endif()

  windows_install_shared_manifest(
    FILES ${LIBDIR}/fftw3/lib/libfftw3-3.dll
    ALL
  )
  if(MSVC_ASAN)
    # The ASAN DLL's can be found in the same folder as the compiler,
    # this is the easiest way to find these.
    string(REPLACE "cl.exe" "clang_rt.asan_dynamic-x86_64.dll" ASAN_DLL ${CMAKE_C_COMPILER})
    string(REPLACE "cl.exe" "clang_rt.asan_dbg_dynamic-x86_64.dll" ASAN_DEBUG_DLL ${CMAKE_C_COMPILER})
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
  windows_install_shared_manifest(
    FILES ${LIBDIR}/gmp/lib/libgmp-10.dll
    ALL
  )
  windows_install_shared_manifest(
    FILES ${LIBDIR}/gmp/lib/libgmpxx.dll
    RELEASE
  )
  windows_install_shared_manifest(
    FILES ${LIBDIR}/gmp/lib/libgmpxx_d.dll
    DEBUG
  )

  if(WITH_WINDOWS_PDB)
    if(WITH_WINDOWS_STRIPPED_PDB)
      # Icky hack for older CMAKE from https://stackoverflow.com/a/21198501
      # `$<CONFIG>` will work in newer CMAKE but the version currently (3.12)
      # on the build-bot does not support this endeavor.
      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/blender_public.pdb
        DESTINATION .
        RENAME blender.pdb
      )
    else()
      install(FILES $<TARGET_PDB_FILE:blender> DESTINATION . RENAME blender.pdb)
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

  # This will not exist for 3.4 and earlier lib folders
  # to ease the transition, support both 3.4 and 3.5 lib
  # folders.
  if(EXISTS ${LIBDIR}/openvdb/python/pyopenvdb_d.pyd)
    install(
      FILES ${LIBDIR}/openvdb/python/pyopenvdb_d.pyd
      DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
      CONFIGURATIONS Debug
    )
    install(
      FILES ${LIBDIR}/openvdb/python/pyopenvdb.pyd
      DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
      CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
    )
  endif()

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
          DESTINATION ${TARGETDIR_LIB}
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )

        install(
          FILES
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python${_PYTHON_VERSION_NO_DOTS}_d.dll
            ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python3_d.dll
          DESTINATION ${TARGETDIR_LIB}
          CONFIGURATIONS Debug
        )
      endif()
    endif()

    if(WITH_PYTHON_INSTALL)
      # NOTE: as far as python is concerned `RelWithDebInfo`
      # is not debug since its without debug flags.

      install(DIRECTORY DESTINATION ${TARGETDIR_VER}/python)
      install(DIRECTORY DESTINATION ${TARGETDIR_VER}/python/lib)

      install(
        DIRECTORY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/lib
        DESTINATION ${BLENDER_VERSION}/python/
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        PATTERN ".svn" EXCLUDE
        PATTERN "*_d.*" EXCLUDE                 # * debug libraries *
        PATTERN "__pycache__" EXCLUDE           # * any cache *
        PATTERN "*.pyc" EXCLUDE                 # * any cache *
        PATTERN "*.pyo" EXCLUDE                 # * any cache *
      )

      install(
        DIRECTORY ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/lib
        DESTINATION ${BLENDER_VERSION}/python/
        CONFIGURATIONS Debug
        PATTERN ".svn" EXCLUDE
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
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python.exe
        DESTINATION ${BLENDER_VERSION}/python/bin
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
      )
      install(
        FILES
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python${_PYTHON_VERSION_NO_DOTS}_d.dll
          ${LIBDIR}/python/${_PYTHON_VERSION_NO_DOTS}/bin/python_d.exe
        DESTINATION ${BLENDER_VERSION}/python/bin
        CONFIGURATIONS Debug
      )

      if(EXISTS ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}/site-packages) #this will only exist for 3.5+
        install(
          DIRECTORY ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}/site-packages/
          DESTINATION ${TARGETDIR_VER}/python/lib/site-packages/
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
          PATTERN ".svn" EXCLUDE
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()
      if(EXISTS ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}_debug/site-packages)
        install(
          DIRECTORY ${LIBDIR}/openimageio/lib/python${PYTHON_VERSION}_debug/site-packages/
          DESTINATION ${TARGETDIR_VER}/python/lib/site-packages/
          CONFIGURATIONS Debug
          PATTERN ".svn" EXCLUDE
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()

      # This will not exist for 3.4 and earlier lib folders
      # to ease the transition, support both 3.4 and 3.5 lib
      # folders.
      if(EXISTS ${USD_LIBRARY_DIR}/python/)
        install(
          DIRECTORY ${USD_LIBRARY_DIR}/python/
          DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
          PATTERN ".svn" EXCLUDE
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()
      if(EXISTS ${USD_LIBRARY_DIR}/debug/python/)
        install(
          DIRECTORY ${USD_LIBRARY_DIR}/debug/python/
          DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
          CONFIGURATIONS Debug
          PATTERN ".svn" EXCLUDE
          PATTERN "__pycache__" EXCLUDE           # * any cache *
          PATTERN "*.pyc" EXCLUDE                 # * any cache *
          PATTERN "*.pyo" EXCLUDE                 # * any cache *
        )
      endif()

      # MaterialX python bindings
      install(
        DIRECTORY ${LIBDIR}/materialx/python/Release/MaterialX
        DESTINATION ${TARGETDIR_VER}/python/lib/site-packages/
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        PATTERN ".svn" EXCLUDE
        PATTERN "__pycache__" EXCLUDE           # * any cache *
        PATTERN "*.pyc" EXCLUDE                 # * any cache *
        PATTERN "*.pyo" EXCLUDE                 # * any cache *
      )
      install(
        DIRECTORY ${LIBDIR}/materialx/python/Debug/MaterialX
        DESTINATION ${TARGETDIR_VER}/python/lib/site-packages/
        CONFIGURATIONS Debug
        PATTERN ".svn" EXCLUDE
        PATTERN "__pycache__" EXCLUDE           # * any cache *
        PATTERN "*.pyc" EXCLUDE                 # * any cache *
        PATTERN "*.pyo" EXCLUDE                 # * any cache *
      )

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

  # Filenames change slightly between FFMPEG versions check both 5.0 and fallback to 4.4
  # to ease the transition between versions.
  if(EXISTS "${LIBDIR}/ffmpeg/lib/avcodec-59.dll")
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
  else()
    windows_install_shared_manifest(
      FILES
        ${LIBDIR}/ffmpeg/lib/avcodec-58.dll
        ${LIBDIR}/ffmpeg/lib/avformat-58.dll
        ${LIBDIR}/ffmpeg/lib/avdevice-58.dll
        ${LIBDIR}/ffmpeg/lib/avutil-56.dll
        ${LIBDIR}/ffmpeg/lib/swscale-5.dll
        ${LIBDIR}/ffmpeg/lib/swresample-3.dll
      ALL
    )
  endif()
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

  windows_install_shared_manifest(
    FILES ${LIBDIR}/sndfile/lib/libsndfile-1.dll
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
    install(
      FILES
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_gpu.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_gpu_glitchworkaround.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_log.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_factory_startup.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_oculus.cmd
        ${CMAKE_SOURCE_DIR}/release/windows/batch/oculus.json
      DESTINATION ${TARGETDIR_LIB}
    )
  endif()

  if(WITH_BLENDER_THUMBNAILER)
    install(
      TARGETS BlendThumb
      DESTINATION "."
    )
  endif()

  if(WITH_DRACO)
    install(
      PROGRAMS $<TARGET_FILE:extern_draco>
      DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
    )
  endif()

  if(WITH_PYTHON_MODULE AND TARGETDIR_BPY)
    install(
      TARGETS blender
      LIBRARY DESTINATION ${TARGETDIR_BPY}
    )
  endif()

  if(PLATFORM_BUNDLED_LIBRARIES)
    install(
      FILES ${PLATFORM_BUNDLED_LIBRARIES}
      DESTINATION ${TARGETDIR_LIB}
    )
  endif()
elseif(APPLE)
  if(NOT WITH_PYTHON_MODULE)
    # Uppercase name for app bundle.
    set_target_properties(blender PROPERTIES OUTPUT_NAME Blender)
  endif()

  # Handy install macro to exclude files, we use \$ escape for the "to"
  # argument when calling so `${BUILD_TYPE}` does not get expanded.
  macro(install_dir from to)
    install(
      DIRECTORY ${from}
      DESTINATION ${to}
      PATTERN ".git" EXCLUDE
      PATTERN ".svn" EXCLUDE
      PATTERN "*.pyc" EXCLUDE
      PATTERN "*.pyo" EXCLUDE
      PATTERN "*.orig" EXCLUDE
      PATTERN "*.rej" EXCLUDE
      PATTERN "__pycache__" EXCLUDE
      PATTERN "__MACOSX" EXCLUDE
      PATTERN ".DS_Store" EXCLUDE
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

  install(
    TARGETS blender
    DESTINATION "."
  )

  install(
    FILES ${OSX_APP_SOURCEDIR}/Contents/PkgInfo
    DESTINATION Blender.app/Contents
  )

  install_dir(
    ${OSX_APP_SOURCEDIR}/Contents/Resources
    Blender.app/Contents/
  )

  if(WITH_BLENDER_THUMBNAILER)
    install(
      TARGETS blender-thumbnailer
      DESTINATION Blender.app/Contents/MacOS/
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
      FILES ${VULKAN_LIBRARY} ${MOLTENVK_LIBRARY}
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

  if(WITH_DRACO)
    install(
      PROGRAMS $<TARGET_FILE:extern_draco>
      DESTINATION ${TARGETDIR_VER}/python/lib/python${PYTHON_VERSION}/site-packages
    )
  endif()
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

set(ASSET_BUNDLE_DIR ${CMAKE_SOURCE_DIR}/../lib/assets/publish/)

if(EXISTS "${ASSET_BUNDLE_DIR}")
  install(
    DIRECTORY ${ASSET_BUNDLE_DIR}
    DESTINATION ${TARGETDIR_VER}/datafiles/assets
    PATTERN ".svn" EXCLUDE
  )
endif()


# -----------------------------------------------------------------------------
# Setup link libraries

add_dependencies(blender makesdna)
target_link_libraries(blender ${LIB})
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

# Always install USD shared library and datafiles regardless if Blender
# itself uses them, the bundled Python module still needs it.
if((DEFINED LIBDIR) AND TARGETDIR_LIB)
  # On windows the usd library sits in ./blender.shared copy the files
  # relative to the location of the USD dll, if the dll does not exist
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
        DESTINATION "./blender.shared"
      )
    elseif(USD_PYTHON_SUPPORT)
      install(DIRECTORY
        ${USD_LIBRARY_DIR}/usd
        DESTINATION ${TARGETDIR_LIB}
      )
    else()
      install(DIRECTORY
        ${USD_LIBRARY_DIR}/usd
        DESTINATION "${TARGETDIR_VER}/datafiles"
      )
    endif()
  endif()
  if(WIN32)
    # If this file exists we are building against a 3.5 22.03 library folder
    # that needs these dll's installed.
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
    # If this file exists we are building against a 3.5 22.11 library folder
    # that needs these dll's installed.
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
if((DEFINED LIBDIR) AND TARGETDIR_LIB AND WITH_MATERIALX )
  install(
    DIRECTORY ${LIBDIR}/materialx/libraries
    DESTINATION "${TARGETDIR_LIB}/materialx"
  )
endif()

if(WIN32)
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

# `vcpkg` substitutes our libraries with theirs, which will cause issues when you you run
# these builds on other systems due to missing DLL's. So we opt out the use of `vcpkg`.
if(WIN32)
  set_target_properties(blender PROPERTIES VS_GLOBAL_VcpkgEnabled "false")
  set_target_properties(blender PROPERTIES
    PDB_NAME "blender_private"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
  )
  if(WITH_WINDOWS_PDB AND WITH_WINDOWS_STRIPPED_PDB)
    # This is slightly messy, but single target generators like ninja will not have the
    # `CMAKE_CFG_INTDIR` variable and multi-target generators like `msbuild` will not have
    # `CMAKE_BUILD_TYPE`. This can be simplified by `target_link_options` and the `$<CONFIG>`
    # generator expression in newer CMAKE (2.13+) but until that time this fill have suffice.
    if(CMAKE_BUILD_TYPE)
      set_property(
        TARGET blender APPEND_STRING PROPERTY LINK_FLAGS
        " /PDBSTRIPPED:${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/blender_public.pdb"
      )
    else()
      set_property(
        TARGET blender APPEND_STRING PROPERTY LINK_FLAGS
        " /PDBSTRIPPED:${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/blender_public.pdb"
      )
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Setup launcher

if(WIN32 AND NOT WITH_PYTHON_MODULE)
  install(
    TARGETS blender blender-launcher
    COMPONENT Blender
    DESTINATION "."
  )
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
      install(
        CODE "\
execute_process(\
  COMMAND \
  \"${CMAKE_SOURCE_DIR}/doc/manpage/blender.1.py\" \
  --blender \"${EXECUTABLE_OUTPUT_PATH}/blender\" \
  --output \"${CMAKE_CURRENT_BINARY_DIR}/blender.1\"\
)"
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
          DESTINATION share/man/man1
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
