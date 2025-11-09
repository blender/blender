# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import requests

website = "https://blender.org/"

r = requests.get(website, verify=True, timeout=30)

assert r.status_code == 200, f"{website} returned a status code {r.status_code}, we expected 200"
assert r.reason == "OK", f"Didn't get 'OK' response from {website}, got {r.reason}"
assert len(r.content) > 256, "The content we got from the web request is too small to be valid"
