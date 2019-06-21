# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

setup_libdirs()

blender_include_dirs(
  ../../intern/clog
  ../../intern/glew-mx
  ../../intern/guardedalloc
  ../blender/blenkernel
  ../blender/blenlib
  ../blender/blenloader
  ../blender/depsgraph
  ../blender/editors/include
  ../blender/imbuf
  ../blender/makesrna
  ../blender/render/extern/include
  ../blender/gpu
  ../blender/makesdna
  ../blender/windowmanager
)

set(LIB
  bf_blenloader
  bf_blenkernel
  bf_blenlib
  bf_depsgraph
  bf_dna
  bf_editor_datafiles
  bf_imbuf
  bf_intern_clog
  bf_intern_guardedalloc
  bf_intern_memutil
  bf_intern_opencolorio
  bf_python
  bf_render
  bf_rna
  bf_windowmanager
)

add_definitions(${GL_DEFINITIONS})
blender_include_dirs("${GLEW_INCLUDE_PATH}")

if(WIN32)
  blender_include_dirs(../../intern/utfconv)
endif()

if(WITH_LIBMV)
  blender_include_dirs(../../intern/libmv)
  add_definitions(-DWITH_LIBMV)
endif()

if(WITH_CYCLES)
  if(WITH_CYCLES_LOGGING)
    blender_include_dirs(../../intern/cycles/blender)
    add_definitions(-DWITH_CYCLES_LOGGING)
  endif()
  list(APPEND LIB
    bf_intern_cycles
  )
endif()

if(WITH_CODEC_FFMPEG)
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_PYTHON)
  blender_include_dirs(../blender/python)
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
    blender_include_dirs(../../extern/sdlew/include)
    add_definitions(-DWITH_SDL_DYNLOAD)
  endif()
  add_definitions(-DWITH_SDL)
endif()

if(WITH_BINRELOC)
  blender_include_dirs(${BINRELOC_INCLUDE_DIRS})
  list(APPEND LIB
    extern_binreloc
  )
  add_definitions(-DWITH_BINRELOC)
endif()

if(WITH_FREESTYLE)
  blender_include_dirs(../blender/freestyle)
  list(APPEND LIB
    bf_freestyle
  )
  add_definitions(-DWITH_FREESTYLE)
endif()

# Setup the exe sources and buildinfo
set(SRC
  creator.c
  creator_args.c
  creator_signals.c

  creator_intern.h
)

# MSVC 2010 gives linking errors with the manifest
if(WIN32 AND NOT UNIX)
  string(SUBSTRING ${BLENDER_VERSION} 0 1 bver1)
  string(SUBSTRING ${BLENDER_VERSION} 2 1 bver2)
  string(SUBSTRING ${BLENDER_VERSION} 3 1 bver3)
  add_definitions(
    -DBLEN_VER_RC_STR=${BLENDER_VERSION}
    -DBLEN_VER_RC_1=${bver1}
    -DBLEN_VER_RC_2=${bver2}
    -DBLEN_VER_RC_3=${bver3}
    -DBLEN_VER_RC_4=0
  )


  list(APPEND SRC
    ${CMAKE_SOURCE_DIR}/release/windows/icons/winblender.rc
  )
endif()

if(WITH_BUILDINFO)
  add_definitions(-DWITH_BUILDINFO)
  # --------------------------------------------------------------------------
  # These defines could all be moved into the header below
  string(REPLACE " " "\ " BUILDINFO_CFLAGS "${CMAKE_C_FLAGS}")
  string(REPLACE " " "\ " BUILDINFO_CXXFLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE " " "\ " BUILDINFO_LINKFLAGS "${PLATFORM_LINKFLAGS}")
  add_definitions(
    # # define in header now, else these get out of date on rebuilds.
    # -DBUILD_DATE="${BUILD_DATE}"
    # -DBUILD_TIME="${BUILD_TIME}"
    # -DBUILD_COMMIT_TIMESTAMP="${BUILD_COMMIT_TIMESTAMP}"
    # -DBUILD_COMMIT_TIME="${BUILD_COMMIT_TIME}"
    # -DBUILD_COMMIT_DATE="${BUILD_COMMIT_DATE}"
    # -DBUILD_HASH="${BUILD_HASH}"
    # -DBUILD_BRANCH="${BUILD_BRANCH}"
    -DWITH_BUILDINFO_HEADER # alternative to lines above
    -DBUILD_PLATFORM="${CMAKE_SYSTEM_NAME}"
    -DBUILD_TYPE="${CMAKE_BUILD_TYPE}"
    -DBUILD_CFLAGS="${BUILDINFO_CFLAGS}"
    -DBUILD_CXXFLAGS="${BUILDINFO_CXXFLAGS}"
    -DBUILD_LINKFLAGS="${BUILDINFO_LINKFLAGS}"
    -DBUILD_SYSTEM="CMake"
  )

  # --------------------------------------------------------------------------
  # write header for values that change each build
  # note, generaed file is in build dir's source/creator
  #       except when used as an include path.

  # include the output directory, where the buildinfo.h file is generated
  include_directories(${CMAKE_CURRENT_BINARY_DIR})


  # XXX, ${buildinfo_h_fake} is used here,
  # because we rely on that file being detected as missing
  # every build so that the real header "buildinfo.h" is updated.
  #
  # Keep this until we find a better way to resolve!

  set(buildinfo_h_real "${CMAKE_CURRENT_BINARY_DIR}/buildinfo.h")
  set(buildinfo_h_fake "${CMAKE_CURRENT_BINARY_DIR}/buildinfo.h_fake")

  if(EXISTS ${buildinfo_h_fake})
    message(FATAL_ERROR "File \"${buildinfo_h_fake}\" found, this should never be created, remove!")
  endif()

  # a custom target that is always built
  add_custom_target(buildinfo ALL
    DEPENDS ${buildinfo_h_fake})

  # creates buildinfo.h using cmake script
  add_custom_command(
    OUTPUT
      ${buildinfo_h_fake}  # ensure we always run
      ${buildinfo_h_real}
    COMMAND ${CMAKE_COMMAND}
    -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
    # overrides only used when non-empty strings
    -DBUILD_DATE=${BUILDINFO_OVERRIDE_DATE}
    -DBUILD_TIME=${BUILDINFO_OVERRIDE_TIME}
    -P ${CMAKE_SOURCE_DIR}/build_files/cmake/buildinfo.cmake)

  # buildinfo.h is a generated file
  set_source_files_properties(
    ${buildinfo_h_real}
    PROPERTIES GENERATED TRUE
    HEADER_FILE_ONLY TRUE)

  unset(buildinfo_h_real)
  unset(buildinfo_h_fake)

  # add deps below, after adding blender
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

  # creates ./bin/bpy.so which can be imported as a python module.
  #
  # note that 'SHARED' works on Linux and Windows,
  # but not OSX which _must_ be 'MODULE'
  add_library(blender MODULE ${SRC})
  set_target_properties(
    blender
    PROPERTIES
      PREFIX ""
      OUTPUT_NAME bpy
      LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
      RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin  # only needed on windows
  )

  if(APPLE)
    set_target_properties(
      blender
      PROPERTIES
        MACOSX_BUNDLE TRUE
        LINK_FLAGS_RELEASE "${PLATFORM_LINKFLAGS}"
        LINK_FLAGS_DEBUG "${PLATFORM_LINKFLAGS_DEBUG}"
    )
  endif()

  if(WIN32)
    # python modules use this
    set_target_properties(
      blender
      PROPERTIES
      SUFFIX ".pyd"
    )
  endif()

else()
  add_executable(blender ${EXETYPE} ${SRC})
  WINDOWS_SIGN_TARGET(blender)
endif()

if(WITH_BUILDINFO)
  # explicitly say that the executable depends on the buildinfo
  add_dependencies(blender buildinfo)
endif()


set(BLENDER_TEXT_FILES
  ${CMAKE_SOURCE_DIR}/release/text/GPL-license.txt
  ${CMAKE_SOURCE_DIR}/release/text/GPL3-license.txt
  ${CMAKE_SOURCE_DIR}/release/text/copyright.txt
  # generate this file
  # ${CMAKE_SOURCE_DIR}/release/text/readme.html
  ${CMAKE_SOURCE_DIR}/release/datafiles/LICENSE-bfont.ttf.txt
)

if(WITH_PYTHON)
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_SOURCE_DIR}/release/text/Python-license.txt
  )
endif()

if(WITH_OPENCOLORIO)
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_SOURCE_DIR}/release/text/ocio-license.txt
  )
endif()

if(WITH_MEM_JEMALLOC)
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_SOURCE_DIR}/release/text/jemalloc-license.txt
  )
endif()

if(WITH_INTERNATIONAL)
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_SOURCE_DIR}/release/datafiles/LICENSE-droidsans.ttf.txt
    ${CMAKE_SOURCE_DIR}/release/datafiles/LICENSE-bmonofont-i18n.ttf.txt
  )
endif()


# -----------------------------------------------------------------------------
# Platform Specific Var: TARGETDIR_VER

if(UNIX AND NOT APPLE)
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_VER ${BLENDER_VERSION})
    else()
      set(TARGETDIR_VER ${PYTHON_SITE_PACKAGES}/${BLENDER_VERSION})
    endif()
  else()
    if(WITH_INSTALL_PORTABLE)
      set(TARGETDIR_VER ${BLENDER_VERSION})
    else()
      set(TARGETDIR_VER share/blender/${BLENDER_VERSION})
    endif()
  endif()

elseif(WIN32)
  set(TARGETDIR_VER ${BLENDER_VERSION})

elseif(APPLE)
  if(WITH_PYTHON_MODULE)
    set(TARGETDIR_VER ${BLENDER_VERSION})
  else()
    set(TARGETDIR_VER Blender.app/Contents/Resources/${BLENDER_VERSION})
  endif()
  # Skip relinking on cpack / install
  set_target_properties(blender PROPERTIES BUILD_WITH_INSTALL_RPATH true)
endif()


# -----------------------------------------------------------------------------
# Install Targets (Generic, All Platforms)


# important to make a clean  install each time, else old scripts get loaded.
install(
  CODE
  "file(REMOVE_RECURSE ${TARGETDIR_VER})"
)

if(WITH_PYTHON)
  # install(CODE "message(\"copying blender scripts...\")")

  # exclude addons_contrib if release
  if("${BLENDER_VERSION_CYCLE}" STREQUAL "release" OR
     "${BLENDER_VERSION_CYCLE}" STREQUAL "rc")
    set(ADDON_EXCLUDE_CONDITIONAL "addons_contrib/*")
  else()
    set(ADDON_EXCLUDE_CONDITIONAL "_addons_contrib/*")  # dummy, wont do anything
  endif()

  # do not install freestyle dir if disabled
  if(NOT WITH_FREESTYLE)
    set(FREESTYLE_EXCLUDE_CONDITIONAL "freestyle/*")
  else()
    set(FREESTYLE_EXCLUDE_CONDITIONAL "_freestyle/*")  # dummy, wont do anything
  endif()

  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/release/scripts
    DESTINATION ${TARGETDIR_VER}
    PATTERN ".git" EXCLUDE
    PATTERN ".gitignore" EXCLUDE
    PATTERN ".arcconfig" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
    PATTERN "${ADDON_EXCLUDE_CONDITIONAL}" EXCLUDE
    PATTERN "${FREESTYLE_EXCLUDE_CONDITIONAL}" EXCLUDE
  )

  unset(ADDON_EXCLUDE_CONDITIONAL)
  unset(FREESTYLE_EXCLUDE_CONDITIONAL)
endif()

# localization
if(WITH_INTERNATIONAL)
  install(
    DIRECTORY
      ${CMAKE_SOURCE_DIR}/release/datafiles/fonts
    DESTINATION ${TARGETDIR_VER}/datafiles
  )

  set(_locale_dir "${CMAKE_SOURCE_DIR}/release/datafiles/locale")
  set(_locale_target_dir ${TARGETDIR_VER}/datafiles/locale)

  file(GLOB _po_files "${_locale_dir}/po/*.po")
  foreach(_po_file ${_po_files})
    msgfmt_simple(${_po_file} _all_mo_files)
  endforeach()

  # Create a custom target which will compile all po to mo
  add_custom_target(
    locales
    DEPENDS ${_all_mo_files})

  add_dependencies(blender locales)

  # Generate INSTALL rules
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

# color management
if(WITH_OPENCOLORIO)
  install(
    DIRECTORY ${CMAKE_SOURCE_DIR}/release/datafiles/colormanagement
    DESTINATION ${TARGETDIR_VER}/datafiles
  )
endif()

# helpful tip when using make
if("${CMAKE_GENERATOR}" MATCHES ".*Makefiles.*")
  # message after building.
  add_custom_command(
    TARGET blender POST_BUILD MAIN_DEPENDENCY blender
    COMMAND ${CMAKE_COMMAND} -E
            echo 'now run: \"make install\" to copy runtime files and scripts to ${TARGETDIR_VER}'
  )
endif()


# -----------------------------------------------------------------------------
# Install Targets (Platform Specific)

if(UNIX AND NOT APPLE)

  if(NOT WITH_PYTHON_MODULE)
    if(WITH_DOC_MANPAGE)
      add_custom_target(
        blender_man_page ALL
        COMMAND ${CMAKE_SOURCE_DIR}/doc/manpage/blender.1.py
                ${EXECUTABLE_OUTPUT_PATH}/blender
                ${CMAKE_CURRENT_BINARY_DIR}/blender.1)
      add_dependencies(blender_man_page blender)
    endif()
  endif()

  # there are a few differences between portable and system install
  if(WITH_PYTHON_MODULE)
    if(WITH_INSTALL_PORTABLE)
      install(
        TARGETS blender
        DESTINATION "."
      )
    else()
      install(
        TARGETS blender
        LIBRARY DESTINATION ${PYTHON_SITE_PACKAGES}
      )
    endif()
    # none of the other files are needed currently
  elseif(WITH_INSTALL_PORTABLE)
    install(
      TARGETS blender
      DESTINATION "."
    )

    if(WITH_DOC_MANPAGE)
      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/blender.1
        DESTINATION "."
      )
    endif()
    install(
      FILES
        ${CMAKE_SOURCE_DIR}/release/freedesktop/blender.desktop
        ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/scalable/apps/blender.svg
        ${CMAKE_SOURCE_DIR}/release/freedesktop/icons/symbolic/apps/blender-symbolic.svg
      DESTINATION "."
    )

    install(
      PROGRAMS
      ${CMAKE_SOURCE_DIR}/release/bin/blender-thumbnailer.py
      DESTINATION "."
    )

    set(BLENDER_TEXT_FILES_DESTINATION ".")
  else()
    # main blender binary
    install(
      TARGETS blender
      DESTINATION bin
    )
    if(WITH_DOC_MANPAGE)
      # manpage only with 'blender' binary
      install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/blender.1
        DESTINATION share/man/man1
      )
    endif()

    # misc files
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
    install(
      PROGRAMS ${CMAKE_SOURCE_DIR}/release/bin/blender-thumbnailer.py
      DESTINATION bin
    )
    set(BLENDER_TEXT_FILES_DESTINATION share/doc/blender)
  endif()

  if(WITH_PYTHON)
    if(WITH_PYTHON_INSTALL)

      install(
        PROGRAMS ${PYTHON_EXECUTABLE}
        DESTINATION ${TARGETDIR_VER}/python/bin
      )

      # on some platforms (like openSUSE) Python is linked
      # to be used from lib64 folder.
      # determine this from Python's libraries path
      #
      # ugh, its possible 'lib64' is just a symlink to 'lib' which causes incorrect use of 'lib64'
      get_filename_component(_pypath_real ${PYTHON_LIBPATH} REALPATH)
      if(${_pypath_real} MATCHES "lib64$")
        set(_target_LIB "lib64")
      else()
        set(_target_LIB "lib")
      endif()
      unset(_pypath_real)

      # Copy the systems python into the install directory
      # install(CODE "message(\"copying a subset of the systems python...\")")
      install(
        DIRECTORY ${PYTHON_LIBPATH}/python${PYTHON_VERSION}
        DESTINATION ${TARGETDIR_VER}/python/${_target_LIB}
        PATTERN "__pycache__" EXCLUDE               # * any cache *
        PATTERN "config-${PYTHON_VERSION}m/*.a" EXCLUDE  # static lib
        PATTERN "lib2to3" EXCLUDE                   # ./lib2to3
        PATTERN "site-packages/*" EXCLUDE           # ./site-packages/*
        PATTERN "tkinter" EXCLUDE                   # ./tkinter
        PATTERN "lib-dynload/_tkinter.*" EXCLUDE    # ./lib-dynload/_tkinter.co
        PATTERN "idlelib" EXCLUDE                   # ./idlelib
        PATTERN "test" EXCLUDE                      # ./test
        PATTERN "turtledemo" EXCLUDE                # ./turtledemo
        PATTERN "turtle.py" EXCLUDE                 # ./turtle.py
      )

      # Needed for distutils/pip
      # get the last part of the include dir, will be 'python{version}{abiflag}',
      get_filename_component(_py_inc_suffix ${PYTHON_INCLUDE_DIR} NAME)
      install(
        FILES ${PYTHON_INCLUDE_DIR}/pyconfig.h
        DESTINATION ${TARGETDIR_VER}/python/include/${_py_inc_suffix}
      )
      unset(_py_inc_suffix)

      if(WITH_PYTHON_INSTALL_NUMPY)
        # Install to the same directory as the source, so debian-like
        # distros are happy with their policy.
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
          PATTERN "include" EXCLUDE               # include dirs all over, we wont use NumPy/CAPI
          PATTERN "*.h" EXCLUDE                   # some includes are not in include dirs
          PATTERN "*.a" EXCLUDE                   # ./core/lib/libnpymath.a - for linking, we dont need.
        )
        unset(_suffix)
      endif()

      # Copy requests, we need to generalize site-packages
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
        set(_requests_deps "certifi" "chardet" "idna" "urllib3")
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

  set(BLENDER_TEXT_FILES_DESTINATION ".")

  if(WITH_PYTHON)
    string(REPLACE "." "" _PYTHON_VERSION_NO_DOTS ${PYTHON_VERSION})

    if(NOT CMAKE_COMPILER_IS_GNUCC)
      install(
        FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.dll
        DESTINATION "."
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
      )

      install(
        FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}_d.dll
        DESTINATION "."
        CONFIGURATIONS Debug
      )
    endif()

    if(WITH_PYTHON_INSTALL)
      # note, as far as python is concerned 'RelWithDebInfo' is not debug since its without debug flags.

      install(DIRECTORY DESTINATION ${TARGETDIR_VER}/python)
      install(DIRECTORY DESTINATION ${TARGETDIR_VER}/python/lib)

      # WARNING: its important that 'CMAKE_INSTALL_CONFIG_NAME' is evaluated at build time
      # and _NOT_ configuration time, when modifying the lines below,
      # check it works in both Release & Debug mode.
      #
      # Edit with extreme care! - Campbell

      # extract python
      install(
        CODE
        "
        message(STATUS \"Extracting Python to: \${CMAKE_INSTALL_PREFIX}/${BLENDER_VERSION}/python\")
        if(\"\${CMAKE_INSTALL_CONFIG_NAME}\" MATCHES \"^([Dd][Ee][Bb][Uu][Gg])$\")
          set(PYTHON_ZIP \"${LIBDIR}/release/python${_PYTHON_VERSION_NO_DOTS}_d.tar.gz\")
        else()
          set(PYTHON_ZIP \"${LIBDIR}/release/python${_PYTHON_VERSION_NO_DOTS}.tar.gz\")
        endif()

        execute_process(
          COMMAND \${CMAKE_COMMAND} -E make_directory
                  \"\${CMAKE_INSTALL_PREFIX}/${BLENDER_VERSION}/python\"
          COMMAND \${CMAKE_COMMAND} -E
                  chdir \"\${CMAKE_INSTALL_PREFIX}/${BLENDER_VERSION}/python\"
                  \${CMAKE_COMMAND} -E
                  tar xzfv \"\${PYTHON_ZIP}\"
        )
        unset(PYTHON_ZIP)
        "
      )

      # release/site-packages
      install(
        DIRECTORY ${LIBDIR}/release/site-packages
        DESTINATION ${BLENDER_VERSION}/python/lib
        PATTERN ".svn" EXCLUDE
        PATTERN "__pycache__" EXCLUDE           # * any cache *
        PATTERN "*.pyc" EXCLUDE                 # * any cache *
        PATTERN "*.pyo" EXCLUDE                 # * any cache *)
      )

      if(WITH_PYTHON_INSTALL_NUMPY)
        set(PYTHON_NUMPY_VERSION 1.15)
        add_custom_command(
          OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${BLENDER_VERSION}/python/lib/site-packages/numpy/__init__.py
          COMMAND ${CMAKE_COMMAND} -E
                  tar xzvf "${LIBDIR}/release/python${_PYTHON_VERSION_NO_DOTS}_numpy_${PYTHON_NUMPY_VERSION}$<$<CONFIG:Debug>:d>.tar.gz"
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${BLENDER_VERSION}/python/lib/site-packages
        )
        add_custom_target(
          python_numpy ALL
          DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${BLENDER_VERSION}/python/lib/site-packages/numpy/__init__.py
        )
        install(
          DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${BLENDER_VERSION}/python/lib/site-packages/numpy
          DESTINATION ${BLENDER_VERSION}/python/lib/site-packages
        )
      endif()


      # TODO(sergey): For unti we've got better way to deal with python binary
      install(
        FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.dll
        DESTINATION ${BLENDER_VERSION}/python/bin
        CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
      )
      install(
        FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}_d.dll
        DESTINATION ${BLENDER_VERSION}/python/bin
        CONFIGURATIONS Debug
      )
      if(WINDOWS_PYTHON_DEBUG)
        install(
          FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}.pdb
          DESTINATION "."
          CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
        )

        install(
          FILES ${LIBDIR}/python/lib/python${_PYTHON_VERSION_NO_DOTS}_d.pdb
          DESTINATION "."
          CONFIGURATIONS Debug
        )
      endif()
    endif()

    unset(_PYTHON_VERSION_NO_DOTS)
  endif()

  # EGL Runtime Components
  if(WITH_GL_EGL)
    if(WIN32)
      install(FILES "${OPENGLES_DLL}"     DESTINATION ".")
      install(FILES "${OPENGLES_EGL_DLL}" DESTINATION ".")

      if(WITH_GL_ANGLE)
        install(FILES "${D3DCOMPILER_DLL}" DESTINATION ".")
      endif()
    endif()
  endif()

  if(WITH_CODEC_FFMPEG)
    install(
      FILES
        ${LIBDIR}/ffmpeg/lib/avcodec-58.dll
        ${LIBDIR}/ffmpeg/lib/avformat-58.dll
        ${LIBDIR}/ffmpeg/lib/avdevice-58.dll
        ${LIBDIR}/ffmpeg/lib/avutil-56.dll
        ${LIBDIR}/ffmpeg/lib/swscale-5.dll
        ${LIBDIR}/ffmpeg/lib/swresample-3.dll
      DESTINATION "."
    )
  endif()

  if(WITH_CODEC_SNDFILE)
    install(
      FILES ${LIBDIR}/sndfile/lib/libsndfile-1.dll
      DESTINATION "."
    )
  endif()

  if(WITH_OPENAL)
    install(
      FILES
        ${LIBDIR}/openal/lib/OpenAL32.dll
      DESTINATION "."
    )
  endif()

  if(WITH_SDL)
    install(
      FILES ${LIBDIR}/sdl/lib/SDL2.dll
      DESTINATION "."
    )
  endif()

  if(WITH_SYSTEM_AUDASPACE)
    install(
      FILES
        ${LIBDIR}/audaspace/lib/audaspace.dll
        ${LIBDIR}/audaspace/lib/audaspace-c.dll
        ${LIBDIR}/audaspace/lib/audaspace-py.dll
      DESTINATION "."
    )
  endif()

  if(NOT CMAKE_CL_64)
    install(
      FILES ${LIBDIR}/thumbhandler/lib/BlendThumb.dll
      DESTINATION "."
    )
  endif()

  install( # x86 builds can run on x64 Windows, so this is required at all times
    FILES ${LIBDIR}/thumbhandler/lib/BlendThumb64.dll
    DESTINATION "."
  )

  install(
    FILES
      ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_gpu.cmd
      ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_gpu_glitchworkaround.cmd
      ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_debug_log.cmd
      ${CMAKE_SOURCE_DIR}/release/windows/batch/blender_factory_startup.cmd
    DESTINATION "."
  )

  if(WITH_DRACO)
    install(
      PROGRAMS $<TARGET_FILE:extern_draco>
      DESTINATION ${TARGETDIR_VER}/python/lib/site-packages
    )
  endif()
elseif(APPLE)
  # Uppercase name for app bundle
  set_target_properties(blender PROPERTIES OUTPUT_NAME Blender)

  # handy install macro to exclude files, we use \$ escape for the "to"
  # argument when calling so ${BUILD_TYPE} does not get expanded
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
      PATTERN "config-${PYTHON_VERSION}m/*.a" EXCLUDE  # static lib
      PATTERN "lib2to3" EXCLUDE                   # ./lib2to3
      PATTERN "tkinter" EXCLUDE                   # ./tkinter
      PATTERN "lib-dynload/_tkinter.*" EXCLUDE    # ./lib-dynload/_tkinter.co
      PATTERN "idlelib" EXCLUDE                   # ./idlelib
      PATTERN "test" EXCLUDE                      # ./test
      PATTERN "turtledemo" EXCLUDE                # ./turtledemo
      PATTERN "turtle.py" EXCLUDE                 # ./turtle.py
    )
  endmacro()

  set(OSX_APP_SOURCEDIR ${CMAKE_SOURCE_DIR}/release/darwin/Blender.app)

  # setup Info.plist
  execute_process(COMMAND date "+%Y-%m-%d"
                  OUTPUT_VARIABLE BLENDER_DATE
                  OUTPUT_STRIP_TRAILING_WHITESPACE)

  set_target_properties(blender PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST ${OSX_APP_SOURCEDIR}/Contents/Info.plist
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${BLENDER_VERSION}${BLENDER_VERSION_CHAR}"
    MACOSX_BUNDLE_LONG_VERSION_STRING "${BLENDER_VERSION}${BLENDER_VERSION_CHAR} ${BLENDER_DATE}")

  # Gather the date in finder-style
  execute_process(COMMAND date "+%m/%d/%Y/%H:%M"
  OUTPUT_VARIABLE SETFILE_DATE
  OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Give the bundle actual creation/modification date
  execute_process(COMMAND SetFile -d ${SETFILE_DATE} -m ${SETFILE_DATE}
                  ${EXECUTABLE_OUTPUT_PATH}/Blender.app)

  install(
    TARGETS blender
    DESTINATION "."
  )

  # install release and app files
  set(BLENDER_TEXT_FILES_DESTINATION Blender.app/Contents/Resources/text)

  install(
    FILES ${OSX_APP_SOURCEDIR}/Contents/PkgInfo
    DESTINATION Blender.app/Contents
  )

  install_dir(
    ${OSX_APP_SOURCEDIR}/Contents/Resources
    Blender.app/Contents/
  )

  if(WITH_OPENMP AND OPENMP_CUSTOM)
    install(
      FILES ${LIBDIR}/openmp/lib/libomp.dylib
      DESTINATION Blender.app/Contents/Resources/lib
    )
  endif()

  if(WITH_LLVM AND NOT LLVM_STATIC)
    install(
      FILES ${LIBDIR}/llvm/lib/libLLVM-3.4.dylib
      DESTINATION Blender.app/Contents/MacOS
    )
  endif()

  # python
  if(WITH_PYTHON AND NOT WITH_PYTHON_MODULE AND NOT WITH_PYTHON_FRAMEWORK)
    # Copy the python libs into the install directory
    install_dir(
      ${PYTHON_LIBPATH}
      ${TARGETDIR_VER}/python/lib
    )

    install(DIRECTORY ${LIBDIR}/python/bin
      DESTINATION ${TARGETDIR_VER}/python
      USE_SOURCE_PERMISSIONS
    )

    # Needed for distutils/pip
    # get the last part of the include dir, will be 'python{version}{abiflag}',
    get_filename_component(_py_inc_suffix ${PYTHON_INCLUDE_DIR} NAME)
    install(
      FILES ${PYTHON_INCLUDE_DIR}/pyconfig.h
      DESTINATION ${TARGETDIR_VER}/python/include/${_py_inc_suffix}
    )
    unset(_py_inc_suffix)
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

if(DEFINED BLENDER_TEXT_FILES_DESTINATION)

  install(
    CODE
    "
    file(READ \"${CMAKE_SOURCE_DIR}/release/text/readme.html\" DATA_SRC)
    string(REGEX REPLACE \"BLENDER_VERSION\" \"${BLENDER_VERSION}\" DATA_DST \"\${DATA_SRC}\")
    file(WRITE \"${CMAKE_BINARY_DIR}/release/text/readme.html\" \"\${DATA_DST}\")
    unset(DATA_SRC)
    unset(DATA_DST)
    "
  )
  list(APPEND BLENDER_TEXT_FILES
    ${CMAKE_BINARY_DIR}/release/text/readme.html
  )

  install(
    FILES ${BLENDER_TEXT_FILES}
    DESTINATION "${BLENDER_TEXT_FILES_DESTINATION}"
  )
endif()

# install more files specified elsewhere
delayed_do_install(${TARGETDIR_VER})

unset(BLENDER_TEXT_FILES)
unset(BLENDER_TEXT_FILES_DESTINATION)


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
  DIRECTORY
    ${CMAKE_SOURCE_DIR}/release/datafiles/studiolights
  DESTINATION ${TARGETDIR_VER}/datafiles
)

# -----------------------------------------------------------------------------
# Setup link libs

add_dependencies(blender makesdna)
target_link_libraries(blender ${LIB})
unset(LIB)

setup_liblinks(blender)


# -----------------------------------------------------------------------------
# Setup launcher

if(WIN32 AND NOT WITH_PYTHON_MODULE)
  install(
    TARGETS blender
    COMPONENT Blender
    DESTINATION "."
  )
  set_target_properties(
    blender
    PROPERTIES
      VS_USER_PROPS "blender.Cpp.user.props"
  )
endif()
