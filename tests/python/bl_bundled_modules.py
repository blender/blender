# SPDX-License-Identifier: GPL-2.0-or-later

# Test that modules we ship with our Python installation are available,
# both for Blender itself and the bundled Python executable.

import os
import subprocess
import sys

app = "Blender" if sys.argv[-1] == "--inside-blender" else "Python"
sys.stderr.write(f"Testing bundled modules in {app} executable.\n")

# General purpose modules.
import bz2
import certifi
import ctypes
import cython
import lzma
import numpy
import requests
import sqlite3
import ssl
import urllib3
import zlib
import zstandard

# VFX platform modules.
from pxr import Usd
import MaterialX
import OpenImageIO
import PyOpenColorIO
import pyopenvdb

# Test modules in bundled Python standalone executable.
if app == "Blender":
    script_filepath = os.path.abspath(__file__)
    proc = subprocess.Popen([sys.executable, script_filepath])
    sys.exit(proc.wait())
