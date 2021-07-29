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

function balance_enable(id, value) {
	$.ajax({
		type : 'POST',
		url : '/balance_enable',
		dataType : 'json',
		data : '{"' + id + '":' + value + "}",
		success : updateConfigData

	});
}

function balance_edit(id, old_value) {
	function onChange(dlg) {
		new_value = $(dlg + " input").attr("value");
		$.ajax({
			type : 'POST',
			url : '/balance_limit',
			dataType : 'json',
			data : '{"' + id + '":' + new_value + "}",
			success : updateConfigData

		});
		$(dlg).dialog("close");
		$(dlg).remove();
	}

	function onCancel(dlg) {
		$(dlg).dialog("close");
		$(dlg).remove();
	}

	inputDialogWidget("#Rules", "New Limit", old_value, onChange, onCancel, true);
}

function clear_jobs()
{
        function onChange(dlg)
    {
     $.ajax({
            type : 'POST',
            url : '/clear',
            dataType : 'json',
            data :'{"clear":true}',
            success :updateJobsData()
        });
             
    
    $(dlg).dialog("close");
    $(dlg).remove();
    
    }
    
    function onCancel(dlg)
    {
      $.ajax({
            type : 'POST',
            url : '/clear',
            dataType : 'json',
            data :'{"clear":false}',
            success :updateJobsData()
        });
             
    
    $(dlg).dialog("close");
    $(dlg).remove();    
            
    }
    
    function Content(dlg)
    {
      return "Also delete files on master?" ;
    }
    DialogWidget("body","CancelJobsConfirme","Cancel All Jobs","Yes","No", onChange, onCancel,Content,true,200,200);

}

function cancel_job(id)
{
		
	function onChange(dlg)
	{
	 $.ajax({
			type : 'POST',
			url : '/cancel_'+id,
			dataType : 'json',
			data :'{"clear":true}',
			success :updateJobsData()
		});
			 
	
	$(dlg).dialog("close");
	$(dlg).remove();
  	
	}
	
	function onCancel(dlg)
	{
	  $.ajax({
			type : 'POST',
			url : '/cancel_'+id,
			dataType : 'json',
			data :'{"clear":false}',
			success :updateJobsData()
		});
			 
	
	$(dlg).dialog("close");
	$(dlg).remove();	
      		
	}
	
	function Content(dlg)
	{
	  return "Also delete files on master?"	;
	}
	DialogWidget("body","CancelConfirme","Job Cancelation Confirm","Yes","No", onChange, onCancel,Content,true,200,200);
}


function pause_job(id)
{
		$.ajax({
			type : 'POST',
			url : '/pause_'+id,
			dataType : 'json',
	        success :updateJobsData()

		});
}
function changeJobChunk(id,value)
{
		$.ajax({
			type : 'POST',
			url : '/edit_'+id,
			dataType : 'json',
			data :'{"chunks":'+value+"}",
			success :updateJobsData()
		});	
	
}

function changeJobPriority(id,value)
{
			$.ajax({
			type : 'POST',
			url : '/edit_'+id,
			dataType : 'json',
			data :'{"priority":'+value+"}",
			success :updateJobsData()
		});	
}

function reset_job_frames(id)
{
    $.ajax({
            type : 'POST',
            url : '/resetall_'+id+'_0',
            dataType : 'json',
            success :updateJobsData()
        });
	
}
function reset_error_frames(id)
{
   $.ajax({
            type : 'POST',
            url : '/reset_'+id+'_0',
            dataType : 'json',
            success :updateJobsData()
        });
      
}

function secondsToHms(d) {
    d = Number(d);
    var h = Math.floor(d / 3600);
    var m = Math.floor(d % 3600 / 60);
    var s = Math.floor(d % 3600 % 60);
    return ((h+":") + (m > 0 ? (h > 0 && m < 10 ? "0" : "") + m + ":" : "0:") + (s < 10 ? "0" : "") + s);
}


function getresult(jobid)
{
	return "/result_"+jobid+".zip";
}

