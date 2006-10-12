/*
 * This is Verse Master Server, a small help library to aid application developers
 * make their applications interact with a Verse master server.
 * 
 * There are two steps to the process:
 * 
 * 1) Send a MS:GET request to a master server. This is done by the verse_ms_get_send()
 *    function, which calls verse_send_ping() internally.
 * 
 * 2) Parse any returned MS:LIST packets. This is a two-step process. The application
 *    still owns the ping callback, and will need to check for received pings that
 *    start with MS:LIST, and call the verse_ms_list_parse() function to parse those.
 * 
 * A successfully parsed MS:LIST packet will result in an array of VMSServer pointers
 * being returned. Each VMSServer instance describes one server. Use the provided
 * functions to query each server structure.
 * 
 * The application should call free() on the returned vector, whenever it is done with
 * the data (perhaps after copying it into application-defined data structures).
 * 
 * For a lot more detail about the Verse master server protocol, please see
 * the spec at <http://verse.blender.org/cms/Master_Server__v2.775.0.html>.
 * 
 * This code was written in 2006 by Emil Brink. It is released as public domain.
 * 
*/

#define	VERSE_MS_VERSION	"1.0"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
	const char	*name;		/* Field name. Upper-case. */
	const char	*value;		/* Field value. Fully parsed, might contain spaces. */
} VMSField;

typedef struct {
	const char	*ip;		/* IP address of server, in dotted decimal:port. */
	unsigned int	num_fields;	/* Number of fields of extra info. */
	VMSField	*field;		/* Vector of fields, or NULL if none. */
} VMSServer;

/* Formats and sends a MS:GET ping packet to the master server. The <fields> argument
 * should be a combination of VERSE_MS_FIELD_ mask values. If <tags> is set, it should
 * be a comma-separated set of include/exclude tags, like "a,b,-c,d,-e".
*/
#define	VERSE_MS_FIELD_DESCRIPTION	(1 << 0)
extern void		verse_ms_get_send(const char *address, int fields, const char *tags);

/* Parses a master server response. This will be a string of the form "MS:LIST IP=blah ...",
 * which is split into one struct per IP, and any additional fields parsed (unquoted etc).
 * Returns an array of VMSServer pointers, which is NULL-terminated. Returns NULL if there
 * was a parse error somewhere in the string, no partial success is possible.
*/
extern VMSServer **	verse_ms_list_parse(const char *list);

/* This is the only standard field name, currently. */
#define	VERSE_MS_FIELD_DESCRIPTION_NAME	"DE"	/* Human-readable server description. */

/* Checks wether the given server has a field with the given name. */
extern int		verse_ms_field_exists(const VMSServer *ms, const char *name);

/* Returns the value for the named field in the given server, if present.
 * If not, NULL is returned.
*/
extern const char *	verse_ms_field_value(const VMSServer *ms, const char *name);

#if defined __cplusplus
}
#endif
