
# Python CTests
if(WITH_BLENDER AND WITH_PYTHON)
	add_subdirectory(python)
endif()

# GTest
add_subdirectory(gtests)
