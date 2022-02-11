#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

find ./libmv/ -type f | sed -r 's/^\.\///' | sort > files.txt
find ./third_party/ -type f | \
    grep -v third_party/ceres | \
    sed -r 's/^\.\///' | sort >> files.txt
