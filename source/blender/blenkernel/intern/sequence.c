
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"

#include "IMB_imbuf.h"

#include "BKE_sequence.h"

/* strip data */

static void free_tstripdata(int len, TStripElem *se)
{
	TStripElem *seo;
	int a;

	seo= se;
	if (!se)
		return;

	for(a=0; a<len; a++, se++) {
		if(se->ibuf) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf = 0;
		}
		if(se->ibuf_comp) {
			IMB_freeImBuf(se->ibuf_comp);
			se->ibuf_comp = 0;
		}
	}

	MEM_freeN(seo);
}

/*
static void new_tstripdata(Sequence *seq)
{
	if(seq->strip) {
		free_tstripdata(seq->strip->len, seq->strip->tstripdata);
		free_tstripdata(seq->strip->endstill, 
				seq->strip->tstripdata_endstill);
		free_tstripdata(seq->strip->startstill, 
				seq->strip->tstripdata_startstill);

		seq->strip->tstripdata= 0;
		seq->strip->tstripdata_endstill= 0;
		seq->strip->tstripdata_startstill= 0;

		if(seq->strip->ibuf_startstill) {
			IMB_freeImBuf(seq->strip->ibuf_startstill);
			seq->strip->ibuf_startstill = 0;
		}

		if(seq->strip->ibuf_endstill) {
			IMB_freeImBuf(seq->strip->ibuf_endstill);
			seq->strip->ibuf_endstill = 0;
		}

		seq->strip->len= seq->len;
	}
}
*/

/* free */

static void seq_free_strip(Strip *strip)
{
	strip->us--;
	if(strip->us>0) return;
	if(strip->us<0) {
		printf("error: negative users in strip\n");
		return;
	}

	if (strip->stripdata) {
		MEM_freeN(strip->stripdata);
	}

	if (strip->proxy) {
		MEM_freeN(strip->proxy);
	}
	if (strip->crop) {
		MEM_freeN(strip->crop);
	}
	if (strip->transform) {
		MEM_freeN(strip->transform);
	}
	if (strip->color_balance) {
		MEM_freeN(strip->color_balance);
	}

	free_tstripdata(strip->len, strip->tstripdata);
	free_tstripdata(strip->endstill, strip->tstripdata_endstill);
	free_tstripdata(strip->startstill, strip->tstripdata_startstill);

	if(strip->ibuf_startstill) {
		IMB_freeImBuf(strip->ibuf_startstill);
		strip->ibuf_startstill = 0;
	}

	if(strip->ibuf_endstill) {
		IMB_freeImBuf(strip->ibuf_endstill);
		strip->ibuf_endstill = 0;
	}

	MEM_freeN(strip);
}

void seq_free_sequence(Sequence *seq)
{
	//XXX Sequence *last_seq = get_last_seq();

	if(seq->strip) seq_free_strip(seq->strip);

	if(seq->anim) IMB_free_anim(seq->anim);
	//XXX if(seq->hdaudio) sound_close_hdaudio(seq->hdaudio);

	/* XXX if (seq->type & SEQ_EFFECT) {
		struct SeqEffectHandle sh = get_sequence_effect(seq);

		sh.free(seq);
	}*/

	//XXX if(seq==last_seq) set_last_seq(NULL);

	MEM_freeN(seq);
}

void seq_free_editing(Editing *ed)
{
	MetaStack *ms;
	Sequence *seq;

	if(ed==NULL)
		return;
	
	//XXX set_last_seq(NULL); /* clear_last_seq doesnt work, it screws up free_sequence */

	SEQ_BEGIN(ed, seq) {
		seq_free_sequence(seq);
	}
	SEQ_END

	while((ms= ed->metastack.first)) {
		BLI_remlink(&ed->metastack, ms);
		MEM_freeN(ms);
	}

	MEM_freeN(ed);
}

/* sequence strip iterator:
 * - builds a full array, recursively into meta strips */

static void seq_count(ListBase *seqbase, int *tot)
{
	Sequence *seq;

	for(seq=seqbase->first; seq; seq=seq->next) {
		(*tot)++;

		if(seq->seqbase.first)
			seq_count(&seq->seqbase, tot);
	}
}

static void seq_build_array(ListBase *seqbase, Sequence ***array, int depth)
{
	Sequence *seq;

	for(seq=seqbase->first; seq; seq=seq->next) {
		seq->depth= depth;

		if(seq->seqbase.first)
			seq_build_array(&seq->seqbase, array, depth+1);

		**array= seq;
		(*array)++;
	}
}

void seq_array(Editing *ed, Sequence ***seqarray, int *tot)
{
	Sequence **array;

	*seqarray= NULL;
	*tot= 0;

	if(ed == NULL)
		return;

	seq_count(&ed->seqbase, tot);

	if(*tot == 0)
		return;

	*seqarray= array= MEM_mallocN(sizeof(Sequence *)*(*tot), "SeqArray");
	seq_build_array(&ed->seqbase, &array, 0);
}

void seq_begin(Editing *ed, SeqIterator *iter)
{
	memset(iter, 0, sizeof(*iter));
	seq_array(ed, &iter->array, &iter->tot);

	if(iter->tot) {
		iter->cur= 0;
		iter->seq= iter->array[iter->cur];
		iter->valid= 1;
	}
}

void seq_next(SeqIterator *iter)
{
	if(++iter->cur < iter->tot)
		iter->seq= iter->array[iter->cur];
	else
		iter->valid= 0;
}

void seq_end(SeqIterator *iter)
{
	if(iter->array)
		MEM_freeN(iter->array);

	iter->valid= 0;
}


