This folder contains several scripts to smoothen the Blender LTS releases.

create_download_urls.py
=======================

This python script is used to generate the download urls which we can
copy-paste directly into the CMS of www.blender.org.

Usage: create_download_urls.py --version 2.83.7

Arguments:
  --version VERSION  Version string in the form of {major}.{minor}.{build}
                     (eg 2.83.7)

The resulting html will be printed to the console.

create_release_notes.py
=======================

This python script is used to generate the release notes which we can
copy-paste directly into the CMS of www.blender.org and stores.

Usage: ./create_release_notes.py --task=T77348 --version=2.83.7

Arguments:
  --version VERSION  Version string in the form of {major}.{minor}.{build}
                     (e.g. 2.83.7)
  --task TASK        Phabricator ticket that is contains the release notes
                     information (e.g. T77348)
  --format FORMAT    Format the result in `text`, `steam`, `wiki` or `html`

Requirements
============

* Python 3.8 or later
* Python phabricator client version 0.7.0
  https://pypi.org/project/phabricator/

For convenience the python modules can be installed using pip

    pip3 install -r ./requirements.txt