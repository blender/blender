# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# This is a script which is used as POST-INSTALL one for regular CMake's
# INSTALL target.
# It is used by buildbot workers to sign every binary which is going into
# the final buundle.

# On Windows Python 3 there only is python.exe, no python3.exe.
#
# On other platforms it is possible to have python2 and python3, and a
# symbolic link to python to either of them. So on those platforms use
# an explicit Python version.
if(WIN32)
  set(PYTHON_EXECUTABLE python)
else()
  set(PYTHON_EXECUTABLE python3)
endif()

execute_process(
  COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/worker_codesign.py"
          "${CMAKE_INSTALL_PREFIX}"
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  RESULT_VARIABLE exit_code
)

if(NOT exit_code EQUAL "0")
    message(FATAL_ERROR "Non-zero exit code of codesign tool")
endif()
