FILE(GLOB Eigen_Sparse_SRCS "*.h")

INSTALL(FILES 
  ${Eigen_Sparse_SRCS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/Eigen/src/Sparse COMPONENT Devel
  )
