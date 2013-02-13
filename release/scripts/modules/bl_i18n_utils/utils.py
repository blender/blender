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
import os
import re
import sys

from bl_i18n_utils import settings


PO_COMMENT_PREFIX = settings.PO_COMMENT_PREFIX
PO_COMMENT_PREFIX_MSG = settings.PO_COMMENT_PREFIX_MSG
PO_COMMENT_PREFIX_SOURCE = settings.PO_COMMENT_PREFIX_SOURCE
PO_COMMENT_PREFIX_SOURCE_CUSTOM = settings.PO_COMMENT_PREFIX_SOURCE_CUSTOM
PO_COMMENT_FUZZY = settings.PO_COMMENT_FUZZY
PO_MSGCTXT = settings.PO_MSGCTXT
PO_MSGID = settings.PO_MSGID
PO_MSGSTR = settings.PO_MSGSTR

PO_HEADER_KEY = settings.PO_HEADER_KEY
PO_HEADER_COMMENT = settings.PO_HEADER_COMMENT
PO_HEADER_COMMENT_COPYRIGHT = settings.PO_HEADER_COMMENT_COPYRIGHT
PO_HEADER_MSGSTR = settings.PO_HEADER_MSGSTR

PARSER_CACHE_HASH = settings.PARSER_CACHE_HASH

WARN_NC = settings.WARN_MSGID_NOT_CAPITALIZED
NC_ALLOWED = settings.WARN_MSGID_NOT_CAPITALIZED_ALLOWED
PARSER_CACHE_HASH = settings.PARSER_CACHE_HASH


##### Misc Utils #####

def stripeol(s):
    return s.rstrip("\n\r")


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
    #tmp = difflib.get_close_matches(key[1], similar_pool, n=1, cutoff=use_similar)
    #if tmp:
        #tmp = tmp[0]
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


class I18nMessage:
    """
    Internal representation of a message.
    """
    __slots__ = ("msgctxt_lines", "msgid_lines", "msgstr_lines", "comment_lines", "is_fuzzy", "is_commented")

    def __init__(self, msgctxt_lines=[], msgid_lines=[], msgstr_lines=[], comment_lines=[],
                 is_commented=False, is_fuzzy=False):
        self.msgctxt_lines = msgctxt_lines
        self.msgid_lines = msgid_lines
        self.msgstr_lines = msgstr_lines
        self.comment_lines = comment_lines
        self.is_fuzzy = is_fuzzy
        self.is_commented = is_commented

    def _get_msgctxt(self):
        return ("".join(self.msgctxt_lines)).replace("\\n", "\n")
    def _set_msgctxt(self, ctxt):
        self.msgctxt_lines = [ctxt]
    msgctxt = property(_get_msgctxt, _set_msgctxt)

    def _get_msgid(self):
        return ("".join(self.msgid_lines)).replace("\\n", "\n")
    def _set_msgid(self, msgid):
        self.msgid_lines = [msgid]
    msgid = property(_get_msgid, _set_msgid)

    def _get_msgstr(self):
        return ("".join(self.msgstr_lines)).replace("\\n", "\n")
    def _set_msgstr(self, msgstr):
        self.msgstr_lines = [msgstr]
    msgstr = property(_get_msgstr, _set_msgstr)

    def _get_sources(self):
        lstrip1 = len(PO_COMMENT_PREFIX_SOURCE)
        lstrip2 = len(PO_COMMENT_PREFIX_SOURCE_CUSTOM)
        return ([l[lstrip1:] for l in self.comment_lines if l.startswith(PO_COMMENT_PREFIX_SOURCE)] +
                [l[lstrip2:] for l in self.comment_lines if l.startswith(PO_COMMENT_PREFIX_SOURCE_CUSTOM)])
    def _set_sources(self, sources):
        # list.copy() is not available in py3.2 ...
        cmmlines = []
        cmmlines[:] = self.comment_lines
        for l in cmmlines:
            if l.startswith(PO_COMMENT_PREFIX_SOURCE) or l.startswith(PO_COMMENT_PREFIX_SOURCE_CUSTOM):
                self.comment_lines.remove(l)
        lines_src = []
        lines_src_custom = []
        for src in  sources:
            if is_valid_po_path(src):
                lines_src.append(PO_COMMENT_PREFIX_SOURCE + src)
            else:
                lines_src_custom.append(PO_COMMENT_PREFIX_SOURCE_CUSTOM + src)
        self.comment_lines += lines_src_custom + lines_src
    sources = property(_get_sources, _set_sources)

    def _get_is_tooltip(self):
        # XXX For now, we assume that all messages > 30 chars are tooltips!
        return len(self.msgid) > 30
    is_tooltip = property(_get_is_tooltip)

    def normalize(self, max_len=80):
        """
        Normalize this message, call this before exporting it...
        Currently normalize msgctxt, msgid and msgstr lines to given max_len (if below 1, make them single line).
        """
        max_len -= 2  # The two quotes!
        # We do not need the full power of textwrap... We just split first at escaped new lines, then into each line
        # if needed... No word splitting, nor fancy spaces handling!
        def _wrap(text, max_len, init_len):
            if len(text) + init_len < max_len:
                return [text]
            lines = text.splitlines()
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
            self.msgctxt_lines = self.msgctxt.replace("\n", "\\n\n").splitlines()
            self.msgid_lines = self.msgid.replace("\n", "\\n\n").splitlines()
            self.msgstr_lines = self.msgstr.replace("\n", "\\n\n").splitlines()
        else:
            init_len = len(PO_MSGCTXT) + 1
            if self.is_commented:
                init_len += len(PO_COMMENT_PREFIX_MSG)
            self.msgctxt_lines = _wrap(self.msgctxt.replace("\n", "\\n\n"), max_len, init_len)

            init_len = len(PO_MSGID) + 1
            if self.is_commented:
                init_len += len(PO_COMMENT_PREFIX_MSG)
            self.msgid_lines = _wrap(self.msgid.replace("\n", "\\n\n"), max_len, init_len)

            init_len = len(PO_MSGSTR) + 1
            if self.is_commented:
                init_len += len(PO_COMMENT_PREFIX_MSG)
            self.msgstr_lines = _wrap(self.msgstr.replace("\n", "\\n\n"), max_len, init_len)


class I18nMessages:
    """
    Internal representation of messages for one language (iso code), with additional stats info.
    """

    # Avoid parsing again!
    # Keys should be (pseudo) file-names, values are tuples (hash, I18nMessages)
    # Note: only used by po parser currently!
    _parser_cache = {}

    def __init__(self, iso="__POT__", kind=None, key=None, src=None):
        self.iso = iso
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

    @staticmethod
    def _new_messages():
        return getattr(collections, 'OrderedDict', dict)()

    @classmethod
    def gen_empty_messages(cls, iso, blender_ver, blender_rev, time, year, default_copyright=True):
        """Generate an empty I18nMessages object (only header is present!)."""
        msgstr = PO_HEADER_MSGSTR.format(blender_ver=str(blender_ver), blender_rev=int(blender_rev),
                                         time=str(time), iso=str(iso))
        comment = ""
        if default_copyright:
            comment = PO_HEADER_COMMENT_COPYRIGHT.format(year=str(year))
        comment = comment + PO_HEADER_COMMENT

        msgs = cls(iso=iso)
        msgs.msgs[PO_HEADER_KEY] = I18nMessage([], [""], [msgstr], [comment], False, True)
        msgs.update_info()

        return msgs

    def normalize(self, max_len=80):
        for msg in self.msgs.values():
            msg.normalize(max_len)

    def merge(self, replace=False, *args):
        pass

    def update(self, ref, use_similar=0.75, keep_old_commented=True):
        """
        Update this I18nMessage with the ref one. Translations from ref are never used. Source comments from ref
        completely replace current ones. If use_similar is not 0.0, it will try to match new messages in ref with an
        existing one. Messages no more found in ref will be marked as commented if keep_old_commented is True,
        or removed.
        """
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
            msg.is_fuzzy = refmsg.is_fuzzy
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
                        msg, refmsg = copy.deepcopy(self.msgs[skey]), ref.msgs[key]
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
        key = ("", "")
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
            if key == PO_HEADER_KEY:
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
        self.nbr_trans_msgs = len(self.trans_msgs)
        self.nbr_ttips = len(self.ttip_msgs)
        self.nbr_trans_ttips = len(self.ttip_msgs & self.trans_msgs)
        self.nbr_comm_msgs = len(self.comm_msgs)

    def print_stats(self, prefix=""):
        """
        Print out some stats about an I18nMessages object.
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

        lines = ("",
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
                 "This translation is currently made of {} signs.\n".format(self.nbr_trans_signs))
        print(prefix.join(lines))

    def parse(self, kind, key, src):
        del self.parsing_errors[:]
        self.parsers[kind](self, src, key)
        if self.parsing_errors:
            print("WARNING! Errors while parsing {}:".format(key))
            for line, error in self.parsing_errors:
                print("    Around line {}: {}".format(line, error))
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

        # Helper function
        def finalize_message(self, line_nr):
            nonlocal reading_msgid, reading_msgstr, reading_msgctxt, reading_comment
            nonlocal is_commented, is_fuzzy, msgid_lines, msgstr_lines, msgctxt_lines, comment_lines

            msgid = "".join(msgid_lines)
            msgctxt = "".join(msgctxt_lines)
            msgkey = (msgctxt, msgid)

            # Never allow overriding existing msgid/msgctxt pairs!
            if msgkey in self.msgs:
                self.parsing_errors.append((line_nr, "{} context/msgid is already in current messages!".format(msgkey)))
                return

            self.msgs[msgkey] = I18nMessage(msgctxt_lines, msgid_lines, msgstr_lines, comment_lines,
                                            is_commented, is_fuzzy)

            # Let's clean up and get ready for next message!
            reading_msgid = reading_msgstr = reading_msgctxt = reading_comment = False
            is_commented = is_fuzzy = False
            msgctxt_lines = []
            msgid_lines = []
            msgstr_lines = []
            comment_lines = []

        # try to use src as file name...
        if os.path.exists(src):
            if not key:
                key = src
            with open(src, 'r', encoding="utf-8") as f:
                src = f.read()

        # Try to use values from cache!
        curr_hash = None
        if key and key in self._parser_cache:
            old_hash, msgs = self._parser_cache[key]
            import hashlib
            curr_hash = hashlib.new(PARSER_CACHE_HASH, src.encode()).digest()
            if curr_hash == old_hash:
                self.msgs = copy.deepcopy(msgs)  # we might edit self.msgs!
                return

        _comm_msgctxt = PO_COMMENT_PREFIX_MSG + PO_MSGCTXT
        _len_msgctxt = len(PO_MSGCTXT + '"')
        _len_comm_msgctxt = len(_comm_msgctxt + '"')
        _comm_msgid = PO_COMMENT_PREFIX_MSG + PO_MSGID
        _len_msgid = len(PO_MSGID + '"')
        _len_comm_msgid = len(_comm_msgid + '"')
        _comm_msgstr = PO_COMMENT_PREFIX_MSG + PO_MSGSTR
        _len_msgstr = len(PO_MSGSTR + '"')
        _len_comm_msgstr = len(_comm_msgstr + '"')
        _len_comm_str = len(PO_COMMENT_PREFIX_MSG + '"')

        # Main loop over all lines in src...
        for line_nr, line in enumerate(src.splitlines()):
            if line == "":
                if reading_msgstr:
                    finalize_message(self, line_nr)
                continue

            elif line.startswith(PO_MSGCTXT) or line.startswith(_comm_msgctxt):
                reading_comment = False
                reading_ctxt = True
                if line.startswith(PO_COMMENT_PREFIX_MSG):
                    is_commented = True
                    line = line[_len_comm_msgctxt:-1]
                else:
                    line = line[_len_msgctxt:-1]
                msgctxt_lines.append(line)

            elif line.startswith(PO_MSGID) or line.startswith(_comm_msgid):
                reading_comment = False
                reading_msgid = True
                if line.startswith(PO_COMMENT_PREFIX_MSG):
                    if not is_commented and reading_ctxt:
                        self.parsing_errors.append((line_nr, "commented msgid following regular msgctxt"))
                    is_commented = True
                    line = line[_len_comm_msgid:-1]
                else:
                    line = line[_len_msgid:-1]
                reading_ctxt = False
                msgid_lines.append(line)

            elif line.startswith(PO_MSGSTR) or line.startswith(_comm_msgstr):
                if not reading_msgid:
                    self.parsing_errors.append((line_nr, "msgstr without a prior msgid"))
                else:
                    reading_msgid = False
                reading_msgstr = True
                if line.startswith(PO_COMMENT_PREFIX_MSG):
                    line = line[_len_comm_msgstr:-1]
                    if not is_commented:
                        self.parsing_errors.append((line_nr, "commented msgstr following regular msgid"))
                else:
                    line = line[_len_msgstr:-1]
                    if is_commented:
                        self.parsing_errors.append((line_nr, "regular msgstr following commented msgid"))
                msgstr_lines.append(line)

            elif line.startswith(PO_COMMENT_PREFIX[0]):
                if line.startswith(PO_COMMENT_PREFIX_MSG):
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
                    elif line.startswith(PO_COMMENT_FUZZY):
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

        if key:
            if not curr_hash:
                import hashlib
                curr_hash = hashlib.new(PARSER_CACHE_HASH, src.encode()).digest()
            self._parser_cache[key] = (curr_hash, self.msgs)

    def write(self, kind, dest):
        self.writers[kind](self, dest)

    def write_messages_to_po(self, fname):
        """
        Write messages in fname po file.
        """
        self.normalize(max_len=0)  # No wrapping for now...
        with open(fname, 'w', encoding="utf-8") as f:
            for msg in self.msgs.values():
                f.write("\n".join(msg.comment_lines))
                # Only mark as fuzzy if msgstr is not empty!
                if msg.is_fuzzy and msg.msgstr:
                    f.write("\n" + PO_COMMENT_FUZZY)
                _p = PO_COMMENT_PREFIX_MSG if msg.is_commented else ""
                _pmsgctxt = _p + PO_MSGCTXT
                _pmsgid = _p + PO_MSGID
                _pmsgstr = _p + PO_MSGSTR
                chunks = []
                if msg.msgctxt:
                    if len(msg.msgctxt_lines) > 1:
                        chunks += [
                            "\n" + _pmsgctxt + "\"\"\n" + _p + "\"",
                            ("\"\n" + _p + "\"").join(msg.msgctxt_lines),
                            "\"",
                        ]
                    else:
                        chunks += ["\n" + _pmsgctxt + "\"" + msg.msgctxt + "\""]
                if len(msg.msgid_lines) > 1:
                    chunks += [
                        "\n" + _pmsgid + "\"\"\n" + _p + "\"",
                        ("\"\n" + _p + "\"").join(msg.msgid_lines),
                        "\"",
                    ]
                else:
                    chunks += ["\n" + _pmsgid + "\"" + msg.msgid + "\""]
                if len(msg.msgstr_lines) > 1:
                    chunks += [
                        "\n" + _pmsgstr + "\"\"\n" + _p + "\"",
                        ("\"\n" + _p + "\"").join(msg.msgstr_lines),
                        "\"",
                    ]
                else:
                    chunks += ["\n" + _pmsgstr + "\"" + msg.msgstr + "\""]
                chunks += ["\n\n"]
                f.write("".join(chunks))

    parsers = {
        "PO": parse_messages_from_po,
#        "PYTUPLE": parse_messages_from_pytuple,
    }

    writers = {
        "PO": write_messages_to_po,
        #"PYDICT": write_messages_to_pydict,
    }


class I18n:
    """
    Internal representation of a whole translation set.
    """

    def __init__(self, src):
        self.trans = {}
        self.update_info()

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

        if TEMPLATE_ISO_ID in self.trans:
            self.nbr_trans = len(self.trans) - 1
            self.nbr_signs = self.trans[TEMPLATE_ISO_ID].nbr_signs
        else:
            self.nbr_trans = len(self.trans)
        for iso, msgs in self.trans.items():
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
            for key, msgs in self.trans:
                if key == TEMPLATE_ISO_ID:
                    continue
                print(prefix + key + ":")
                msgs.print_stats(prefix=msgs_prefix)
                print(prefix)

        nbr_contexts = len(self.contexts - {CONTEXT_DEFAULT})
        if nbr_contexts != 1:
            if nbr_contexts == 0:
                nbr_contexts = "No"
            _ctx_txt = "s are"
        else:
            _ctx_txt = " is"
        lines = ("",
                 "Average stats for all {} translations:\n".format(self.nbr_trans),
                 "    {:>6.1%} done!\n".format(self.lvl / self.nbr_trans),
                 "    {:>6.1%} of messages are tooltips.\n".format(self.lvl_ttips / self.nbr_trans),
                 "    {:>6.1%} of tooltips are translated.\n".format(self.lvl_trans_ttips / self.nbr_trans),
                 "    {:>6.1%} of translated messages are tooltips.\n".format(self.lvl_ttips_in_trans / self.nbr_trans),
                 "    {:>6.1%} of messages are commented.\n".format(self.lvl_comm / self.nbr_trans),
                 "    The org msgids are currently made of {} signs.\n".format(self.nbr_signs),
                 "    All processed translations are currently made of {} signs.\n".format(self.nbr_trans_signs),
                 "    {} specific context{} present:\n            {}\n"
                 "".format(self.nbr_contexts, _ctx_txt, "\n            ".join(self.contexts - {CONTEXT_DEFAULT})),
                 "\n")
        print(prefix.join(lines))


##### Parsers #####

#def parse_messages_from_pytuple(self, src, key=None):
    #"""
    #Returns a dict of tuples similar to the one returned by parse_messages_from_po (one per language, plus a 'pot'
    #one keyed as '__POT__').
    #"""
    ## src may be either a string to be interpreted as py code, or a real tuple!
    #if isinstance(src, str):
        #src = eval(src)
#
    #curr_hash = None
    #if key and key in _parser_cache:
        #old_hash, ret = _parser_cache[key]
        #import hashlib
        #curr_hash = hashlib.new(PARSER_CACHE_HASH, str(src).encode()).digest()
        #if curr_hash == old_hash:
            #return ret
#
    #pot = new_messages()
    #states = gen_states()
    #stats = gen_stats()
    #ret = {"__POT__": (pot, states, stats)}
    #for msg in src:
        #key = msg[0]
        #messages[msgkey] = gen_message(msgid_lines, msgstr_lines, comment_lines, msgctxt_lines)
        #pot[key] = gen_message(msgid_lines=[key[1]], msgstr_lines=[
        #for lang, trans, (is_fuzzy, comments) in msg[2:]:
            #if trans and not is_fuzzy:
                #i18n_dict.setdefault(lang, dict())[key] = trans
#
    #if key:
        #if not curr_hash:
            #import hashlib
            #curr_hash = hashlib.new(PARSER_CACHE_HASH, str(src).encode()).digest()
        #_parser_cache[key] = (curr_hash, val)
    #return ret