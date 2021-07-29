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

import netrender.versioning as versioning
from netrender.utils import *

import time

# Jobs status
JOB_WAITING = 0 # before all data has been entered
JOB_PAUSED = 1 # paused by user
JOB_FINISHED = 2 # finished rendering
JOB_QUEUED = 3 # ready to be dispatched

JOB_STATUS_TEXT = {
        JOB_WAITING: "Waiting",
        JOB_PAUSED: "Paused",
        JOB_FINISHED: "Finished",
        JOB_QUEUED: "Queued"
        }

JOB_TRANSITION_STARTED = "Started"
JOB_TRANSITION_PAUSED = "Paused"
JOB_TRANSITION_RESUMED = "Resumed"
JOB_TRANSITION_FINISHED = "Finished"
JOB_TRANSITION_RESTARTED = "Restarted"

JOB_TRANSITIONS = {
       (JOB_WAITING, JOB_QUEUED) : JOB_TRANSITION_STARTED,
       (JOB_QUEUED, JOB_PAUSED) : JOB_TRANSITION_PAUSED,
       (JOB_PAUSED, JOB_QUEUED) : JOB_TRANSITION_RESUMED,
       (JOB_QUEUED, JOB_FINISHED) : JOB_TRANSITION_FINISHED,
       (JOB_FINISHED, JOB_QUEUED) : JOB_TRANSITION_RESTARTED
       }

# Job types (depends on the dependency type)
JOB_BLENDER = 1
JOB_PROCESS = 2
JOB_VCS     = 3

JOB_TYPES = {
                JOB_BLENDER: "Blender",
                JOB_PROCESS: "Process",
                JOB_VCS:     "Versioned",
            }

JOB_SUB_RENDER = 1
JOB_SUB_BAKING = 2

# Job subtypes
JOB_SUBTYPES = {
                JOB_SUB_RENDER: "Render",
                JOB_SUB_BAKING: "Baking",
            }


# Frames status
FRAME_QUEUED = 0
FRAME_DISPATCHED = 1
FRAME_DONE = 2
FRAME_ERROR = 3

FRAME_STATUS_TEXT = {
        FRAME_QUEUED: "Queued",
        FRAME_DISPATCHED: "Dispatched",
        FRAME_DONE: "Done",
        FRAME_ERROR: "Error"
        }

# Tags
TAG_BAKING = "baking"
TAG_RENDER = "render"

TAG_ALL = set((TAG_BAKING, TAG_RENDER))

class LogFile:
    def __init__(self, job_id = 0, slave_id = 0, frames = []):
        self.job_id = job_id
        self.slave_id = slave_id
        self.frames = frames

    def serialize(self):
        return 	{
                            "job_id": self.job_id,
                            "slave_id": self.slave_id,
                            "frames": self.frames
                        }

    @staticmethod
    def materialize(data):
        if not data:
            return None

        logfile = LogFile()
        logfile.job_id = data["job_id"]
        logfile.slave_id = data["slave_id"]
        logfile.frames = data["frames"]

        return logfile

class RenderSlave:
    _slave_map = {}

    def __init__(self, info = None):
        self.id = ""
        self.total_done = 0
        self.total_error = 0
        self.last_seen = 0.0

        if info:
            self.name = info.name
            self.address = info.address
            self.stats = info.stats
            self.tags = info.tags
        else:
            self.name = ""
            self.address = ("",0)
            self.stats = ""
            self.tags = set()

    def serialize(self):
        return 	{
                            "id": self.id,
                            "name": self.name,
                            "address": self.address,
                            "stats": self.stats,
                            "total_done": self.total_done,
                            "total_error": self.total_error,
                            "last_seen": self.last_seen,
                            "tags": tuple(self.tags)
                        }

    @staticmethod
    def materialize(data, cache = True):
        if not data:
            return None

        slave_id = data["id"]

        if cache and slave_id in RenderSlave._slave_map:
            return RenderSlave._slave_map[slave_id]

        slave = RenderSlave()
        slave.id = slave_id
        slave.name = data["name"]
        slave.address = data["address"]
        slave.stats = data["stats"]
        slave.total_done = data["total_done"]
        slave.total_error = data["total_error"]
        slave.last_seen = data["last_seen"]
        slave.tags = set(data["tags"])

        if cache:
            RenderSlave._slave_map[slave_id] = slave

        return slave

class VersioningInfo:
    def __init__(self, info = None):
        self._system = None
        self.wpath = ""
        self.rpath = ""
        self.revision = ""

    @property
    def system(self):
        return self._system

    @system.setter
    def system(self, value):
        self._system = versioning.SYSTEMS[value]

    def update(self):
        self.system.update(self)

    def serialize(self):
        return {
                "wpath": self.wpath,
                "rpath": self.rpath,
                "revision": self.revision,
                "system": self.system.name
                }

    @staticmethod
    def generate(system, path):
        vs = VersioningInfo()
        vs.wpath = path
        vs.system = system

        vs.rpath = vs.system.path(path)
        vs.revision = vs.system.revision(path)

        return vs


    @staticmethod
    def materialize(data):
        if not data:
            return None

        vs = VersioningInfo()
        vs.wpath = data["wpath"]
        vs.rpath = data["rpath"]
        vs.revision = data["revision"]
        vs.system = data["system"]

        return vs


class RenderFile:
    def __init__(self, filepath = "", index = 0, start = -1, end = -1, signature = 0):
        self.filepath = filepath
        self.original_path = filepath
        self.signature = signature
        self.index = index
        self.start = start
        self.end = end
        self.force = False


    def serialize(self):
        return 	{
                    "filepath": self.filepath,
                    "original_path": self.original_path,
                    "index": self.index,
                    "start": self.start,
                    "end": self.end,
                    "signature": self.signature,
                    "force": self.force

                }

    @staticmethod
    def materialize(data):
        if not data:
            return None

        rfile = RenderFile(data["filepath"], data["index"], data["start"], data["end"], data["signature"])
        rfile.original_path = data["original_path"]
        rfile.force = data["force"]

        return rfile

class RenderJob:
    def __init__(self, info = None):
        self.id = ""

        self.resolution = None

        self.usage = 0.0
        self.last_dispatched = 0.0
        self.frames = []
        self.transitions = []

        self._status = None

        if info:
            self.type = info.type
            self.subtype = info.subtype
            self.name = info.name
            self.category = info.category
            self.tags = info.tags
            self.status = info.status
            self.files = info.files
            self.chunks = info.chunks
            self.priority = info.priority
            self.blacklist = info.blacklist
            self.version_info = info.version_info
            self.render = info.render
        else:
            self.type = JOB_BLENDER
            self.subtype = JOB_SUB_RENDER
            self.name = ""
            self.category = "None"
            self.tags = set()
            self.status = JOB_WAITING
            self.files = []
            self.chunks = 0
            self.priority = 0
            self.blacklist = []
            self.version_info = None
            self.render = "BLENDER_RENDER"

    @property
    def status(self):
        """Status of the job (waiting, paused, finished or queued)"""
        return self._status

    @status.setter
    def status(self, value):
        transition = JOB_TRANSITIONS.get((self.status, value), None)
        if transition:
            self.transitions.append((transition, time.time()))

        self._status = value

    @property
    def time_started(self):
        started_time = None
        for transition, time_value in self.transitions:
            if transition == JOB_TRANSITION_STARTED:
                started_time = time_value
                break

        return started_time

    @property
    def time_finished(self):
        finished_time = None
        if self.status == JOB_FINISHED:
            for transition, time_value in self.transitions:
                if transition == JOB_TRANSITION_FINISHED:
                    finished_time = time_value

        return finished_time

    def hasRenderResult(self):
        return self.subtype == JOB_SUB_RENDER

    def rendersWithBlender(self):
        return self.subtype == JOB_SUB_RENDER

    def addFile(self, file_path, start=-1, end=-1, signed=True):
        def isFileInFrames():
            if start == end == -1:
                return True

            for rframe in self.frames:
                if start <= rframe.number<= end:
                    return True

            return False


        if isFileInFrames():
            if signed:
                signature = hashFile(file_path)
            else:
                signature = None
            self.files.append(RenderFile(file_path, len(self.files), start, end, signature))

    def addFrame(self, frame_number, command = ""):
        frame = RenderFrame(frame_number, command)
        self.frames.append(frame)
        return frame

    def __len__(self):
        return len(self.frames)

    def countFrames(self, status=FRAME_QUEUED):
        total = 0
        for f in self.frames:
            if f.status == status:
                total += 1

        return total

    def countSlaves(self):
        return len(set((frame.slave for frame in self.frames if frame.status == FRAME_DISPATCHED)))

    def statusText(self):
        return JOB_STATUS_TEXT[self.status]

    def framesStatus(self):
        results = {
                                FRAME_QUEUED: 0,
                                FRAME_DISPATCHED: 0,
                                FRAME_DONE: 0,
                                FRAME_ERROR: 0
                            }

        for frame in self.frames:
            results[frame.status] += 1

        return results

    def __contains__(self, frame_number):
        for f in self.frames:
            if f.number == frame_number:
                return True
        else:
            return False

    def __getitem__(self, frame_number):
        for f in self.frames:
            if f.number == frame_number:
                return f
        else:
            return None

    def serialize(self, frames = None,withFiles=True,withFrames=True):
        min_frame = min((f.number for f in frames)) if frames else -1
        max_frame = max((f.number for f in frames)) if frames else -1
        data={
                            "id": self.id,
                            "type": self.type,
                            "subtype": self.subtype,
                            "name": self.name,
                            "category": self.category,
                            "tags": tuple(self.tags),
                            "status": self.status,
                            "transitions": self.transitions,
                            "chunks": self.chunks,
                            "priority": self.priority,
                            "usage": self.usage,
                            "blacklist": self.blacklist,
                            "last_dispatched": self.last_dispatched,
                            "version_info": self.version_info.serialize() if self.version_info else None,
                            "resolution": self.resolution,
                            "render": self.render
                        }
        if (withFiles):
           data["files"]=[f.serialize() for f in self.files if f.start == -1 or not frames or (f.start <= max_frame and f.end >= min_frame)]

        if (withFrames):
           data["frames"]=[f.serialize() for f in self.frames if not frames or f in frames]

        return data
    @staticmethod
    def materialize(data):
        if not data:
            return None

        job = RenderJob()
        job.id = data["id"]
        job.type = data["type"]
        job.subtype = data["subtype"]
        job.name = data["name"]
        job.category = data["category"]
        job.tags = set(data["tags"])
        job.status = data["status"]
        job.transitions = data["transitions"]
        job.files = [RenderFile.materialize(f) for f in data["files"]]
        job.frames = [RenderFrame.materialize(f) for f in data["frames"]]
        job.chunks = data["chunks"]
        job.priority = data["priority"]
        job.usage = data["usage"]
        job.blacklist = data["blacklist"]
        job.last_dispatched = data["last_dispatched"]
        job.resolution = data["resolution"]
        job.render=data["render"]

        version_info = data.get("version_info", None)
        if version_info:
            job.version_info = VersioningInfo.materialize(version_info)

        return job

class RenderFrame:
    def __init__(self, number = 0, command = ""):
        self.number = number
        self.time = 0
        self.status = FRAME_QUEUED
        self.slave = None
        self.command = command
        self.results = []   # List of filename of result files associated with this frame

    def statusText(self):
        return FRAME_STATUS_TEXT[self.status]

    def serialize(self):
        return 	{
                            "number": self.number,
                            "time": self.time,
                            "status": self.status,
                            "slave": None if not self.slave else self.slave.serialize(),
                            "command": self.command,
                            "results": self.results
                        }

    @staticmethod
    def materialize(data):
        if not data:
            return None

        frame = RenderFrame()
        frame.number = data["number"]
        frame.time = data["time"]
        frame.status = data["status"]
        frame.slave = RenderSlave.materialize(data["slave"])
        frame.command = data["command"]
        frame.results = data["results"]

        return frame
