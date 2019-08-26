INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
)

LINK_DIRECTORIES(
  ${PROJECT_BINARY_DIR}/lemon
)

SET(TEST_WITH_VALGRIND "NO" CACHE STRING
  "Run the test with valgrind (YES/NO).")
SET(VALGRIND_FLAGS "" CACHE STRING "Valgrind flags used by the tests.")

SET(TESTS
  adaptors_test
  arc_look_up_test
  bellman_ford_test
  bfs_test
  bpgraph_test
  circulation_test
  connectivity_test
  counter_test
  dfs_test
  digraph_test
  dijkstra_test
  dim_test
  edge_set_test
  error_test
  euler_test
  fractional_matching_test
  gomory_hu_test
  graph_copy_test
  graph_test
  graph_utils_test
  hao_orlin_test
  heap_test
  kruskal_test
  lgf_reader_writer_test
  lgf_test
  maps_test
  matching_test
  max_cardinality_search_test
  max_clique_test
  max_flow_test
  min_cost_arborescence_test
  min_cost_flow_test
  min_mean_cycle_test
  nagamochi_ibaraki_test
  path_test
  planarity_test
  radix_sort_test
  random_test
  suurballe_test
  time_measure_test
  tsp_test
  unionfind_test
)

IF(LEMON_HAVE_LP)
  IF(${CMAKE_BUILD_TYPE} STREQUAL "Maintainer")
    ADD_EXECUTABLE(lp_test lp_test.cc)
  ELSE()
    ADD_EXECUTABLE(lp_test EXCLUDE_FROM_ALL lp_test.cc)
  ENDIF()

  SET(LP_TEST_LIBS lemon)

  IF(LEMON_HAVE_GLPK)
    SET(LP_TEST_LIBS ${LP_TEST_LIBS} ${GLPK_LIBRARIES})
  ENDIF()
  IF(LEMON_HAVE_CPLEX)
    SET(LP_TEST_LIBS ${LP_TEST_LIBS} ${ILOG_LIBRARIES})
  ENDIF()
  IF(LEMON_HAVE_CLP)
    SET(LP_TEST_LIBS ${LP_TEST_LIBS} ${COIN_CLP_LIBRARIES})
  ENDIF()
  IF(LEMON_HAVE_SOPLEX)
    SET(LP_TEST_LIBS ${LP_TEST_LIBS} ${SOPLEX_LIBRARIES})
  ENDIF()

  TARGET_LINK_LIBRARIES(lp_test ${LP_TEST_LIBS})
  ADD_TEST(lp_test lp_test)
  ADD_DEPENDENCIES(check lp_test)

  IF(WIN32 AND LEMON_HAVE_GLPK)
    GET_TARGET_PROPERTY(TARGET_LOC lp_test LOCATION)
    GET_FILENAME_COMPONENT(TARGET_PATH ${TARGET_LOC} PATH)
    ADD_CUSTOM_COMMAND(TARGET lp_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${GLPK_BIN_DIR}/glpk.dll ${TARGET_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy ${GLPK_BIN_DIR}/libltdl3.dll ${TARGET_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy ${GLPK_BIN_DIR}/zlib1.dll ${TARGET_PATH}
    )
  ENDIF()

  IF(WIN32 AND LEMON_HAVE_CPLEX)
    GET_TARGET_PROPERTY(TARGET_LOC lp_test LOCATION)
    GET_FILENAME_COMPONENT(TARGET_PATH ${TARGET_LOC} PATH)
    ADD_CUSTOM_COMMAND(TARGET lp_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${ILOG_CPLEX_DLL} ${TARGET_PATH}
    )
  ENDIF()
ENDIF()

IF(LEMON_HAVE_MIP)
  IF(${CMAKE_BUILD_TYPE} STREQUAL "Maintainer")
    ADD_EXECUTABLE(mip_test mip_test.cc)
  ELSE()
    ADD_EXECUTABLE(mip_test EXCLUDE_FROM_ALL mip_test.cc)
  ENDIF()

  SET(MIP_TEST_LIBS lemon)

  IF(LEMON_HAVE_GLPK)
    SET(MIP_TEST_LIBS ${MIP_TEST_LIBS} ${GLPK_LIBRARIES})
  ENDIF()
  IF(LEMON_HAVE_CPLEX)
    SET(MIP_TEST_LIBS ${MIP_TEST_LIBS} ${ILOG_LIBRARIES})
  ENDIF()
  IF(LEMON_HAVE_CBC)
    SET(MIP_TEST_LIBS ${MIP_TEST_LIBS} ${COIN_CBC_LIBRARIES})
  ENDIF()

  TARGET_LINK_LIBRARIES(mip_test ${MIP_TEST_LIBS})
  ADD_TEST(mip_test mip_test)
  ADD_DEPENDENCIES(check mip_test)

  IF(WIN32 AND LEMON_HAVE_GLPK)
    GET_TARGET_PROPERTY(TARGET_LOC mip_test LOCATION)
    GET_FILENAME_COMPONENT(TARGET_PATH ${TARGET_LOC} PATH)
    ADD_CUSTOM_COMMAND(TARGET mip_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${GLPK_BIN_DIR}/glpk.dll ${TARGET_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy ${GLPK_BIN_DIR}/libltdl3.dll ${TARGET_PATH}
      COMMAND ${CMAKE_COMMAND} -E copy ${GLPK_BIN_DIR}/zlib1.dll ${TARGET_PATH}
    )
  ENDIF()

  IF(WIN32 AND LEMON_HAVE_CPLEX)
    GET_TARGET_PROPERTY(TARGET_LOC mip_test LOCATION)
    GET_FILENAME_COMPONENT(TARGET_PATH ${TARGET_LOC} PATH)
    ADD_CUSTOM_COMMAND(TARGET mip_test POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${ILOG_CPLEX_DLL} ${TARGET_PATH}
    )
  ENDIF()
ENDIF()

FOREACH(TEST_NAME ${TESTS})
  IF(${CMAKE_BUILD_TYPE} STREQUAL "Maintainer")
    ADD_EXECUTABLE(${TEST_NAME} ${TEST_NAME}.cc)
  ELSE()
    ADD_EXECUTABLE(${TEST_NAME} EXCLUDE_FROM_ALL ${TEST_NAME}.cc)
  ENDIF()
  TARGET_LINK_LIBRARIES(${TEST_NAME} lemon)
    IF(TEST_WITH_VALGRIND)
      ADD_TEST(${TEST_NAME}
        valgrind --error-exitcode=1 ${VALGRIND_FLAGS}
        ${CMAKE_CURRENT_BINARY_DIR}/${TEST_NAME} )
    ELSE()
      ADD_TEST(${TEST_NAME} ${TEST_NAME})
    ENDIF()
  ADD_DEPENDENCIES(check ${TEST_NAME})
ENDFOREACH()
