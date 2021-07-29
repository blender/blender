# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
This is a pure python module (no blender deps),
that parses EDL files and could be used outside of blender.
"""

class TimeCode:
    """
    Simple timecode class
    also supports conversion from other time strings used by EDL
    """
    __slots__ = (
        "fps",
        "hours",
        "minutes",
        "seconds",
        "frame",
    )

    def __init__(self, data, fps):
        self.fps = fps
        if type(data) == str:
            self.from_string(data)
            frame = self.as_frame()
            self.from_frame(frame)
        else:
            self.from_frame(data)

    def from_string(self, text):
        # hh:mm:ss:ff
        # No dropframe support yet

        if text.lower().endswith("mps"):  # 5.2mps
            return self.from_frame(int(float(text[:-3]) * self.fps))
        elif text.lower().endswith("s"):  # 5.2s
            return self.from_frame(int(float(text[:-1]) * self.fps))
        elif text.isdigit():  # 1234
            return self.from_frame(int(text))
        elif ":" in text:  # hh:mm:ss:ff
            text = text.replace(";", ":").replace(",", ":").replace(".", ":")
            text = text.split(":")

            self.hours = int(text[0])
            self.minutes = int(text[1])
            self.seconds = int(text[2])
            self.frame = int(text[3])
            return self
        else:
            print("ERROR: could not convert this into timecode %r" % text)
            return self

    def from_frame(self, frame):

        if frame < 0:
            frame = -frame
            neg = True
        else:
            neg = False

        fpm = 60 * self.fps
        fph = 60 * fpm

        if frame < fph:
            self.hours = 0
        else:
            self.hours = int(frame / fph)
            frame = frame % fph

        if frame < fpm:
            self.minutes = 0
        else:
            self.minutes = int(frame / fpm)
            frame = frame % fpm

        if frame < self.fps:
            self.seconds = 0
        else:
            self.seconds = int(frame / self.fps)
            frame = frame % self.fps

        self.frame = frame

        if neg:
            self.frame = -self.frame
            self.seconds = -self.seconds
            self.minutes = -self.minutes
            self.hours = -self.hours

        return self

    def as_frame(self):
        abs_frame = self.frame
        abs_frame += self.seconds * self.fps
        abs_frame += self.minutes * 60 * self.fps
        abs_frame += self.hours * 60 * 60 * self.fps

        return abs_frame

    def as_string(self):
        self.from_frame(int(self))
        return "%.2d:%.2d:%.2d:%.2d" % (self.hours, self.minutes, self.seconds, self.frame)

    def __repr__(self):
        return self.as_string()

    # Numeric stuff, may as well have this
    def __neg__(self):
        return TimeCode(-int(self), self.fps)

    def __int__(self):
        return self.as_frame()

    def __sub__(self, other):
        return TimeCode(int(self) - int(other), self.fps)

    def __add__(self, other):
        return TimeCode(int(self) + int(other), self.fps)

    def __mul__(self, other):
        return TimeCode(int(self) * int(other), self.fps)

    def __div__(self, other):
        return TimeCode(int(self) // int(other), self.fps)

    def __abs__(self):
        return TimeCode(abs(int(self)), self.fps)

    def __iadd__(self, other):
        return self.from_frame(int(self) + int(other))

    def __imul__(self, other):
        return self.from_frame(int(self) * int(other))

    def __idiv__(self, other):
        return self.from_frame(int(self) // int(other))
# end timecode


"""Comments
Comments can appear at the beginning of the EDL file (header) or between the edit lines in the EDL. The first block of comments in the file is defined to be the header comments and they are associated with the EDL as a whole. Subsequent comments in the EDL file are associated with the first edit line that appears after them.
Edit Entries
<filename|tag>  <EditMode>  <TransitionType>[num]  [duration]  [srcIn]  [srcOut]  [recIn]  [recOut]

    * <filename|tag>: Filename or tag value. Filename can be for an MPEG file, Image file, or Image file template. Image file templates use the same pattern matching as for command line glob, and can be used to specify images to encode into MPEG. i.e. /usr/data/images/image*.jpg
    * <EditMode>: 'V' | 'A' | 'VA' | 'B' | 'v' | 'a' | 'va' | 'b' which equals Video, Audio, Video_Audio edits (note B or b can be used in place of VA or va).
    * <TransitonType>: 'C' | 'D' | 'E' | 'FI' | 'FO' | 'W' | 'c' | 'd' | 'e' | 'fi' | 'fo' | 'w'. which equals Cut, Dissolve, Effect, FadeIn, FadeOut, Wipe.
    * [num]: if TransitionType = Wipe, then a wipe number must be given. At the moment only wipe 'W0' and 'W1' are supported.
    * [duration]: if the TransitionType is not equal to Cut, then an effect duration must be given. Duration is in frames.
    * [srcIn]: Src in. If no srcIn is given, then it defaults to the first frame of the video or the first frame in the image pattern. If srcIn isn't specified, then srcOut, recIn, recOut can't be specified.
    * [srcOut]: Src out. If no srcOut is given, then it defaults to the last frame of the video - or last image in the image pattern. if srcOut isn't given, then recIn and recOut can't be specified.
    * [recIn]: Rec in. If no recIn is given, then it is calculated based on its position in the EDL and the length of its input.
      [recOut]: Rec out. If no recOut is given, then it is calculated based on its position in the EDL and the length of its input. first frame of the video.

For srcIn, srcOut, recIn, recOut, the values can be specified as either timecode, frame number, seconds, or mps seconds. i.e.
[tcode | fnum | sec | mps], where:

    * tcode : SMPTE timecode in hh:mm:ss:ff
    * fnum : frame number (the first decodable frame in the video is taken to be frame 0).
    * sec : seconds with 's' suffix (e.g. 5.2s)
    * mps : seconds with 'mps' suffix (e.g. 5.2mps). This corresponds to the 'seconds' value displayed by Windows MediaPlayer.

More notes,
Key

"""

enum = 0
TRANSITION_UNKNOWN = enum
TRANSITION_CUT = enum
enum += 1
TRANSITION_DISSOLVE = enum
enum += 1
TRANSITION_EFFECT = enum
enum += 1
TRANSITION_FADEIN = enum
enum += 1
TRANSITION_FADEOUT = enum
enum += 1
TRANSITION_WIPE = enum
enum += 1
TRANSITION_KEY = enum
enum += 1

TRANSITION_DICT = {
    "c": TRANSITION_CUT,
    "d": TRANSITION_DISSOLVE,
    "e": TRANSITION_EFFECT,
    "fi": TRANSITION_FADEIN,
    "fo": TRANSITION_FADEOUT,
    "w": TRANSITION_WIPE,
    "k": TRANSITION_KEY,
    }

enum = 0
EDIT_UNKNOWN = 1 << enum
enum += 1
EDIT_VIDEO = 1 << enum
enum += 1
EDIT_AUDIO = 1 << enum
enum += 1
EDIT_AUDIO_STEREO = 1 << enum
enum += 1
EDIT_VIDEO_AUDIO = 1 << enum
enum += 1

EDIT_DICT = {
    "none": 0,  # TODO, investigate this more.
    "v": EDIT_VIDEO,
    "a": EDIT_AUDIO,
    "aa": EDIT_AUDIO_STEREO,
    "va": EDIT_VIDEO_AUDIO,
    "b": EDIT_VIDEO_AUDIO,
    }


enum = 0
WIPE_UNKNOWN = enum
WIPE_0 = enum
enum += 1
WIPE_1 = enum
enum += 1

enum = 0
KEY_UNKNOWN = enum
KEY_BG = enum  # K B
enum += 1
KEY_IN = enum  # This is assumed if no second type is set
enum += 1
KEY_OUT = enum  # K O
enum += 1

BLACK_ID = {
    "bw",
    "bl",
    "blk",
    "black",
    }


"""
Most sytems:
Non-dropframe: 1:00:00:00 - colon in last position
Dropframe: 1:00:00;00 - semicolon in last position
PAL/SECAM: 1:00:00:00 - colon in last position

SONY:
Non-dropframe: 1:00:00.00 - period in last position
Dropframe: 1:00:00,00 - comma in last position
PAL/SECAM: 1:00:00.00 - period in last position
"""

"""
t = abs(timecode('-124:-12:-43:-22', 25))
t /= 2
print t
"""

class EditDecision:
    __slots__ = (
        "number",
        "reel",
        "transition_duration",
        "edit_type",
        "transition_type",
        "wipe_type",
        "key_type",
        "key_fade",
        "srcIn",
        "srcOut",
        "recIn",
        "recOut",
        "m2",
        "filename",
        "custom_data",
    )

    @staticmethod
    def edit_flags_to_text(flag):
        return "/".join([item for item, val in EDIT_DICT.items() if val & flag])

    @staticmethod
    def strip_digits(text):
        return "".join(filter(lambda x: not x.isdigit(), text))

    def __init__(self, text=None, fps=25):
        # print text
        self.number = -1
        self.reel = ""  # Uniqie name for this 'file' but not filename, when BL signifies black
        self.transition_duration = 0
        self.edit_type = EDIT_UNKNOWN
        self.transition_type = TRANSITION_UNKNOWN
        self.wipe_type = WIPE_UNKNOWN
        self.key_type = KEY_UNKNOWN
        self.key_fade = -1  # true/false
        self.srcIn = None   # Where on the original field recording the event begins
        self.srcOut = None  # Where on the original field recording the event ends
        self.recIn = None   # Beginning of the original event in the edited program
        self.recOut = None  # End of the original event in the edited program
        self.m2 = None      # fps set by the m2 command
        self.filename = ""

        self.custom_data = []  # use for storing any data you want (blender strip for eg)

        if text is not None:
            self.read(text, fps)

    def __repr__(self):
        txt = "num: %d, " % self.number
        txt += "reel: %s, " % self.reel
        txt += "edit_type: "
        txt += EditDecision.edit_flags_to_text(self.edit_type) + ", "

        txt += "trans_type: "
        for item, val in TRANSITION_DICT.items():
            if val == self.transition_type:
                txt += item + ", "
                break

        txt += "m2: "
        if self.m2:
            txt += "%g" % float(self.m2.fps)
            txt += "\n\t"
            txt += self.m2.data
        else:
            txt += "nil"

        txt += ", "
        txt += "recIn: " + str(self.recIn) + ", "
        txt += "recOut: " + str(self.recOut) + ", "
        txt += "srcIn: " + str(self.srcIn) + ", "
        txt += "srcOut: " + str(self.srcOut) + ", "

        return txt

    def read(self, line, fps):
        line = line.split()
        index = 0
        self.number = int(line[index])
        index += 1
        self.reel = line[index].lower()
        index += 1

        # AA/V can be an edit type
        self.edit_type = 0
        for edit_type in line[index].lower().split("/"):
            # stripping digits is done because we don't do 'a1, a2...'
            self.edit_type |= EDIT_DICT[EditDecision.strip_digits(edit_type)]
        index += 1

        tx_name = "".join([c for c in line[index].lower() if not c.isdigit()])
        self.transition_type = TRANSITION_DICT[tx_name]  # advance the index later

        if self.transition_type == TRANSITION_WIPE:
            tx_num = "".join([c for c in line[index].lower() if c.isdigit()])
            if tx_num:
                tx_num = int(tx_num)
            else:
                tx_num = 0

            self.wipe_type = tx_num

        elif self.transition_type == TRANSITION_KEY:  # UNTESTED

            val = line[index + 1].lower()

            if val == "b":
                self.key_type = KEY_BG
                index += 1
            elif val == "o":
                self.key_type = KEY_OUT
                index += 1
            else:
                self.key_type = KEY_IN  # if no args given

            # there may be an (F) after, eg 'K B (F)'
            # in the docs this should only be after K B but who knows, it may be after K O also?
            val = line[index + 1].lower()
            if val == "(f)":
                index += 1
                self.key_fade = True
            else:
                self.key_fade = False

        index += 1

        if self.transition_type in {TRANSITION_DISSOLVE, TRANSITION_EFFECT, TRANSITION_FADEIN, TRANSITION_FADEOUT, TRANSITION_WIPE}:
            self.transition_duration = TimeCode(line[index], fps)
            index += 1

        if index < len(line):
            self.srcIn = TimeCode(line[index], fps)
            index += 1
        if index < len(line):
            self.srcOut = TimeCode(line[index], fps)
            index += 1

        if index < len(line):
            self.recIn = TimeCode(line[index], fps)
            index += 1
        if index < len(line):
            self.recOut = TimeCode(line[index], fps)
            index += 1

    def renumber(self):
        self.edits.sort(key=lambda e: int(e.recIn))
        for i, edit in enumerate(self.edits):
            edit.number = i

    def clean(self):
        """
        Clean up double ups
        """
        self.renumber()

        # TODO
    def as_name(self):
        cut_type = "nil"
        for k, v in TRANSITION_DICT.items():
            if v == self.transition_type:
                cut_type = k
                break

        return "%d_%s_%s" % (self.number, self.reel, cut_type)


class M2:
    __slots__ = (
        "reel",
        "fps",
        "time",
        "data",
        "index",
        "tot",
    )

    def __init__(self):
        self.reel = None
        self.fps = None
        self.time = None
        self.data = None

        self.index = -1
        self.tot = -1

    def read(self, line, fps):

        # M2   TAPEC          050.5                00:08:11:08
        words = line.split()

        self.reel = words[1].lower()
        self.fps = float(words[2])
        self.time = TimeCode(words[3], fps)

        self.data = line


class EditList:
    __slots__ = (
        "edits",
        "title",
    )

    def __init__(self):
        self.edits = []
        self.title = ""

    def parse(self, filename, fps):
        try:
            file = open(filename, "r", encoding="utf-8")
        except:
            return False

        self.edits = []
        edits_m2 = []  # edits with m2's

        has_m2 = False

        for line in file:
            line = " ".join(line.split())

            if not line or line.startswith(("*", "#")):
                continue
            elif line.startswith("TITLE:"):
                self.title = " ".join(line.split()[1:])
            elif line.split()[0].lower() == "m2":
                has_m2 = True
                m2 = M2()
                m2.read(line, fps)
                edits_m2.append(m2)
            elif not line.split()[0].isdigit():
                print("Ignoring:", line)
            else:
                self.edits.append(EditDecision(line, fps))
                edits_m2.append(self.edits[-1])

        if has_m2:
            # Group indexes
            i = 0
            for item in edits_m2:
                if isinstance(item, M2):
                    item.index = i
                    i += 1
                else:
                    # not an m2
                    i = 0

            # Set total group indexes
            for item in reversed(edits_m2):
                if isinstance(item, M2):
                    if tot_m2 == -1:
                        tot_m2 = item.index + 1

                    item.tot = tot_m2
                else:
                    # not an m2
                    tot_m2 = -1

            for i, item in enumerate(edits_m2):
                if isinstance(item, M2):
                    # make a list of all items that match the m2's reel name
                    edits_m2_tmp = [item_tmp for item_tmp in edits_m2 if (isinstance(item, M2) or item_tmp.reel == item.reel)]

                    # get the new index
                    i_tmp = edits_m2_tmp.index(item)

                    # Seek back to get the edit.
                    edit = edits_m2[i_tmp - item.tot]

                    # Note, docs say time should also match with edit start time
                    # but from final cut pro, this seems not to be the case
                    if not isinstance(edit, EditDecision):
                        print("ERROR!", "M2 incorrect")
                    else:
                        edit.m2 = item

        file.close()
        return True

    def overlap_test(self, edit_test):
        recIn = int(edit_test.recIn)
        recOut = int(edit_test.recOut)

        for edit in self.edits:
            if edit is edit_test:
                break

            recIn_other = int(edit.recIn)
            recOut_other = int(edit.recOut)

            if recIn_other < recIn < recOut_other:
                return True
            if recIn_other < recOut < recOut_other:
                return True

            if recIn < recIn_other < recOut:
                return True
            if recIn < recOut_other < recOut:
                return True

        return False

    def reels_as_dict(self):
        reels = {}
        for edit in self.edits:
            reels.setdefault(edit.reel, []).append(edit)

        return reels
