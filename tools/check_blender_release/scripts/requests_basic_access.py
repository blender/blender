# SPDX-License-Identifier: GPL-2.0-or-later
import requests

r = requests.get("https://blender.org/", verify=True)

assert r.status_code == 200
assert r.reason == "OK"
assert True if r.ok else False
assert len(r.content) > 256
