#!/usr/bin/env python3
#
# Utility functions for make update and make tests.

import subprocess
import sys

def call(cmd):
    print(" ".join(cmd))

    # Flush to ensure correct order output on Windows.
    sys.stdout.flush()
    sys.stderr.flush()

    retcode = subprocess.call(cmd)
    if retcode != 0:
      sys.exit(retcode)
