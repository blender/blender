************
Installation
************

Troubleshooting Installation Errors
===================================

NumPy
-----

We now include NumPy code in Sverchok nodes, this means that you should have
an up-to-date version of NumPy on your machine. Normally if you get your Blender
from official site, precompiled NumPy will be included with Python, however this
isn't always the case. The windows builds on blender buildbot may contain 
a cut down version of NumPy due to the licenses under which it can be spread
in compiled form. In any operation system if you have correct python version 
installed stand-alone, you may have not proper version of numpy itself.

If you get an error when enabling Sverchok the last lines of the error
are important, if it mentions:

-  ImportError: No module named 'numpy'
-  multiarray
-  DLL failure
-  Module use of python33.dll conflicts with this version of Python

then here are steps to fix that [#]_:

- download and install Python 3.4.(1) for your os
- download and install NumPy 1.8 (for python 3.4) for your os.
- in the Blender directory rename the `python` folder to `_python` so
  Blender uses your local Python 3.4 install.

binaries:

- python: https://www.python.org/downloads/release/python-341/
- numpy: http://sourceforge.net/projects/numpy/files/NumPy/

To confirm that NumPy is installed properly on your system, for py3.4,
launch your python34 interpretter/console and the following NumPy
import should produce no error.::

    Python 3.4.1 (v3.4.1:c0e311e010fc, May 18 2014, 10:38:22) <edited>
    Type "help", "copyright", "credits" or "license" for more information.
    >>> import numpy
    >>>

.. [#] If you get an error, this means NumPy failed to install. We can't really troubleshoot that
