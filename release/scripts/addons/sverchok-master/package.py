import zipfile
import os
import sys

root_folder = os.path.dirname(__file__)
addon_folder = os.path.join(root_folder, 'distribute', 'sverchok')
target_folder = os.path.join(root_folder)

file_excludes = [".directory",
                 ".DS",
                 "__MACOSX",
                 "kdev4",
                 "~"]

def zipfolder(foldername, target_dir):
    zipobj = zipfile.ZipFile(foldername + '.zip', 'w', zipfile.ZIP_DEFLATED)
    rootlen = len(target_dir) + 1
    for base, dirs, files in os.walk(target_dir):
        files_list = []
        for f in files:
            for ex in file_excludes:
                if ex in f:
                    break
            files_list.append(f)
        for file in files_list:
            if "sverchok" in base and "__pycache__" not in base and "kdev4" not in base:
                fn = os.path.join(base, file)
                zipobj.write(fn, fn[rootlen:])

#zipfolder(addon_folder, target_folder)
#sys.exit()
