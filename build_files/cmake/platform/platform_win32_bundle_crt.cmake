# First generate the manifest for tests since it will not need the dependency on the CRT.
configure_file(${CMAKE_SOURCE_DIR}/release/windows/manifest/blender.exe.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/tests.exe.manifest @ONLY)

if(WITH_WINDOWS_BUNDLE_CRT)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  set(CMAKE_INSTALL_OPENMP_LIBRARIES ${WITH_OPENMP})

  # This sometimes can change when updates are installed and the compiler version
  # changes, so test if it exists and if not, give InstallRequiredSystemLibraries
  # another chance to figure out the path.
  if(MSVC_REDIST_DIR AND NOT EXISTS "${MSVC_REDIST_DIR}")
    unset(MSVC_REDIST_DIR CACHE)
  endif()

  include(InstallRequiredSystemLibraries)

  # Install the CRT to the blender.crt Sub folder.
  install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ./blender.crt COMPONENT Libraries)

  # Generating the manifest is a relativly expensive operation since
  # it is collecting an sha1 hash for every file required. so only do
  # this work when the libs have either changed or the manifest does
  # not exist yet.

  string(SHA1 libshash "${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}")
  set(manifest_trigger_file "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/crt_${libshash}")

  if(NOT EXISTS ${manifest_trigger_file})
    set(CRTLIBS "")
    foreach(lib ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
      get_filename_component(filename ${lib} NAME)
      file(SHA1 "${lib}" sha1_file)
      string(APPEND CRTLIBS "    <file name=\"${filename}\" hash=\"${sha1_file}\"  hashalg=\"SHA1\" />\n")
    endforeach()
    configure_file(${CMAKE_SOURCE_DIR}/release/windows/manifest/blender.crt.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/blender.crt.manifest @ONLY)
    file(TOUCH ${manifest_trigger_file})
  endif()

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/blender.crt.manifest DESTINATION ./blender.crt)
  set(BUNDLECRT "<dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"blender.crt\" version=\"1.0.0.0\" /></dependentAssembly></dependency>")
endif()
configure_file(${CMAKE_SOURCE_DIR}/release/windows/manifest/blender.exe.manifest.in ${CMAKE_CURRENT_BINARY_DIR}/blender.exe.manifest @ONLY)
