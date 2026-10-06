# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This will run in a post install action to clean up any pdb files
# we do not wish to ship

file(GLOB_RECURSE PDB_FILES "${CLEAN_DIR}/*.pdb")

if(PDB_FILES)
  file(REMOVE ${PDB_FILES})
endif()
