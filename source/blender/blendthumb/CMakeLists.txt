# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# -----------------------------------------------------------------------------
# Shared Thumbnail Extraction Logic

include_directories(
  ../blenlib
  ../blenloader_core
  ../makesdna
  ../../../intern/guardedalloc
)

include_directories(
  SYSTEM
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  src/blendthumb.hh
  src/blendthumb_extract.cc
  src/blendthumb_png.cc
)

if(WITH_BUILDINFO)
  list(APPEND SRC
    "$<TARGET_OBJECTS:buildinfoobj>"
  )
endif()

if(WIN32)
  # -----------------------------------------------------------------------------
  # Build `BlendThumb.dll`

  set(SRC_WIN32
    src/blendthumb_win32.cc
    src/blendthumb_win32.def
    src/blendthumb_win32.rc
    src/blendthumb_win32_dll.cc
  )

  add_definitions(-DNOMINMAX)

  add_library(BlendThumb SHARED ${SRC} ${SRC_WIN32})

  setup_platform_linker_flags(BlendThumb)
  setup_platform_linker_libs(BlendThumb)

  target_link_libraries(BlendThumb PRIVATE bf_blenlib bf_blenloader_core dbghelp.lib Version.lib Comctl32.lib)
  # `blenlib` drags in a whole bunch of dependencies on shared libraries, none of which are used
  # by `blenthumb`, but will cause load issues since the debug linker will not eliminate them.
  # Link with /OPT:ref to force elimination of those unused dependencies this is already
  # enabled by default on the release mode flags.
  set_target_properties(BlendThumb PROPERTIES LINK_FLAGS "/OPT:ref")
  set_target_properties(BlendThumb PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:msvcrt")
  set_target_properties(BlendThumb PROPERTIES VS_GLOBAL_VcpkgEnabled "false")

elseif(APPLE)
  # -----------------------------------------------------------------------------
  # Build `blender-thumbnailer.appex` app extension.
  set(SRC_APPEX
    src/thumbnail_provider.h
    src/thumbnail_provider.mm
  )

  add_executable(blender-thumbnailer MACOSX_BUNDLE ${SRC} ${SRC_APPEX})
  setup_platform_linker_flags(blender-thumbnailer)
  setup_platform_linker_libs(blender-thumbnailer)
  target_link_libraries(blender-thumbnailer
    bf_blenlib
    bf_blenloader_core
    # Avoid linker error about undefined _main symbol.
    "-e _NSExtensionMain"
    "-framework QuickLookThumbnailing"
  )
  # The RPATH here points to the main Blender Resources/lib directory.
  # Avoid duplicating the large `dylibs` (~300MB).
  set_target_properties(blender-thumbnailer PROPERTIES
    INSTALL_RPATH "@loader_path/../../../../Resources/lib"
    # Prevent Xcode from overwriting the signature.
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
  )
  # CMake needs the target defined in the same file as add_custom_command.
  # It needs to be code-signed (ad-hoc in this case)
  # even on developer machine to generate thumbnails.
  # Command taken from XCode build process.
  add_custom_command(
    TARGET blender-thumbnailer POST_BUILD
    COMMAND codesign --deep --force --sign -
      --entitlements "${CMAKE_SOURCE_DIR}/release/darwin/thumbnailer_entitlements.plist"
      --timestamp=none $<TARGET_BUNDLE_DIR:blender-thumbnailer>
  )
elseif(UNIX)
  # -----------------------------------------------------------------------------
  # Build `blender-thumbnailer` executable

  set(SRC_CMD
    src/blender_thumbnailer.cc
  )

  add_executable(blender-thumbnailer ${SRC} ${SRC_CMD})
  setup_platform_linker_flags(blender-thumbnailer)
  setup_platform_linker_libs(blender-thumbnailer)
  target_link_libraries(blender-thumbnailer PRIVATE bf_blenlib bf_blenloader_core)
  if(DEFINED PTHREADS_LIBRARIES)
    target_link_libraries(blender-thumbnailer PRIVATE ${PTHREADS_LIBRARIES})
  endif()
endif()
