# SPDX-FileCopyrightText: 2006 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  .
  ../common
  ../../blenkernel
  ../../blenlib
  ../../blenloader
  ../../blentranslation
  ../../bmesh
  ../../depsgraph
  ../../editors/include
  ../../makesrna
  ../../windowmanager
  ../../../../intern/clog
  ../../../../intern/guardedalloc
  ../../../../intern/utfconv
)

set(INC_SYS
  ${ALEMBIC_INCLUDE_DIRS}
  ${OPENEXR_INCLUDE_DIRS}
)

if(WITH_BOOST)
  list(APPEND INC_SYS
    ${BOOST_INCLUDE_DIR}
  )
endif()

set(SRC
  intern/abc_axis_conversion.cc
  intern/abc_customdata.cc
  intern/abc_reader_archive.cc
  intern/abc_reader_camera.cc
  intern/abc_reader_curves.cc
  intern/abc_reader_mesh.cc
  intern/abc_reader_nurbs.cc
  intern/abc_reader_object.cc
  intern/abc_reader_points.cc
  intern/abc_reader_transform.cc
  intern/abc_util.cc
  intern/alembic_capi.cc

  exporter/abc_archive.cc
  exporter/abc_custom_props.cc
  exporter/abc_export_capi.cc
  exporter/abc_hierarchy_iterator.cc
  exporter/abc_subdiv_disabler.cc
  exporter/abc_writer_abstract.cc
  exporter/abc_writer_camera.cc
  exporter/abc_writer_curves.cc
  exporter/abc_writer_hair.cc
  exporter/abc_writer_instance.cc
  exporter/abc_writer_mball.cc
  exporter/abc_writer_mesh.cc
  exporter/abc_writer_nurbs.cc
  exporter/abc_writer_points.cc
  exporter/abc_writer_transform.cc

  ABC_alembic.h
  intern/abc_axis_conversion.h
  intern/abc_customdata.h
  intern/abc_reader_archive.h
  intern/abc_reader_camera.h
  intern/abc_reader_curves.h
  intern/abc_reader_mesh.h
  intern/abc_reader_nurbs.h
  intern/abc_reader_object.h
  intern/abc_reader_points.h
  intern/abc_reader_transform.h
  intern/abc_util.h

  exporter/abc_archive.h
  exporter/abc_custom_props.h
  exporter/abc_hierarchy_iterator.h
  exporter/abc_subdiv_disabler.h
  exporter/abc_writer_abstract.h
  exporter/abc_writer_camera.h
  exporter/abc_writer_curves.h
  exporter/abc_writer_hair.h
  exporter/abc_writer_instance.h
  exporter/abc_writer_mball.h
  exporter/abc_writer_mesh.h
  exporter/abc_writer_nurbs.h
  exporter/abc_writer_points.h
  exporter/abc_writer_transform.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  PRIVATE bf::dna
  bf_io_common

  ${ALEMBIC_LIBRARIES}
  ${OPENEXR_LIBRARIES}
)

if(WITH_BOOST)
  list(APPEND LIB
    ${BOOST_LIBRARIES}
  )
endif()

blender_add_lib(bf_alembic "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

if(WITH_GTESTS)
  set(TEST_SRC
    tests/abc_export_test.cc
    tests/abc_matrix_test.cc
  )
  set(TEST_INC
  )
  set(TEST_LIB
    bf_alembic
  )
  include(GTestTesting)
  blender_add_test_lib(bf_io_alembic_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
endif()
