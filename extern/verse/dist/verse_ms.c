/*
 * A helper library to send and parse master server pings. See the relevant
 * header for details.
 * 
 * This code was written in 2006 by Emil Brink. It is released as public domain.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "verse.h"
#include "verse_ms.h"

/* Build and send a MS:GET packet. */
void verse_ms_get_send(const char *address, int fields, const char *tags)
{
	char	req[128];

	strcpy(req, "MS:GET IP=");
	if(fields & VERSE_MS_FIELD_DESCRIPTION)
		strcat(req, "DE");
	if(tags != NULL)
	{
		strcat(req, " TA=");
		strcat(req, tags);
	}
	verse_send_ping(address, req);
}

/* Skip assign, i.e. "NAME=" string, at <msg>. Stores name into <put>, and then updates
 * it. Returns NULL on parse error, in which case the <put> pointer is not advanced.
*/
static const char * skip_assign(char **put, const char *msg)
{
	if(isalpha(*msg))
	{
		char	*p = put != NULL ? *put : NULL;

		if(p != NULL)
			*p++ = *msg;
		msg++;
		while(*msg && (isalnum(*msg) || *msg == '_'))
		{
			if(p != NULL)
				*p++ = *msg;
			msg++;
		}
		if(*msg == '=')
		{
			if(p != NULL)
				*p++ = '\0';
			if(put != NULL)
				*put = p;
			return msg + 1;
		}
	}
	return NULL;
}

/** Skip value at <msg>, optionally storing de-quoted version through <put>,
 * which is advanced. Returns NULL on parse error, without updating <put>.
*/
static const char * skip_value(char **put, const char *msg)
{
	char	*p = (put != NULL) ? *put : NULL;

	if(*msg == '"')
	{
		msg++;
		while(*msg != '\0' && *msg != '"')
		{
			if(*msg == '\\')
			{
				if(msg[1] != '\0')
	 				msg++;
				else
					return NULL;
			}
			if(p != NULL)
				*p++ = *msg;
			msg++;
		}
		if(*msg == '"')
		{
			if(p != NULL)
				*p++ = '\0';
			if(put != NULL)
				*put = p;
			msg++;
			if(*msg == '\0' || isspace(*msg))
				return msg;
		}
		return NULL;
	}
	while(*msg && !isspace(*msg))
	{
		if(*msg == '"')
			return NULL;
		if(p != NULL)
			*p++ = *msg;
		msg++;
	}
	if(p != NULL)
		*p++ = '\0';
	if(put != NULL)
		*put = p;
	return msg;
}

static const char * put_field(VMSField *field, char **put, const char *src)
{
	const char	*ptr;
	char		*base = *put;

	if((ptr = skip_assign(put, src)) != NULL && ptr - src > 1)
	{
		field->name = base;
		src = ptr;
		base = *put;
		if((ptr = skip_value(put, src)) != NULL)
		{
			field->value = base;
			return ptr;
		}
	}
	return NULL;
}

static int cmp_fields(const void *a, const void *b)
{
	return strcmp(((const VMSField *) a)->name, ((const VMSField *) b)->name);
}

VMSServer ** verse_ms_list_parse(const char *msg)
{
	const char	*word[384];	/* Takes quite a lot of stack space. */
	const char	*ptr;
	char		*put;
	size_t		num_word = 0, i, j, num_ip = 0, num_field, space = 0;
	VMSServer		**desc, *next;
	VMSField	*field;

	if(strncmp(msg, "MS:LIST", 7) == 0)
		msg += 7;
	if(*msg != ' ')
		return NULL;

	/* Step one: split the string into words, at whitespace. Split is aware
	 * of quoting rules for value assignment, this is crucial. This split is
	 * non-invasive, meaning each "word" will be a suffix.
	*/
	while(*msg)
	{
		while(isspace(*msg))
			msg++;
		ptr = skip_assign(NULL, msg);
		if(ptr != NULL)
		{
			space += ptr - msg;
			word[num_word++] = msg;
			msg = ptr;
			ptr = skip_value(NULL, msg);
			if(ptr == NULL)
			{
				fprintf(stderr, "Parse error\n");
				return NULL;
			}
			space += ptr - msg + 1;
			msg = ptr;
		}
		else if(*msg != '\0')
		{
			fprintf(stderr, "Parse error\n");
			return NULL;
		}
	}
	/* Now, count how many words begin with "IP=". */
	for(i = 0; i < num_word; i++)
	{
		if(strncmp(word[i], "IP=", 3) == 0)
			num_ip++;
	}
/*	printf("found %u IPs, %u bytes\n", num_ip, space);
	printf("%u IP and %u words -> %u fields total\n", num_ip, num_word, num_word - num_ip);
*/	num_field = num_word - num_ip;
	/* Allocate the descriptions. */
/*	printf("allocating %u bytes\n", (num_ip + 1) * (sizeof *desc) + num_ip * sizeof **desc + num_field * sizeof (VMSField) + space);
	printf(" %u for pointers, %u for structs, %u for fields, %u string\n",
	       (num_ip + 1) * (sizeof *desc), num_ip * sizeof **desc, num_field * sizeof (VMSField), space);
*/	desc = malloc((num_ip + 1) * (sizeof *desc) + num_ip * sizeof **desc + num_field * sizeof (VMSField) + space);
	next = (VMSServer *) (desc + (num_ip + 1));
/*	printf("desc store at %u\n", (char *) next - (char *) desc);*/
	field = (VMSField *) (next + num_ip);
/*	printf("field store at %u\n", (char *) field - (char *) desc);*/
	put  = (char *) (field + num_field);
/*	printf("string store at %u\n", put - (char *) desc);*/
	for(i = j = 0; i < num_word;)
	{
		if(strncmp(word[i], "IP=", 3) == 0)
		{
			desc[j] = next;
			next->ip = put;
			ptr = skip_value(&put, word[i] + 3);
			next->num_fields = 0;
			next->field = field;
			for(i++; i < num_word && strncmp(word[i], "IP=", 3) != 0; i++, next->num_fields++, field++)
				put_field(&next->field[next->num_fields], &put, word[i]);
			if(next->num_fields > 0)	/* Sort the fields, for binary search later. */
				qsort(next->field, next->num_fields, sizeof *next->field, cmp_fields);
			j++;
			next++;
		}
		else
			i++;
	}
	desc[j] = NULL;
	return desc;
}

/* A binary search, exploiting that the fields are sorted. */
static const VMSField * field_find(const VMSServer *ms, const char *name)
{
	int	lo, hi, mid, rel;

	if(ms == NULL || name == NULL)
		return NULL;
	lo = 0;
	hi = ms->num_fields;
	while(lo <= hi)
	{
		mid = (lo + hi) / 2;
		rel = strcmp(name, ms->field[mid].name);
		if(rel == 0)
			return &ms->field[mid];
		if(rel < 0)
			hi = mid - 1;
		else
			lo = mid + 1;
	}
	return NULL;
}

int verse_ms_field_exists(const VMSServer *ms, const char *name)
{
	if(ms == NULL || name == NULL)
		return 0;
	return field_find(ms, name) != NULL;
}

const char * verse_ms_field_value(const VMSServer *ms, const char *name)
{
	const VMSField	*f;

	if((f = field_find(ms, name)) != NULL)
		return f->value;
	return NULL;
}

#if defined VERSE_MS_STANDALONE

int main(void)
{
	VMSServer	**servers = verse_ms_list_parse("MS:LIST IP=127.0.0.1:4951 DE=\"A test server, mainly for Eskil\" COOL=yes BACKUP=daily LANG=sv_SE "
						"IP=130.237.221.74 DE=\"Test server on a puny laptop\" COOL=yes DORKY=no OPEN=absolutely "
						"IP=127.0.0.1:5151 DE=\"This is a back slash: '\\\\', cool huh?\" "
						"IP=127.0.0.1:6676 DE=\"a quote looks like this: \\\"\"  IP=127.0.0.1:1122 ");

	if(servers != NULL)
	{
		int	i, j;

		printf("Server info:\n");
		for(i = 0; servers[i] != NULL; i++)
		{
			printf("%u: IP=%s\n", i, servers[i]->ip);
			for(j = 0; j < servers[i]->num_fields; j++)
				printf(" %s='%s'\n", servers[i]->field[j].name, servers[i]->field[j].value);
		}
		free(servers);
	}
	return EXIT_SUCCESS;
}

#endif		/* VERSE_MS_STANDALONE */
