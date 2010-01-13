lastFrame = -1
maxFrame = -1
minFrame = -1

function request(url, data)
{
	xmlhttp = new XMLHttpRequest();
	xmlhttp.open("POST", url, false);
	xmlhttp.send(data);
	window.location.reload()	
}

function edit(id, info)
{
	request("/edit_" + id, info)
}

function clear_jobs()
{
	var r=confirm("Also delete files on master?");
	
	if (r==true) {
		request('/clear', "{'clear':True}");
	} else {
		request('/clear', "{'clear':False}");
	}
}

function cancel_job(id)
{
	var r=confirm("Also delete files on master?");
	
	if (r==true) {
		request('/cancel_' + id, "{'clear':True}");
	} else {
		request('/cancel_' + id, "{'clear':False}");
	}
}

function balance_edit(id, old_value)
{
	var new_value = prompt("New limit", old_value);
	if (new_value != null && new_value != "") {
		request("/balance_limit", "{" + id + ":'" + new_value + "'}");
	}
}

function balance_enable(id, value)
{
	request("/balance_enable", "{" + id + ":" + value + "}");
}

function showThumb(job, frame)
{
	if (lastFrame != -1) {
		if (maxFrame != -1 && minFrame != -1) {
			if (frame >= minFrame && frame <= maxFrame) {
				for(i = minFrame; i <= maxFrame; i=i+1) {
					toggleThumb(job, i);
				}
				minFrame = -1;
				maxFrame = -1;
				lastFrame = -1;
			} else if (frame > maxFrame) {
				for(i = maxFrame+1; i <= frame; i=i+1) {
					toggleThumb(job, i);
				}
				maxFrame = frame;
				lastFrame = frame;
			} else {
				for(i = frame; i <= minFrame-1; i=i+1) {
					toggleThumb(job, i);
				}
				minFrame = frame;
				lastFrame = frame;
			}
		} else if (frame == lastFrame) {
			toggleThumb(job, frame);
		} else if (frame < lastFrame) {
			minFrame = frame;
			maxFrame = lastFrame;

			for(i = minFrame; i <= maxFrame-1; i=i+1) {
				toggleThumb(job, i);
			}
			lastFrame = frame;
		} else {
			minFrame = lastFrame;
			maxFrame = frame;

			for(i = minFrame+1; i <= maxFrame; i=i+1) {
				toggleThumb(job, i);
			}
			lastFrame = frame;
		}
	} else {
		toggleThumb(job, frame);
	}
}

function toggleThumb(job, frame)
{
	img = document.images["thumb" + frame];
	url = "/thumb_" + job + "_" + frame + ".jpg"

	if (img.style.display == "block") {
		img.style.display = "none";
		img.src = "";
		lastFrame = -1;
	} else {
		img.src = url;
		img.style.display = "block";
		lastFrame = frame;
	}
}

function returnObjById( id )
{
    if (document.getElementById)
        var returnVar = document.getElementById(id);
    else if (document.all)
        var returnVar = document.all[id];
    else if (document.layers)
        var returnVar = document.layers[id];
    return returnVar;
}

function toggleDisplay( className, value1, value2 )
{
	style = getStyle(className)
	
	if (style.style["display"] == value1) {
		style.style["display"] = value2;
	} else {
		style.style["display"] = value1;
	}
}

function getStyle(className) {
    var classes = document.styleSheets[0].rules || document.styleSheets[0].cssRules
    for(var x=0;x<classes.length;x++) {
        if(classes[x].selectorText==className) {
        	return classes[x]; 
        }
    }
}