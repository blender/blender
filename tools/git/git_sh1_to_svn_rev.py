#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

# generate svn rev-sha1 mapping

import os
CURRENT_DIR = os.path.abspath(os.path.dirname(__file__))
SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(CURRENT_DIR, "..", "..", ".."))))

print("creating git-log of %r" % SOURCE_DIR)
os.chdir(SOURCE_DIR)
os.system('git log --all --format="%H %cd" --date=iso > "' + CURRENT_DIR + '/git_log.txt"')

print("creating mapping...")
os.chdir(CURRENT_DIR)
time_to_sha1 = {}
f = "git_log.txt"
with open(f, "r", encoding="utf-8") as fh:
    for l in fh:
        sha1 = l[:40]
        time = l[41:60]
        time_to_sha1[time] = sha1
os.remove("git_log.txt")

# for reverse mapping
rev_sha1_ls = []

with open("rev_to_sha1.py", "w", encoding="utf-8") as fh_dst:
    fh_dst.write("data = {\n")

    f = "git_sh1_to_svn_rev.fossils"
    with open(f, "r", encoding="utf-8") as fh:
        for l in fh:
            # skip 'SVN:'
            rev, time = l[4:].split("\t", 1)
            time = time.split("Z", 1)[0].replace("T", " ", 1)
            sha1 = time_to_sha1.get(time)
            if sha1 is not None:
                fh_dst.write('%s: "%s",\n' % (rev, sha1))

                rev_sha1_ls.append((rev, sha1))

    fh_dst.write('}\n')

print("written: rev_to_sha1.py")

with open("sha1_to_rev.py", "w", encoding="utf-8") as fh_dst:
    fh_dst.write("data = {\n")
    for rev, sha1 in rev_sha1_ls:
        fh_dst.write('"%s": %s,\n' % (sha1, rev))
    fh_dst.write('}\n')

print("written: sha1_to_rev.py")
