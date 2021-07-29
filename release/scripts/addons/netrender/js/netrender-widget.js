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

var panelHeaderClass = "ui-widget ui-state-active ui-corner-top";
var panelContentClass = "ui-widget ui-widget-content ui-corner-bottom";
var tableBorder = "ui-widget-content ui-corner-bottom ui-corner-top";

function toggleme() {
	$(this).next().toggle("fast");
}

function hoverInPanel() {
	$(this).removeClass("ui-state-active");
	$(this).addClass("ui-state-hover");
}

function hoverOutPanel() {
	$(this).addClass("ui-state-active");
	$(this).removeClass("ui-state-hover");
}

function hoverInHeader() {

	$(this).addClass("ui-state-hover");
}

function hoverOutHeader() {

	$(this).removeClass("ui-state-hover");
}

function addTableHeaderbyList(table, header) {
	$(table).append("<tr>");
	$.each(header, function(index, h) {
		$(table + ' tr:last').append('<th>');
		$(table + ' th:last').append(h);
		$(table + ' th:last').hover(hoverInHeader, hoverOutHeader);

	});
}

function addTableHeaderbyObj(table, header) {
	$(table).append("<tr>");
	for(h in header) {
		$(table + ' tr:last').append('<th>');
		$(table + ' th:last').append(h);
		$(table + ' th:last').hover(hoverInHeader, hoverOutHeader);
	}
}

function addTableRowByHeader(table, fields, row, cellf) {
	$(table).append("<tr>");
	$.each(fields, function(index, field) {

		$(table + " tr:last").append('<td align="center">');
		$(table + " td:last").html(cellf(field, row));
	});
}

function addTableRowByObj(table, row, cellf) {

	$(table).append("<tr>");

	$.each(row, function(index, val) {
		$(table + " tr:last").append("<td>");
		$(table + " td:last").html(cellf(val, row));
	  

	});
}

/*
 * create a simple pannel widget
 *  parent (string): parent selector
 *  name (string): name used for the panel, this will create two div named name_PanelHead,name_Panelcontent
 *  header (string): name of header
 *  content (function): a callback function to display contente of the panel
 */

function createPanelwidget(parent, name, header, content) {

	$(parent).append($('<div id="' + name + 'Panelhead" class="' + panelHeaderClass + '">'));
	$("#" + name + "Panelhead").click(toggleme);
	$("#" + name + "Panelhead").hover(hoverInPanel, hoverOutPanel);
	$("#" + name + "Panelhead").append(header);
	$(parent).append($('<div id="' + name + '_Panelcontent" class="' + panelContentClass + '">'));
	$("#" + name + "_Panelcontent").append(content);
}

/*
 * create a jquery tabed widget
 * param;
 * 	 parent (string):  parent elemenet selector in form of '#name'
 *   name (string ): name of the widget
 *   tabs_descriptions: array of object { name:"tabs-1", f_content: function for display tab content}
 *                          f_content= function(name){} where name is the tab name
 */

function createTabswidget(parent, name, tabs_descriptions) {
	$(parent).append('<div id="' + name + '">');
	$("#" + name).append("<ul>");
	$.each(tabs_descriptions, function(index, tab) {
		$("ul:last").append('<li>');
		$("li:last").append('<a href="#' + tab.name + '">');
		$("a:last").append(tab.name);
	});
	$.each(tabs_descriptions, function(index, tab) {
		$("#" + name).append('<div id="' + tab.name + '">');
		if(tab.f_content) {
			$("div:last").append(tab.f_content(tab.name));
		} else {
			$("div:last").append(tab.name);
		}
	});

	$("#" + name).tabs();
}

/*
 * create a input value dialog box widget
 * parent (string): parent selector in form "#parent"
 * title (string): title string for the dialog
 * value (string): the current value for the input field
 * f_onchange(function): function what to do when value is ok
 * f_oncancel(function): function what to do when dialog is canceld
 * ismod(boolean): dialog is modal ?
 */
function inputDialogWidget(parent, title, value, f_onchange, f_oncancel, ismodal) {
	$(parent).append('<div id="Inputdialog" title="' + title + '">');
	$("#Inputdialog").append('<input id="value" value="' + value + '">');
	$("#Inputdialog").dialog({
		modal : ismodal,
		buttons : {
			"Change" : function() {
				f_onchange("#Inputdialog");
			},
			"Cancel" : function() {
				f_oncancel("#Inputdialog");
			}
		}

	});
}

/*
 * Generic dialog widget
 */
function DialogWidget(parent, name, title, n_onchange, n_oncancel, f_onchange, f_oncancel, f_content, ismodal, h, w) {
	$(parent).append('<div id="' + name + 'Dialog" title="' + title + '">');
	$("#" + name + "Dialog").append(f_content("#" + name + "Dialog"));
	param = {};
	param.modal = ismodal;
	param.minHeight = h;
	param.minWidth = w;

	if(f_onchange || f_oncancel) {
		param.buttons = {};
	}
	if(f_onchange) {
		param.buttons[n_onchange] = function() {
			f_onchange("#" + name + "Dialog");
		};
	}
	if(f_oncancel) {
		param.buttons[n_oncancel] = function() {
			f_oncancel("#" + name + "Dialog");
		};
	}
	$("#" + name + "Dialog").dialog(param);
}

/*
 * create paged table
 */
function createPagedTable(parent, name, header, objects, f_cell, maxItemPerPage, nTitle, pTitle) {
	var curMin = 0;
	var curMax = maxItemPerPage;

	function nextPage() {
		curMin += maxItemPerPage;
		curMax += maxItemPerPage;
		if(curMax > objects.length) {
			curMax = objects.length;
			curMin = curMax - maxItemPerPage;
		}
		updateTable(curMin, curMax);
	}

	function previousPage() {
		curMin -= maxItemPerPage;
		curMax -= maxItemPerPage;
		if(curMin < 0) {
			curMin = 0;
			curMax = maxItemPerPage;

		}
		updateTable(curMin, curMax);
	}

	function updateTable(start_index, stop_index) {

		createTable(parent, name, header, objects.slice(start_index, stop_index), f_cell);
		if(objects.length > maxItemPerPage) {
			$("#" + name + "_pItemP").remove();
			$(parent).append('<button title="' + pTitle + '" id="' + name + '_pItemP"> < </button>');
			$("#" + name + "_pItemP").click(previousPage);
			$("#" + name + "_pItemP").button();

			$("#" + name + "_nItemP").remove();
			$(parent).append('<button title="' + nTitle + '" id="' + name + '_nItemP"> > </button>');
			$("#" + name + "_nItemP").click(nextPage);
			$("#" + name + "_nItemP").button();
		}
	}

	updateTable(curMin, curMax);
	return name;
}

/* create a simple table
 * parent: parent selector
 * name: name of the table
 * header: array of header
 * object: array of object
 * f_cell; dipslay cell content
 * viewH: if header need to be displayed
 * f_displayrow: function returning if row need to be displayed.
 */
function createTable(parent, name, header, objects, f_cell, viewH, f_displayrow) {

	if(viewH == null) {
		viewH = true;
	}
	if(f_displayrow == null) {
		f_displayrow = function(index, obj) {
			return true;
		};
	}

	$("#" + name).remove();
	$(parent).append('<table id="' + name + '" class="' + tableBorder + '">');
	if(viewH == true) {
		addTableHeaderbyList("#" + name, header);
	}
	$.each(objects, function(index, obj) {
		if(f_displayrow(index, obj)) {
			addTableRowByHeader("#" + name, header, obj, f_cell);
		}
	});

	$("#" + name + " a").button();

}

function createPlayerWidget(parent, buttons) {
	var button_name = ["fbackward", "backward", "play", "stop", "forward", "fforward"];

	function playerCell(name, a_button) {
		if(a_button.name == name) {
			return '<button id="' + name + '" title="' + a_button.title + '"></button>';
		}
		return "";
	}


	$(parent).append('<div align="center" id="playerPanel">');
	$.each(buttons, function(index, a_button) {
		$("#playerPanel").append('<button id="' + a_button.name + '" title="' + a_button.title + '">');
		$('#' + a_button.name).click(a_button.f_click);
		$('#' + a_button.name).button({
			icons : {
				primary : a_button.icon
			},
			text : false
		});
	});
}

function createImgViewer(parent, f_src, numImg, f_imginfo) {
	var currentFrame = 0;
	var playTimer;
	function firstImg() {
		currentFrame = 0;
		updateImg();
	}

	function lastImg() {
		currentFrame = numImg;
		updateImg();
	}

	function nextImg() {
		currentFrame += 1;
		if(currentFrame > numImg) {
			currentFrame = 1;
		}
		updateImg();
	}

	function previousImg() {
		currentFrame -= 1;
		if(currentFrame < 0) {
			currentFrame = numImg;
		}
		updateImg();

	}

	function updateImg() {
		$("#thumbImg").attr('src', f_src(currentFrame));
		updateImageInfo();
	}

	function playImg() {
		playTimer = window.setInterval(nextImg, 2000);
	}

	function stopImg() {
		window.clearInterval(playTimer);
	}

	function updateImageInfo() {
		$('#imageInfo').remove();
		$(parent).append('<div id="imageInfo" align="center">');
		$('#imageInfo').html(f_imginfo(currentFrame));

	}

	var buttons = [new butObj("fbackward", firstImg, "ui-icon-seek-first", "first frame"), new butObj("backward", previousImg, "ui-icon-seek-prev", "previous frame"), new butObj("play", playImg, "ui-icon-play", "start slide show"), new butObj("stop", stopImg, "ui-icon-stop", "stop slide show"), new butObj("forward", nextImg, "ui-icon-seek-next", "next frame"), new butObj("fforward", lastImg, "ui-icon-seek-end", "last frame")];

	$(parent).append('<div id="imageViewer" align="center">');
	$('#imageViewer').append('<img id="thumbImg"></img>');
	updateImg();
	createPlayerWidget('#imageViewer', buttons);

}

function butObj(name, f_click, icon, title) {
	this.name = name;
	this.title = title;
	this.f_click = f_click;
	this.icon = icon;

}

function namevalue(name, value) {
	this.name = name;
	this.value = value;
}

function createFilesTable(parent, job_id, type) {
	function getfile(data) {
		function cell(name, file) {
			if(name == "file") {
				return file.name;
			}
		}

		files = data;
		createPagedTable(parent, type + "FilesTable", ["file"], files, cell, 10, "next files page", "previous files page");
	}


	$.getJSON("/html/" + type + "files_" + job_id, null, getfile);

}

function cellSlaveTable(name, row) {
	if(name == "address") {
		return row.address[0];
	}
	if(name == "tags") {
		if(row.tags.length) {
			retstr = row.tags[0];
			for( i = 1;i < row.tags.length;i++) {
				retstr = retsr + "," + row.tags[i];
			}
			return retstr;
		}

		return "all";
	}

	if(name == "last_seen") {
		return Date(1000 * row.last_seen);
	}
	if(name == "job") {
		if(row.job_name != "None") {
			retstr = '<button title="Show Job Info" onclick="showJob(' + "'" + row.job_id + "'" + ',' + "'" + row.job_name + "'" + ");" + '"' + ">" + row.job_name + "</button>";
			return retstr;
		} else {
			return row.job_name;
		}
	}

	return row[name];
}