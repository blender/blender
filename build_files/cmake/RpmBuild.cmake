#
# VLMC RPM Finder
# Authors: Rohit Yadav <rohityadav89@gmail.com>
#

find_program(RPMBUILD
    NAMES rpmbuild
    PATHS "/usr/bin")

mark_as_advanced(RPMBUILD)

if(RPMBUILD)
    get_filename_component(RPMBUILD_PATH ${RPMBUILD} ABSOLUTE)
    message(STATUS "Found rpmbuild : ${RPMBUILD_PATH}")
    set(RPMBUILD_FOUND "YES")
else(RPMBUILD) 
    message(STATUS "rpmbuild NOT found. RPM generation will not be available")
    set(RPMBUILD_FOUND "NO")
endif()
