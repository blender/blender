# CMake script to test if a file exists. Errors if the file does not exist.
# Expect actual arguments to start at index 3 (cmake -P <script_name>)

# Expect one argument
if(NOT (CMAKE_ARGC EQUAL "4"))
    message(FATAL_ERROR "Test Internal Error: Unexpected ARGC Value: ${CMAKE_ARGC}.")
endif()

set(FILE_PATH "${CMAKE_ARGV3}")

if(NOT ( EXISTS "${FILE_PATH}" ))
    message(FATAL_ERROR "Test failed: File `${FILE_PATH}` does not exist.")
endif()
