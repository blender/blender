# This is called by cmake as an extermal process from
# ./source/creator/CMakeLists.txt to write ./source/creator/buildinfo.h

# The FindSubversion.cmake module is part of the standard distribution
include(FindSubversion)

# Extract working copy information for SOURCE_DIR into MY_XXX variables
# with a default in case anything fails, for examble when using git-svn
set(MY_WC_REVISION "unknown")
# Guess if this is a SVN working copy and then look up the revision
if(EXISTS ${SOURCE_DIR}/.svn/)
	if(Subversion_FOUND)
		Subversion_WC_INFO(${SOURCE_DIR} MY)
	endif()
endif()

# BUILD_PLATFORM and BUILD_PLATFORM are taken from CMake
# but BUILD_DATE and BUILD_TIME are plataform dependant
if(UNIX)
	execute_process(COMMAND date "+%Y-%m-%d" OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND date "+%H:%M:%S" OUTPUT_VARIABLE BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(WIN32)
	execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND cmd /c time /t OUTPUT_VARIABLE BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# Write a file with the SVNVERSION define
file(WRITE buildinfo.h.txt
	"#define BUILD_REV \"${MY_WC_REVISION}\"\n"
	"#define BUILD_DATE \"${BUILD_DATE}\"\n"
	"#define BUILD_TIME \"${BUILD_TIME}\"\n"
)

# Copy the file to the final header only if the version changes
# and avoid needless rebuilds
# TODO: verify this comment is true, as BUILD_TIME probably changes
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        buildinfo.h.txt buildinfo.h)
