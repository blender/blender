#!/usr/bin/env python3.5

"""This Python script installs a new version of BAM here."""

import pathlib

my_dir = pathlib.Path(__file__).absolute().parent


def main():
    import argparse

    parser = argparse.ArgumentParser(description="This script installs a new version of BAM here.")
    parser.add_argument('wheelfile', type=pathlib.Path,
                        help='Location of the wheel file to install.')

    args = parser.parse_args()
    install(args.wheelfile.expanduser())


def install(wheelfile: pathlib.Path):
    import json
    import os
    import re

    assert_is_zipfile(wheelfile)
    wipe_preexisting()

    print('Installing %s' % wheelfile)
    target = my_dir / 'blender_bam-unpacked.whl'
    print('Creating target directory %s' % target)
    target.mkdir(parents=True)

    extract(wheelfile, target)
    copy_files(target)

    version = find_version(target)
    print('This is BAM version %s' % (version, ))
    update_init_file(version)

    print('Done installing %s' % wheelfile.name)


def assert_is_zipfile(wheelfile: pathlib.Path):
    import zipfile

    # In Python 3.6 conversion to str is not necessary any more:
    if not zipfile.is_zipfile(str(wheelfile)):
        log.error('%s is not a valid ZIP file!' % wheelfile)
        raise SystemExit()


def wipe_preexisting():
    import shutil

    for existing in sorted(my_dir.glob('blender_bam-*.whl')):
        if existing.is_dir():
            print('Wiping pre-existing directory %s' % existing)
            # In Python 3.6 conversion to str is not necessary any more:
            shutil.rmtree(str(existing))
        else:
            print('Wiping pre-existing file %s' % existing)
            existing.unlink()


def extract(wheelfile: pathlib.Path, target: pathlib.Path):
    import os
    import zipfile

    # In Python 3.6 conversion to str is not necessary any more:
    os.chdir(str(target))

    print('Extracting wheel')
    # In Python 3.6 conversion to str is not necessary any more:
    with zipfile.ZipFile(str(wheelfile)) as whlzip:
        whlzip.extractall()

    os.chdir(str(my_dir))


def copy_files(target: pathlib.Path):
    import shutil

    print('Copying some files from wheel to other locations')
    # In Python 3.6 conversion to str is not necessary any more:
    shutil.copy(str(target / 'bam' / 'blend' / 'blendfile_path_walker.py'), './blend')
    shutil.copy(str(target / 'bam' / 'blend' / 'blendfile.py'), './blend')
    shutil.copy(str(target / 'bam' / 'utils' / 'system.py'), './utils')


def find_version(target: pathlib.Path):
    import json
    import shutil

    print('Obtaining version number from wheel.')

    distinfo = next(target.glob('*.dist-info'))
    with (distinfo / 'metadata.json').open() as infofile:
        metadata = json.load(infofile)

    print('Wiping dist-info directory.')
    shutil.rmtree(str(distinfo))

    # "1.2.3" -> (1, 2, 3)
    str_ver = metadata['version']
    return tuple(int(x) for x in str_ver.split('.'))


def update_init_file(version: tuple):
    import os
    import re

    print('Updating __init__.py to have the correct version.')
    version_line_re = re.compile(r'^\s+[\'"]version[\'"]: (\([0-9,]+\)),')

    with open('__init__.py', 'r') as infile, \
         open('__init__.py~whl~installer~', 'w') as outfile:

        for line in infile:
            if version_line_re.match(line):
                outfile.write("    'version': %s,%s" % (version, os.linesep))
            else:
                outfile.write(line)

    os.unlink('__init__.py')
    os.rename('__init__.py~whl~installer~', '__init__.py')

if __name__ == '__main__':
    main()
