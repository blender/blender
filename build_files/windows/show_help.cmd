echo.
echo Convenience targets
echo - release ^(identical to the official blender.org builds^)
echo - full ^(same as release minus the cuda kernels^)
echo - lite
echo - headless
echo - cycles
echo - bpy
echo.
echo Utilities ^(not associated with building^)
echo - clean ^(Target must be set^)
echo - update ^(Update both SVN and GIT^)
echo - code_update ^(Update only GIT^)
echo - nobuild ^(only generate project files^)
echo - showhash ^(Show git hashes of source tree^)
echo - test ^(Run automated tests with ctest^)
echo - format [path] ^(Format the source using clang-format, path is optional, requires python 3.x to be available^)
echo.
echo Configuration options
echo - verbose ^(enable diagnostic output during configuration^)
echo - developer ^(enable faster builds, error checking and tests, recommended for developers^)
echo - with_tests ^(enable building unit tests^)
echo - nobuildinfo ^(disable buildinfo^)
echo - debug ^(Build an unoptimized debuggable build^)
echo - packagename [newname] ^(override default cpack package name^)
echo - builddir [newdir] ^(override default build folder^)
echo - 2019 ^(build with visual studio 2019^)
echo - 2019pre ^(build with visual studio 2019 pre-release^)
echo - 2019b ^(build with visual studio 2019 Build Tools^)
echo - 2022 ^(build with visual studio 2022^)
echo - 2022pre ^(build with visual studio 2022 pre-release^)
echo - 2022b ^(build with visual studio 2022 Build Tools^)

echo.
echo Documentation Targets ^(Not associated with building^)
echo - doc_py ^(Generate sphinx python api docs^)

echo.
echo Experimental options
echo - with_opengl_tests ^(enable both the render and draw opengl test suites^)
echo - clang ^(enable building with clang^)
echo - asan ^(enable asan when building with clang^)
echo - ninja ^(enable building with ninja instead of msbuild^)
echo.
