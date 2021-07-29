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

import os
import bpy

from . import communication

# Set/created upon register.
profiles_path = ''
profiles_file = ''


class _BIPMeta(type):
    """Metaclass for BlenderIdProfile."""

    def __str__(self):
        # noinspection PyUnresolvedReferences
        return '%s(user_id=%r)' % (self.__qualname__, self.user_id)


class BlenderIdProfile(metaclass=_BIPMeta):
    """Current Blender ID profile.

    This is always stored at class level, as there is only one current
    profile anyway.
    """

    user_id = ''
    username = ''
    token = ''
    expires = ''
    subclients = {}

    @classmethod
    def reset(cls):
        cls.user_id = ''
        cls.username = ''
        cls.token = ''
        cls.expires = ''
        cls.subclients = {}

    @classmethod
    def read_json(cls):
        """Updates the active profile information from the JSON file."""

        cls.reset()

        active_profile = get_active_profile()
        if not active_profile:
            return

        for key, value in active_profile.items():
            if hasattr(cls, key):
                setattr(cls, key, value)
            else:
                print('Skipping key %r from profile JSON' % key)

    @classmethod
    def save_json(cls, make_active_profile=False):
        """Updates the JSON file with the active profile information."""

        jsonfile = get_profiles_data()
        jsonfile['profiles'][cls.user_id] = {
            'username': cls.username,
            'token': cls.token,
            'expires': cls.expires,
            'subclients': cls.subclients,
        }

        if make_active_profile:
            jsonfile['active_profile'] = cls.user_id

        save_profiles_data(jsonfile)


def register():
    global profiles_path, profiles_file

    profiles_path = bpy.utils.user_resource('CONFIG', 'blender_id', create=True)
    profiles_file = os.path.join(profiles_path, 'profiles.json')


def _create_default_file():
    """Creates the default profile file, returning its contents."""
    import json

    profiles_default_data = {
        'active_profile': None,
        'profiles': {}
    }

    os.makedirs(profiles_path, exist_ok=True)

    # Populate the file, ensuring that its permissions are restrictive enough.
    old_umask = os.umask(0o077)
    try:
        with open(profiles_file, 'w', encoding='utf8') as outfile:
            json.dump(profiles_default_data, outfile)
    finally:
        os.umask(old_umask)

    return profiles_default_data


def get_profiles_data():
    """Returns the profiles.json content from a blender_id folder in the
    Blender config directory. If the file does not exist we create one with the
    basic data structure.
    """
    import json

    # if the file does not exist
    if not os.path.exists(profiles_file):
        return _create_default_file()

    # try parsing the file
    with open(profiles_file, 'r', encoding='utf8') as f:
        try:
            file_data = json.load(f)
            file_data['active_profile']
            file_data['profiles']
            return file_data
        except (ValueError,  # malformed json data
                KeyError):  # it doesn't have the expected content
            print('(%s) '
                  'Warning: profiles.json is either empty or malformed. '
                  'The file will be reset.' % __name__)

            # overwrite the file
            return _create_default_file()


def get_active_user_id():
    """Get the id of the currently active profile. If there is no
    active profile on the file, this function will return None.
    """

    return get_profiles_data()['active_profile']


def get_active_profile():
    """Pick the active profile from profiles.json. If there is no
    active profile on the file, this function will return None.

    @returns: dict like {'user_id': 1234, 'username': 'email@blender.org'}
    """
    file_content = get_profiles_data()
    user_id = file_content['active_profile']
    if not user_id or user_id not in file_content['profiles']:
        return None

    profile = file_content['profiles'][user_id]
    profile['user_id'] = user_id
    return profile


def get_profile(user_id):
    """Loads the profile data for a given user_id if existing
    else it returns None.
    """

    file_content = get_profiles_data()
    if not user_id or user_id not in file_content['profiles']:
        return None

    profile = file_content['profiles'][user_id]
    return dict(
        username=profile['username'],
        token=profile['token']
    )


def save_profiles_data(all_profiles: dict):
    """Saves the profiles data to JSON."""
    import json

    with open(profiles_file, 'w', encoding='utf8') as outfile:
        json.dump(all_profiles, outfile, sort_keys=True)


def save_as_active_profile(auth_result: communication.AuthResult, username, subclients):
    """Saves the given info as the active profile."""

    BlenderIdProfile.user_id = auth_result.user_id
    BlenderIdProfile.token = auth_result.token
    BlenderIdProfile.expires = auth_result.expires

    BlenderIdProfile.username = username
    BlenderIdProfile.subclients = subclients

    BlenderIdProfile.save_json(make_active_profile=True)


def logout(user_id):
    """Invalidates the token and state of active for this user.
    This is different from switching the active profile, where the active
    profile is changed but there isn't an explicit logout.
    """
    import json

    file_content = get_profiles_data()

    # Remove user from 'active profile'
    if file_content['active_profile'] == user_id:
        file_content['active_profile'] = ""

    # Remove both user and token from profiles list
    if user_id in file_content['profiles']:
        del file_content['profiles'][user_id]

    with open(profiles_file, 'w', encoding='utf8') as outfile:
        json.dump(file_content, outfile)
