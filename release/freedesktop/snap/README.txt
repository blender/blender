
Snap Package Instructions
=========================

This folder contains the scripts for creating and uploading the snap on:
https://snapcraft.io/blender


Setup
-----

This has only been tested to work on Ubuntu.

# Install required packages
sudo apt install snapd snapcraft


Steps
-----

# Build the snap file
python3 bundle.py --version 2.XX --url https://download.blender.org/release/Blender2.XX/blender-2.XX-x86_64.tar.bz2

# Install snap to test
# --dangerous is needed since the snap has not been signed yet
# --classic is required for installing Blender in general
sudo snap install --dangerous --classic blender_2.XX_amd64.snap

# Upload
snapcraft push --release=stable blender_2.XX_amd64.snap


Release Values
--------------

stable: final release
candidate: release candidates

