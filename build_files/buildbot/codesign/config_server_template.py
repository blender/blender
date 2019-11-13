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

# Configuration of a code signer which is specific to the code signing server.
#
# NOTE: DO NOT put any sensitive information here, put it in an actual
# configuration on the signing machine.

from pathlib import Path

from codesign.config_common import *

# URL to the timestamping authority.
TIMESTAMP_AUTHORITY_URL = 'http://timestamp.digicert.com'

# Full path to the certificate used for signing.
#
# The path and expected file format might vary depending on a platform.
#
# On Windows it is usually is a PKCS #12 key (.pfx), so the path will look
# like Path('C:\\Secret\\Blender.pfx').
CERTIFICATE_FILEPATH: Path

# https://docs.python.org/3/library/logging.config.html#configuration-dictionary-schema
LOGGING = {
    'version': 1,
    'formatters': {
        'default': {'format': '%(asctime)-15s %(levelname)8s %(name)s %(message)s'}
    },
    'handlers': {
        'console': {
            'class': 'logging.StreamHandler',
            'formatter': 'default',
            'stream': 'ext://sys.stderr',
        }
    },
    'loggers': {
        'codesign': {'level': 'INFO'},
    },
    'root': {
        'level': 'WARNING',
        'handlers': [
            'console',
        ],
    }
}
