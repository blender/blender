# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

##################################################################################################
# msys2 Builds
##################################################################################################
# This installs msys to compile ffmpeg/libsndfile/fftw3/gmp
##################################################################################################
# Note - no compiler is actually installed here, we just use the tools
##################################################################################################

macro(download_package package_name)
  # This will:
  # 1 - Download the required package from either the upstream location or blender mirror
  #     depending on `MSYS2_USE_UPSTREAM_PACKAGES`.
  # 2 - Set a global variable [package_name]_FILE to point to the downloaded file.
  # 3 - Verify the hash if FORCE_CHECK_HASH is on.
  set(URL ${MSYS2_${package_name}_URL})
  set(HASH ${MSYS2_${package_name}_HASH})
  string(REPLACE "/" ";" _url_list ${URL})
  list(GET _url_list -1 _file_name)
  set(_final_filename "${DOWNLOAD_DIR}/${_file_name}")
  set(MSYS2_${package_name}_FILE ${_final_filename})
  if(NOT EXISTS "${_final_filename}")
    if(MSYS2_USE_UPSTREAM_PACKAGES)
      set(_final_url ${URL})
    else()
      set(_final_url "https://projects.blender.org/blender/lib-windows_x64/media/branch/build_environment/${_file_name}")
    endif()
    message(STATUS "Downloading ${_final_filename} from ${_final_url}")
    file(
      DOWNLOAD ${_final_url} ${_final_filename}
      TIMEOUT 1800  # seconds
      EXPECTED_HASH SHA1=${HASH}
      TLS_VERIFY ON
      SHOW_PROGRESS
    )
  endif()
  if(EXISTS "${_final_filename}")
    if(FORCE_CHECK_HASH)
      file(SHA1 ${_final_filename} LOCAL_HASH)
      if(NOT ${HASH} STREQUAL ${LOCAL_HASH})
        message(FATAL_ERROR "${_final_filename} SHA1 mismatch\nExpected\t: ${HASH}\nActual\t: ${LOCAL_HASH}")
      endif()
    endif()
  endif()
  unset(URL)
  unset(HASH)
  unset(_final_url)
  unset(_final_filename)
  unset(_url_list)
  unset(_file_name)
endmacro()

# Note we use URL here rather than URI as the dependencies checker will check all `*_URI`
# variables for package/license/homepage requirements since none of this will end up
# on end users systems the requirements are not as strict.
set(MSYS2_BASE_URL https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20221028.tar.xz)
set(MSYS2_BASE_HASH 545cc6a4c36bb98058f2b2945c5d06de523516db)

set(MSYS2_NASM_URL http://www.nasm.us/pub/nasm/releasebuilds/2.13.02/win64/nasm-2.13.02-win64.zip)
set(MSYS2_NASM_HASH 6ae5eaffde68aa7450fadd7f45ba5c6df3dce558)

set(MSYS2_PERL_URL https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_5380_5361/strawberry-perl-5.38.0.1-64bit-portable.zip)
set(MSYS2_PERL_HASH 987c870cc2401e481e3ddbdd1462d2a52da34187)

set(MSYS2_GAS_URL https://raw.githubusercontent.com/FFmpeg/gas-preprocessor/7380ac24e1cd23a5e6d76c6af083d8fc5ab9e943/gas-preprocessor.pl)
set(MSYS2_GAS_HASH 313c45e9ae7e4b6c13475e65ee4063593dac2cbe)

set(MSYS2_AR_URL https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-12.2.0/ar-lib)
set(MSYS2_AR_HASH 77194f45708a80f502102fa881a8a5cb048b03af)

download_package(BASE)
download_package(NASM)
download_package(PERL)
download_package(GAS)
download_package(AR)

message(STATUS "LIBDIR = ${LIBDIR}")
macro(cmake_to_msys_path MsysPath ResultingPath)
  string(REPLACE ":" "" TmpPath "${MsysPath}")
  string(SUBSTRING ${TmpPath} 0 1 Drive)
  string(SUBSTRING ${TmpPath} 1 255 PathPart)
  string(TOLOWER ${Drive} LowerDrive)
  string(CONCAT ${ResultingPath} "/" ${LowerDrive} ${PathPart})
endmacro()
cmake_to_msys_path(${LIBDIR} msys2_LIBDIR)
message(STATUS "msys2_LIBDIR = ${msys2_LIBDIR}")

# Make msys2 root directory
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOWNLOAD_DIR}/msys2
    WORKING_DIRECTORY ${DOWNLOAD_DIR}
  )
endif()

# Extract msys2
if((NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd") AND
   (EXISTS "${MSYS2_BASE_FILE}"))
  message(STATUS "Extracting msys2 base")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar jxf ${MSYS2_BASE_FILE}
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2
  )

  # Start msys2 with command "exit" - does initial setup etc then exits
  message(STATUS "Performing first-time load for msys2")
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c exit
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )

  # Do initial upgrade of pacman packages (only required for initial setup, to get
  # latest packages as opposed to what the installer comes with).
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c
      "pacman -Sy --noconfirm && exit"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
endif()

# If m4 isn't there, the others probably aren't either
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/m4.exe")
  # Refresh pacman repositories (similar to debian's `apt update`)
  message(STATUS "Refreshing pacman")
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c
      "pacman -Syy --noconfirm && exit"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )

  message(STATUS "Installing required packages")

  if(NOT BLENDER_PLATFORM_WINDOWS_ARM)
    # A newer runtime package is required since the tools downloaded in the next step
    # depend on it, we however cannot update the runtime from a msys2_shell.cmd since
    # bash.exe will lock the runtime dlls so we execute pacman directly.
    #
    # For now only run this on X64 as there are some known issues (but unknown to me)
    # with newer msys2 on Windows on ARM.
    execute_process(
      COMMAND ${DOWNLOAD_DIR}/msys2/msys64/usr/bin/pacman -S msys2-runtime --noconfirm
      WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
    )
  endif()

  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c
      "pacman -S patch m4 coreutils pkgconf make diffutils autoconf-wrapper --noconfirm && exit"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
endif()

# Strip out the copy of perl that comes with msys2 if it exists, otherwise python builds break
if(EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/perl.exe")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E rm ${DOWNLOAD_DIR}/msys2/msys64/usr/bin/perl.exe
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
endif()

# Strip out the copy of link that comes with some packages if it exists,
# otherwise meson builds break.
if(EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/link.exe")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E rm ${DOWNLOAD_DIR}/msys2/msys64/usr/bin/link.exe
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
endif()

# extract nasm
if((NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/nasm.exe") AND (EXISTS "${MSYS2_NASM_FILE}"))
  message(STATUS "Extracting nasm")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar jxf "${MSYS2_NASM_FILE}"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
      "${DOWNLOAD_DIR}/nasm-2.13.02/nasm.exe"
      "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/nasm.exe"
  )
endif()
set(NASM_PATH "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/nasm.exe")

# make perl root directory
if(NOT EXISTS "${DOWNLOAD_DIR}/perl")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOWNLOAD_DIR}/perl
    WORKING_DIRECTORY ${DOWNLOAD_DIR}
  )
endif()

# extract perl
if((NOT EXISTS "${DOWNLOAD_DIR}/perl/portable.perl") AND (EXISTS "${MSYS2_PERL_FILE}"))
  message(STATUS "Extracting perl")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar jxf ${MSYS2_PERL_FILE}
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/perl
  )
endif()

# Get gas-preprocessor for ffmpeg
# This is required for Windows ARM64 builds
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/gas-preprocessor.pl")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
      ${MSYS2_GAS_FILE}
      "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/gas-preprocessor.pl"
  )
endif()

# Get ar-lib
message(STATUS "Checking for ar-lib")
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/ar-lib")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
      ${MSYS2_AR_FILE}
      "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/ar-lib"
  )
endif()

if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/ming64sh.cmd")
  message(STATUS "Installing ming64sh.cmd")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy
      ${PATCH_DIR}/ming64sh.cmd
      ${DOWNLOAD_DIR}/msys2/msys64/ming64sh.cmd
  )
endif()
