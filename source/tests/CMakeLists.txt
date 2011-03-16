# -*- mode: cmake; indent-tabs-mode: t; -*-
# $Id: CMakeLists.txt 34198 2011-01-09 15:12:08Z campbellbarton $
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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

# --env-system-scripts allows to run without WITH_INSTALL

# Use '--write-blend=/tmp/test.blend' to view output


set(TEST_SRC_DIR ${CMAKE_SOURCE_DIR}/../lib/tests)
set(TEST_OUT_DIR ${CMAKE_BINARY_DIR}/tests)

# ugh, any better way to do this on testing only?
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_OUT_DIR}) 

#~ if(NOT IS_DIRECTORY ${TEST_SRC_DIR})
#~ 	message(FATAL_ERROR "CMake test directory not found!")
#~ endif()

# all calls to blender use this
if(APPLE)
	set(TEST_BLENDER_EXE ${EXECUTABLE_OUTPUT_PATH}/blender.app/Contents/MacOS/blender)
else()
	set(TEST_BLENDER_EXE ${EXECUTABLE_OUTPUT_PATH}/blender)
endif()

set(TEST_BLENDER_EXE ${TEST_BLENDER_EXE} --background --factory-startup --env-system-scripts ${CMAKE_SOURCE_DIR}/release/scripts)

# OBJ Import tests
add_test(import_obj_cube ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.obj\(filepath='${TEST_SRC_DIR}/io_tests/obj/cube.obj'\)
	--md5=4d090508b812b5e08168aa2614746bda --md5_method=SCENE
)

add_test(import_obj_nurbs_cyclic ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.obj\(filepath='${TEST_SRC_DIR}/io_tests/obj/nurbs_cyclic.obj'\)
	--md5=9e0da7b65b4c4f818a203d56af2d3a4b --md5_method=SCENE
	--write-blend=/root/foo99.blend
)

add_test(import_obj_makehuman ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.obj\(filepath='${TEST_SRC_DIR}/io_tests/obj/makehuman.obj'\)
	--md5=e0829dc078b0789e1d81f1071235bc4f --md5_method=SCENE
)

# OBJ Export tests
add_test(export_obj_cube ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.obj\(filepath='${TEST_OUT_DIR}/export_obj_cube.obj',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_obj_cube.obj
	--md5_source=${TEST_OUT_DIR}/export_obj_cube.mtl
	--md5=70bdc394c2726203ad26c085176e3484 --md5_method=FILE
)

add_test(export_obj_nurbs ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.obj\(filepath='${TEST_OUT_DIR}/export_obj_nurbs.obj',use_selection=False,use_nurbs=True\)
	--md5_source=${TEST_OUT_DIR}/export_obj_nurbs.obj
	--md5_source=${TEST_OUT_DIR}/export_obj_nurbs.mtl
	--md5=a733ae4fa4a591ea9b0912da3af042de --md5_method=FILE
)

add_test(export_obj_all_objects ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.obj\(filepath='${TEST_OUT_DIR}/export_obj_all_objects.obj',use_selection=False,use_nurbs=True\)
	--md5_source=${TEST_OUT_DIR}/export_obj_all_objects.obj
	--md5_source=${TEST_OUT_DIR}/export_obj_all_objects.mtl
	--md5=6e033a6a9c923d7aa3613b36e373f55b --md5_method=FILE
)



# PLY Import tests
add_test(import_ply_cube ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_mesh.ply\(filepath='${TEST_SRC_DIR}/io_tests/ply/cube_ascii.ply'\)
	--md5=527134343c27fc0ea73115b85fbfd3ac --md5_method=SCENE
)

add_test(import_ply_bunny ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_mesh.ply\(filepath='${TEST_SRC_DIR}/io_tests/ply/bunny2.ply'\)
	--md5=6ea5b8533400a17accf928b8fd024eaa --md5_method=SCENE
)

add_test(import_ply_small_holes ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_mesh.ply\(filepath='${TEST_SRC_DIR}/io_tests/ply/many_small_holes.ply'\)
	--md5=c3093e26ecae5b6d59fbbcf2a0d0b39f --md5_method=SCENE
)

# PLY Export tests (TODO)



# STL Import tests
add_test(import_stl_cube ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_mesh.stl\(filepath='${TEST_SRC_DIR}/io_tests/stl/cube.stl'\)
	--md5=8ceb5bb7e1cb5f4342fa1669988c66b4 --md5_method=SCENE
)

add_test(import_stl_conrod ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_mesh.stl\(filepath='${TEST_SRC_DIR}/io_tests/stl/conrod.stl'\)
	--md5=690a4b8eb9002dcd8631c5a575ea7348 --md5_method=SCENE
)

add_test(import_stl_knot_max_simplified ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_mesh.stl\(filepath='${TEST_SRC_DIR}/io_tests/stl/knot_max_simplified.stl'\)
	--md5=baf82803f45a84ec4ddbad9cef57dd3e --md5_method=SCENE
)

# STL Export tests (TODO)



# X3D Import
add_test(import_x3d_cube ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.x3d\(filepath='${TEST_SRC_DIR}/io_tests/x3d/color_cube.x3d'\)
	--md5=c80538e272812c9d765d43df269d8a9b --md5_method=SCENE
)

add_test(import_x3d_teapot ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.x3d\(filepath='${TEST_SRC_DIR}/io_tests/x3d/teapot.x3d'\)
	--md5=fa19713ff71d4b3893dcbe0ab3a73955 --md5_method=SCENE
	--write-blend=/root/foo99.blend
)

add_test(import_x3d_suzanne_material ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.x3d\(filepath='${TEST_SRC_DIR}/io_tests/x3d/suzanne_material.x3d'\)
	--md5=52a59dcf731904ac49953dd82c020ae5 --md5_method=SCENE
)

# X3D Export
add_test(export_x3d_cube ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.x3d\(filepath='${TEST_OUT_DIR}/export_x3d_cube.x3d',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_x3d_cube.x3d
	--md5=9198ca86f19b68a4f1eb75bb48cb3dea --md5_method=FILE
)

add_test(export_x3d_nurbs ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.x3d\(filepath='${TEST_OUT_DIR}/export_x3d_nurbs.x3d',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_x3d_nurbs.x3d
	--md5=078c0ca5a08f123cd2cdac48afb54853 --md5_method=FILE
)

add_test(export_x3d_all_objects ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.x3d\(filepath='${TEST_OUT_DIR}/export_x3d_all_objects.x3d',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_x3d_all_objects.x3d
	--md5=75df3075412ff60611ea05bc469e593b --md5_method=FILE
)



# 3DS Import
add_test(import_3ds_cube ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.autodesk_3ds\(filepath='${TEST_SRC_DIR}/io_tests/3ds/cube.3ds'\)
	--md5=cb5a45c35a343c3f5beca2a918472951 --md5_method=SCENE
)

add_test(import_3ds_hierarchy_lara ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.autodesk_3ds\(filepath='${TEST_SRC_DIR}/io_tests/3ds/hierarchy_lara.3ds'\)
	--md5=2e9812099b26ad607fdcf4c7be918c71 --md5_method=SCENE
	--write-blend=/root/foo99.blend
)

add_test(import_3ds_hierarchy_greek_trireme ${TEST_BLENDER_EXE}
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.import_scene.autodesk_3ds\(filepath='${TEST_SRC_DIR}/io_tests/3ds/hierarchy_greek_trireme.3ds'\)
	--md5=d05b922d7be20356d8409d1f768a3a9a --md5_method=SCENE
)

# 3DS Export
add_test(export_3ds_cube ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.autodesk_3ds\(filepath='${TEST_OUT_DIR}/export_3ds_cube.3ds',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_3ds_cube.3ds
	--md5=0df6cfb130052d01e31ef77d391d4cc0 --md5_method=FILE
)

add_test(export_3ds_nurbs ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.autodesk_3ds\(filepath='${TEST_OUT_DIR}/export_3ds_nurbs.3ds',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_3ds_nurbs.3ds
	--md5=ba1a6d43346fee3bcadc7e30e3c95935 --md5_method=FILE
)

add_test(export_3ds_all_objects ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.autodesk_3ds\(filepath='${TEST_OUT_DIR}/export_3ds_all_objects.3ds',use_selection=False\)
	--md5_source=${TEST_OUT_DIR}/export_3ds_all_objects.3ds
	--md5=87349a4699f1006e8194fb0ac05ac9c8 --md5_method=FILE
)



# FBX Export
# 'use_metadata=False' for reliable md5's
add_test(export_fbx_cube ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/all_quads.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.fbx\(filepath='${TEST_OUT_DIR}/export_fbx_cube.fbx',use_selection=False,use_metadata=False\)
	--md5_source=${TEST_OUT_DIR}/export_fbx_cube.fbx
	--md5=a0806dc974d814f338b821ab326a6be0 --md5_method=FILE
)

add_test(export_fbx_nurbs ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_geometry/nurbs.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.fbx\(filepath='${TEST_OUT_DIR}/export_fbx_nurbs.fbx',use_selection=False,use_metadata=False\)
	--md5_source=${TEST_OUT_DIR}/export_fbx_nurbs.fbx
	--md5=2cda2b37ee6698aff4129af48fce1291 --md5_method=FILE
)

add_test(export_fbx_all_objects ${TEST_BLENDER_EXE}
	${TEST_SRC_DIR}/io_tests/blend_scene/all_objects.blend
	--python ${CMAKE_CURRENT_LIST_DIR}/bl_test.py --
	--run={'FINISHED'}&bpy.ops.export_scene.fbx\(filepath='${TEST_OUT_DIR}/export_fbx_all_objects.fbx',use_selection=False,use_metadata=False\)
	--md5_source=${TEST_OUT_DIR}/export_fbx_all_objects.fbx
	--md5=be69cf0baf51dcf43f579183310cb383 --md5_method=FILE
)
