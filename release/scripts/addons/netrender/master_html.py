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

import os
import shutil
from netrender.utils import *
import netrender.model
import json
#import rpdb2

# bitwise definition of the different files type

CACHE_FILES=1
FLUID_FILES=2
OTHER_FILES=4

src_folder = os.path.split(__file__)[0]

#function to return counter of different type of files
# job: the job that contain files

def countFiles(job):
    tot_cache = 0
    tot_fluid = 0
    tot_other = 0
    for file in job.files:
        if file.filepath.endswith(".bphys"):
           tot_cache += 1
        elif file.filepath.endswith((".bobj.gz", ".bvel.gz")):
           tot_fluid += 1
        elif not file == job.files[0]:
           tot_other += 1
    return tot_cache,tot_fluid,tot_other;


def get(handler):
    def output(text):
        handler.wfile.write(bytes(text, encoding='utf8'))

    def head(title, refresh = False):
        output("<html><head>")
        if refresh:
            output("<meta http-equiv='refresh' content=5>")
        output("<script src='/html/netrender.js' type='text/javascript'></script>")
        output("<title>")
        output(title)
        output("</title></head><body>")
        output("<link rel='stylesheet' href='/html/netrender.css' type='text/css'>")


    def link(text, url, script=""):
        return "<a href='%s' %s>%s</a>" % (url, script, text)

    def tag(name, text, attr=""):
        return "<%s %s>%s</%s>" % (name, attr, text, name)

    def startTable(border=1, class_style = None, caption = None):
        output("<table border='%i'" % border)

        if class_style:
            output(" class='%s'" % class_style)

        output(">")

        if caption:
            output("<caption>%s</caption>" % caption)

    def headerTable(*headers):
        output("<thead><tr>")

        for c in headers:
            output("<td>" + c + "</td>")

        output("</tr></thead>")

    def rowTable(*data, id = None, class_style = None, extra = None):
        output("<tr")

        if id:
            output(" id='%s'" % id)

        if class_style:
            output(" class='%s'" % class_style)

        if extra:
            output(" %s" % extra)

        output(">")

        for c in data:
            output("<td>" + str(c) + "</td>")

        output("</tr>")

    def endTable():
        output("</table>")

    def checkbox(title, value, script=""):
        return """<input type="checkbox" title="%s" %s %s>""" % (title, "checked" if value else "", ("onclick=\"%s\"" % script) if script else "")

    def sendjson(message):
        handler.send_head(content = "application/json")
        output(json.dumps(message,sort_keys=False))

    def sendFile(filename,content_type):
        f = open(os.path.join(src_folder,filename), 'rb')

        handler.send_head(content = content_type)
        shutil.copyfileobj(f, handler.wfile)

        f.close()
    # return serialized version of job for html interface
    # job: the base job
    # includeFiles: boolean to indicate if we want file to be serialized too into job
    # includeFrames; boolean to  indicate if we want frame to be serialized too into job
    def gethtmlJobInfo(job,includeFiles=True,includeFrames=True):
        if (job):
             results = job.framesStatus()
             serializedJob = job.serialize(withFiles=includeFiles, withFrames=includeFrames)
             serializedJob["p_rule"] = handler.server.balancer.applyPriorities(job)
             serializedJob["e_rule"] = handler.server.balancer.applyExceptions(job)
             serializedJob["wait"] = int(time.time() - job.last_dispatched) if job.status != netrender.model.JOB_FINISHED else "N/A"
             serializedJob["length"] = len(job);
             serializedJob["done"] = results[netrender.model.FRAME_DONE]
             serializedJob["dispatched"] = results[netrender.model.FRAME_DISPATCHED]
             serializedJob["error"] = results[netrender.model.FRAME_ERROR]
             tot_cache, tot_fluid, tot_other = countFiles(job)
             serializedJob["totcache"] = tot_cache
             serializedJob["totfluid"] = tot_fluid
             serializedJob["totother"] = tot_other
             serializedJob["wktime"] = (time.time()-job.start_time ) if job.status != netrender.model.JOB_FINISHED else (job.finish_time-job.start_time)
        else:
             serializedJob={"name":"invalid job"}

        return  serializedJob;

    # return serialized files based on cumulative file_type
    # job_id: id of the job
    # message: serialized content
    # file_type: any combinaison of CACHE_FILE,FLUID_FILES, OTHER_FILES

    def getFiles(job_id,message,file_type):

        job=handler.server.getJobID(job_id)
        print ("job.files.length="+str(len(job.files)))

        for file in job.files:
            filedata=file.serialize()
            filedata["name"] = os.path.split(file.filepath)[1]

            if file.filepath.endswith(".bphys") and (file_type & CACHE_FILES):
               message.append(filedata);
               continue
            if file.filepath.endswith((".bobj.gz", ".bvel.gz")) and (file_type & FLUID_FILES):
               message.append(filedata);
               continue
            if (not file == job.files[0]) and (file_type & OTHER_FILES) and (not file.filepath.endswith((".bobj.gz", ".bvel.gz"))) and not file.filepath.endswith(".bphys"):
               message.append(filedata);
               continue



    if handler.path == "/html/netrender.js":
        sendFile("netrender.js","text/javascript")

    elif handler.path == "/html/netrender.css":
        sendFile("netrender.css","text/css")

    elif handler.path =="/html/newui":
        sendFile("newui.html","text/html")

    elif handler.path.startswith("/html/js"):
         path, filename = os.path.split(handler.path)
         sendFile("js/"+filename,"text/javascript")

    elif handler.path.startswith("/html/css/images"):
         path, filename = os.path.split(handler.path)
         sendFile("css/images/"+filename,"image/png")

    elif handler.path.startswith("/html/css"):
         path, filename = os.path.split(handler.path)
         sendFile("css/"+filename,"text/css")
    # return all master rules information
    elif handler.path == "/html/rules":
         message = []
         for rule in handler.server.balancer.rules:
            message.append(rule.serialize())
         for rule in handler.server.balancer.priorities:
            message.append(rule.serialize())
         for rule in handler.server.balancer.exceptions:
            message.append(rule.serialize())
         sendjson(message)
    #return all slaves list
    elif handler.path == "/html/slaves":
         message = []
         for slave in handler.server.slaves:
            serializedSlave = slave.serialize()
            if  slave.job:
                serializedSlave["job_name"] = slave.job.name
                serializedSlave["job_id"] = slave.job.id
            else:
                serializedSlave["job_name"] = "None"
                serializedSlave["job_id"] = "0"
            message.append(serializedSlave)
         sendjson(message)
    # return all job list
    elif handler.path == "/html/jobs":
         message = []
         for job in handler.server.jobs:
             if job:
                message.append(gethtmlJobInfo(job, False, False))
         sendjson(message)
     #return a job information
    elif handler.path.startswith("/html/job_"):

         job_id = handler.path[10:]
         job = handler.server.getJobID(job_id)

         message = []
         if job:

             message.append(gethtmlJobInfo(job, includeFiles=False))
         sendjson(message)
    # return all frames for a job
    elif handler.path.startswith("/html/frames_"):

         job_id = handler.path[13:]
         job = handler.server.getJobID(job_id)

         message = []
         if job:
             for f in job.frames:
              message.append(f.serialize())

         sendjson(message)
    # return physic cache files
    elif handler.path.startswith("/html/cachefiles_"):
         job_id = handler.path[17:]
         message = []
         getFiles(job_id, message, CACHE_FILES);
         sendjson(message)
    #return fluid cache files
    elif handler.path.startswith("/html/fluidfiles_"):
         job_id = handler.path[17:]

         message = []
         getFiles(job_id, message, FLUID_FILES);
         sendjson(message)

    #return list of other files ( images, sequences ...)
    elif handler.path.startswith("/html/otherfiles_"):
         job_id = handler.path[17:]

         message = []
         getFiles(job_id, message, OTHER_FILES);
         sendjson(message)
    # return blend file info
    elif handler.path.startswith("/html/blendfile_"):
         job_id = handler.path[16:]
         job = handler.server.getJobID(job_id)
         message = []
         if job:
             if job.files:
                message.append(job.files[0].serialize())
         sendjson(message)
    # return black listed slaves for a job
    elif handler.path.startswith("/html/blacklist_"):

         job_id = handler.path[16:]
         job = handler.server.getJobID(job_id)

         message = []
         if job:
           for slave_id in job.blacklist:
               slave = handler.server.slaves_map.get(slave_id, None)
               message.append(slave.serialize())
         sendjson(message)
    # return all slaves currently assigned to a job

    elif handler.path.startswith("/html/slavesjob_"):

         job_id = handler.path[16:]
         job = handler.server.getJobID(job_id)
         message = []
         if job:
           for slave in handler.server.slaves:
               if slave.job and slave.job == job:
                   message.append(slave.serialize())
           sendjson(message)
    # here begin code for simple ui
    elif handler.path == "/html" or handler.path == "/":
        handler.send_head(content = "text/html")
        head("NetRender", refresh = True)

        output("<h2>Jobs</h2>")

        startTable()
        headerTable(
                        "&nbsp;",
                        "id",
                        "name",
                        "category",
                        "tags",
                        "type",
                        "chunks",
                        "priority",
                        "usage",
                        "wait",
                        "status",
                        "total",
                        "done",
                        "dispatched",
                        "error",
                        "priority",
                        "exception",
                        "started",
                        "finished"
                    )

        handler.server.balance()

        for job in handler.server.jobs:
            results = job.framesStatus()

            time_finished = job.time_finished
            time_started = job.time_started

            rowTable(
                        """<button title="cancel job" onclick="cancel_job('%s');">X</button>""" % job.id +
                        """<button title="pause job" onclick="request('/pause_%s', null);">P</button>""" % job.id +
                        """<button title="reset all frames" onclick="request('/resetall_%s_0', null);">R</button>""" % job.id,
                        job.id,
                        link(job.name, "/html/job" + job.id),
                        job.category if job.category else "<i>None</i>",
                        ";".join(sorted(job.tags)) if job.tags else "<i>None</i>",
                        "%s [%s]" % (netrender.model.JOB_TYPES[job.type], netrender.model.JOB_SUBTYPES[job.subtype]),
                        str(job.chunks) +
                        """<button title="increase chunks size" onclick="request('/edit_%s', &quot;{'chunks': %i}&quot;);">+</button>""" % (job.id, job.chunks + 1) +
                        """<button title="decrease chunks size" onclick="request('/edit_%s', &quot;{'chunks': %i}&quot;);" %s>-</button>""" % (job.id, job.chunks - 1, "disabled=True" if job.chunks == 1 else ""),
                        str(job.priority) +
                        """<button title="increase priority" onclick="request('/edit_%s', &quot;{'priority': %i}&quot;);">+</button>""" % (job.id, job.priority + 1) +
                        """<button title="decrease priority" onclick="request('/edit_%s', &quot;{'priority': %i}&quot;);" %s>-</button>""" % (job.id, job.priority - 1, "disabled=True" if job.priority == 1 else ""),
                        "%0.1f%%" % (job.usage * 100),
                        "%is" % int(time.time() - job.last_dispatched) if job.status != netrender.model.JOB_FINISHED else "N/A",
                        job.statusText(),
                        len(job),
                        results[netrender.model.FRAME_DONE],
                        results[netrender.model.FRAME_DISPATCHED],
                        str(results[netrender.model.FRAME_ERROR]) +
                        """<button title="reset error frames" onclick="request('/reset_%s_0', null);" %s>R</button>""" % (job.id, "disabled=True" if not results[netrender.model.FRAME_ERROR] else ""),
                        "yes" if handler.server.balancer.applyPriorities(job) else "no",
                        "yes" if handler.server.balancer.applyExceptions(job) else "no",
                        time.ctime(time_started) if time_started else "Not Started",
                        time.ctime(time_finished) if time_finished else "Not Finished"
                    )

        endTable()

        output("<h2>Slaves</h2>")

        startTable()
        headerTable("name", "address", "tags", "last seen", "stats", "job")

        for slave in handler.server.slaves:
            rowTable(slave.name, slave.address[0], ";".join(sorted(slave.tags)) if slave.tags else "<i>All</i>", time.ctime(slave.last_seen), slave.stats, link(slave.job.name, "/html/job" + slave.job.id) if slave.job else "None")
        endTable()

        output("<h2>Configuration</h2>")

        output("""<button title="remove all jobs" onclick="clear_jobs();">CLEAR JOB LIST</button>""")

        output("<br />")

        output(link("new interface", "/html/newui"))

        startTable(caption = "Rules", class_style = "rules")

        headerTable("type", "enabled", "description", "limit")

        for rule in handler.server.balancer.rules:
            rowTable(
                        "rating",
                        checkbox("", rule.enabled, "balance_enable('%s', '%s')" % (rule.id(), str(not rule.enabled).lower())),
                        rule,
                        rule.str_limit() +
                        """<button title="edit limit" onclick="balance_edit('%s', '%s');">edit</button>""" % (rule.id(), str(rule.limit)) if hasattr(rule, "limit") else "&nbsp;"
                    )

        for rule in handler.server.balancer.priorities:
            rowTable(
                        "priority",
                        checkbox("", rule.enabled, "balance_enable('%s', '%s')" % (rule.id(), str(not rule.enabled).lower())),
                        rule,
                        rule.str_limit() +
                        """<button title="edit limit" onclick="balance_edit('%s', '%s');">edit</button>""" % (rule.id(), str(rule.limit)) if hasattr(rule, "limit") else "&nbsp;"
                    )

        for rule in handler.server.balancer.exceptions:
            rowTable(
                        "exception",
                        checkbox("", rule.enabled, "balance_enable('%s', '%s')" % (rule.id(), str(not rule.enabled).lower())),
                        rule,
                        rule.str_limit() +
                        """<button title="edit limit" onclick="balance_edit('%s', '%s');">edit</button>""" % (rule.id(), str(rule.limit)) if hasattr(rule, "limit") else "&nbsp;"
                    )

        endTable()
        output("</body></html>")

    elif handler.path.startswith("/html/job"):
        handler.send_head(content = "text/html")
        job_id = handler.path[9:]

        head("NetRender")

        output(link("Back to Main Page", "/html"))

        job = handler.server.getJobID(job_id)

        if job:
            output("<h2>Job Information</h2>")

            job.initInfo()

            startTable()

            rowTable("resolution", "%ix%i at %i%%" % job.resolution)

            rowTable("tags", ";".join(sorted(job.tags)) if job.tags else "<i>None</i>")

            rowTable("results", link("download all", resultURL(job_id)))

            endTable()


            if job.type == netrender.model.JOB_BLENDER:
                output("<h2>Files</h2>")

                startTable()
                headerTable("path")

                tot_cache = 0
                tot_fluid = 0
                tot_other = 0

                rowTable(job.files[0].original_path)
                tot_cache, tot_fluid, tot_other = countFiles(job)

                if tot_cache > 0:
                    rowTable("%i physic cache files" % tot_cache, class_style = "toggle", extra = "onclick='toggleDisplay(&quot;.cache&quot;, &quot;none&quot;, &quot;table-row&quot;)'")
                    for file in job.files:
                        if file.filepath.endswith(".bphys"):
                            rowTable(os.path.split(file.filepath)[1], class_style = "cache")

                if tot_fluid > 0:
                    rowTable("%i fluid bake files" % tot_fluid, class_style = "toggle", extra = "onclick='toggleDisplay(&quot;.fluid&quot;, &quot;none&quot;, &quot;table-row&quot;)'")
                    for file in job.files:
                        if file.filepath.endswith((".bobj.gz", ".bvel.gz")):
                            rowTable(os.path.split(file.filepath)[1], class_style = "fluid")

                if tot_other > 0:
                    rowTable("%i other files" % tot_other, class_style = "toggle", extra = "onclick='toggleDisplay(&quot;.other&quot;, &quot;none&quot;, &quot;table-row&quot;)'")
                    for file in job.files:
                        if (
                            not file.filepath.endswith(".bphys")
                            and not file.filepath.endswith((".bobj.gz", ".bvel.gz"))
                            and not file == job.files[0]
                            ):

                            rowTable(file.filepath, class_style = "other")

                endTable()
            elif job.type == netrender.model.JOB_VCS:
                output("<h2>Versioning</h2>")

                startTable()

                rowTable("System", job.version_info.system.name)
                rowTable("Remote Path", job.version_info.rpath)
                rowTable("Working Path", job.version_info.wpath)
                rowTable("Revision", job.version_info.revision)
                rowTable("Render File", job.files[0].filepath)

                endTable()

            if job.blacklist:
                output("<h2>Blacklist</h2>")

                startTable()
                headerTable("name", "address")

                for slave_id in job.blacklist:
                    slave = handler.server.slaves_map.get(slave_id, None)
                    if slave:
                        rowTable(slave.name, slave.address[0])

                endTable()

            output("<h2>Transitions</h2>")

            startTable()
            headerTable("Event", "Time")

            for transition, time_value in job.transitions:
                rowTable(transition, time.ctime(time_value))

            endTable()

            output("<h2>Frames</h2>")

            startTable()

            if job.hasRenderResult():
                headerTable("no", "status", "render time", "slave", "log", "result", "")

                for frame in job.frames:
                    rowTable(
                             frame.number,
                             frame.statusText(),
                             "%.1fs" % frame.time,
                             frame.slave.name if frame.slave else "&nbsp;",
                             link("view log", logURL(job_id, frame.number)) if frame.log_path else "&nbsp;",
                             link("view result", renderURL(job_id, frame.number))  + " [" +
                             tag("span", "show", attr="class='thumb' onclick='showThumb(%s, %i)'" % (job.id, frame.number)) + "]" if frame.status == netrender.model.FRAME_DONE else "&nbsp;",
                             "<img name='thumb%i' title='hide thumbnails' src='' class='thumb' onclick='showThumb(%s, %i)'>" % (frame.number, job.id, frame.number)
                             )
            else:
                headerTable("no", "status", "process time", "slave", "log")

                for frame in job.frames:
                    rowTable(
                             frame.number,
                             frame.statusText(),
                             "%.1fs" % frame.time,
                             frame.slave.name if frame.slave else "&nbsp;",
                             link("view log", logURL(job_id, frame.number)) if frame.log_path else "&nbsp;"
                             )

            endTable()
        else:
            output("no such job")

        output(link("Back to Main Page", "/html"))

        output("</body></html>")

