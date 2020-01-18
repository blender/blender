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

if(MSVC)
  set(PYTARGET ${HARVEST_TARGET}/python/${PYTHON_SHORT_VERSION_NO_DOTS})
  set(PYSRC ${LIBDIR}/python/)

  if(BUILD_MODE STREQUAL Release)
    add_custom_command(
      OUTPUT ${PYTARGET}/bin/python${PYTHON_POSTFIX}.exe
      COMMAND echo packaging python
      COMMAND echo this should ouput at ${PYTARGET}/bin/python${PYTHON_POSTFIX}.exe
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PYTARGET}/libs
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.lib ${PYTARGET}/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python.exe ${PYTARGET}/bin/python.exe
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python${PYTHON_SHORT_VERSION_NO_DOTS}.dll ${PYTARGET}/bin/python${PYTHON_SHORT_VERSION_NO_DOTS}.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python3${PYTHON_POSTFIX}.dll ${PYTARGET}/bin/python3${PYTHON_POSTFIX}.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python${PYTHON_SHORT_VERSION_NO_DOTS}.pdb ${PYTARGET}/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}.pdb
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYSRC}/include/ ${PYTARGET}/include/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYSRC}/lib/ ${PYTARGET}/lib/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYSRC}/DLLs/ ${PYTARGET}/DLLs/
      COMMAND cd ${PYTARGET}/lib/ && for /d /r . %%d in (__pycache__) do @if exist "%%d" echo "%%d" && rd /s/q "%%d"
    )
    add_custom_target(Package_Python ALL DEPENDS external_python external_numpy external_python_site_packages OUTPUT ${HARVEST_TARGET}/python/${PYTHON_SHORT_VERSION_NO_DOTS}/bin/python${PYTHON_POSTFIX}.exe)
  endif()

  if(BUILD_MODE STREQUAL Debug)
    add_custom_command(
      OUTPUT ${PYTARGET}/bin/python${PYTHON_POSTFIX}.exe
      COMMAND echo packaging python
      COMMAND echo this should ouput at ${PYTARGET}/bin/python${PYTHON_POSTFIX}.exe
      COMMAND ${CMAKE_COMMAND} -E make_directory ${PYTARGET}/libs
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib ${PYTARGET}/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.lib
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python${PYTHON_POSTFIX}.exe ${PYTARGET}/bin/python${PYTHON_POSTFIX}.exe
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.dll ${PYTARGET}/bin/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python3${PYTHON_POSTFIX}.dll ${PYTARGET}/bin/python3${PYTHON_POSTFIX}.dll
      COMMAND ${CMAKE_COMMAND} -E copy ${PYSRC}/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.pdb ${PYTARGET}/libs/python${PYTHON_SHORT_VERSION_NO_DOTS}${PYTHON_POSTFIX}.pdb
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYSRC}/include/ ${PYTARGET}/include/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYSRC}/lib/ ${PYTARGET}/lib/
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${PYSRC}/DLLs/ ${PYTARGET}/DLLs/
      COMMAND cd ${PYTARGET}/lib/ && for /d /r . %%d in (__pycache__) do @if exist "%%d" echo "%%d" && rd /s/q "%%d"
    )
    add_custom_target(Package_Python ALL DEPENDS external_python external_numpy external_python_site_packages OUTPUT ${PYTARGET}/bin/python${PYTHON_POSTFIX}.exe)
  endif()
endif()
