This folder contains a script to generate release notes and download URLs
for Blender LTS releases.

Ensure required Python modules are installed before running:

    pip3 install -r ./requirements.txt

Then run for example:

    ./create_release_notes.py --version 3.3.2 --format=html

Available arguments:

    --version VERSION  Version string in the form of {major}.{minor}.{build}
                       (e.g. 3.3.2)
    --issue ISSUE      Gitea issue that is contains the release notes
                       information (e.g. #77348)
    --format FORMAT    Format the result in `text`, `steam`, `wiki` or `html`
