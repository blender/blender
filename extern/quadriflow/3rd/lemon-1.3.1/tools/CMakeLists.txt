INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_BINARY_DIR}
)

LINK_DIRECTORIES(
  ${PROJECT_BINARY_DIR}/lemon
)

ADD_EXECUTABLE(lgf-gen lgf-gen.cc)
TARGET_LINK_LIBRARIES(lgf-gen lemon)

ADD_EXECUTABLE(dimacs-to-lgf dimacs-to-lgf.cc)
TARGET_LINK_LIBRARIES(dimacs-to-lgf lemon)

ADD_EXECUTABLE(dimacs-solver dimacs-solver.cc)
TARGET_LINK_LIBRARIES(dimacs-solver lemon)

INSTALL(
  TARGETS lgf-gen dimacs-to-lgf dimacs-solver
  RUNTIME DESTINATION bin
  COMPONENT bin
)

IF(NOT WIN32)
  INSTALL(
    PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/lemon-0.x-to-1.x.sh
    DESTINATION bin
    COMPONENT bin
  )
ENDIF()
