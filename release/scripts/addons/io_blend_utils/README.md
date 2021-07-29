Updating this module
--------------------

This module contains copies of files belonging to
[BAM](https://pypi.python.org/pypi/blender-bam/). Fixes should be
committed to BAM and then copied here, to keep versions in sync.


Bundling BAM with Blender
-------------------------

Blender is bundled with a version of [BAM](https://pypi.python.org/pypi/blender-bam/).
To update this version, first build a new [wheel](http://pythonwheels.com/) file in
BAM itself:

    python3 setup.py bdist_wheel

Since we do not want to have binaries in the addons repository, unpack this wheel to Blender
by running:

    python3 install_whl.py /path/to/blender-asset-manager/dist/blender_bam-xxx.whl

This script also updates `__init__.py` to update the version number of the extracted
wheel, and removes any pre-existing older versions of the BAM wheels.

The version number and `.whl` extension are maintained in the directory name on purpose.
This way it is clear that it is not a directory to import directly into Blender itself.
Furthermore, I (Sybren) hope that it helps to get changes made in the addons repository
back into the BAM repository.


Running bam-pack from the wheel
-------------------------------

This is the way that Blender runs bam-pack:

    PYTHONPATH=./path/to/blender_bam-xxx.whl python3 -m bam.pack
