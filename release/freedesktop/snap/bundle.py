#!/usr/bin/env python3

import argparse
import os
import pathlib
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--version", required=True)
parser.add_argument("--url", required=True)
parser.add_argument("--grade", default="stable", choices=["stable", "devel"])
args = parser.parse_args()

yaml_text = pathlib.Path("snapcraft.yaml.in").read_text()
yaml_text = yaml_text.replace("@VERSION@", args.version)
yaml_text = yaml_text.replace("@URL@", args.url)
yaml_text = yaml_text.replace("@GRADE@", args.grade)
pathlib.Path("snapcraft.yaml").write_text(yaml_text)

subprocess.call(["snapcraft", "clean"])
subprocess.call(["snapcraft", "snap"])
