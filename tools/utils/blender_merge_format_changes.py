#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import subprocess
import sys

# We unfortunately ended with three commits instead of a single one to be handled as
# 'clang-format' commit, we are handling them as a single 'block'.
format_commits = (
    'e12c08e8d170b7ca40f204a5b0423c23a9fbc2c1',
    '91a9cd0a94000047248598394c41ac30f893f147',
    '3076d95ba441cd32706a27d18922a30f8fd28b8a',
)
pre_format_commit = format_commits[0] + '~1'


def get_string(cmd):
    return subprocess.run(cmd, stdout=subprocess.PIPE).stdout.decode('utf8').strip()


# Parse arguments.
mode = None
base_branch = 'main'
if len(sys.argv) >= 2:
    # Note that recursive conflict resolution strategy has to reversed in rebase compared to merge.
    # See https://git-scm.com/docs/git-rebase#Documentation/git-rebase.txt--m
    if sys.argv[1] == '--rebase':
        mode = 'rebase'
        recursive_format_commit_merge_options = '-Xignore-all-space -Xtheirs'
    elif sys.argv[1] == '--merge':
        mode = 'merge'
        recursive_format_commit_merge_options = '-Xignore-all-space -Xours'
    if len(sys.argv) == 4:
        if sys.argv[2] == '--base_branch':
            base_branch = sys.argv[3]

if mode is None:
    print("Merge or rebase Blender main (or another base branch) into a branch in 3 steps,")
    print("to automatically merge clang-format changes.")
    print("")
    print("  --rebase     Perform equivalent of 'git rebase main'")
    print("  --merge      Perform equivalent of 'git merge main'")
    print("")
    print("Optional arguments:")
    print("  --base_branch <branch name>  Use given branch instead of main")
    print("                               (assuming that base branch has already been updated")
    print("                                and has the initial clang-format commit).")
    sys.exit(0)

# Verify we are in the right directory.
root_path = get_string(['git', 'rev-parse', '--show-superproject-working-tree'])
if os.path.realpath(root_path) != os.path.realpath(os.getcwd()):
    print("BLENDER MERGE: must run from blender repository root directory")
    sys.exit(1)

# Abort if a rebase is still progress.
rebase_merge = get_string(['git', 'rev-parse', '--git-path', 'rebase-merge'])
rebase_apply = get_string(['git', 'rev-parse', '--git-path', 'rebase-apply'])
merge_head = get_string(['git', 'rev-parse', '--git-path', 'MERGE_HEAD'])
if os.path.exists(rebase_merge) or \
   os.path.exists(rebase_apply) or \
   os.path.exists(merge_head):
    print("BLENDER MERGE: rebase or merge in progress, complete it first")
    sys.exit(1)

# Abort if uncommitted changes.
changes = get_string(['git', 'status', '--porcelain', '--untracked-files=no'])
if len(changes) != 0:
    print("BLENDER MERGE: detected uncommitted changes, can't run")
    sys.exit(1)

# Setup command, with commit message for merge commits.
if mode == 'rebase':
    mode_cmd = 'rebase'
else:
    branch = get_string(['git', 'rev-parse', '--abbrev-ref', 'HEAD'])
    mode_cmd = 'merge --no-edit -m "Merge \'' + base_branch + '\' into \'' + branch + '\'"'

# Rebase up to the clang-format commit.
code = os.system('git merge-base --is-ancestor ' + pre_format_commit + ' HEAD')
if code != 0:
    code = os.system('git ' + mode_cmd + ' ' + pre_format_commit)
    if code != 0:
        print("BLENDER MERGE: resolve conflicts, complete " + mode + " and run again")
        sys.exit(code)

# Rebase clang-format commit.
code = os.system('git merge-base --is-ancestor ' + format_commits[-1] + ' HEAD')
if code != 0:
    os.system('git ' + mode_cmd + ' ' + recursive_format_commit_merge_options + ' ' + format_commits[-1])
    paths = get_string(('git', '--no-pager', 'diff', '--name-only', format_commits[-1])).replace('\n', ' ')
    if sys.platform == 'win32' and len(paths) > 8000:
        # Windows command-line does not accept more than 8191 chars.
        os.system('make format')
    else:
        os.system('make format PATHS="' + paths + '"')
    os.system('git add -u')
    count = int(get_string(['git', 'rev-list', '--count', '' + format_commits[-1] + '..HEAD']))
    if count == 1 or mode == 'merge':
        # Amend if we just have a single commit or are merging.
        os.system('git commit --amend --no-edit')
    else:
        # Otherwise create a commit for formatting.
        os.system('git commit -m "Cleanup: apply clang format"')

# Rebase remaining commits
code = os.system('git ' + mode_cmd + ' ' + base_branch)
if code != 0:
    print("BLENDER MERGE: resolve conflicts, complete " + mode + " and you're done")
else:
    print("BLENDER MERGE: done")
sys.exit(code)
