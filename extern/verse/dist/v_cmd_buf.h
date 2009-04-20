/*
**
*/

#include <stdlib.h>

#include "verse_header.h"
#include "v_pack.h"

#define	V_NOQ_MAX_PACKET_SIZE	1500

typedef enum {
	VCMDBS_10 = 0,
	VCMDBS_20 = 1,
	VCMDBS_30 = 2,
	VCMDBS_80 = 3,
	VCMDBS_160 = 4,
	VCMDBS_320 = 5,
	VCMDBS_1500 = 6,
	VCMDBS_COUNT = 7
} VCMDBufSize;

typedef struct {
	void			*next;
	uint32			packet;
	unsigned int	address_size;
	unsigned int	address_sum;
	VCMDBufSize		buf_size;
	unsigned int	size;
} VCMDBufHead;

typedef struct {
	VCMDBufHead	head;
	uint8		buf[10];
} VCMDBuffer10;

typedef struct {
	VCMDBufHead	head;
	uint8		buf[20];
} VCMDBuffer20;

typedef struct {
	VCMDBufHead head;
	uint8		buf[30];
} VCMDBuffer30;

typedef struct {
	VCMDBufHead	head;
	uint8		buf[80];
} VCMDBuffer80;

typedef struct {
	VCMDBufHead	head;
	uint8		buf[160];
} VCMDBuffer160;

typedef struct {
	VCMDBufHead	head;
	uint8		buf[320];
} VCMDBuffer320;


typedef struct {
	VCMDBufHead	head;
	uint8		buf[1500];
} VCMDBuffer1500;

extern VCMDBufHead	*v_cmd_buf_allocate(VCMDBufSize buf_size);
extern void		v_cmd_buf_free(VCMDBufHead *head);

extern void		v_cmd_buf_set_size(VCMDBufHead *head, unsigned int size);
extern void		v_cmd_buf_set_address_size(VCMDBufHead *head, unsigned int size);
extern void		v_cmd_buf_set_unique_address_size(VCMDBufHead *head, unsigned int size);
extern boolean		v_cmd_buf_compare(VCMDBufHead *a, VCMDBufHead *b);
