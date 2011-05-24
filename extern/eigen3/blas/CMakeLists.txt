
project(EigenBlas CXX)

if( NOT DEFINED EIGEN_Fortran_COMPILER_WORKS OR EIGEN_Fortran_COMPILER_WORKS)

  enable_language(Fortran OPTIONAL)

  if(CMAKE_Fortran_COMPILER_WORKS)
    set(EIGEN_Fortran_COMPILER_WORKS TRUE CACHE INTERNAL "workaround cmake's enable_language issue")
  else()
    set(EIGEN_Fortran_COMPILER_WORKS FALSE CACHE INTERNAL "workaround cmake's enable_language issue")
  endif()
  
endif()

if(CMAKE_Fortran_COMPILER_WORKS)

add_custom_target(blas)

set(EigenBlas_SRCS single.cpp double.cpp complex_single.cpp complex_double.cpp xerbla.cpp
    complexdots.f
    srotm.f srotmg.f drotm.f drotmg.f
    lsame.f   chpr2.f  ctbsv.f  dspmv.f  dtbmv.f  dtpsv.f ssbmv.f  sspr.f   stpmv.f     zhpr2.f  ztbsv.f chbmv.f  chpr.f   ctpmv.f     dspr2.f dtbsv.f  sspmv.f  stbmv.f  stpsv.f   zhbmv.f  zhpr.f   ztpmv.f chpmv.f  ctbmv.f  ctpsv.f    dsbmv.f  dspr.f   dtpmv.f   sspr2.f  stbsv.f  zhpmv.f  ztbmv.f  ztpsv.f
)

add_library(eigen_blas_static ${EigenBlas_SRCS})
add_library(eigen_blas SHARED ${EigenBlas_SRCS})

if(EIGEN_STANDARD_LIBRARIES_TO_LINK_TO)
  target_link_libraries(eigen_blas_static ${EIGEN_STANDARD_LIBRARIES_TO_LINK_TO})
  target_link_libraries(eigen_blas        ${EIGEN_STANDARD_LIBRARIES_TO_LINK_TO})
endif()

add_dependencies(blas eigen_blas eigen_blas_static)

install(TARGETS eigen_blas
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)


if(EIGEN_LEAVE_TEST_IN_ALL_TARGET)
  add_subdirectory(testing) # can't do EXCLUDE_FROM_ALL here, breaks CTest
else()
  add_subdirectory(testing EXCLUDE_FROM_ALL)
endif()

endif(CMAKE_Fortran_COMPILER_WORKS)
