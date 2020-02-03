# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

from pathlib import Path

# Timeout in seconds for the signing process.
#
# This is how long buildbot packing step will wait signing server to
# perform signing.
#
# NOTE: Notarization could take a long time, hence the rather high value
# here. Might consider using different timeout for different platforms.
TIMEOUT_IN_SECONDS = 45 * 60 * 60

# Directory which is shared across buildbot worker and signing server.
#
# This is where worker puts files requested for signing as well as where
# server puts signed files.
SHARED_STORAGE_DIR: Path
