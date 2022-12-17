# SPDX-License-Identifier: GPL-2.0-or-later

# CVE Check requirements
#
# - A working installation of intels cve-bin-tool [1] has to be available in
#   your path
#
# - Not strictly required, but highly recommended is obtaining a NVD key from
#   nist since it significantly speeds up downloading/updating the required
#   databases one can request a key on the following website:
#   https://nvd.nist.gov/developers/request-an-api-key

# Bill of Materials construction
#
# This constructs a CSV cve-bin-tool [1] can read and process. Sadly
# cve-bin-tool at this point does not take a list of CPE's and output a check
# based on that list. so we need to pick apart the CPE retrieve the vendor,
# product and version tokens and generate a CSV.
#
# [1] https://github.com/intel/cve-bin-tool

# Because not all deps are downloaded (ie python packages) but can still have a
# xxx_CPE declared loop over all variables and look for variables ending in CPE.

set(SBOMCONTENTS)
get_cmake_property(_variableNames VARIABLES)
foreach(_variableName ${_variableNames})
  if(_variableName MATCHES "CPE$")
    string(REPLACE ":" ";" CPE_LIST ${${_variableName}})
    string(REPLACE "_CPE" "_ID" CPE_DEPNAME ${_variableName})
    list(GET CPE_LIST 3 CPE_VENDOR)
    list(GET CPE_LIST 4 CPE_NAME)
    list(GET CPE_LIST 5 CPE_VERSION)
    set(${CPE_DEPNAME} "${CPE_VENDOR},${CPE_NAME},${CPE_VERSION}")
    set(SBOMCONTENTS "${SBOMCONTENTS}${CPE_VENDOR},${CPE_NAME},${CPE_VERSION},,,\n")
  endif()
endforeach()
configure_file(${CMAKE_SOURCE_DIR}/cmake/cve_check.csv.in ${CMAKE_CURRENT_BINARY_DIR}/cve_check.csv @ONLY)

# Custom Targets
#
# This defines two new custom targets one could run in the build folder
# `cve_check` which will output the report to the console, and `cve_check_html`
# which will write out blender_dependencies.html in the build folder that one
# could share with other people or be used to get more information on the
# reported CVE's.
#
# cve-bin-tool takes data from the nist nvd database which rate limits
# unauthenticated requests to 1 requests per 6 seconds making the database
# download take "quite a bit" of time.
#
# When adding -DCVE_CHECK_NVD_KEY=your_api_key_here to your cmake invocation
# this key will be passed on to cve-bin-tool speeding up the process.
#
if(DEFINED CVE_CHECK_NVD_KEY)
  set(NVD_ARGS --nvd-api-key ${CVE_CHECK_NVD_KEY})
endif()

# This will just report to the console
add_custom_target(cve_check
  COMMAND cve-bin-tool
    ${NVD_ARGS}
    -i ${CMAKE_CURRENT_BINARY_DIR}/cve_check.csv
    --affected-versions
  SOURCES ${CMAKE_CURRENT_BINARY_DIR}/cve_check.csv
)

# This will write out blender_dependencies.html
add_custom_target(cve_check_html
  COMMAND cve-bin-tool
    ${NVD_ARGS}
    -i ${CMAKE_CURRENT_BINARY_DIR}/cve_check.csv
    -f html
  SOURCES ${CMAKE_CURRENT_BINARY_DIR}/cve_check.csv
)
