Auto Render Regression suite
==================================

Running
==================================

./blender -b -P /path/to/tests/auto/render_test_files.py

If desired the blender executable path can be set in test_config.py to
run the script outside of Blender with:

python render_test_files.py

Results
==================================

It saves all renders and additional info into tests/auto/test_renders
The reference renders are in tests/auto/reference_renders

Comparisons
==================================

Manual comparison is possible, easier is to use automatic comparsion with
OpenImageIO installed. If it's in PATH then it will be found automatically,
otherwise the path can be set in test_config.py.

For comparing two Blender versions, you can run the tests with old version,
copy the .png files from test_renders to reference_renders and then run the
tests with the newer version.

Notes
==================================

* test_run.py is executed for each .blend file.
* The script renders every cycles file twice on the CPU (SVM and OSL).
