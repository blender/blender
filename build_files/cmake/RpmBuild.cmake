#
# VLMC RPM Finder
# Authors: Rohit Yadav <rohityadav89@gmail.com>
#

if(NOT DEFINED RPMBUILD)

	find_program(RPMBUILD
		NAMES rpmbuild
		PATHS "/usr/bin")

	mark_as_advanced(RPMBUILD)

	if(RPMBUILD)
		message(STATUS "RPM Build Found: ${RPMBUILD}")
	else() 
		message(STATUS "RPM Build Not Found (rpmbuild). RPM generation will not be available")
	endif()

endif()

if(RPMBUILD)
	set(RPMBUILD_FOUND TRUE)
else() 
	set(RPMBUILD_FOUND FALSE)
endif()