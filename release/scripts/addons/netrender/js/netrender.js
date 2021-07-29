/*# ##### BEGIN GPL LICENSE BLOCK #####
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
 # ##### END GPL LICENSE BLOCK #####*/

var rulesTableHeader = ["type", "enabled", "descritpiton", "limit", "value"];
var slaveTableHeaderWithJob = ["name", "last_seen", "stats", "address", "tags", "total_done", "total_error", "job"];
var slaveTableHeader = ["name", "last_seen", "stats", "address"];
var jobTableHeader = ["action", "id", "name", "category", "tags", "type", "chunks", "priority", "usage", "wait", "status", "length", "done", "dispatched", "error", "priority r", "exception r"];
var framesTableHeader = ["no", "status", "render time", "slave", "log", "result"];
var JOB_TYPES = ["None", "Blender", "Process", "Versioned"];
var JOB_STATUS_TEXT = ["Waiting", "Paused", "Finished", "Queued"];
var JOB_SUBTYPE = ["None", "BLENDER", "CYCLE"];
var FRAME_STATUS_TEXT = ["Queued", "Dispatched", "Done", "error"];
var JOB_TYPE_NONE = 0;
var JOB_TYPE_BLENDER = 1;
var JOB_TYPE_PROCESS = 2;
var JOB_TYPE_VERSIONED = 3;
var maxFramesPerPage = 10;
var maxItemsperTable = 10;

function setupPage() {
	/*$.themes.init({themeBase: 'http://ajax.googleapis.com/ajax/libs/jqueryui/1.7.2/themes/', icons: '/html/css/images/themes.gif',previews: '/html/css/images/themes-preview.gif'});*/

	$('body').addClass("ui-widget-content");
	setupJobsPanel();
	setupSlavesPanel();
	setupConfigPanel();

	var refreshSlaveData = window.setInterval(updateSlavesData, 5000);
	var refreshJobsData = window.setInterval(updateJobsData, 5000);
}

function setupJobsPanel() {
	createPanelwidget("body", "jobs", "Jobs", "");
	updateJobsData();

}

function updateJobsData() {
	$.ajax({
		type : 'GET',
		url : '/html/jobs',
		dataType : 'json',
		contentType : 'application/json',
		success : function(jobs) {
			changeJobsTable(jobs);
		}
	});
}

function changeJobsTable(jobs) {

	function celladd(name, row) {

		if(name == "category") {
			if(row.category === "") {
				return "None";
			}
		}

		if(name == "tags") {
			retstr = "[ ";
			for(var i = 0; i < row.tags.length; i++) {
				retstr = retstr + row.tags[i] + " ";
			}
			retstr = retstr + "]";
			return retstr;
		}

		if(name == "usage") {
			return Math.floor(row.usage * 100) + "%";
		}

		if(name == "type") {
			return JOB_TYPES[row.type] + "[" + row.render + "]";
		}

		if(name == "status") {
			return JOB_STATUS_TEXT[row.status];
		}
		if(name == "priority r") {
			if(row.p_rule) {
				return "yes";
			} else {
				return "no";
			}
		}
		if(name == "exception r") {
			if(row.e_rule) {
				return "yes";
			} else {
				return "no";
			}
		}
		if(name == "name") {
			retstr = '<button title="Show Job Info" onclick="showJob(' + "'" + row.id + "'" + ',' + "'" + row.name + "'" + ");" + '"' + ">" + row.name + "</button>";
			return retstr;
		}

		if(name == "action") {

			if(row.status != 2) {
				disabled = "";
			} else {
				disabled = "disabled";
			}
			retstr = '<button ' + disabled + ' title="Pause Job" onclick="pause_job(' + row.id + ');">P</button>';

			if(row.error > 0) {
				disabled = "";

			} else {
				disabled = "disabled";
			}
			retstr += '<button ' + disabled + ' title="Reset Frame" onclcik="reset_job_frames(' + row.id + ')">R</button><button title="Remove Job" onclick="cancel_job(' + row.id + ')">X</button>';
			return retstr;
		}
		if(name == "chunks") {
			retstr = '<button title="increase chunks size" onclick="changeJobChunk(' + row.id + ',' + (row.chunks + 1) + ');">+</button>' + " " + row.chunks + " ";
			if(row.chunks > 1) {
				disabled = '';
			} else {
				disabled = 'disabled';
			}
			retstr += '<button ' + disabled + ' title="decrease chunks size" onclick="changeJobChunk(' + row.id + ',' + (row.chunks - 1) + ');">-</button>';
			return retstr;
		}
		if(name == "priority") {
			retstr = '<button title="increase job priority" onclick="changeJobPriority(' + row.id + ',' + (row.priority + 1) + ');">+</button>' + " " + row.priority + " ";
			if(row.priority > 1) {
				disabled = "";
			} else {
				disabled = 'disabled';
			}
			retstr += '<button ' + disabled + ' title="decrease job priority" onclick="changeJobPriority(' + row.id + ',' + (row.priority - 1) + ');">-</button>';
			return retstr;
		}
		if(name == "wait") {
			if(row.wait != "N/A") {
				return secondsToHms(row.wait);
			}
		}
		if(name == "error") {
			if(row.error > 0) {
				disabled = "";
			} else {
				disabled = 'disabled';
			}
			retstr = '<button ' + disabled + ' title="reset error frames" onclick="reset_error_frames(' + row.id + ');">R</button>' + ' ' + row.error;
			return retstr;
		}

		return row[name];
	}


	$("#b_jobCancel").remove();
	createTable("#jobs_Panelcontent", "jobsTable", jobTableHeader, jobs, celladd);
	$("#jobsTable button").button();

	if(jobs.length) {
		$("#jobs_Panelcontent").append('<button id="b_jobCancel" title="clear all jobs" onclick="clear_jobs();">Cancel all jobs</button>');
		$("#b_jobCancel").button();
	}

}

function setupSlavesPanel() {
	createPanelwidget("body", "slaves", "Slaves", "");
	updateSlavesData();

}


$("#slavesTable button").button();

function updateSlavesData() {
	$.ajax({
		type : 'GET',
		url : '/html/slaves',
		dataType : 'json',
		contentType : 'application/json',
		success : function(slaves) {
			changeSlaveTable(slaves);
		}
	});
}

function changeSlaveTable(slaves) {

	createTable("#slaves_Panelcontent", "slavesTable", slaveTableHeaderWithJob, slaves, cellSlaveTable);
	$("#slavesTable button").button();

}

function setupConfigPanel() {
	function confTab(name) {
		if(name == "Rules") {
			updateConfigData();
			return "";
		}
		if(name == "Interface") {
			$("#Interface").append('<a id="oldinterface" href="/">');
			$("#oldinterface").html('Simple Interface');
			$("#oldinterface").button();
			$("#Interface").append('<div>Select the jquery theme you want to use</div>');
            $("#Interface").append('<div id="switcher">');           
            $('#switcher').themeswitcher();
 
		}
		return "";
	}

	createPanelwidget("body", "configure", "Configuration", "");
	createTabswidget("#configure_Panelcontent", "conf", [{
		name : "Rules",
		f_content : confTab
	}, {
		name : "Interface",
		f_content : confTab
	}]);
	updateConfigData();
	/*$("#themeselector").themes({compact: false});*/
}

function updateConfigData() {
	$.ajax({
		type : 'GET',
		url : '/html/rules',
		dataType : 'json',
		contentType : 'application/json',
		success : function(rules) {
			changeConfigureTable(rules);
		}
	});
}

function changeConfigureTable(rules) {
	var checked = {
		"true" : "checked",
		"false" : ""
	};

	function celladd(name, row) {

		if(name == "enabled") {
			retstr = '<input type="checkbox" title="" ' + checked[row.enabled];
			retstr = retstr + ' onclick="balance_enable(';
			retstr = retstr + "'" + row.id + "'," + "'" + (!row.enabled) + "')" + '" >';
			return retstr;
		}

		if(name == "limit") {
			return row.limit_str;
		}
		if(name == "value") {
			if(row.editable) {
				retstr = '<button title="edit limit" onclick="balance_edit(' + "'" + row.id + "'" + ',' + "'" + row.limit + "'" + ");" + '"' + ">edit</button>";
				return retstr;
			}
			return "";
		}

		if(name != "id") {
			return row[name];
		}
	}

	createTable("#Rules", "rulesTable", rulesTableHeader, rules, celladd);
	$("#rulesTable button").button();
}

function showJob(id, name) {

	var job = {};
    
	function general(tab_name) {
		var cumulate_rendertime = 0;
		$.each(job.frames, function(index, frame) {
			cumulate_rendertime += frame.time;
		});
		var info = [new namevalue("resolution", job.resolution[0] + 'x' + job.resolution[1] + ' at ' + job.resolution[2] + '%'), new namevalue("tags", job.tags), new namevalue("result", getresult(id)), new namevalue("frames", job.frames.length), new namevalue("status", job.status), new namevalue("job name", job.name), new namevalue("type", job.type), new namevalue("render", job.subtype), new namevalue("render time", secondsToHms(job.wktime)), new namevalue("cumulate render time", secondsToHms(cumulate_rendertime))];

		function cellview(name, row) {

			if(name == "name") {
				return '<b>' + row.name + ':</b>';
			}
			if(name == "value") {
				switch (row.name) {
					case "tags":
						retstr = "[ ";
						for(var i = 0; i < row.value.length; i++) {
							retstr = retstr + row.value[i] + " ";
						}
						retstr = retstr + "]";
						return retstr;
					case "result":
						return '<a title="load result" href="' + row.value + '">load</a>';
					case "status":
						return JOB_STATUS_TEXT[job.status];
					case "type":
						return JOB_TYPES[job.type];
					case "render":
						return job.render;

					default:
						return row.value;
				}
			}

		}

		createTable("#" + tab_name, "generalTable", ["name", "value"], info, cellview, false);
		return "";
	}

	function files(tab_name) {

		function fileJobCache() {
			createFilesTable("#JobCache_Panelcontent", id, "cache");
			return "";
		}

		function fileJobFluid() {
			createFilesTable("#JobFluid_Panelcontent", id, "fluid");
			return "";
		}

		function fileJobOther() {
			createFilesTable("#JobOthers_Panelcontent", id, "other");
			return "";
		}

		function filePath(data) {
			$("#JobPath_Panelcontent").html(data[0].filepath);
		}

		function showFilesBlenderJob() {

			createPanelwidget("#" + tab_name, "JobPath", "Job Path", function() {
				return "";
			});
			$.getJSON("/html/blendfile_" + id, null, filePath);
			if(job.totcache) {
				createPanelwidget("#" + tab_name, "JobCache", "Physic cache files", fileJobCache);
			}
			if(job.totfluid) {
				createPanelwidget("#" + tab_name, "JobFluid", "Fluid cache Files", fileJobFluid);
			}
			if(job.totother) {
				createPanelwidget("#" + tab_name, "JobOthers", "Other Files", fileJobOther);
			}

		}

		function showFilesVersioned() {
			function cellview(name, row) {
				if(name == "name") {
					return '<b>' + row.name + ':</b>';
				}

				if(name == "value") {
					return row.value;
				}
			}

			var info = [new namevalue("system", job.version_info.system.name), new namevalue("Remote Path", job.version_info.rpath), new namevalue("Working Path", job.version_info.wpath), new namevalue("Revision", job.version_info.revision), new namevalue("Render File", job.files[0].filepath)];
		}

		switch(job.type) {
			case JOB_TYPE_BLENDER:
				showFilesBlenderJob();

				break;
			case JOB_TYPE_VERSIONED:
				showFilesVersioned();
				break;
			default:
				return tab_name;
		}
		return "";
	}

	function frames(tab_name) {

		function frameCell(name, frame) {
			if(name == "no") {
				return frame.number;
			}
			if(name == "status") {
				return FRAME_STATUS_TEXT[frame.status];
			}
			if(frame.status) {
				if(name == "render time") {
					return Math.floor(frame.time) + "s";
				}
				if(name == "slave") {
					if(frame.slave) {
						return frame.slave.name;
					} else {
						return "none";
					}
				}
				if(name == "log") {
					return '<a href="/log_' + id + '_' + frame.number + '.log" target="_blank">view log</a>';
				}
				if(frame.status > 1) {
					if(name == "result") {
						return '<a href="/render_' + id + '_' + frame.number + '.exr" target="_blank">view result</a>';
					}
				}

			}
			return "N/A";
		}

		createPagedTable("#" + tab_name, "framesTable", framesTableHeader, job.frames, frameCell, 10, "next frames page", "previous frames page");
		return "";
	}

	function thumbs(tab_name) {

		function img_src(img_num) {
			return '/thumb_' + id + '_' + job.frames[img_num].number + '.jpg';
		}

		function img_info(img_num) {
			return "<b>Job:</b>" + job.name + '  <b>frame:</b>' + job.frames[img_num].number;
		}

		createImgViewer("#" + tab_name, img_src, job.frames.length, img_info);

	}

	function slaves(tab_name) {

		function createBlackList(data) {
			function blacklist() {
				if(data.length === 0) {
					return "None";
				}
				createPagedTable("#BlackList_Panelcontent", "blacklistTable", slaveTableHeader, data, cellSlaveTable, maxItemsperTable, "next slaves page", "previous slaves page");
				return "";
			}

			createPanelwidget('#' + tab_name, "BlackList", "Black Listed slaves", blacklist);
		}

		function createSlaveList(data) {
			function slavelist() {
				if(data.length === 0) {
					return "None";
				}
				createPagedTable("#SlavesList_Panelcontent", "slaveslistTable", slaveTableHeader, data, cellSlaveTable, maxItemsperTable, "next slaves page", "previous slaves page");
				return "";
			}

			createPanelwidget('#' + tab_name, "SlavesList", "Currently assigned slaves", slavelist);
		}


		$.getJSON("/html/blacklist_" + id, null, createBlackList);
		$.getJSON("/html/slavesjob_" + id, null, createSlaveList);

		return "";
	}

	function onCancel(dlg) {
		$(dlg).dialog("close");
		$(dlg).remove();
	}

	function jobDialog(dlg) {
		$("#showjobs").remove();
		var tab_descript = [{
			name : "General",
			f_content : general
		}, {
			name : "Files",
			f_content : files
		}, {
			name : "Frames",
			f_content : frames
		}, {
			name : "Thumbs",
			f_content : thumbs
		}, {
			name : "slaves",
			f_content : slaves

		}];

		createTabswidget(dlg, "showjobs", tab_descript);

		return "";
	}

	function retJobData(data) {
		job = data[0];
		$("#JobInfo" + id).remove();
		DialogWidget('body', 'JobInfo' + id, "Job Information", "", "Go Back", null, onCancel, jobDialog, true, 700, 800);
		$("#General a").button();
	}


	$.getJSON("/html/job_" + id, null, retJobData);

}
