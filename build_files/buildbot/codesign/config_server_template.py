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

CODESIGN_DIRECTORY = Path(__file__).absolute().parent
BLENDER_GIT_ROOT_DIRECTORY = CODESIGN_DIRECTORY.parent.parent.parent

################################################################################
# Common configuration.

# Directory where folders for codesign requests and signed result are stored.
# For example, /data/codesign
SHARED_STORAGE_DIR: Path

################################################################################
# macOS-specific configuration.

MACOS_ENTITLEMENTS_FILE = \
    BLENDER_GIT_ROOT_DIRECTORY / 'release' / 'darwin' / 'entitlements.plist'

# Identity of the Developer ID Application certificate which is to be used for
# codesign tool.
# Use `security find-identity -v -p codesigning` to find the identity.
#
# NOTE: This identity is just an example from release/darwin/README.txt.
MACOS_CODESIGN_IDENTITY = 'AE825E26F12D08B692F360133210AF46F4CF7B97'

# User name (Apple ID) which will be used to request notarization.
MACOS_XCRUN_USERNAME = 'me@example.com'

# One-time application password which will be used to request notarization.
MACOS_XCRUN_PASSWORD = '@keychain:altool-password'

# Timeout in seconds within which the notarial office is supposed to reply.
MACOS_NOTARIZE_TIMEOUT_IN_SECONDS = 60 * 60

################################################################################
# Windows-specific configuration.

# URL to the timestamping authority.
WIN_TIMESTAMP_AUTHORITY_URL = 'http://timestamp.digicert.com'

# Full path to the certificate used for signing.
#
# The path and expected file format might vary depending on a platform.
#
# On Windows it is usually is a PKCS #12 key (.pfx), so the path will look
# like Path('C:\\Secret\\Blender.pfx').
WIN_CERTIFICATE_FILEPATH: Path

################################################################################
# Logging configuration, common for all platforms.

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
