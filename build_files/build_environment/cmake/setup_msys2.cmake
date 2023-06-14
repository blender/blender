# SPDX-License-Identifier: GPL-2.0-or-later

##################################################################################################
# msys2 Builds
##################################################################################################
# This installs msys to compile ffmpeg/libsndfile/fftw3/gmp
##################################################################################################
# Note - no compiler is actually installed here, we just use the tools
##################################################################################################

message("LIBDIR = ${LIBDIR}")
macro(cmake_to_msys_path MsysPath ResultingPath)
  string(REPLACE ":" "" TmpPath "${MsysPath}")
  string(SUBSTRING ${TmpPath} 0 1 Drive)
  string(SUBSTRING ${TmpPath} 1 255 PathPart)
  string(TOLOWER ${Drive} LowerDrive)
  string(CONCAT ${ResultingPath} "/" ${LowerDrive} ${PathPart})
endmacro()
cmake_to_msys_path(${LIBDIR} msys2_LIBDIR)
message("msys2_LIBDIR = ${msys2_LIBDIR}")

# Get msys2-base (currently x64 only)
message("Checking for msys2 base")
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2-base-x86_64-20221028.tar.xz")
  message("Downloading msys2-base")
  file(DOWNLOAD "https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-20221028.tar.xz" "${DOWNLOAD_DIR}/msys2-base-x86_64-20221028.tar.xz")
endif()

# Make msys2 root directory
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOWNLOAD_DIR}/msys2
    WORKING_DIRECTORY ${DOWNLOAD_DIR}
  )
endif()

# Extract msys2
if((NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd") AND (EXISTS "${DOWNLOAD_DIR}/msys2-base-x86_64-20221028.tar.xz"))
  message("Extracting msys2 base")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar jxf ${DOWNLOAD_DIR}/msys2-base-x86_64-20221028.tar.xz
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2
  )

  # Start msys2 with command "exit" - does initial setup etc then exits
  message("Performing first-time load for msys2")
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c exit
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )

  # Do initial upgrade of pacman packages (only required for initial setup, to get
  # latest packages as opposed to to what the installer comes with)
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c "pacman -Syu --noconfirm && exit"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
endif()

# If m4 isn't there, the others probably aren't either
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/m4.exe")
  # Refresh pacman repositories (similar to debian's `apt update`)
  message("Refreshing pacman")
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c "pacman -Syy --noconfirm && exit"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
  
  message("Installing required packages")
  execute_process(
    COMMAND ${DOWNLOAD_DIR}/msys2/msys64/msys2_shell.cmd -defterm -no-start -clang64 -c "pacman -S patch m4 coreutils pkgconf make diffutils autoconf-wrapper --noconfirm && exit"
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

# Strip out the copy of link that comes with some packages if it exists, otherwise meson builds break
if(EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/link.exe")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E rm ${DOWNLOAD_DIR}/msys2/msys64/usr/bin/link.exe
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/msys2/msys64
  )
endif()

message("Checking for nasm")
if(NOT EXISTS "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip")
  message("Downloading nasm")
  file(DOWNLOAD "http://www.nasm.us/pub/nasm/releasebuilds/2.13.02/win64/nasm-2.13.02-win64.zip" "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip")
endif()

# extract nasm
if((NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/nasm.exe") AND (EXISTS "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip"))
  message("Extracting nasm")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar jxf "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip"
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/nasm-2.13.02/nasm.exe" "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/nasm.exe"
  )
endif()

message("Checking for perl")
# download perl for libvpx
if(NOT EXISTS "${DOWNLOAD_DIR}/strawberry-perl-5.22.1.3-64bit-portable.zip")
  message("Downloading perl")
  file(DOWNLOAD "http://strawberryperl.com/download/5.22.1.3/strawberry-perl-5.22.1.3-64bit-portable.zip" "${DOWNLOAD_DIR}/strawberry-perl-5.22.1.3-64bit-portable.zip")
endif()

# make perl root directory
if(NOT EXISTS "${DOWNLOAD_DIR}/perl")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${DOWNLOAD_DIR}/perl
    WORKING_DIRECTORY ${DOWNLOAD_DIR}
  )
endif()

# extract perl
if((NOT EXISTS "${DOWNLOAD_DIR}/perl/portable.perl") AND (EXISTS "${DOWNLOAD_DIR}/strawberry-perl-5.22.1.3-64bit-portable.zip"))
  message("Extracting perl")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E tar jxf ${DOWNLOAD_DIR}/strawberry-perl-5.22.1.3-64bit-portable.zip
    WORKING_DIRECTORY ${DOWNLOAD_DIR}/perl
  )
endif()

# Get gas-preprocessor for ffmpeg
# This is required for Windows ARM64 builds
message("Checking for gas-preprocessor.pl")
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/gas-preprocessor.pl")
  message("Downloading gas-preprocessor.pl")
  file(DOWNLOAD "https://raw.githubusercontent.com/FFmpeg/gas-preprocessor/9309c67acb535ca6248f092e96131d8eb07eefc1/gas-preprocessor.pl" "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/gas-preprocessor.pl")
endif()

# Get ar-lib
message("Checking for ar-lib")
if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/ar-lib")
  message("Downloading ar-lib")
  file(DOWNLOAD "https://raw.githubusercontent.com/gcc-mirror/gcc/releases/gcc-12.2.0/ar-lib" "${DOWNLOAD_DIR}/msys2/msys64/usr/bin/ar-lib")
endif()

if(NOT EXISTS "${DOWNLOAD_DIR}/msys2/msys64/ming64sh.cmd")
  message("Installing ming64sh.cmd")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/ming64sh.cmd  ${DOWNLOAD_DIR}/msys2/msys64/ming64sh.cmd
  )
endif()
