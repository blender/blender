function request(url, data) {
	xmlhttp = new XMLHttpRequest();
	xmlhttp.open("POST", url, false);
	xmlhttp.send(data);
	window.location.reload()	
}

function edit(id, info) {
	request("/edit_" + id, info)
}