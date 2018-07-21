# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

# Some misc utilities...

import collections
import concurrent.futures
import copy
import hashlib
import os
import re
import struct
import sys
import tempfile
#import time

from bl_i18n_utils import (
    settings,
    utils_rtl,
)

import bpy


##### Misc Utils #####
from bpy.app.translations import locale_explode


_valid_po_path_re = re.compile(r"^\S+:[0-9]+$")


def is_valid_po_path(path):
    return bool(_valid_po_path_re.match(path))


def get_best_similar(data):
    import difflib
    key, use_similar, similar_pool = data

    # try to find some close key in existing messages...
    # Optimized code inspired by difflib.get_close_matches (as we only need the best match).
    # We also consider to never make a match when len differs more than -len_key / 2, +len_key * 2 (which is valid
    # as long as use_similar is not below ~0.7).
    # Gives an overall ~20% of improvement!

    # tmp = difflib.get_close_matches(key[1], similar_pool, n=1, cutoff=use_similar)
    # if tmp:
    #     tmp = tmp[0]
    tmp = None
    s = difflib.SequenceMatcher()
    s.set_seq2(key[1])
    len_key = len(key[1])
    min_len = len_key // 2
    max_len = len_key * 2
    for x in similar_pool:
        if min_len < len(x) < max_len:
            s.set_seq1(x)
            if s.real_quick_ratio() >= use_similar and s.quick_ratio() >= use_similar:
                sratio = s.ratio()
                if sratio >= use_similar:
                    tmp = x
                    use_similar = sratio
    return key, tmp


def locale_match(loc1, loc2):
    """
    Return:
        -n if loc1 is a subtype of loc2 (e.g. 'fr_FR' is a subtype of 'fr').
        +n if loc2 is a subtype of loc1.
        n becomes smaller when both locales are more similar (e.g. (sr, sr_SR) are more similar than (sr, sr_SR@latin)).
        0 if they are exactly the same.
        ... (Ellipsis) if they cannot match!
    Note: We consider that 'sr_SR@latin' is a subtype of 'sr@latin', 'sr_SR' and 'sr', but 'sr_SR' and 'sr@latin' won't
          match (will return ...)!
    Note: About similarity, diff in variants are more important than diff in countries, currently here are the cases:
            (sr, sr_SR)             -> 1
            (sr@latin, sr_SR@latin) -> 1
            (sr, sr@latin)          -> 2
            (sr_SR, sr_SR@latin)    -> 2
            (sr, sr_SR@latin)       -> 3
    """
    if loc1 == loc2:
        return 0
    l1, c1, v1, *_1 = locale_explode(loc1)
    l2, c2, v2, *_2 = locale_explode(loc2)

    if l1 == l2:
        if c1 == c2:
            if v1 == v2:
                return 0
            elif v2 is None:
                return -2
            elif v1 is None:
                return 2
            return ...
        elif c2 is None:
            if v1 == v2:
                return -1
            elif v2 is None:
                return -3
            return ...
        elif c1 is None:
            if v1 == v2:
                return 1
            elif v1 is None:
                return 3
            return ...
    return ...


def find_best_isocode_matches(uid, iso_codes):
    """
    Return an ordered tuple of elements in iso_codes that can match the given uid, from most similar to lesser ones.
    """
    tmp = ((e, locale_match(e, uid)) for e in iso_codes)
    return tuple(e[0] for e in sorted((e for e in tmp if e[1] is not ... and e[1] >= 0), key=lambda e: e[1]))


def get_po_files_from_dir(root_dir, langs=set()):
    """
    Yield tuples (uid, po_path) of translations for each po file found in the given dir, which should be either
    a dir containing po files using language uid's as names (e.g. fr.po, es_ES.po, etc.), or
    a dir containing dirs which names are language uids, and containing po files of the same names.
    """
    found_uids = set()
    for p in os.listdir(root_dir):
        uid = None
        po_file = os.path.join(root_dir, p)
        print(p)
        if p.endswith(".po") and os.path.isfile(po_file):
            uid = p[:-3]
            if langs and uid not in langs:
                continue
        elif os.path.isdir(p):
            uid = p
            if langs and uid not in langs:
                continue
            po_file = os.path.join(root_dir, p, p + ".po")
            if not os.path.isfile(po_file):
                continue
        else:
            continue
        if uid in found_uids:
            printf("WARNING! {} id has been found more than once! only first one has been loaded!".format(uid))
            continue
        found_uids.add(uid)
        yield uid, po_file


def enable_addons(addons=None, support=None, disable=False, check_only=False):
    """
    Enable (or disable) addons based either on a set of names, or a set of 'support' types.
    Returns the list of all affected addons (as fake modules)!
    If "check_only" is set, no addon will be enabled nor disabled.
    """
    import addon_utils

    if addons is None:
        addons = {}
    if support is None:
        support = {}

    userpref = bpy.context.user_preferences
    used_ext = {ext.module for ext in userpref.addons}

    ret = [
        mod for mod in addon_utils.modules()
        if ((addons and mod.__name__ in addons) or
            (not addons and addon_utils.module_bl_info(mod)["support"] in support))
    ]

    if not check_only:
        for mod in ret:
            module_name = mod.__name__
            if disable:
                if module_name not in used_ext:
                    continue
                print("    Disabling module ", module_name)
                bpy.ops.wm.addon_disable(module=module_name)
            else:
                if module_name in used_ext:
                    continue
                print("    Enabling module ", module_name)
                bpy.ops.wm.addon_enable(module=module_name)

        # XXX There are currently some problems with bpy/rna...
        #     *Very* tricky to solve!
        #     So this is a hack to make all newly added operator visible by
        #     bpy.types.OperatorProperties.__subclasses__()
        for cat in dir(bpy.ops):
            cat = getattr(bpy.ops, cat)
            for op in dir(cat):
                getattr(cat, op).get_rna()

    return ret


##### Main Classes #####

class I18nMessage:
    """
    Internal representation of a message.
    """
    __slots__ = ("msgctxt_lines", "msgid_lines", "msgstr_lines", "comment_lines", "is_fuzzy", "is_commented",
                 "settings")

    def __init__(self, msgctxt_lines=None, msgid_lines=None, msgstr_lines=None, comment_lines=None,
                 is_commented=False, is_fuzzy=False, settings=settings):
        self.settings = settings
        self.msgctxt_lines = msgctxt_lines or []
        self.msgid_lines = msgid_lines or []
        self.msgstr_lines = msgstr_lines or []
        self.comment_lines = comment_lines or []
        self.is_fuzzy = is_fuzzy
        self.is_commented = is_commented

    def _get_msgctxt(self):
        return "".join(self.msgctxt_lines)

    def _set_msgctxt(self, ctxt):
        self.msgctxt_lines = [ctxt]
    msgctxt = property(_get_msgctxt, _set_msgctxt)

    def _get_msgid(self):
        return "".join(self.msgid_lines)

    def _set_msgid(self, msgid):
        self.msgid_lines = [msgid]
    msgid = property(_get_msgid, _set_msgid)

    def _get_msgstr(self):
        return "".join(self.msgstr_lines)

    def _set_msgstr(self, msgstr):
        self.msgstr_lines = [msgstr]
    msgstr = property(_get_msgstr, _set_msgstr)

    def _get_sources(self):
        lstrip1 = len(self.settings.PO_COMMENT_PREFIX_SOURCE)
        lstrip2 = len(self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM)
        return ([l[lstrip1:] for l in self.comment_lines if l.startswith(self.settings.PO_COMMENT_PREFIX_SOURCE)] +
                [l[lstrip2:] for l in self.comment_lines
                 if l.startswith(self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM)])

    def _set_sources(self, sources):
        cmmlines = self.comment_lines.copy()
        for l in cmmlines:
            if (
                    l.startswith(self.settings.PO_COMMENT_PREFIX_SOURCE) or
                    l.startswith(self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM)
            ):
                self.comment_lines.remove(l)
        lines_src = []
        lines_src_custom = []
        for src in sources:
            if is_valid_po_path(src):
                lines_src.append(self.settings.PO_COMMENT_PREFIX_SOURCE + src)
            else:
                lines_src_custom.append(self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM + src)
        self.comment_lines += lines_src_custom + lines_src
    sources = property(_get_sources, _set_sources)

    def _get_is_tooltip(self):
        # XXX For now, we assume that all messages > 30 chars are tooltips!
        return len(self.msgid) > 30
    is_tooltip = property(_get_is_tooltip)

    def copy(self):
        # Deepcopy everything but the settings!
        return self.__class__(msgctxt_lines=self.msgctxt_lines[:], msgid_lines=self.msgid_lines[:],
                              msgstr_lines=self.msgstr_lines[:], comment_lines=self.comment_lines[:],
                              is_commented=self.is_commented, is_fuzzy=self.is_fuzzy, settings=self.settings)

    def normalize(self, max_len=80):
        """
        Normalize this message, call this before exporting it...
        Currently normalize msgctxt, msgid and msgstr lines to given max_len (if below 1, make them single line).
        """
        max_len -= 2  # The two quotes!

        def _splitlines(text):
            lns = text.splitlines()
            return [l + "\n" for l in lns[:-1]] + lns[-1:]

        # We do not need the full power of textwrap... We just split first at escaped new lines, then into each line
        # if needed... No word splitting, nor fancy spaces handling!
        def _wrap(text, max_len, init_len):
            if len(text) + init_len < max_len:
                return [text]
            lines = _splitlines(text)
            ret = []
            for l in lines:
                tmp = []
                cur_len = 0
                words = l.split(' ')
                for w in words:
                    cur_len += len(w) + 1
                    if cur_len > (max_len - 1) and tmp:
                        ret.append(" ".join(tmp) + " ")
                        del tmp[:]
                        cur_len = len(w) + 1
                    tmp.append(w)
                if tmp:
                    ret.append(" ".join(tmp))
            return ret

        if max_len < 1:
            self.msgctxt_lines = _splitlines(self.msgctxt)
            self.msgid_lines = _splitlines(self.msgid)
            self.msgstr_lines = _splitlines(self.msgstr)
        else:
            init_len = len(self.settings.PO_MSGCTXT) + 1
            if self.is_commented:
                init_len += len(self.settings.PO_COMMENT_PREFIX_MSG)
            self.msgctxt_lines = _wrap(self.msgctxt, max_len, init_len)

            init_len = len(self.settings.PO_MSGID) + 1
            if self.is_commented:
                init_len += len(self.settings.PO_COMMENT_PREFIX_MSG)
            self.msgid_lines = _wrap(self.msgid, max_len, init_len)

            init_len = len(self.settings.PO_MSGSTR) + 1
            if self.is_commented:
                init_len += len(self.settings.PO_COMMENT_PREFIX_MSG)
            self.msgstr_lines = _wrap(self.msgstr, max_len, init_len)

        # Be sure comment lines are not duplicated (can happen with sources...).
        tmp = []
        for l in self.comment_lines:
            if l not in tmp:
                tmp.append(l)
        self.comment_lines = tmp

    _esc_quotes = re.compile(r'(?!<\\)((?:\\\\)*)"')
    _unesc_quotes = re.compile(r'(?!<\\)((?:\\\\)*)\\"')
    _esc_names = ("msgctxt_lines", "msgid_lines", "msgstr_lines")
    _esc_names_all = _esc_names + ("comment_lines",)

    @classmethod
    def do_escape(cls, txt):
        """Replace some chars by their escaped versions!"""
        if "\n" in txt:
            txt = txt.replace("\n", r"\n")
        if "\t" in txt:
            txt.replace("\t", r"\t")
        if '"' in txt:
            txt = cls._esc_quotes.sub(r'\1\"', txt)
        return txt

    @classmethod
    def do_unescape(cls, txt):
        """Replace escaped chars by real ones!"""
        if r"\n" in txt:
            txt = txt.replace(r"\n", "\n")
        if r"\t" in txt:
            txt = txt.replace(r"\t", "\t")
        if r'\"' in txt:
            txt = cls._unesc_quotes.sub(r'\1"', txt)
        return txt

    def escape(self, do_all=False):
        names = self._esc_names_all if do_all else self._esc_names
        for name in names:
            setattr(self, name, [self.do_escape(l) for l in getattr(self, name)])

    def unescape(self, do_all=True):
        names = self._esc_names_all if do_all else self._esc_names
        for name in names:
            setattr(self, name, [self.do_unescape(l) for l in getattr(self, name)])


class I18nMessages:
    """
    Internal representation of messages for one language (iso code), with additional stats info.
    """

    # Avoid parsing again!
    # Keys should be (pseudo) file-names, values are tuples (hash, I18nMessages)
    # Note: only used by po parser currently!
    #_parser_cache = {}

    def __init__(self, uid=None, kind=None, key=None, src=None, settings=settings):
        self.settings = settings
        self.uid = uid if uid is not None else settings.PARSER_TEMPLATE_ID
        self.msgs = self._new_messages()
        self.trans_msgs = set()
        self.fuzzy_msgs = set()
        self.comm_msgs = set()
        self.ttip_msgs = set()
        self.contexts = set()
        self.nbr_msgs = 0
        self.nbr_trans_msgs = 0
        self.nbr_ttips = 0
        self.nbr_trans_ttips = 0
        self.nbr_comm_msgs = 0
        self.nbr_signs = 0
        self.nbr_trans_signs = 0
        self.parsing_errors = []
        if kind and src:
            self.parse(kind, key, src)
        self.update_info()

        self._reverse_cache = None

    @staticmethod
    def _new_messages():
        return getattr(collections, 'OrderedDict', dict)()

    @classmethod
    def gen_empty_messages(cls, uid, blender_ver, blender_hash, time, year, default_copyright=True, settings=settings):
        """Generate an empty I18nMessages object (only header is present!)."""
        fmt = settings.PO_HEADER_MSGSTR
        msgstr = fmt.format(blender_ver=str(blender_ver), blender_hash=blender_hash, time=str(time), uid=str(uid))
        comment = ""
        if default_copyright:
            comment = settings.PO_HEADER_COMMENT_COPYRIGHT.format(year=str(year))
        comment = comment + settings.PO_HEADER_COMMENT

        msgs = cls(uid=uid, settings=settings)
        key = settings.PO_HEADER_KEY
        msgs.msgs[key] = I18nMessage([key[0]], [key[1]], msgstr.split("\n"), comment.split("\n"),
                                     False, False, settings=settings)
        msgs.update_info()

        return msgs

    def normalize(self, max_len=80):
        for msg in self.msgs.values():
            msg.normalize(max_len)

    def escape(self, do_all=False):
        for msg in self.msgs.values():
            msg.escape(do_all)

    def unescape(self, do_all=True):
        for msg in self.msgs.values():
            msg.unescape(do_all)

    def check(self, fix=False):
        """
        Check consistency between messages and their keys!
        Check messages using format stuff are consistant between msgid and msgstr!
        If fix is True, tries to fix the issues.
        Return a list of found errors (empty if everything went OK!).
        """
        ret = []
        default_context = self.settings.DEFAULT_CONTEXT
        _format = re.compile(self.settings.CHECK_PRINTF_FORMAT).findall
        done_keys = set()
        rem = set()
        tmp = {}
        for key, msg in self.msgs.items():
            msgctxt, msgid, msgstr = msg.msgctxt, msg.msgid, msg.msgstr
            real_key = (msgctxt or default_context, msgid)
            if key != real_key:
                ret.append("Error! msg's context/message do not match its key ({} / {})".format(real_key, key))
                if real_key in self.msgs:
                    ret.append("Error! msg's real_key already used!")
                    if fix:
                        rem.add(real_key)
                elif fix:
                    tmp[real_key] = msg
            done_keys.add(key)
            if '%' in msgid and msgstr and _format(msgid) != _format(msgstr):
                if not msg.is_fuzzy:
                    ret.append("Error! msg's format entities are not matched in msgid and msgstr ({} / \"{}\")"
                               "".format(real_key, msgstr))
                if fix:
                    msg.msgstr = ""
        for k in rem:
            del self.msgs[k]
        self.msgs.update(tmp)
        return ret

    def clean_commented(self):
        self.update_info()
        nbr = len(self.comm_msgs)
        for k in self.comm_msgs:
            del self.msgs[k]
        return nbr

    def rtl_process(self):
        keys = []
        trans = []
        for k, m in self.msgs.items():
            keys.append(k)
            trans.append(m.msgstr)
        trans = utils_rtl.log2vis(trans, self.settings)
        for k, t in zip(keys, trans):
            self.msgs[k].msgstr = t

    def merge(self, msgs, replace=False):
        """
        Merge translations from msgs into self, following those rules:
            * If a msg is in self and not in msgs, keep self untouched.
            * If a msg is in msgs and not in self, skip it.
            * Else (msg both in self and msgs):
                * If self is not translated and msgs is translated or fuzzy, replace by msgs.
                * If self is fuzzy, and msgs is translated, replace by msgs.
                * If self is fuzzy, and msgs is fuzzy, and replace is True, replace by msgs.
                * If self is translated, and msgs is translated, and replace is True, replace by msgs.
                * Else, skip it!
        """
        for k, m in msgs.msgs.items():
            if k not in self.msgs:
                continue
            sm = self.msgs[k]
            if (sm.is_commented or m.is_commented or not m.msgstr):
                continue
            if (not sm.msgstr or replace or (sm.is_fuzzy and (not m.is_fuzzy or replace))):
                sm.msgstr = m.msgstr
                sm.is_fuzzy = m.is_fuzzy

    def update(self, ref, use_similar=None, keep_old_commented=True):
        """
        Update this I18nMessage with the ref one. Translations from ref are never used. Source comments from ref
        completely replace current ones. If use_similar is not 0.0, it will try to match new messages in ref with an
        existing one. Messages no more found in ref will be marked as commented if keep_old_commented is True,
        or removed.
        """
        if use_similar is None:
            use_similar = self.settings.SIMILAR_MSGID_THRESHOLD

        similar_pool = {}
        if use_similar > 0.0:
            for key, msg in self.msgs.items():
                if msg.msgstr:  # No need to waste time with void translations!
                    similar_pool.setdefault(key[1], set()).add(key)

        msgs = self._new_messages().fromkeys(ref.msgs.keys())
        ref_keys = set(ref.msgs.keys())
        org_keys = set(self.msgs.keys())
        new_keys = ref_keys - org_keys
        removed_keys = org_keys - ref_keys

        # First process keys present in both org and ref messages.
        for key in ref_keys - new_keys:
            msg, refmsg = self.msgs[key], ref.msgs[key]
            msg.sources = refmsg.sources
            msg.is_commented = refmsg.is_commented
            msgs[key] = msg

        # Next process new keys.
        if use_similar > 0.0:
            with concurrent.futures.ProcessPoolExecutor() as exctr:
                for key, msgid in exctr.map(get_best_similar,
                                            tuple((nk, use_similar, tuple(similar_pool.keys())) for nk in new_keys)):
                    if msgid:
                        # Try to get the same context, else just get one...
                        skey = (key[0], msgid)
                        if skey not in similar_pool[msgid]:
                            skey = tuple(similar_pool[msgid])[0]
                        # We keep org translation and comments, and mark message as fuzzy.
                        msg, refmsg = self.msgs[skey].copy(), ref.msgs[key]
                        msg.msgctxt = refmsg.msgctxt
                        msg.msgid = refmsg.msgid
                        msg.sources = refmsg.sources
                        msg.is_fuzzy = True
                        msg.is_commented = refmsg.is_commented
                        msgs[key] = msg
                    else:
                        msgs[key] = ref.msgs[key]
        else:
            for key in new_keys:
                msgs[key] = ref.msgs[key]

        # Add back all "old" and already commented messages as commented ones, if required
        # (and translation was not void!).
        if keep_old_commented:
            for key in removed_keys:
                msgs[key] = self.msgs[key]
                msgs[key].is_commented = True
                msgs[key].sources = []

        # Special 'meta' message, change project ID version and pot creation date...
        key = self.settings.PO_HEADER_KEY
        rep = []
        markers = ("Project-Id-Version:", "POT-Creation-Date:")
        for mrk in markers:
            for rl in ref.msgs[key].msgstr_lines:
                if rl.startswith(mrk):
                    for idx, ml in enumerate(msgs[key].msgstr_lines):
                        if ml.startswith(mrk):
                            rep.append((idx, rl))
        for idx, txt in rep:
            msgs[key].msgstr_lines[idx] = txt

        # And finalize the update!
        self.msgs = msgs

    def update_info(self):
        self.trans_msgs.clear()
        self.fuzzy_msgs.clear()
        self.comm_msgs.clear()
        self.ttip_msgs.clear()
        self.contexts.clear()
        self.nbr_signs = 0
        self.nbr_trans_signs = 0
        for key, msg in self.msgs.items():
            if key == self.settings.PO_HEADER_KEY:
                continue
            if msg.is_commented:
                self.comm_msgs.add(key)
            else:
                if msg.msgstr:
                    self.trans_msgs.add(key)
                if msg.is_fuzzy:
                    self.fuzzy_msgs.add(key)
                if msg.is_tooltip:
                    self.ttip_msgs.add(key)
                self.contexts.add(key[0])
                self.nbr_signs += len(msg.msgid)
                self.nbr_trans_signs += len(msg.msgstr)
        self.nbr_msgs = len(self.msgs)
        self.nbr_trans_msgs = len(self.trans_msgs - self.fuzzy_msgs)
        self.nbr_ttips = len(self.ttip_msgs)
        self.nbr_trans_ttips = len(self.ttip_msgs & (self.trans_msgs - self.fuzzy_msgs))
        self.nbr_comm_msgs = len(self.comm_msgs)

    def print_info(self, prefix="", output=print, print_stats=True, print_errors=True):
        """
        Print out some info about an I18nMessages object.
        """
        lvl = 0.0
        lvl_ttips = 0.0
        lvl_comm = 0.0
        lvl_trans_ttips = 0.0
        lvl_ttips_in_trans = 0.0
        if self.nbr_msgs > 0:
            lvl = float(self.nbr_trans_msgs) / float(self.nbr_msgs)
            lvl_ttips = float(self.nbr_ttips) / float(self.nbr_msgs)
            lvl_comm = float(self.nbr_comm_msgs) / float(self.nbr_msgs + self.nbr_comm_msgs)
        if self.nbr_ttips > 0:
            lvl_trans_ttips = float(self.nbr_trans_ttips) / float(self.nbr_ttips)
        if self.nbr_trans_msgs > 0:
            lvl_ttips_in_trans = float(self.nbr_trans_ttips) / float(self.nbr_trans_msgs)

        lines = []
        if print_stats:
            lines += [
                "",
                "{:>6.1%} done! ({} translated messages over {}).\n"
                "".format(lvl, self.nbr_trans_msgs, self.nbr_msgs),
                "{:>6.1%} of messages are tooltips ({} over {}).\n"
                "".format(lvl_ttips, self.nbr_ttips, self.nbr_msgs),
                "{:>6.1%} of tooltips are translated ({} over {}).\n"
                "".format(lvl_trans_ttips, self.nbr_trans_ttips, self.nbr_ttips),
                "{:>6.1%} of translated messages are tooltips ({} over {}).\n"
                "".format(lvl_ttips_in_trans, self.nbr_trans_ttips, self.nbr_trans_msgs),
                "{:>6.1%} of messages are commented ({} over {}).\n"
                "".format(lvl_comm, self.nbr_comm_msgs, self.nbr_comm_msgs + self.nbr_msgs),
                "This translation is currently made of {} signs.\n".format(self.nbr_trans_signs)
            ]
        if print_errors and self.parsing_errors:
            lines += ["WARNING! Errors during parsing:\n"]
            lines += ["    Around line {}: {}\n".format(line, error) for line, error in self.parsing_errors]
        output(prefix.join(lines))

    def invalidate_reverse_cache(self, rebuild_now=False):
        """
        Invalidate the reverse cache used by find_best_messages_matches.
        """
        self._reverse_cache = None
        if rebuild_now:
            src_to_msg, ctxt_to_msg, msgid_to_msg, msgstr_to_msg = {}, {}, {}, {}
            for key, msg in self.msgs.items():
                if msg.is_commented:
                    continue
                ctxt, msgid = key
                ctxt_to_msg.setdefault(ctxt, set()).add(key)
                msgid_to_msg.setdefault(msgid, set()).add(key)
                msgstr_to_msg.setdefault(msg.msgstr, set()).add(key)
                for src in msg.sources:
                    src_to_msg.setdefault(src, set()).add(key)
            self._reverse_cache = (src_to_msg, ctxt_to_msg, msgid_to_msg, msgstr_to_msg)

    def find_best_messages_matches(self, msgs, msgmap, rna_ctxt, rna_struct_name, rna_prop_name, rna_enum_name):
        """
        Try to find the best I18nMessages (i.e. context/msgid pairs) for the given UI messages:
            msgs: an object containing properties listed in msgmap's values.
            msgmap: a dict of various messages to use for search:
                        {"but_label": subdict, "rna_label": subdict, "enum_label": subdict,
                        "but_tip": subdict, "rna_tip": subdict, "enum_tip": subdict}
                    each subdict being like that:
                        {"msgstr": id, "msgid": id, "msg_flags": id, "key": set()}
                  where msgstr and msgid are identifiers of string props in msgs (resp. translated and org message),
                        msg_flags is not used here, and key is a set of matching (msgctxt, msgid) keys for the item.
            The other parameters are about the RNA element from which the strings come from, if it could be determined:
                rna_ctxt: the labels' i18n context.
                rna_struct_name, rna_prop_name, rna_enum_name: should be self-explanatory!
        """
        # Build helper mappings.
        # Note it's user responsibility to know when to invalidate (and hence force rebuild) this cache!
        if self._reverse_cache is None:
            self.invalidate_reverse_cache(True)
        src_to_msg, ctxt_to_msg, msgid_to_msg, msgstr_to_msg = self._reverse_cache

    #    print(len(src_to_msg), len(ctxt_to_msg), len(msgid_to_msg), len(msgstr_to_msg))

        # Build RNA key.
        src, src_rna, src_enum = bpy.utils.make_rna_paths(rna_struct_name, rna_prop_name, rna_enum_name)
        print("src: ", src_rna, src_enum)

        # Labels.
        elbl = getattr(msgs, msgmap["enum_label"]["msgstr"])
        if elbl:
            # Enum items' labels have no i18n context...
            k = ctxt_to_msg[self.settings.DEFAULT_CONTEXT].copy()
            if elbl in msgid_to_msg:
                k &= msgid_to_msg[elbl]
            elif elbl in msgstr_to_msg:
                k &= msgstr_to_msg[elbl]
            else:
                k = set()
            # We assume if we already have only one key, it's the good one!
            if len(k) > 1 and src_enum in src_to_msg:
                k &= src_to_msg[src_enum]
            msgmap["enum_label"]["key"] = k
        rlbl = getattr(msgs, msgmap["rna_label"]["msgstr"])
        #print("rna label: " + rlbl, rlbl in msgid_to_msg, rlbl in msgstr_to_msg)
        if rlbl:
            k = ctxt_to_msg[rna_ctxt].copy()
            if k and rlbl in msgid_to_msg:
                k &= msgid_to_msg[rlbl]
            elif k and rlbl in msgstr_to_msg:
                k &= msgstr_to_msg[rlbl]
            else:
                k = set()
            # We assume if we already have only one key, it's the good one!
            if len(k) > 1 and src_rna in src_to_msg:
                k &= src_to_msg[src_rna]
            msgmap["rna_label"]["key"] = k
        blbl = getattr(msgs, msgmap["but_label"]["msgstr"])
        blbls = [blbl]
        if blbl.endswith(self.settings.NUM_BUTTON_SUFFIX):
            # Num buttons report their label with a trailing ': '...
            blbls.append(blbl[:-len(self.settings.NUM_BUTTON_SUFFIX)])
        print("button label: " + blbl)
        if blbl and elbl not in blbls and (rlbl not in blbls or rna_ctxt != self.settings.DEFAULT_CONTEXT):
            # Always Default context for button label :/
            k = ctxt_to_msg[self.settings.DEFAULT_CONTEXT].copy()
            found = False
            for bl in blbls:
                if bl in msgid_to_msg:
                    k &= msgid_to_msg[bl]
                    found = True
                    break
                elif bl in msgstr_to_msg:
                    k &= msgstr_to_msg[bl]
                    found = True
                    break
            if not found:
                k = set()
            # XXX No need to check against RNA path here, if blabel is different
            #     from rlabel, should not match anyway!
            msgmap["but_label"]["key"] = k

        # Tips (they never have a specific context).
        etip = getattr(msgs, msgmap["enum_tip"]["msgstr"])
        #print("enum tip: " + etip)
        if etip:
            k = ctxt_to_msg[self.settings.DEFAULT_CONTEXT].copy()
            if etip in msgid_to_msg:
                k &= msgid_to_msg[etip]
            elif etip in msgstr_to_msg:
                k &= msgstr_to_msg[etip]
            else:
                k = set()
            # We assume if we already have only one key, it's the good one!
            if len(k) > 1 and src_enum in src_to_msg:
                k &= src_to_msg[src_enum]
            msgmap["enum_tip"]["key"] = k
        rtip = getattr(msgs, msgmap["rna_tip"]["msgstr"])
        #print("rna tip: " + rtip)
        if rtip:
            k = ctxt_to_msg[self.settings.DEFAULT_CONTEXT].copy()
            if k and rtip in msgid_to_msg:
                k &= msgid_to_msg[rtip]
            elif k and rtip in msgstr_to_msg:
                k &= msgstr_to_msg[rtip]
            else:
                k = set()
            # We assume if we already have only one key, it's the good one!
            if len(k) > 1 and src_rna in src_to_msg:
                k &= src_to_msg[src_rna]
            msgmap["rna_tip"]["key"] = k
            # print(k)
        btip = getattr(msgs, msgmap["but_tip"]["msgstr"])
        #print("button tip: " + btip)
        if btip and btip not in {rtip, etip}:
            k = ctxt_to_msg[self.settings.DEFAULT_CONTEXT].copy()
            if btip in msgid_to_msg:
                k &= msgid_to_msg[btip]
            elif btip in msgstr_to_msg:
                k &= msgstr_to_msg[btip]
            else:
                k = set()
            # XXX No need to check against RNA path here, if btip is different from rtip, should not match anyway!
            msgmap["but_tip"]["key"] = k

    def parse(self, kind, key, src):
        del self.parsing_errors[:]
        self.parsers[kind](self, src, key)
        if self.parsing_errors:
            print("{} ({}):".format(key, src))
            self.print_info(print_stats=False)
            print("The parser solved them as well as it could...")
        self.update_info()

    def parse_messages_from_po(self, src, key=None):
        """
        Parse a po file.
        Note: This function will silently "arrange" mis-formated entries, thus using afterward write_messages() should
              always produce a po-valid file, though not correct!
        """
        reading_msgid = False
        reading_msgstr = False
        reading_msgctxt = False
        reading_comment = False
        is_commented = False
        is_fuzzy = False
        msgctxt_lines = []
        msgid_lines = []
        msgstr_lines = []
        comment_lines = []

        default_context = self.settings.DEFAULT_CONTEXT

        # Helper function
        def finalize_message(self, line_nr):
            nonlocal reading_msgid, reading_msgstr, reading_msgctxt, reading_comment
            nonlocal is_commented, is_fuzzy, msgid_lines, msgstr_lines, msgctxt_lines, comment_lines

            msgid = I18nMessage.do_unescape("".join(msgid_lines))
            msgctxt = I18nMessage.do_unescape("".join(msgctxt_lines))
            msgkey = (msgctxt or default_context, msgid)

            # Never allow overriding existing msgid/msgctxt pairs!
            if msgkey in self.msgs:
                self.parsing_errors.append((line_nr, "{} context/msgid is already in current messages!".format(msgkey)))
                return

            self.msgs[msgkey] = I18nMessage(msgctxt_lines, msgid_lines, msgstr_lines, comment_lines,
                                            is_commented, is_fuzzy, settings=self.settings)

            # Let's clean up and get ready for next message!
            reading_msgid = reading_msgstr = reading_msgctxt = reading_comment = False
            is_commented = is_fuzzy = False
            msgctxt_lines = []
            msgid_lines = []
            msgstr_lines = []
            comment_lines = []

        # try to use src as file name...
        if os.path.isfile(src):
            if os.stat(src).st_size > self.settings.PARSER_MAX_FILE_SIZE:
                # Security, else we could read arbitrary huge files!
                print("WARNING: skipping file {}, too huge!".format(src))
                return
            if not key:
                key = src
            with open(src, 'r', encoding="utf-8") as f:
                src = f.read()

        _msgctxt = self.settings.PO_MSGCTXT
        _comm_msgctxt = self.settings.PO_COMMENT_PREFIX_MSG + _msgctxt
        _len_msgctxt = len(_msgctxt + '"')
        _len_comm_msgctxt = len(_comm_msgctxt + '"')
        _msgid = self.settings.PO_MSGID
        _comm_msgid = self.settings.PO_COMMENT_PREFIX_MSG + _msgid
        _len_msgid = len(_msgid + '"')
        _len_comm_msgid = len(_comm_msgid + '"')
        _msgstr = self.settings.PO_MSGSTR
        _comm_msgstr = self.settings.PO_COMMENT_PREFIX_MSG + _msgstr
        _len_msgstr = len(_msgstr + '"')
        _len_comm_msgstr = len(_comm_msgstr + '"')
        _comm_str = self.settings.PO_COMMENT_PREFIX_MSG
        _comm_fuzzy = self.settings.PO_COMMENT_FUZZY
        _len_comm_str = len(_comm_str + '"')

        # Main loop over all lines in src...
        for line_nr, line in enumerate(src.splitlines()):
            if line == "":
                if reading_msgstr:
                    finalize_message(self, line_nr)
                continue

            elif line.startswith(_msgctxt) or line.startswith(_comm_msgctxt):
                reading_comment = False
                reading_ctxt = True
                if line.startswith(_comm_str):
                    is_commented = True
                    line = line[_len_comm_msgctxt:-1]
                else:
                    line = line[_len_msgctxt:-1]
                msgctxt_lines.append(line)

            elif line.startswith(_msgid) or line.startswith(_comm_msgid):
                reading_comment = False
                reading_msgid = True
                if line.startswith(_comm_str):
                    if not is_commented and reading_ctxt:
                        self.parsing_errors.append((line_nr, "commented msgid following regular msgctxt"))
                    is_commented = True
                    line = line[_len_comm_msgid:-1]
                else:
                    line = line[_len_msgid:-1]
                reading_ctxt = False
                msgid_lines.append(line)

            elif line.startswith(_msgstr) or line.startswith(_comm_msgstr):
                if not reading_msgid:
                    self.parsing_errors.append((line_nr, "msgstr without a prior msgid"))
                else:
                    reading_msgid = False
                reading_msgstr = True
                if line.startswith(_comm_str):
                    line = line[_len_comm_msgstr:-1]
                    if not is_commented:
                        self.parsing_errors.append((line_nr, "commented msgstr following regular msgid"))
                else:
                    line = line[_len_msgstr:-1]
                    if is_commented:
                        self.parsing_errors.append((line_nr, "regular msgstr following commented msgid"))
                msgstr_lines.append(line)

            elif line.startswith(_comm_str[0]):
                if line.startswith(_comm_str):
                    if reading_msgctxt:
                        if is_commented:
                            msgctxt_lines.append(line[_len_comm_str:-1])
                        else:
                            msgctxt_lines.append(line)
                            self.parsing_errors.append((line_nr, "commented string while reading regular msgctxt"))
                    elif reading_msgid:
                        if is_commented:
                            msgid_lines.append(line[_len_comm_str:-1])
                        else:
                            msgid_lines.append(line)
                            self.parsing_errors.append((line_nr, "commented string while reading regular msgid"))
                    elif reading_msgstr:
                        if is_commented:
                            msgstr_lines.append(line[_len_comm_str:-1])
                        else:
                            msgstr_lines.append(line)
                            self.parsing_errors.append((line_nr, "commented string while reading regular msgstr"))
                else:
                    if reading_msgctxt or reading_msgid or reading_msgstr:
                        self.parsing_errors.append((line_nr,
                                                    "commented string within msgctxt, msgid or msgstr scope, ignored"))
                    elif line.startswith(_comm_fuzzy):
                        is_fuzzy = True
                    else:
                        comment_lines.append(line)
                    reading_comment = True

            else:
                if reading_msgctxt:
                    msgctxt_lines.append(line[1:-1])
                elif reading_msgid:
                    msgid_lines.append(line[1:-1])
                elif reading_msgstr:
                    line = line[1:-1]
                    msgstr_lines.append(line)
                else:
                    self.parsing_errors.append((line_nr, "regular string outside msgctxt, msgid or msgstr scope"))
                    #self.parsing_errors += (str(comment_lines), str(msgctxt_lines), str(msgid_lines), str(msgstr_lines))

        # If no final empty line, last message is not finalized!
        if reading_msgstr:
            finalize_message(self, line_nr)
        self.unescape()

    def write(self, kind, dest):
        self.writers[kind](self, dest)

    def write_messages_to_po(self, fname, compact=False):
        """
        Write messages in fname po file.
        """
        default_context = self.settings.DEFAULT_CONTEXT

        def _write(self, f, compact):
            _msgctxt = self.settings.PO_MSGCTXT
            _msgid = self.settings.PO_MSGID
            _msgstr = self.settings.PO_MSGSTR
            _comm = self.settings.PO_COMMENT_PREFIX_MSG

            self.escape()

            for num, msg in enumerate(self.msgs.values()):
                if compact and (msg.is_commented or msg.is_fuzzy or not msg.msgstr_lines):
                    continue
                if not compact:
                    f.write("\n".join(msg.comment_lines))
                # Only mark as fuzzy if msgstr is not empty!
                if msg.is_fuzzy and msg.msgstr_lines:
                    f.write("\n" + self.settings.PO_COMMENT_FUZZY)
                _p = _comm if msg.is_commented else ""
                chunks = []
                if msg.msgctxt and msg.msgctxt != default_context:
                    if len(msg.msgctxt_lines) > 1:
                        chunks += [
                            "\n" + _p + _msgctxt + "\"\"\n" + _p + "\"",
                            ("\"\n" + _p + "\"").join(msg.msgctxt_lines),
                            "\"",
                        ]
                    else:
                        chunks += ["\n" + _p + _msgctxt + "\"" + msg.msgctxt + "\""]
                if len(msg.msgid_lines) > 1:
                    chunks += [
                        "\n" + _p + _msgid + "\"\"\n" + _p + "\"",
                        ("\"\n" + _p + "\"").join(msg.msgid_lines),
                        "\"",
                    ]
                else:
                    chunks += ["\n" + _p + _msgid + "\"" + msg.msgid + "\""]
                if len(msg.msgstr_lines) > 1:
                    chunks += [
                        "\n" + _p + _msgstr + "\"\"\n" + _p + "\"",
                        ("\"\n" + _p + "\"").join(msg.msgstr_lines),
                        "\"",
                    ]
                else:
                    chunks += ["\n" + _p + _msgstr + "\"" + msg.msgstr + "\""]
                chunks += ["\n\n"]
                f.write("".join(chunks))

            self.unescape()

        self.normalize(max_len=0)  # No wrapping for now...
        if isinstance(fname, str):
            with open(fname, 'w', encoding="utf-8") as f:
                _write(self, f, compact)
        # Else assume fname is already a file(like) object!
        else:
            _write(self, fname, compact)

    def write_messages_to_mo(self, fname):
        """
        Write messages in fname mo file.
        """
        # XXX Temp solution, until I can make own mo generator working...
        import subprocess
        with tempfile.NamedTemporaryFile(mode='w+', encoding="utf-8") as tmp_po_f:
            self.write_messages_to_po(tmp_po_f)
            cmd = (
                self.settings.GETTEXT_MSGFMT_EXECUTABLE,
                "--statistics",  # show stats
                tmp_po_f.name,
                "-o",
                fname,
            )
            print("Running ", " ".join(cmd))
            ret = subprocess.call(cmd)
            print("Finished.")
            return
        # XXX Code below is currently broken (generates corrupted mo files it seems :( )!
        # Using http://www.gnu.org/software/gettext/manual/html_node/MO-Files.html notation.
        # Not generating hash table!
        # Only translated, unfuzzy messages are taken into account!
        default_context = self.settings.DEFAULT_CONTEXT
        msgs = tuple(v for v in self.msgs.values() if not (v.is_fuzzy or v.is_commented) and v.msgstr and v.msgid)
        msgs = sorted(msgs[:2],
                      key=lambda e: (e.msgctxt + e.msgid) if (e.msgctxt and e.msgctxt != default_context) else e.msgid)
        magic_nbr = 0x950412de
        format_rev = 0
        N = len(msgs)
        O = 32
        T = O + N * 8
        S = 0
        H = T + N * 8
        # Prepare our data! we need key (optional context and msgid), translation, and offset and length of both.
        # Offset are relative to start of their own list.
        EOT = b"0x04"  # Used to concatenate context and msgid
        _msgid_offset = 0
        _msgstr_offset = 0

        def _gen(v):
            nonlocal _msgid_offset, _msgstr_offset
            msgid = v.msgid.encode("utf-8")
            msgstr = v.msgstr.encode("utf-8")
            if v.msgctxt and v.msgctxt != default_context:
                msgctxt = v.msgctxt.encode("utf-8")
                msgid = msgctxt + EOT + msgid
            # Don't forget the final NULL char!
            _msgid_len = len(msgid) + 1
            _msgstr_len = len(msgstr) + 1
            ret = ((msgid, _msgid_len, _msgid_offset), (msgstr, _msgstr_len, _msgstr_offset))
            _msgid_offset += _msgid_len
            _msgstr_offset += _msgstr_len
            return ret
        msgs = tuple(_gen(v) for v in msgs)
        msgid_start = H
        msgstr_start = msgid_start + _msgid_offset
        print(N, msgstr_start + _msgstr_offset)
        print(msgs)

        with open(fname, 'wb') as f:
            # Header...
            f.write(struct.pack("=8I", magic_nbr, format_rev, N, O, T, S, H, 0))
            # Msgid's length and offset.
            f.write(b"".join(struct.pack("=2I", length, msgid_start + offset) for (_1, length, offset), _2 in msgs))
            # Msgstr's length and offset.
            f.write(b"".join(struct.pack("=2I", length, msgstr_start + offset) for _1, (_2, length, offset) in msgs))
            # No hash table!
            # Msgid's.
            f.write(b"\0".join(msgid for (msgid, _1, _2), _3 in msgs) + b"\0")
            # Msgstr's.
            f.write(b"\0".join(msgstr for _1, (msgstr, _2, _3) in msgs) + b"\0")

    parsers = {
        "PO": parse_messages_from_po,
    }

    writers = {
        "PO": write_messages_to_po,
        "PO_COMPACT": lambda s, fn: s.write_messages_to_po(fn, True),
        "MO": write_messages_to_mo,
    }


class I18n:
    """
    Internal representation of a whole translation set.
    """

    @staticmethod
    def _parser_check_file(path, maxsize=settings.PARSER_MAX_FILE_SIZE,
                           _begin_marker=settings.PARSER_PY_MARKER_BEGIN,
                           _end_marker=settings.PARSER_PY_MARKER_END):
        if os.stat(path).st_size > maxsize:
            # Security, else we could read arbitrary huge files!
            print("WARNING: skipping file {}, too huge!".format(path))
            return None, None, None, False
        txt = ""
        with open(path) as f:
            txt = f.read()
        _in = 0
        _out = len(txt)
        if _begin_marker:
            _in = None
            if _begin_marker in txt:
                _in = txt.index(_begin_marker) + len(_begin_marker)
        if _end_marker:
            _out = None
            if _end_marker in txt:
                _out = txt.index(_end_marker)
        if _in is not None and _out is not None:
            in_txt, txt, out_txt = txt[:_in], txt[_in:_out], txt[_out:]
        elif _in is not None:
            in_txt, txt, out_txt = txt[:_in], txt[_in:], None
        elif _out is not None:
            in_txt, txt, out_txt = None, txt[:_out], txt[_out:]
        else:
            in_txt, txt, out_txt = None, txt, None
        return in_txt, txt, out_txt, (True if "translations_tuple" in txt else False)

    @staticmethod
    def _dst(self, path, uid, kind):
        if isinstance(path, str):
            if kind == 'PO':
                if uid == self.settings.PARSER_TEMPLATE_ID:
                    if not path.endswith(".pot"):
                        return os.path.join(os.path.dirname(path), "blender.pot")
                if not path.endswith(".po"):
                    return os.path.join(os.path.dirname(path), uid + ".po")
            elif kind == 'PY':
                if not path.endswith(".py"):
                    if self.src.get(self.settings.PARSER_PY_ID):
                        return self.src[self.settings.PARSER_PY_ID]
                    return os.path.join(os.path.dirname(path), "translations.py")
        return path

    def __init__(self, kind=None, src=None, langs=set(), settings=settings):
        self.settings = settings
        self.trans = {}
        self.src = {}  # Should have the same keys as self.trans (plus PARSER_PY_ID for py file)!
        self.dst = self._dst  # A callable that transforms src_path into dst_path!
        if kind and src:
            self.parse(kind, src, langs)
        self.update_info()

    def _py_file_get(self):
        return self.src.get(self.settings.PARSER_PY_ID)

    def _py_file_set(self, value):
        self.src[self.settings.PARSER_PY_ID] = value
    py_file = property(_py_file_get, _py_file_set)

    def escape(self, do_all=False):
        for trans in self.trans.values():
            trans.escape(do_all)

    def unescape(self, do_all=True):
        for trans in self.trans.values():
            trans.unescape(do_all)

    def update_info(self):
        self.nbr_trans = 0
        self.lvl = 0.0
        self.lvl_ttips = 0.0
        self.lvl_trans_ttips = 0.0
        self.lvl_ttips_in_trans = 0.0
        self.lvl_comm = 0.0
        self.nbr_signs = 0
        self.nbr_trans_signs = 0
        self.contexts = set()

        if self.settings.PARSER_TEMPLATE_ID in self.trans:
            self.nbr_trans = len(self.trans) - 1
            self.nbr_signs = self.trans[self.settings.PARSER_TEMPLATE_ID].nbr_signs
        else:
            self.nbr_trans = len(self.trans)
        for msgs in self.trans.values():
            msgs.update_info()
            if msgs.nbr_msgs > 0:
                self.lvl += float(msgs.nbr_trans_msgs) / float(msgs.nbr_msgs)
                self.lvl_ttips += float(msgs.nbr_ttips) / float(msgs.nbr_msgs)
                self.lvl_comm += float(msgs.nbr_comm_msgs) / float(msgs.nbr_msgs + msgs.nbr_comm_msgs)
            if msgs.nbr_ttips > 0:
                self.lvl_trans_ttips = float(msgs.nbr_trans_ttips) / float(msgs.nbr_ttips)
            if msgs.nbr_trans_msgs > 0:
                self.lvl_ttips_in_trans = float(msgs.nbr_trans_ttips) / float(msgs.nbr_trans_msgs)
            if self.nbr_signs == 0:
                self.nbr_signs = msgs.nbr_signs
            self.nbr_trans_signs += msgs.nbr_trans_signs
            self.contexts |= msgs.contexts

    def print_stats(self, prefix="", print_msgs=True):
        """
        Print out some stats about an I18n object.
        If print_msgs is True, it will also print all its translations' stats.
        """
        if print_msgs:
            msgs_prefix = prefix + "    "
            for key, msgs in self.trans.items():
                if key == self.settings.PARSER_TEMPLATE_ID:
                    continue
                print(prefix + key + ":")
                msgs.print_stats(prefix=msgs_prefix)
                print(prefix)

        nbr_contexts = len(self.contexts - {bpy.app.translations.contexts.default})
        if nbr_contexts != 1:
            if nbr_contexts == 0:
                nbr_contexts = "No"
            _ctx_txt = "s are"
        else:
            _ctx_txt = " is"
        lines = ((
            "",
            "Average stats for all {} translations:\n".format(self.nbr_trans),
            "    {:>6.1%} done!\n".format(self.lvl / self.nbr_trans),
            "    {:>6.1%} of messages are tooltips.\n".format(self.lvl_ttips / self.nbr_trans),
            "    {:>6.1%} of tooltips are translated.\n".format(self.lvl_trans_ttips / self.nbr_trans),
            "    {:>6.1%} of translated messages are tooltips.\n".format(self.lvl_ttips_in_trans / self.nbr_trans),
            "    {:>6.1%} of messages are commented.\n".format(self.lvl_comm / self.nbr_trans),
            "    The org msgids are currently made of {} signs.\n".format(self.nbr_signs),
            "    All processed translations are currently made of {} signs.\n".format(self.nbr_trans_signs),
            "    {} specific context{} present:\n".format(self.nbr_contexts, _ctx_txt)) +
            tuple("            " + c + "\n" for c in self.contexts - {bpy.app.translations.contexts.default}) +
            ("\n",)
        )
        print(prefix.join(lines))

    @classmethod
    def check_py_module_has_translations(clss, src, settings=settings):
        """
        Check whether a given src (a py module, either a directory or a py file) has some i18n translation data,
        and returns a tuple (src_file, translations_tuple) if yes, else (None, None).
        """
        txts = []
        if os.path.isdir(src):
            for root, dnames, fnames in os.walk(src):
                for fname in fnames:
                    if not fname.endswith(".py"):
                        continue
                    path = os.path.join(root, fname)
                    _1, txt, _2, has_trans = clss._parser_check_file(path)
                    if has_trans:
                        txts.append((path, txt))
        elif src.endswith(".py") and os.path.isfile(src):
            _1, txt, _2, has_trans = clss._parser_check_file(src)
            if has_trans:
                txts.append((src, txt))
        for path, txt in txts:
            tuple_id = "translations_tuple"
            env = globals().copy()
            exec(txt, env)
            if tuple_id in env:
                return path, env[tuple_id]
        return None, None  # No data...

    def parse(self, kind, src, langs=set()):
        self.parsers[kind](self, src, langs)

    def parse_from_po(self, src, langs=set()):
        """
        src must be a tuple (dir_of_pos, pot_file), where:
            * dir_of_pos may either contains iso_CODE.po files, and/or iso_CODE/iso_CODE.po files.
            * pot_file may be None (in which case there will be no ref messages).
        if langs set is void, all languages found are loaded.
        """
        root_dir, pot_file = src
        if pot_file and os.path.isfile(pot_file):
            self.trans[self.settings.PARSER_TEMPLATE_ID] = I18nMessages(self.settings.PARSER_TEMPLATE_ID, 'PO',
                                                                        pot_file, pot_file, settings=self.settings)
            self.src_po[self.settings.PARSER_TEMPLATE_ID] = pot_file

        for uid, po_file in get_po_files_from_dir(root_dir, langs):
            self.trans[uid] = I18nMessages(uid, 'PO', po_file, po_file, settings=self.settings)
            self.src_po[uid] = po_file

    def parse_from_py(self, src, langs=set()):
        """
        src must be a valid path, either a py file or a module directory (in which case all py files inside it
        will be checked, first file macthing will win!).
        if langs set is void, all languages found are loaded.
        """
        default_context = self.settings.DEFAULT_CONTEXT
        self.src[self.settings.PARSER_PY_ID], msgs = self.check_py_module_has_translations(src, self.settings)
        if msgs is None:
            self.src[self.settings.PARSER_PY_ID] = src
            msgs = ()
        for key, (sources, gen_comments), *translations in msgs:
            if self.settings.PARSER_TEMPLATE_ID not in self.trans:
                self.trans[self.settings.PARSER_TEMPLATE_ID] = I18nMessages(self.settings.PARSER_TEMPLATE_ID,
                                                                            settings=self.settings)
                self.src[self.settings.PARSER_TEMPLATE_ID] = self.src[self.settings.PARSER_PY_ID]
            if key in self.trans[self.settings.PARSER_TEMPLATE_ID].msgs:
                print("ERROR! key {} is defined more than once! Skipping re-definitions!")
                continue
            custom_src = [c for c in sources if c.startswith("bpy.")]
            src = [c for c in sources if not c.startswith("bpy.")]
            common_comment_lines = [self.settings.PO_COMMENT_PREFIX_GENERATED + c for c in gen_comments] + \
                                   [self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM + c for c in custom_src] + \
                                   [self.settings.PO_COMMENT_PREFIX_SOURCE + c for c in src]
            ctxt = [key[0]] if key[0] else [default_context]
            self.trans[self.settings.PARSER_TEMPLATE_ID].msgs[key] = I18nMessage(ctxt, [key[1]], [""],
                                                                                 common_comment_lines, False, False,
                                                                                 settings=self.settings)
            for uid, msgstr, (is_fuzzy, user_comments) in translations:
                if uid not in self.trans:
                    self.trans[uid] = I18nMessages(uid, settings=self.settings)
                    self.src[uid] = self.src[self.settings.PARSER_PY_ID]
                comment_lines = [self.settings.PO_COMMENT_PREFIX + c for c in user_comments] + common_comment_lines
                self.trans[uid].msgs[key] = I18nMessage(ctxt, [key[1]], [msgstr], comment_lines, False, is_fuzzy,
                                                        settings=self.settings)
        # key = self.settings.PO_HEADER_KEY
        # for uid, trans in self.trans.items():
        #     if key not in trans.msgs:
        #         trans.msgs[key]
        self.unescape()

    def write(self, kind, langs=set()):
        self.writers[kind](self, langs)

    def write_to_po(self, langs=set()):
        """
        Write all translations into po files. By default, write in the same files (or dir) as the source, specify
        a custom self.dst function to write somewhere else!
        Note: If langs is set and you want to export the pot template as well, langs must contain PARSER_TEMPLATE_ID
              ({} currently).
        """.format(self.settings.PARSER_TEMPLATE_ID)
        keys = self.trans.keys()
        if langs:
            keys &= langs
        for uid in keys:
            dst = self.dst(self, self.src.get(uid, ""), uid, 'PO')
            self.trans[uid].write('PO', dst)

    def write_to_py(self, langs=set()):
        """
        Write all translations as python code, either in a "translations.py" file under same dir as source(s), or in
        specified file if self.py_file is set (default, as usual can be customized with self.dst callable!).
        Note: If langs is set and you want to export the pot template as well, langs must contain PARSER_TEMPLATE_ID
              ({} currently).
        """.format(self.settings.PARSER_TEMPLATE_ID)
        default_context = self.settings.DEFAULT_CONTEXT

        def _gen_py(self, langs, tab="    "):
            _lencomm = len(self.settings.PO_COMMENT_PREFIX)
            _lengen = len(self.settings.PO_COMMENT_PREFIX_GENERATED)
            _lensrc = len(self.settings.PO_COMMENT_PREFIX_SOURCE)
            _lencsrc = len(self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM)
            ret = [
                "# NOTE: You can safely move around this auto-generated block (with the begin/end markers!),",
                "#       and edit the translations by hand.",
                "#       Just carefully respect the format of the tuple!",
                "",
                "# Tuple of tuples "
                "((msgctxt, msgid), (sources, gen_comments), (lang, translation, (is_fuzzy, comments)), ...)",
                "translations_tuple = (",
            ]
            # First gather all keys (msgctxt, msgid) - theoretically, all translations should share the same, but...
            # Note: using an ordered dict if possible (stupid sets cannot be ordered :/ ).
            keys = I18nMessages._new_messages()
            for trans in self.trans.values():
                keys.update(trans.msgs)
            # Get the ref translation (ideally, PARSER_TEMPLATE_ID one, else the first one that pops up!
            # Ref translation will be used to generate sources "comments"
            ref = self.trans.get(self.settings.PARSER_TEMPLATE_ID) or self.trans[list(self.trans.keys())[0]]
            # Get all languages (uids) and sort them (PARSER_TEMPLATE_ID and PARSER_PY_ID excluded!)
            translations = self.trans.keys() - {self.settings.PARSER_TEMPLATE_ID, self.settings.PARSER_PY_ID}
            if langs:
                translations &= langs
            translations = [('"' + lng + '"', " " * (len(lng) + 6), self.trans[lng]) for lng in sorted(translations)]
            print(k for k in keys.keys())
            for key in keys.keys():
                if ref.msgs[key].is_commented:
                    continue
                # Key (context + msgid).
                msgctxt, msgid = ref.msgs[key].msgctxt, ref.msgs[key].msgid
                if not msgctxt:
                    msgctxt = default_context
                ret.append(tab + "(({}, \"{}\"),".format('"' + msgctxt + '"' if msgctxt else "None", msgid))
                # Common comments (mostly sources!).
                sources = []
                gen_comments = []
                for comment in ref.msgs[key].comment_lines:
                    if comment.startswith(self.settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM):
                        sources.append(comment[_lencsrc:])
                    elif comment.startswith(self.settings.PO_COMMENT_PREFIX_SOURCE):
                        sources.append(comment[_lensrc:])
                    elif comment.startswith(self.settings.PO_COMMENT_PREFIX_GENERATED):
                        gen_comments.append(comment[_lengen:])
                if not (sources or gen_comments):
                    ret.append(tab + " ((), ()),")
                else:
                    if len(sources) > 1:
                        ret.append(tab + ' (("' + sources[0] + '",')
                        ret += [tab + '   "' + s + '",' for s in sources[1:-1]]
                        ret.append(tab + '   "' + sources[-1] + '"),')
                    else:
                        ret.append(tab + " ((" + ('"' + sources[0] + '",' if sources else "") + "),")
                    if len(gen_comments) > 1:
                        ret.append(tab + '  ("' + gen_comments[0] + '",')
                        ret += [tab + '   "' + s + '",' for s in gen_comments[1:-1]]
                        ret.append(tab + '   "' + gen_comments[-1] + '")),')
                    else:
                        ret.append(tab + "  (" + ('"' + gen_comments[0] + '",' if gen_comments else "") + ")),")
                # All languages
                for lngstr, lngsp, trans in translations:
                    if trans.msgs[key].is_commented:
                        continue
                    # Language code and translation.
                    ret.append(tab + " (" + lngstr + ', "' + trans.msgs[key].msgstr + '",')
                    # User comments and fuzzy.
                    comments = []
                    for comment in trans.msgs[key].comment_lines:
                        if comment.startswith(self.settings.PO_COMMENT_PREFIX):
                            comments.append(comment[_lencomm:])
                    ret.append(tab + lngsp + "(" + ("True" if trans.msgs[key].is_fuzzy else "False") + ",")
                    if len(comments) > 1:
                        ret.append(tab + lngsp + ' ("' + comments[0] + '",')
                        ret += [tab + lngsp + '  "' + s + '",' for s in comments[1:-1]]
                        ret.append(tab + lngsp + '  "' + comments[-1] + '"))),')
                    else:
                        ret[-1] = ret[-1] + " (" + (('"' + comments[0] + '",') if comments else "") + "))),"

                ret.append(tab + "),")
            ret += [
                ")",
                "",
                "translations_dict = {}",
                "for msg in translations_tuple:",
                tab + "key = msg[0]",
                tab + "for lang, trans, (is_fuzzy, comments) in msg[2:]:",
                tab * 2 + "if trans and not is_fuzzy:",
                tab * 3 + "translations_dict.setdefault(lang, {})[key] = trans",
                "",
            ]
            return ret

        self.escape(True)
        dst = self.dst(self, self.src.get(self.settings.PARSER_PY_ID, ""), self.settings.PARSER_PY_ID, 'PY')
        print(dst)
        prev = txt = nxt = ""
        if os.path.exists(dst):
            if not os.path.isfile(dst):
                print("WARNING: trying to write as python code into {}, which is not a file! Aborting.".format(dst))
                return
            prev, txt, nxt, has_trans = self._parser_check_file(dst)
            if prev is None and nxt is None:
                print("WARNING: Looks like given python file {} has no auto-generated translations yet, will be added "
                      "at the end of the file, you can move that section later if needed...".format(dst))
                txt = ([txt, "", self.settings.PARSER_PY_MARKER_BEGIN] +
                       _gen_py(self, langs) +
                       ["", self.settings.PARSER_PY_MARKER_END])
            else:
                # We completely replace the text found between start and end markers...
                txt = _gen_py(self, langs)
        else:
            printf("Creating python file {} containing translations.".format(dst))
            txt = [
                "# ***** BEGIN GPL LICENSE BLOCK *****",
                "#",
                "# This program is free software; you can redistribute it and/or",
                "# modify it under the terms of the GNU General Public License",
                "# as published by the Free Software Foundation; either version 2",
                "# of the License, or (at your option) any later version.",
                "#",
                "# This program is distributed in the hope that it will be useful,",
                "# but WITHOUT ANY WARRANTY; without even the implied warranty of",
                "# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
                "# GNU General Public License for more details.",
                "#",
                "# You should have received a copy of the GNU General Public License",
                "# along with this program; if not, write to the Free Software Foundation,",
                "# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.",
                "#",
                "# ***** END GPL LICENSE BLOCK *****",
                "",
                self.settings.PARSER_PY_MARKER_BEGIN,
                "",
            ]
            txt += _gen_py(self, langs)
            txt += [
                "",
                self.settings.PARSER_PY_MARKER_END,
            ]
        with open(dst, 'w') as f:
            f.write((prev or "") + "\n".join(txt) + (nxt or ""))
        self.unescape()

    parsers = {
        "PO": parse_from_po,
        "PY": parse_from_py,
    }

    writers = {
        "PO": write_to_po,
        "PY": write_to_py,
    }
