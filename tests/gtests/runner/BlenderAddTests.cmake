# Disable ASAN leak detection when trying to discover tests.
set(ENV{ASAN_OPTIONS} "detect_leaks=0")
include(GoogleTestAddTests)
