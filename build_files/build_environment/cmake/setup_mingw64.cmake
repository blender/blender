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
# ***** END GPL LICENSE BLOCK *****

####################################################################################################################
# Mingw64 Builds
####################################################################################################################
# This installs mingw64+msys to compile ffmpeg/iconv/libsndfile/lapack/fftw3
####################################################################################################################

message("LIBDIR = ${LIBDIR}")
macro(cmake_to_msys_path MsysPath ResultingPath)
	string(REPLACE ":" "" TmpPath "${MsysPath}")
	string(SUBSTRING ${TmpPath} 0 1 Drive)
	string(SUBSTRING ${TmpPath} 1 255 PathPart)
	string(TOLOWER ${Drive} LowerDrive)
	string(CONCAT ${ResultingPath} "/" ${LowerDrive} ${PathPart})
endmacro()
cmake_to_msys_path(${LIBDIR} mingw_LIBDIR)
message("mingw_LIBDIR = ${mingw_LIBDIR}")

message("Checking for mingw64")
# download ming64
if(NOT EXISTS "${DOWNLOAD_DIR}/x86_64-4.9.4-release-win32-seh-rt_v5-rev0.7z")
	message("Downloading mingw64")
	file(DOWNLOAD "https://nchc.dl.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/4.9.4/threads-win32/seh/x86_64-4.9.4-release-win32-seh-rt_v5-rev0.7z" "${DOWNLOAD_DIR}/x86_64-4.9.4-release-win32-seh-rt_v5-rev0.7z")
endif()

# make mingw root directory
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E make_directory ${DOWNLOAD_DIR}/mingw
		WORKING_DIRECTORY ${DOWNLOAD_DIR}
	)
endif()

# extract mingw64
if((NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/ming64sh.cmd") AND (EXISTS "${DOWNLOAD_DIR}/x86_64-4.9.4-release-win32-seh-rt_v5-rev0.7z"))
	message("Extracting mingw64")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar jxf ${DOWNLOAD_DIR}/x86_64-4.9.4-release-win32-seh-rt_v5-rev0.7z
		WORKING_DIRECTORY ${DOWNLOAD_DIR}/mingw
	)
endif()

message("Checking for pkg-config")
if(NOT EXISTS "${DOWNLOAD_DIR}/pkg-config-lite-0.28-1_bin-win32.zip")
	message("Downloading pkg-config")
	file(DOWNLOAD "https://nchc.dl.sourceforge.net/project/pkgconfiglite/0.28-1/pkg-config-lite-0.28-1_bin-win32.zip" "${DOWNLOAD_DIR}/pkg-config-lite-0.28-1_bin-win32.zip")
endif()

# extract pkgconfig
if((NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/pkg-config.exe") AND (EXISTS "${DOWNLOAD_DIR}/pkg-config-lite-0.28-1_bin-win32.zip"))
	message("Extracting pkg-config")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar jxf "${DOWNLOAD_DIR}/pkg-config-lite-0.28-1_bin-win32.zip"
		WORKING_DIRECTORY ${DOWNLOAD_DIR}/
	)

	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/pkg-config-lite-0.28-1/bin/pkg-config.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/pkg-config.exe"
	)

endif()

message("Checking for nasm")
if(NOT EXISTS "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip")
	message("Downloading nasm")
	file(DOWNLOAD "http://www.nasm.us/pub/nasm/releasebuilds/2.13.02/win64/nasm-2.13.02-win64.zip" "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip")
endif()

# extract nasm
if((NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/nasm.exe") AND (EXISTS "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip"))
	message("Extracting nasm")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar jxf "${DOWNLOAD_DIR}/nasm-2.13.02-win64.zip"
		WORKING_DIRECTORY ${DOWNLOAD_DIR}/
	)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/nasm-2.13.02/nasm.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/nasm.exe"
	)

endif()
SET(NASM_PATH ${DOWNLOAD_DIR}/mingw/mingw64/bin/nasm.exe)

message("Checking for mingwGet")
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw-get-0.6.2-mingw32-beta-20131004-1-bin.zip")
	message("Downloading mingw-get")
	file(DOWNLOAD "https://nchc.dl.sourceforge.net/project/mingw/Installer/mingw-get/mingw-get-0.6.2-beta-20131004-1/mingw-get-0.6.2-mingw32-beta-20131004-1-bin.zip" "${DOWNLOAD_DIR}/mingw-get-0.6.2-mingw32-beta-20131004-1-bin.zip")
endif()

# extract mingw_get
if((NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/mingw-get.exe") AND (EXISTS "${DOWNLOAD_DIR}/mingw-get-0.6.2-mingw32-beta-20131004-1-bin.zip"))
	message("Extracting mingw-get")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar jxf "${DOWNLOAD_DIR}/mingw-get-0.6.2-mingw32-beta-20131004-1-bin.zip"
		WORKING_DIRECTORY ${DOWNLOAD_DIR}/mingw/mingw64/
	)
endif()

if((EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/mingw-get.exe") AND (NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/msys/1.0/bin/make.exe"))
	message("Installing MSYS")
	execute_process(
		COMMAND ${DOWNLOAD_DIR}/mingw/mingw64/bin/mingw-get install msys msys-patch
		WORKING_DIRECTORY  ${DOWNLOAD_DIR}/mingw/mingw64/bin/
	)
endif()

if((EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/mingw-get.exe") AND (NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/msys/1.0/bin/mktemp.exe"))
	message("Installing mktemp")
	execute_process(
		COMMAND ${DOWNLOAD_DIR}/mingw/mingw64/bin/mingw-get install msys msys-mktemp
		WORKING_DIRECTORY  ${DOWNLOAD_DIR}/mingw/mingw64/bin/
	)
endif()

message("Checking for CoreUtils")
# download old core_utils for pr.exe (ffmpeg needs it to build)
if(NOT EXISTS "${DOWNLOAD_DIR}/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2")
	message("Downloading CoreUtils 5.97")
	file(DOWNLOAD "https://nchc.dl.sourceforge.net/project/mingw/MSYS/Base/msys-core/_obsolete/coreutils-5.97-MSYS-1.0.11-2/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2" "${DOWNLOAD_DIR}/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2")
endif()

if((EXISTS "${DOWNLOAD_DIR}/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2") AND (NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/msys/1.0/bin/pr.exe"))
	message("Installing pr from CoreUtils 5.97")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E make_directory ${DOWNLOAD_DIR}/tmp_coreutils
		WORKING_DIRECTORY ${DOWNLOAD_DIR}
	)

	execute_process(
		COMMAND ${CMAKE_COMMAND} -E tar jxf ${DOWNLOAD_DIR}/coreutils-5.97-MSYS-1.0.11-snapshot.tar.bz2
		WORKING_DIRECTORY ${DOWNLOAD_DIR}/tmp_coreutils/
	)

	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy ${DOWNLOAD_DIR}/tmp_coreutils/coreutils-5.97/bin/pr.exe "${DOWNLOAD_DIR}/mingw/mingw64/msys/1.0/bin/pr.exe"
		WORKING_DIRECTORY ${DOWNLOAD_DIR}/tmp_coreutils/
	)
endif()

if(NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/ming64sh.cmd")
	message("Installing ming64sh.cmd")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy ${PATCH_DIR}/ming64sh.cmd  ${DOWNLOAD_DIR}/mingw/mingw64/ming64sh.cmd
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

# get yasm for vpx
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/yasm.exe")
	message("Downloading yasm")
	file(DOWNLOAD "http://www.tortall.net/projects/yasm/releases/yasm-1.3.0-win64.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/yasm.exe")
endif()

message("checking x86_64-w64-mingw32-strings.exe")
# copy strings.exe to x86_64-w64-mingw32-strings.exe for x264
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-strings.exe")
	message("fixing x86_64-w64-mingw32-strings.exe")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/mingw/mingw64/bin/strings.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-strings.exe"
	)
endif()

message("checking x86_64-w64-mingw32-ar.exe")
# copy ar.exe to x86_64-w64-mingw32-ar.exe for x264
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-ar.exe")
	message("fixing x86_64-w64-mingw32-ar.exe")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/mingw/mingw64/bin/ar.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-ar.exe"
	)
endif()

message("checking x86_64-w64-mingw32-strip.exe")
# copy strip.exe to x86_64-w64-mingw32-strip.exe for x264
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-strip.exe")
	message("fixing x86_64-w64-mingw32-strip.exe")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/mingw/mingw64/bin/strip.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-strip.exe"
	)
endif()

message("checking x86_64-w64-mingw32-ranlib.exe")
# copy ranlib.exe to x86_64-w64-mingw32-ranlib.exe for x264
if(NOT EXISTS "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-ranlib.exe")
	message("fixing x86_64-w64-mingw32-ranlib.exe")
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E copy "${DOWNLOAD_DIR}/mingw/mingw64/bin/ranlib.exe" "${DOWNLOAD_DIR}/mingw/mingw64/bin/x86_64-w64-mingw32-ranlib.exe"
	)
endif()
