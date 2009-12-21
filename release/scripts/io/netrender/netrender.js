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