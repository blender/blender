#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import collections
import os
import subprocess
import sys

# Hashes to be ignored
#
# The system sometimes fails to match commits and suggests to backport
# revision which was already ported. In order to solve that we can:
#
# - Explicitly ignore some of the commits.
# - Move the synchronization point forward.
IGNORE_HASHES = {
}

# Start revisions from both repositories.
CYCLES_START_COMMIT = b"b941eccba81bbb1309a0eb4977fc3a77796f4ada"  # blender-v2.92
BLENDER_START_COMMIT = b"02948a2cab44f74ed101fc1b2ad9fe4431123e85"  # v2.92

# Prefix which is common for all the subjects.
GIT_SUBJECT_COMMON_PREFIX = b"Subject: [PATCH] "

# Marker which indicates begin of new file in the patch set.
GIT_FILE_SECTION_MARKER = b"diff --git"

# Marker of the end of the patchset.
GIT_PATCHSET_END_MARKER = b"-- "

# Prefix of topic to be omitted
SUBJECT_SKIP_PREFIX = (
    b"Cycles: ",
    b"cycles: ",
    b"Cycles Standalone: ",
    b"Cycles standalone: ",
    b"cycles standalone: ",
)


def subject_strip(common_prefix, subject):
    for prefix in SUBJECT_SKIP_PREFIX:
        full_prefix = common_prefix + prefix
        if subject.startswith(full_prefix):
            subject = subject[len(full_prefix):].capitalize()
            subject = common_prefix + subject
            break
    return subject


def replace_file_prefix(path, prefix, replace_prefix):
    tokens = path.split(b' ')
    prefix_len = len(prefix)
    for i, t in enumerate(tokens):
        for x in (b"a/", b"b/"):
            if t.startswith(x + prefix):
                tokens[i] = x + replace_prefix + t[prefix_len + 2:]
    return b' '.join(tokens)


def cleanup_patch(patch, accept_prefix, replace_prefix):
    assert accept_prefix[0] != b'/'
    assert replace_prefix[0] != b'/'

    full_accept_prefix = GIT_FILE_SECTION_MARKER + b" a/" + accept_prefix

    with open(patch, "rb") as f:
        content = f.readlines()

    clean_content = []
    do_skip = False
    for line in content:
        if line.startswith(GIT_SUBJECT_COMMON_PREFIX):
            # Skip possible prefix like "Cycles:", we already know change is
            # about Cycles since it's being committed to a Cycles repository.
            line = subject_strip(GIT_SUBJECT_COMMON_PREFIX, line)

            # Dots usually are omitted in the topic
            line = line.replace(b".\n", b"\n")
        elif line.startswith(GIT_FILE_SECTION_MARKER):
            if not line.startswith(full_accept_prefix):
                do_skip = True
            else:
                do_skip = False
                line = replace_file_prefix(line, accept_prefix, replace_prefix)
        elif line.startswith(GIT_PATCHSET_END_MARKER):
            do_skip = False
        elif line.startswith(b"---") or line.startswith(b"+++"):
            line = replace_file_prefix(line, accept_prefix, replace_prefix)

        if not do_skip:
            clean_content.append(line)

    with open(patch, "wb") as f:
        f.writelines(clean_content)


# Get mapping from commit subject to commit hash.
#
# It'll actually include timestamp of the commit to the map key, so commits with
# the same subject wouldn't conflict with each other.
def commit_map_get(repository, path, start_commit):
    command = (b"git",
               b"--git-dir=" + os.path.join(repository, b'.git'),
               b"--work-tree=" + repository,
               b"log", b"--format=%H %at %s", b"--reverse",
               start_commit + b'..HEAD',
               b'--',
               os.path.join(repository, path),
               b':(exclude)' + os.path.join(repository, b'intern/cycles/blender'))
    lines = subprocess.check_output(command).split(b"\n")
    commit_map = collections.OrderedDict()
    for line in lines:
        if line:
            commit_sha, stamped_subject = line.split(b' ', 1)
            stamp, subject = stamped_subject.split(b' ', 1)
            subject = subject_strip(b"", subject).rstrip(b".")
            stamped_subject = stamp + b" " + subject

            if commit_sha in IGNORE_HASHES:
                continue
            commit_map[stamped_subject] = commit_sha
    return commit_map


# Get difference between two lists of commits.
# Returns two lists: first are the commits to be ported from Cycles to Blender,
# second one are the commits to be ported from Blender to Cycles.
def commits_get_difference(cycles_map, blender_map):
    cycles_to_blender = []
    for stamped_subject, commit_hash in cycles_map.items():
        if stamped_subject not in blender_map:
            cycles_to_blender.append(commit_hash)

    blender_to_cycles = []
    for stamped_subject, commit_hash in blender_map.items():
        if stamped_subject not in cycles_map:
            blender_to_cycles.append(commit_hash)

    return cycles_to_blender, blender_to_cycles


# Transfer commits from one repository to another.
# Doesn't do actual commit just for the safety.
def transfer_commits(commit_hashes,
                     from_repository,
                     to_repository,
                     dst_is_cycles):
    patch_index = 1
    for commit_hash in commit_hashes:
        command = (
            b"git",
            b"--git-dir=" + os.path.join(from_repository, b'.git'),
            b"--work-tree=" + from_repository,
            b"format-patch", b"-1",
            b"--start-number", bytes(str(patch_index), 'utf-8'),
            b"-o", to_repository,
            commit_hash,
            b'--',
            b':(exclude)' + os.path.join(from_repository, b'intern/cycles/blender'),
        )
        patch_file = subprocess.check_output(command).rstrip(b"\n")
        if dst_is_cycles:
            cleanup_patch(patch_file, b"intern/cycles", b"src")
        else:
            cleanup_patch(patch_file, b"src", b"intern/cycles")
        patch_index += 1


def main():
    if len(sys.argv) != 3:
        print("Usage: %s /path/to/cycles/ /path/to/blender/" % sys.argv[0])
        return

    cycles_repository = sys.argv[1].encode()
    blender_repository = sys.argv[2].encode()

    cycles_map = commit_map_get(cycles_repository, b'', CYCLES_START_COMMIT)
    blender_map = commit_map_get(blender_repository, b"intern/cycles", BLENDER_START_COMMIT)
    diff = commits_get_difference(cycles_map, blender_map)

    transfer_commits(diff[0], cycles_repository, blender_repository, False)
    transfer_commits(diff[1], blender_repository, cycles_repository, True)

    print("Missing commits were saved to the blender and cycles repositories.")
    print("Check them and if they're all fine run:")
    print("")
    print("  git am *.patch")


if __name__ == '__main__':
    main()
