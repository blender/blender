# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = [
    "token_from_asset_url",
]


def token_from_asset_url(remote_url: str) -> str | None:
    """
    Return the access token of the extension repository an asset library was installed from,
    or None when the library isn't from an extension or the repository doesn't define the token
    for its asset libraries.
    """
    from bl_pkg import asset_auth_token_from_url

    # We might want to return more of the `auth`,
    # it's accessible from this function but currently unused.
    return asset_auth_token_from_url(remote_url)
