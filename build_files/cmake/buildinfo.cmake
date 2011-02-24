# this is called by cmake as an extermal process from
# ./source/creator/CMakeLists.txt to write ./source/creator/buildinfo.h

# the FindSubversion.cmake module is part of the standard distribution
include(FindSubversion)
# extract working copy information for SOURCE_DIR into MY_XXX variables
if(Subversion_FOUND)
	Subversion_WC_INFO(${SOURCE_DIR} MY)
else()
	set(MY_WC_REVISION "unknown")
endif()

# BUILD_PLATFORM and BUILD_PLATFORM are taken from CMake
if(UNIX)
	execute_process(COMMAND date "+%Y-%m-%d" OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND date "+%H:%M:%S" OUTPUT_VARIABLE BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)  
endif()

if(WIN32)
	execute_process(COMMAND cmd /c date /t OUTPUT_VARIABLE BUILD_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
	execute_process(COMMAND cmd /c time /t OUTPUT_VARIABLE BUILD_TIME OUTPUT_STRIP_TRAILING_WHITESPACE)  
endif()

# write a file with the SVNVERSION define
file(WRITE buildinfo.h.txt
	"#define BUILD_REV ${MY_WC_REVISION}\n"
	"#define BUILD_DATE ${BUILD_DATE}\n"
	"#define BUILD_TIME ${BUILD_TIME}\n"
)

# copy the file to the final header only if the version changes
# reduces needless rebuilds
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        buildinfo.h.txt buildinfo.h)
