# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
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

# Dynamically loaded modules, to ensure they have satisfactory dependencies.
import _blake2

# VFX platform modules.
from pxr import Usd
import MaterialX
import OpenImageIO
import PyOpenColorIO

# Test both old and new names, remove when all 4.4 libs have landed.
try:
    import pyopenvdb
except ModuleNotFoundError:
    import openvdb
    import oslquery

# Test modules in bundled Python standalone executable.
if app == "Blender":
    script_filepath = os.path.abspath(__file__)
    proc = subprocess.Popen([sys.executable, script_filepath])
    sys.exit(proc.wait())
