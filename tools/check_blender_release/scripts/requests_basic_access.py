# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import requests

# Test this specific endpoint because this will check for a connection to the
# buildbot master. We should be quite sure that this endpoint is up if the
# builder is running the release check scripts.
website = "https://builder.blender.org/admin/"

r = requests.get(website, verify=True, timeout=30)

assert r.status_code == 200, f"{website} returned a status code {r.status_code}, we expected 200"
assert r.reason == "OK", f"Didn't get 'OK' response from {website}, got {r.reason}"
assert len(r.content) > 256, "The content we got from the web request is too small to be valid"
