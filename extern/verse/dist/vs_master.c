/*
 * Master server communication code.
*/

#include <stdio.h>
#include <string.h>

#include "verse.h"
#include "v_util.h"
#include "vs_server.h"

#define	MASTER_SERVER_PERIOD	(60.0)	/* Period between ANNOUNCE to master server, in seconds. */

static struct {
	boolean		enabled;
	boolean		started;
	const char	*master;
	char		desc[64];
	const char	*tags;
	VUtilTimer	timer;
} server_info;

#define	LEFT(d)	(sizeof server_info.desc - (d - server_info.desc) - 1)

void vs_master_set_enabled(boolean enabled)
{
	server_info.enabled = enabled;
}

const char * vs_master_get_address(void)
{
	return server_info.master;
}

void vs_master_set_address(const char *address)
{
	server_info.master = address;
}

void vs_master_set_desc(const char *desc)
{
	const char	*src = desc;
	char		*dst = server_info.desc;

	for(; *src != '\0' && LEFT(dst) > 0;)
	{
		if(*src == '"')
		{
			if(LEFT(dst) < 2)
				break;
			*dst++ = '\\';
		}
		else if(*src == '\\')
		{
			if(LEFT(dst) < 2)
				break;
			*dst++ = '\\';
		}
		*dst++ = *src++;
	}
	*dst = '\0';
}

void vs_master_set_tags(const char *tags)
{
	server_info.tags = tags;	/* This needs more protection, instead of relying on the master server. */
}

void vs_master_update(void)
{
	if(!server_info.enabled || server_info.master == NULL)
		return;

	if(!server_info.started)
	{
		v_timer_start(&server_info.timer);
		v_timer_advance(&server_info.timer, MASTER_SERVER_PERIOD);
		server_info.started = TRUE;
		return;
	}
	if(v_timer_elapsed(&server_info.timer) < MASTER_SERVER_PERIOD)
		return;
	verse_send_ping(server_info.master, "MS:ANNOUNCE");
	v_timer_start(&server_info.timer);
/*	printf("MS:ANNOUNCE sent to %s\n", server_info.master);*/
}

/* Check if a description request, of the form "A,B,C,...,D" includes the given keyword. This needs to
 * do more than just a simple strstr(), since the keyword may be a prefix. Shades of OpenGL extensions.
*/
static int desc_has_keyword(const char *desc, const char *keyword)
{
	const char	*ptr;

	if(desc == NULL || *desc == '\0')	/* Quick-check for empty description. */
		return 0;

	if((ptr = strstr(desc, keyword)) != NULL)
	{
		size_t	kl = strlen(keyword);

		return ptr[kl] == ',' || ptr[kl] == '\0';
	}
	return 0;
}

static int keyword_fits(size_t used, size_t max, const char *key, const char *value)
{
	size_t	vsize = 0;

	if(key != NULL && value != NULL)
		vsize += 1 + strlen(key) + 1 + 1 + strlen(value) + 1;

	return max - 1 - used >= vsize;
}

static char * append_desc(char *buf, const char *key, const char *value)
{
	return buf + sprintf(buf, " %s=\"%s\"", key, value);
}

void vs_master_handle_describe(const char *address, const char *message)
{
	char	desc[1380] = "DESCRIPTION", *put = desc + 11;

	if(desc_has_keyword(message, "DE") && server_info.desc != NULL && keyword_fits(put - desc, sizeof desc, "DE", server_info.desc))
		put = append_desc(put, "DE", server_info.desc);
	if(desc_has_keyword(message, "TA") && server_info.tags != NULL && keyword_fits(put - desc, sizeof desc, "TA", server_info.tags))
		put = append_desc(put, "TA", server_info.tags);
	verse_send_ping(address, desc);
}
