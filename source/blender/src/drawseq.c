/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _WIN32
#include "BLI_winstuff.h"
#endif

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"

#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_plugin_types.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_drawseq.h"
#include "BIF_editseq.h"
#include "BIF_drawimage.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_interface.h"

#include "BSE_view.h"
#include "BSE_drawipo.h"
#include "BSE_sequence.h"
#include "BSE_seqaudio.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "blendef.h"	/* CFRA */
#include "mydevice.h"	/* REDRAWSEQ */

int no_rightbox=0, no_leftbox= 0;

static void EmbossBoxf(float x1, float y1, float x2, float y2, int sel, unsigned int dark, unsigned int light)
{

	if(sel) cpack(dark);
	else cpack(light);
	if(sel) glLineWidth(2.0);
	fdrawline(x1,  y2,  x2,  y2); 	/* top */
	if(no_leftbox==0) fdrawline(x1,  y1,  x1,  y2);	/* left */

	if(sel) glLineWidth(1.0);

	if(sel) cpack(light);
	else cpack(dark);
	fdrawline(x1,  y1,  x2,  y1); /* bottom */
	if(no_rightbox==0) fdrawline(x2,  y1,  x2,  y2); 	/* right */

}

static char *give_seqname(Sequence *seq)
{
	if(seq->type==SEQ_META) {
		if(seq->name[2]) return seq->name+2;
		return "META";
	}
	else if(seq->type==SEQ_IMAGE) return "IMAGE";
	else if(seq->type==SEQ_SCENE) return "SCENE";
	else if(seq->type==SEQ_MOVIE) return "MOVIE";
	else if(seq->type==SEQ_SOUND) return "AUDIO";
	else if(seq->type<SEQ_EFFECT) return seq->strip->dir;
	else if(seq->type==SEQ_CROSS) return "CROSS";
	else if(seq->type==SEQ_GAMCROSS) return "GAMMA CROSS";
	else if(seq->type==SEQ_ADD) return "ADD";
	else if(seq->type==SEQ_SUB) return "SUB";
	else if(seq->type==SEQ_MUL) return "MUL";
	else if(seq->type==SEQ_ALPHAOVER) return "ALPHAOVER";
	else if(seq->type==SEQ_ALPHAUNDER) return "ALPHAUNDER";
	else if(seq->type==SEQ_OVERDROP) return "OVER DROP";
	else if(seq->type==SEQ_SWEEP) return "SWEEP";
	else if(seq->type==SEQ_PLUGIN) {
		if(seq->plugin && seq->plugin->doit) return seq->plugin->pname;
		return "PLUGIN";
	}
	else return "EFFECT";

}
static void draw_cfra_seq(void)
{
	glColor3ub(0x30, 0x90, 0x50);
	glLineWidth(2.0);
	glBegin(GL_LINES);
	glVertex2f(G.scene->r.cfra, G.v2d->cur.ymin);
	glVertex2f(G.scene->r.cfra, G.v2d->cur.ymax);
	glEnd();
	glLineWidth(1.0);
}

static unsigned int seq_color(Sequence *seq)
{
	switch(seq->type) {
	case SEQ_META:
		return 0x509090;
	case SEQ_MOVIE:
		return 0x805040;
	case SEQ_SCENE:
		if(seq->scene==G.scene) return 0x709050;
		return 0x609060;
	case SEQ_CROSS:
		return 0x505090;
	case SEQ_GAMCROSS:
		return 0x5040A0;
	case SEQ_ADD:
		return 0x6060A0;
	case SEQ_SUB:
		return 0x8060A0;
	case SEQ_MUL:
		return 0x8080A0;
	case SEQ_ALPHAOVER:
		return 0x6080A0;
	case SEQ_ALPHAUNDER:
		return 0x9080A0;
	case SEQ_OVERDROP:
		return 0x5080B0;
		case SEQ_SWEEP:
		return 0x2080B0;
	case SEQ_PLUGIN:
		return 0x906000;
	case SEQ_SOUND:
		if(seq->flag & SEQ_MUTE) return 0x707060; else return 0x787850;
	default:
		return 0x906060;
	}

}
static void drawmeta_contents(Sequence *seqm, float x1, float y1, float x2, float y2)
{
	Sequence *seq;
	float dx;
	int nr;

	nr= 0;
	WHILE_SEQ(&seqm->seqbase) {
		nr++;
	}
	END_SEQ

	dx= (x2-x1)/nr;

	WHILE_SEQ(&seqm->seqbase) {
		cpack(seq_color(seq));
		glRectf(x1,  y1,  x1+0.9*dx,  y2);
		EmbossBoxf(x1, y1, x1+0.9*dx, y2, 0, 0x404040, 0xB0B0B0);
		x1+= dx;
	}
	END_SEQ
}

void drawseqwave(Sequence *seq, float x1, float y1, float x2, float y2)
{
	float f, height, midy;
	int offset, sofs, eofs;
	signed short* s;
	bSound *sound;

	audio_makestream(seq->sound);
	if(seq->sound->stream==NULL) return;

	if (seq->flag & SEQ_MUTE) glColor3ub(0x70, 0x80, 0x80); else glColor3ub(0x70, 0xc0, 0xc0);
	sound = seq->sound;
	sofs = ((int)( (((float)(seq->startdisp-seq->start))/(float)G.scene->r.frs_sec)*(float)G.scene->audio.mixrate*4.0 )) & (~3);
	eofs = ((int)( (((float)(seq->enddisp-seq->start))/(float)G.scene->r.frs_sec)*(float)G.scene->audio.mixrate*4.0 )) & (~3);

	for (f=x1; f<=x2; f+=0.2) {
		offset = (int) ((float)sofs + ((f-x1)/(x2-x1)) * (float)(eofs-sofs)) & (~3);
		if (offset >= sound->streamlen) offset = sound->streamlen-1;
		s = (signed short*)(((Uint8*)sound->stream) + offset);
		midy = (y1+y2)/2;
		height = ( ( ((float)s[0]/32768 + (float)s[1]/32768)/2 ) * (y2-y1) )/2;
		glBegin(GL_LINES);
		glVertex2f(f, midy-height);
		glVertex2f(f, midy+height);
		glEnd();
	}
}

void drawseq(Sequence *seq)
{
	float v1[2], v2[2], x1, x2, y1, y2;
	unsigned int body, dark, light;
	int len, size;
	short mval[2];
	char str[120], *strp;


	if(seq->startdisp > seq->enddisp) body= 0x707070;

	body= seq_color(seq);
	dark= 0x202020;
	light= 0xB0B0B0;

	if(G.moving && (seq->flag & SELECT)) {
		if(seq->flag & SEQ_OVERLAP) dark= light= 0x4040FF;
		else {
			if(seq->flag & (SEQ_LEFTSEL+SEQ_RIGHTSEL));
			else dark= light= 0xFFFFFF;
		}
	}

	/* body */
	if(seq->startstill) x1= seq->start;
	else x1= seq->startdisp;
	y1= seq->machine+0.2;
	if(seq->endstill) x2= seq->start+seq->len;
	else x2= seq->enddisp;
	y2= seq->machine+0.8;

	cpack(body);
	glRectf(x1,  y1,  x2,  y2);
	if (seq->type == SEQ_SOUND) drawseqwave(seq, x1, y1, x2, y2);
	EmbossBoxf(x1, y1, x2, y2, seq->flag & 1, dark, light);

	v1[1]= y1;
	v2[1]= y2;
	if(seq->type < SEQ_EFFECT) {

		/* decoration: the bars */
		x1= seq->startdisp;
		x2= seq->enddisp;

		if(seq->startofs) {
			cpack(0x707070);
			glRectf((float)(seq->start),  y1-0.2,  x1,  y1);
			EmbossBoxf((float)(seq->start), y1-0.2, x1, y1, seq->flag & 1, dark, light);
		}
		if(seq->endofs) {
			cpack(0x707070);
			glRectf(x2,  y2,  (float)(seq->start+seq->len),  y2+0.2);
			EmbossBoxf(x2, y2, (float)(seq->start+seq->len), y2+0.2, seq->flag & 1, dark, light);
		}

		if(seq->startstill) {
			cpack(body);
			glRectf(x1,  y1+0.1,  (float)(seq->start),  y1+0.5);
			no_rightbox= 1;
			EmbossBoxf(x1, y1+0.1, (float)(seq->start), y1+0.5, seq->flag & 1, dark, light);
			no_rightbox= 0;
		}
		if(seq->endstill) {
			cpack(body);
			glRectf((float)(seq->start+seq->len),  y1+0.1,  x2,  y1+0.5);
			no_leftbox= 1;
			EmbossBoxf((float)(seq->start+seq->len), y1+0.1, x2, y1+0.5, seq->flag & 1, dark, light);
			no_leftbox= 0;
		}

	}

	/* calculate if seq is long enough to print a name */
	x1= seq->startdisp+seq->handsize;
	x2= seq->enddisp-seq->handsize;

	/* but first the contents of a meta */
	if(seq->type==SEQ_META) drawmeta_contents(seq, x1, y1+0.15, x2, y2-0.15);

	if(x1<G.v2d->cur.xmin) x1= G.v2d->cur.xmin;
	else if(x1>G.v2d->cur.xmax) x1= G.v2d->cur.xmax;
	if(x2<G.v2d->cur.xmin) x2= G.v2d->cur.xmin;
	else if(x2>G.v2d->cur.xmax) x2= G.v2d->cur.xmax;

	if(x1 != x2) {
		v1[0]= x1;
		ipoco_to_areaco_noclip(G.v2d, v1, mval);
		x1= mval[0];
		v2[0]= x2;
		ipoco_to_areaco_noclip(G.v2d, v2, mval);
		x2= mval[0];
		size= x2-x1;

		if(seq->name[2]) {
			sprintf(str, "%s: %s (%d)", give_seqname(seq), seq->name+2, seq->len);
		}else{
			if(seq->type == SEQ_META) {
				sprintf(str, "%d %s", seq->len, give_seqname(seq));
			}
			else if(seq->type == SEQ_SCENE) {
				if(seq->scene) sprintf(str, "%d %s %s", seq->len, give_seqname(seq), seq->scene->id.name+2);
				else sprintf(str, "%d %s", seq->len, give_seqname(seq));

			}
			else if(seq->type == SEQ_IMAGE) {
				sprintf(str, "%d %s%s", seq->len, seq->strip->dir, seq->strip->stripdata->name);
			}
			else if(seq->type & SEQ_EFFECT) {
				if(seq->seq3!=seq->seq2 && seq->seq1!=seq->seq3)
					sprintf(str, "%d %s: %d-%d (use %d)", seq->len, give_seqname(seq), seq->seq1->machine, seq->seq2->machine, seq->seq3->machine);
				else
					sprintf(str, "%d %s: %d-%d", seq->len, give_seqname(seq), seq->seq1->machine, seq->seq2->machine);
			}
			else if (seq->type == SEQ_SOUND) {
				sprintf(str, "%d %s", seq->len, seq->strip->stripdata->name);
			}
			else if (seq->type == SEQ_MOVIE) {
				sprintf(str, "%d %s%s", seq->len, seq->strip->dir, seq->strip->stripdata->name);
			}
		}


		strp= str;

		while( (len= BMF_GetStringWidth(G.font, strp)) > size) {
			if(len < 10) break;
			if(strp[1]==0) break;
			strp++;
		}

		mval[0]= (x1+x2-len+1)/2;
		mval[1]= 1;
		areamouseco_to_ipoco(G.v2d, mval, &x1, &x2);

		if(seq->flag & SELECT) cpack(0xFFFFFF);
		else cpack(0x0);
		glRasterPos3f(x1,  y1+0.2, 0.0);
		BMF_DrawString(G.font, strp);
	}

	if(seq->type < SEQ_EFFECT) {
		/* decoration: triangles */
		x1= seq->startdisp;
		x2= seq->enddisp;

		body+= 0x101010;
		dark= 0x202020;
		light= 0xB0B0B0;

		/* left triangle */
		if(seq->flag & SEQ_LEFTSEL) {
			cpack(body+0x20);
			if(G.moving) {
				if(seq->flag & SEQ_OVERLAP) dark= light= 0x4040FF;
				else dark= light= 0xFFFFFF;
			}
		}
		else {
			cpack(body);
		}

		glBegin(GL_TRIANGLES);
			v1[0]= x1; glVertex2fv(v1);
			v2[0]= x1; glVertex2fv(v2);
			v2[0]+= seq->handsize; v2[1]= (y1+y2)/2.0; glVertex2fv(v2); v2[1]= y2;
		glEnd();

		cpack(light);

		glBegin(GL_LINE_STRIP);
			v1[0]= x1; glVertex2fv(v1);
			v2[0]= x1; glVertex2fv(v2);
			v2[0]+= seq->handsize; v2[1]= (y1+y2)/2.0; glVertex2fv(v2); v2[1]= y2;
			cpack(dark);
			glVertex2fv(v1);
		glEnd();
	}

	if(G.moving || (seq->flag & SEQ_LEFTSEL)) {
		cpack(0xFFFFFF);
		glRasterPos3f(x1,  y1+0.2, 0.0);
		sprintf(str, "%d", seq->startdisp);
		BMF_DrawString(G.font, str);
	}

		/* right triangle */
	if(seq->type < SEQ_EFFECT) {
		dark= 0x202020;
		light= 0xB0B0B0;

		if(seq->flag & SEQ_RIGHTSEL) {
			cpack(body+0x20);
			if(G.moving) {
				if(seq->flag & SEQ_OVERLAP) dark= light= 0x4040FF;
				else dark= light= 0xFFFFFF;
			}
		}
		else {
			cpack(body);
		}
		glBegin(GL_TRIANGLES);
			v2[0]= x2; glVertex2fv(v2);
			v1[0]= x2; glVertex2fv(v1);
			v2[0]-= seq->handsize; v2[1]= (y1+y2)/2.0; glVertex2fv(v2); v2[1]= y2;
		glEnd();

		cpack(dark);
		glBegin(GL_LINE_STRIP);
			v2[0]= x2; glVertex2fv(v2);
			v1[0]= x2; glVertex2fv(v1);
			v1[0]-= seq->handsize; v1[1]= (y1+y2)/2.0; glVertex2fv(v1); v1[1]= y2;
			cpack(light);
			glVertex2fv(v2);
		glEnd();
	}

	if(G.moving || (seq->flag & SEQ_RIGHTSEL)) {
		cpack(0xFFFFFF);
		glRasterPos3f(x2-seq->handsize/2,  y1+0.2, 0.0);
		sprintf(str, "%d", seq->enddisp-1);
		BMF_DrawString(G.font, str);
	}
}

static Sequence *special_seq_update= 0;

void set_special_seq_update(int val)
{
	int x;

	/* if mouse over a sequence && LEFTMOUSE */
	if(val) {
		special_seq_update= find_nearest_seq(&x);
	}
	else special_seq_update= 0;
}


static void draw_image_seq(void)
{
	SpaceSeq *sseq;
	StripElem *se;
	struct ImBuf *ibuf;
	int x1, y1;

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	curarea->win_swap= WIN_BACK_OK;

	ibuf= (ImBuf *)give_ibuf_seq( (G.scene->r.cfra));

	if(special_seq_update) {
       se = special_seq_update->curelem;
       if(se) {
           if(se->ok==2) {
               if(se->se1)
                   ibuf= se->se1->ibuf;
           }
           else ibuf= se->ibuf;
       }
	}
	if(ibuf==0 || ibuf->rect==0) return;

	sseq= curarea->spacedata.first;
	if(sseq==0) return;

	/* calc location */
	x1= curarea->winrct.xmin+(curarea->winx-sseq->zoom*ibuf->x)/2;
	y1= curarea->winrct.ymin+(curarea->winy-sseq->zoom*ibuf->y)/2;

	rectwrite_part(curarea->winrct.xmin, curarea->winrct.ymin,
				curarea->winrct.xmax, curarea->winrct.ymax,
				x1, y1, ibuf->x, ibuf->y, (float)sseq->zoom,(float)sseq->zoom, ibuf->rect);

}

static void draw_extra_seqinfo(void)
{
	extern Sequence *last_seq;
	StripElem *se, *last;
	float xco, xfac;
	int sta, end;
	char str[256];

	if(last_seq==0) return;

	/* xfac: size of 1 pixel */
	xfac= G.v2d->cur.xmax - G.v2d->cur.xmin;
	xfac/= (float)(G.v2d->mask.xmax-G.v2d->mask.xmin);
	xco= G.v2d->cur.xmin+40*xfac;

	BIF_ThemeColor(TH_TEXT);

	/* NAME */
	glRasterPos3f(xco,  0.3, 0.0);
	strcpy(str, give_seqname(last_seq));
	BMF_DrawString(G.font, str);
	xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;

	if(last_seq->type==SEQ_SCENE && last_seq->scene) {
		glRasterPos3f(xco,  0.3, 0.0);
		BMF_DrawString(G.font, last_seq->scene->id.name+2);
		xco += xfac*BMF_GetStringWidth(G.font, last_seq->scene->id.name+2) +30.0*xfac;
	}

	/* LEN */
	if(last_seq->type & SEQ_EFFECT)
		sprintf(str, "len: %d   From %d - %d", last_seq->len, last_seq->startdisp, last_seq->enddisp-1);
	else
		sprintf(str, "len: %d (%d)", last_seq->enddisp-last_seq->startdisp, last_seq->len);

	glRasterPos3f(xco,  0.3, 0.0);
	BMF_DrawString(G.font, str);
	xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;

	if(last_seq->type==SEQ_IMAGE) {

		/* CURRENT */
		se= (StripElem *)give_stripelem(last_seq,  (G.scene->r.cfra));
		if(se) {
			sprintf(str, "cur: %s", se->name);
			glRasterPos3f(xco,  0.3, 0.0);
			BMF_DrawString(G.font, str);
			xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;
		}

		/* FIRST AND LAST */

		if(last_seq->strip) {
			se= last_seq->strip->stripdata;
			last= se+last_seq->len-1;
			if(last_seq->startofs) se+= last_seq->startofs;
			if(last_seq->endofs) last-= last_seq->endofs;

			sprintf(str, "First: %s at %d     Last: %s at %d", se->name, last_seq->startdisp, last->name, last_seq->enddisp-1);
			glRasterPos3f(xco,  0.3, 0.0);
			BMF_DrawString(G.font, str);
			xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;

			/* orig size */
			sprintf(str, "OrigSize: %d x %d", last_seq->strip->orx, last_seq->strip->ory);
			glRasterPos3f(xco,  0.3, 0.0);
			BMF_DrawString(G.font, str);
			xco += xfac*BMF_GetStringWidth(G.font, str) +30.0*xfac;
		}
	}
	else if(last_seq->type==SEQ_MOVIE) {

		sta= last_seq->startofs;
		end= last_seq->len-1-last_seq->endofs;

		sprintf(str, "%s   %s%s  First: %d at %d     Last: %d at %d     Cur: %d",
				last_seq->name+2, last_seq->strip->dir, last_seq->strip->stripdata->name,
				sta, last_seq->startdisp, end, last_seq->enddisp-1,  (G.scene->r.cfra)-last_seq->startdisp);

		glRasterPos3f(xco,  0.3, 0.0);
		BMF_DrawString(G.font, str);
	}
	else if(last_seq->type==SEQ_SOUND) {

		sta= last_seq->startofs;
		end= last_seq->len-1-last_seq->endofs;

		sprintf(str, "%s   %s%s  First: %d at %d     Last: %d at %d     Cur: %d     Gain: %.2f dB     Pan: %.2f",
				last_seq->name+2, last_seq->strip->dir, last_seq->strip->stripdata->name,
				sta, last_seq->startdisp, end, last_seq->enddisp-1,  (G.scene->r.cfra)-last_seq->startdisp,
				last_seq->level, last_seq->pan);

		glRasterPos3f(xco,  0.3, 0.0);
		BMF_DrawString(G.font, str);
	}
}

#define SEQ_BUT_PLUGIN	1
#define SEQ_BUT_MOVIE	2
#define SEQ_BUT_IMAGE	3

void do_seqbuttons(short val)
{
	extern Sequence *last_seq;
	StripElem *se;

	switch(val) {
	case SEQ_BUT_PLUGIN:
		new_stripdata(last_seq);
		free_imbuf_effect_spec(CFRA);
		break;

	case SEQ_BUT_MOVIE:
		se= last_seq->curelem;
		if(se && se->ibuf ) {
			IMB_freeImBuf(se->ibuf);
			se->ibuf= 0;
		}
		break;
	case SEQ_BUT_IMAGE:
		break;
	}

	allqueue(REDRAWSEQ, 0);
}

static void seq_panel_properties(short cntrl)	// SEQ_HANDLER_PROPERTIES
{
	extern Sequence *last_seq;
	uiBlock *block;

	block= uiNewBlock(&curarea->uiblocks, "seq_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(SEQ_HANDLER_PROPERTIES);  // for close and esc
	if(uiNewPanel(curarea, block, "Transform Properties", "Seq", 10, 230, 318, 204)==0) return;

	if(last_seq==NULL) return;

	if(last_seq->type==SEQ_PLUGIN) {
		PluginSeq *pis;
		VarStruct *varstr;
		int a, xco, yco;

		uiDefBut(block, LABEL, 0, "Striptype: Plugin", 10,50,70,20, 0, 0, 0, 0, 0, "");

		pis= last_seq->plugin;
		if(pis->vars==0) return;

		varstr= pis->varstr;
		if(varstr) {
			for(a=0; a<pis->vars; a++, varstr++) {
				xco= 150*(a/6)+10;
				yco= 125 - 20*(a % 6)+1;
				uiDefBut(block, varstr->type, SEQ_BUT_PLUGIN, varstr->name, xco,yco,150,19, &(pis->data[a]), varstr->min, varstr->max, 100, 0, varstr->tip);

			}
		}
	}
	else if(last_seq->type==SEQ_IMAGE) {

		uiDefBut(block, LABEL, 0, "Striptype: Image", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Stripname: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

	}
	else if(last_seq->type==SEQ_META) {

		uiDefBut(block, LABEL, 0, "Striptype: Meta", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Stripname: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

	}
	else if(last_seq->type==SEQ_SCENE) {

		uiDefBut(block, LABEL, 0, "Striptype: Scene", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Stripname: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

	}
	else if(last_seq->type==SEQ_MOVIE) {

		if(last_seq->mul==0.0) last_seq->mul= 1.0;

		uiDefBut(block, LABEL, 0, "Striptype: Movie", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Stripname: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

		uiDefButS(block, TOG|BIT|4, SEQ_BUT_MOVIE, "FilterY ", 10,90,150,19, &last_seq->flag, 0.0, 21.0, 100, 0, "");
		uiDefButF(block, NUM, SEQ_BUT_MOVIE, "Mul:", 10,70,150,19, &last_seq->mul, 0.001, 5.0, 100, 0, "");

	}
	else if(last_seq->type==SEQ_SOUND) {

		uiDefBut(block, LABEL, 0, "Striptype: Audio", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Stripname: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

		uiDefButS(block, TOG|BIT|5, 0, "Mute", 10,90,120,19, &last_seq->flag, 0.0, 21.0, 100, 0, "");
		uiDefButF(block, NUM, SEQ_BUT_MOVIE, "Gain (dB):", 10,70,150,19, &last_seq->level, -96.0, 6.0, 100, 0, "");
		uiDefButF(block, NUM, SEQ_BUT_MOVIE, "Pan:", 	10,50,150,19, &last_seq->pan, -1.0, 1.0, 100, 0, "");
	}
	else if(last_seq->type>=SEQ_EFFECT) {
		uiDefBut(block, LABEL, 0, "Striptype: Effect", 10,140,150,20, 0, 0, 0, 0, 0, "");
		uiDefBut(block, TEX, 0, "Stripname: ", 10,120,150,19, last_seq->name+2, 0.0, 21.0, 100, 0, "");

		if(last_seq->type==SEQ_SWEEP){
			SweepVars *sweep = (SweepVars *)last_seq->varstr;
			char formatstring[1024];

			strcpy(formatstring, "Select Sweep Type %t|Left to Right %x0|Right to Left %x1|Bottom to Top %x2|Top to Bottom %x3|Top left to Bottom right%x4|Bottom right to Top left %x5|Bottom left to Top right %x6|Top right to Bottom left %x7|Horizontal out %x8|Horizontal in %x9|Vertical out %x10|Vertical in %x11|Hor/Vert out %x12|Hor/Vert in %x13|Bottom left to Top right out %x14|Top left to Bottom right in %x15|Top left to Bottom right out %x16|Bottom left to Top right in %x17|Diagonal out %x18|Diagonal in %x19|Diagonal out 2 %x20|Diagonal in 2 %x21|");

			uiDefButS(block, MENU,SEQ_BUT_MOVIE, formatstring,	10,90,220,22, &sweep->sweeptype, 0, 0, 0, 0, "What type of sweep should be performed");
		}

	}


}

static void seq_blockhandlers(ScrArea *sa)
{
	SpaceSeq *sseq= sa->spacedata.first;
	short a;

	/* warning; blocks need to be freed each time, handlers dont remove (for ipo moved to drawipospace) */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);

	for(a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(sseq->blockhandler[a]) {

		case SEQ_HANDLER_PROPERTIES:
			seq_panel_properties(sseq->blockhandler[a+1]);
			break;

		}
		/* clear action value for event */
		sseq->blockhandler[a+1]= 0;
	}
	uiDrawBlocksPanels(sa, 0);

}

void drawseqspace(ScrArea *sa, void *spacedata)
{
	SpaceSeq *sseq;
	Editing *ed;
	Sequence *seq;
	float col[3];
	int ofsx, ofsy;

	ed= G.scene->ed;

	sseq= curarea->spacedata.first;
	if(sseq->mainb==1) {
		draw_image_seq();
		return;
	}

	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();

	BIF_GetThemeColor3fv(TH_BACK, col);
	if(ed && ed->metastack.first) glClearColor(col[0], col[1], col[2]-1.0, 0.0);
	else glClearColor(col[0], col[1], col[2], 0.0);

	glClear(GL_COLOR_BUFFER_BIT);

	calc_scrollrcts(G.v2d, curarea->winx, curarea->winy);

	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {
			ofsx= curarea->winrct.xmin;	/* because of mywin */
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
		}
	}


	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);

	BIF_ThemeColorShade(TH_BACK, -20);
	glRectf(G.v2d->cur.xmin,  0.0,  G.v2d->cur.xmax,  1.0);

	boundbox_seq();
	calc_ipogrid();
	draw_ipogrid();
	draw_cfra_seq();

	/* sequences: first deselect */

	if(ed) {
		seq= ed->seqbasep->first;
		while(seq) {
			if(seq->flag & SELECT); else drawseq(seq);
			seq= seq->next;
		}
	}
	ed= G.scene->ed;
	if(ed) {
		seq= ed->seqbasep->first;
		while(seq) {
			if(seq->flag & SELECT) drawseq(seq);
			seq= seq->next;
		}
	}

	draw_extra_seqinfo();

	/* restore viewport */
	mywinset(curarea->win);

	/* ortho at pixel level curarea */
	myortho2(-0.375, curarea->winx-0.375, -0.375, curarea->winy-0.375);

	if(curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {
			drawscroll(0);
		}
	}

	draw_area_emboss(sa);

	if(sseq->mainb==0) {
		/* it is important to end a view in a transform compatible with buttons */
		bwin_scalematrix(sa->win, sseq->blockscale, sseq->blockscale, sseq->blockscale);
		seq_blockhandlers(sa);
	}

	curarea->win_swap= WIN_BACK_OK;
}


