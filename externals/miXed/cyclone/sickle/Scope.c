/* Copyright (c) 2002-2005 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* LATER cache gui commands */
/* LATER think about resizing scheme.  Currently mouse events are not bound
   to any part of Scope~'s 'widget' as such, but to a special item, which is
   created only for a selected Scope~.  For the other scheme see the 'comment'
   class (no indicator there, though -- neither a handle, nor a pointer change).
   One way or the other, the traffic from the gui layer should be kept possibly
   low, at least in run-mode. */

#include <stdio.h>
#include <string.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "common/loud.h"
#include "common/grow.h"
#include "common/fitter.h"
#include "unstable/forky.h"
#include "sickle/sic.h"

#ifdef KRZYSZCZ
//#define SCOPE_DEBUG
#endif

/* these are powers of 2 + margins */
#define SCOPE_DEFWIDTH     130  /* CHECKED */
#define SCOPE_MINWIDTH      66
#define SCOPE_DEFHEIGHT     66  /* CHECKED */
#define SCOPE_MINHEIGHT     34
#define SCOPE_DEFPERIOD    256
#define SCOPE_MINPERIOD      2
#define SCOPE_MAXPERIOD   8092
#define SCOPE_DEFBUFSIZE   128
#define SCOPE_MINBUFSIZE     8
#define SCOPE_MAXBUFSIZE   800  /* LATER rethink */
#define SCOPE_WARNBUFSIZE  256
#define SCOPE_DEFMINVAL     -1.
#define SCOPE_DEFMAXVAL      1.
#define SCOPE_DEFDELAY       0
#define SCOPE_MINDELAY       0
#define SCOPE_TRIGLINEMODE   0
#define SCOPE_TRIGUPMODE     1
#define SCOPE_TRIGDOWNMODE   2
#define SCOPE_DEFTRIGMODE    SCOPE_TRIGLINEMODE
#define SCOPE_MINTRIGMODE    SCOPE_TRIGLINEMODE
#define SCOPE_MAXTRIGMODE    SCOPE_TRIGDOWNMODE
#define SCOPE_DEFTRIGLEVEL   0.
#define SCOPE_MINCOLOR       0
#define SCOPE_MAXCOLOR     255
#define SCOPE_DEFFGRED     102
#define SCOPE_DEFFGGREEN   255
#define SCOPE_DEFFGBLUE     51
#define SCOPE_DEFBGRED     135
#define SCOPE_DEFBGGREEN   135
#define SCOPE_DEFBGBLUE    135
#define SCOPE_SELCOLOR       "#8080ff"  /* a bit lighter shade of blue */
#define SCOPE_FGWIDTH        0.7  /* line width is float */
#define SCOPE_GRIDWIDTH      0.9
#define SCOPE_SELBDWIDTH     1.0
#define SCOPEHANDLE_WIDTH   10    /* item size is int */
#define SCOPEHANDLE_HEIGHT  10
/* these are performance-related hacks, LATER investigate */
#define SCOPE_GUICHUNKMONO  16
#define SCOPE_GUICHUNKXY    32

typedef struct _scope
{
    t_sic      x_sic;
    t_glist   *x_glist;
    t_canvas  *x_canvas;  /* also an 'isvised' flag */
    char       x_tag[64];
    char       x_fgtag[64];
    char       x_bgtag[64];
    char       x_gridtag[64];
    int        x_width;
    int        x_height;
    float      x_minval;
    float      x_maxval;
    int        x_delay;
    int        x_trigmode;
    float      x_triglevel;
    unsigned char  x_fgred;
    unsigned char  x_fggreen;
    unsigned char  x_fgblue;
    unsigned char  x_bgred;
    unsigned char  x_bggreen;
    unsigned char  x_bgblue;
    int        x_xymode;
    float     *x_xbuffer;
    float     *x_ybuffer;
    float      x_xbufini[SCOPE_DEFBUFSIZE];
    float      x_ybufini[SCOPE_DEFBUFSIZE];
    int        x_allocsize;
    int        x_bufsize;
    int        x_bufphase;
    int        x_period;
    int        x_phase;
    int        x_precount;
    int        x_retrigger;
    float      x_ksr;
    float      x_currx;
    float      x_curry;
    float      x_trigx;
    int        x_frozen;
    t_clock   *x_clock;
    t_pd      *x_handle;
    int	      scale_offset_x;
    int       scale_offset_y;
} t_scope;

typedef struct _scopehandle
{
    t_pd       h_pd;
    t_scope   *h_master;
    t_symbol  *h_bindsym;
    char       h_pathname[64];
    char       h_outlinetag[64];
    int        h_adjust_x;
    int        h_adjust_y;
    int        h_constrain;
    int        h_dragon;
    int        h_dragx;
    int        h_dragy;
} t_scopehandle;

static t_class *scope_class;
static t_class *scopehandle_class;

static void scope_clear(t_scope *x, int withdelay)
{
    x->x_bufphase = 0;
    x->x_phase = 0;
    x->x_precount = (withdelay ? (int)(x->x_delay * x->x_ksr) : 0);
    /* CHECKED delay does not matter (refman is wrong) */
    x->x_retrigger = (x->x_trigmode != SCOPE_TRIGLINEMODE);
    x->x_trigx = x->x_triglevel;
}

static t_int *scope_monoperform(t_int *w)
{
    t_scope *x = (t_scope *)(w[1]);
    int bufphase = x->x_bufphase;
    int bufsize = x->x_bufsize;
    if (bufphase < bufsize)
    {
	int nblock = (int)(w[2]);
	if (x->x_precount >= nblock)
	    x->x_precount -= nblock;
	else
	{
	    t_float *in = (t_float *)(w[3]);
	    int phase = x->x_phase;
	    int period = x->x_period;
	    float *bp1 = x->x_xbuffer + bufphase;
	    float *bp2 = x->x_ybuffer + bufphase;
	    float currx = x->x_currx;
	    if (x->x_precount > 0)
	    {
		nblock -= x->x_precount;
		in += x->x_precount;
		x->x_precount = 0;
	    }
	    while (x->x_retrigger)
	    {
		float triglevel = x->x_triglevel;
		if (x->x_trigmode == SCOPE_TRIGUPMODE)
		{
		    if (x->x_trigx < triglevel)
		    {
			while (nblock--) if (*in++ >= triglevel)
			{
			    x->x_retrigger = 0;
			    break;
			}
		    }
		    else while (nblock--) if (*in++ < triglevel)
		    {
			x->x_trigx = triglevel - 1.;
			break;
		    }
		}
		else
		{
		    if (x->x_trigx > triglevel)
		    {
			while (nblock--) if (*in++ <= triglevel)
			{
			    x->x_retrigger = 0;
			    break;
			}
		    }
		    else while (nblock--) if (*in++ > triglevel)
		    {
			x->x_trigx = triglevel + 1.;
			break;
		    }
		}
		if (nblock <= 0)
		    return (w + 4);
	    }
	    while (nblock--)
	    {
		if (phase)
		{
		    float f = *in++;
		    /* CHECKED */
		    if ((currx < 0 && (f < currx || f > -currx)) ||
			(currx > 0 && (f > currx || f < -currx)))
			currx = f;
		}
		else currx = *in++;
		if (currx != currx)
		    currx = 0.;  /* CHECKED NaNs bashed to zeros */
		if (++phase == period)
		{
		    phase = 0;
		    if (++bufphase == bufsize)
		    {
			*bp1 = *bp2 = currx;
			clock_delay(x->x_clock, 0);
			break;
		    }
		    else *bp1++ = *bp2++ = currx;
		}
	    }
	    x->x_currx = currx;
	    x->x_bufphase = bufphase;
	    x->x_phase = phase;
	}
    }
    return (w + 4);
}

static t_int *scope_xyperform(t_int *w)
{
    t_scope *x = (t_scope *)(w[1]);
    int bufphase = x->x_bufphase;
    int bufsize = x->x_bufsize;
    if (bufphase < bufsize)
    {
	int nblock = (int)(w[2]);
	if (x->x_precount >= nblock)
	    x->x_precount -= nblock;
	else
	{
	    t_float *in1 = (t_float *)(w[3]);
	    t_float *in2 = (t_float *)(w[4]);
	    int phase = x->x_phase;
	    int period = x->x_period;
	    float freq = 1. / period;
	    float *bp1 = x->x_xbuffer + bufphase;
	    float *bp2 = x->x_ybuffer + bufphase;
	    float currx = x->x_currx;
	    float curry = x->x_curry;
	    if (x->x_precount > 0)
	    {
		nblock -= x->x_precount;
		in1 += x->x_precount;
		in2 += x->x_precount;
		x->x_precount = 0;
	    }
	    if (x->x_retrigger)
	    {
		/* CHECKME and FIXME */
		x->x_retrigger = 0;
	    }
	    while (nblock--)
	    {
		if (phase)
		{
		    /* CHECKME */
		    currx += *in1++;
		    curry += *in2++;
		}
		else
		{
		    currx = *in1++;
		    curry = *in2++;
		}
		if (currx != currx)
		    currx = 0.;  /* CHECKME NaNs bashed to zeros */
		if (curry != curry)
		    curry = 0.;  /* CHECKME NaNs bashed to zeros */
		if (++phase == period)
		{
		    phase = 0;
		    if (++bufphase == bufsize)
		    {
			*bp1 = currx * freq;
			*bp2 = curry * freq;
			clock_delay(x->x_clock, 0);
			break;
		    }
		    else
		    {
			*bp1++ = currx * freq;
			*bp2++ = curry * freq;
		    }
		}
	    }
	    x->x_currx = currx;
	    x->x_curry = curry;
	    x->x_bufphase = bufphase;
	    x->x_phase = phase;
	}
    }
    return (w + 5);
}

static void scope_setxymode(t_scope *x, int xymode);

static void scope_dsp(t_scope *x, t_signal **sp)
{
    x->x_ksr = sp[0]->s_sr * 0.001;
    scope_setxymode(x,
		    forky_hasfeeders((t_object *)x, x->x_glist, 1, &s_signal));
    if (x->x_xymode)
	dsp_add(scope_xyperform, 4, x, (t_int)sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
    else
	dsp_add(scope_monoperform, 3, x, (t_int)sp[0]->s_n, sp[0]->s_vec);
}

static t_canvas *scope_getcanvas(t_scope *x, t_glist *glist)
{
    if (glist != x->x_glist)
    {
	loudbug_bug("scope_getcanvas");
	x->x_glist = glist;
    }
    return (x->x_canvas = glist_getcanvas(glist));
}

/* answers the question:  ``can we draw and where to?'' */
static t_canvas *scope_isvisible(t_scope *x)
{
    return (glist_isvisible(x->x_glist) ? x->x_canvas : 0);
}

static void scope_period(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float period = (s ? x->x_period : SCOPE_DEFPERIOD);
    int result = loud_floatarg(*(t_pd *)x, (s ? 0 : 2), ac, av, &period,
			       SCOPE_MINPERIOD, SCOPE_MAXPERIOD,
			       /* LATER rethink warning rules */
			       (s ? LOUD_CLIP : LOUD_CLIP | LOUD_WARN), 0,
			       "samples per element");
    if (!s && result == LOUD_ARGOVER)
	fittermax_warning(*(t_pd *)x,
			  "more than %g samples per element requested",
			  SCOPE_MAXPERIOD);
    if (!s || result == LOUD_ARGOK || result == LOUD_ARGOVER)
    {
	x->x_period = (int)period;
	scope_clear(x, 0);
    }
}

static void scope_float(t_scope *x, t_float f)
{
    t_atom at;
    SETFLOAT(&at, f);
    scope_period(x, &s_float, 1, &at);
}

static void scope_bufsize(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float bufsize = (s ? x->x_bufsize : SCOPE_DEFBUFSIZE);
    int result = loud_floatarg(*(t_pd *)x, (s ? 0 : 4), ac, av, &bufsize,
			       SCOPE_MINBUFSIZE, SCOPE_WARNBUFSIZE,
			       /* LATER rethink warning rules */
			       (s ? LOUD_CLIP : LOUD_CLIP | LOUD_WARN), 0,
			       "display elements");
    if (result == LOUD_ARGOVER)
    {
	bufsize = (s ? x->x_bufsize : SCOPE_DEFBUFSIZE);
	result = loud_floatarg(*(t_pd *)x, (s ? 0 : 4), ac, av, &bufsize,
			       0, SCOPE_MAXBUFSIZE, 0, LOUD_CLIP | LOUD_WARN,
			       "display elements");
	if (!s && result == LOUD_ARGOK)
	    fittermax_warning(*(t_pd *)x,
			      "more than %g display elements requested",
			      SCOPE_WARNBUFSIZE);
    }
    if (!s)
    {
	x->x_allocsize = SCOPE_DEFBUFSIZE;
	x->x_bufsize = 0;
	x->x_xbuffer = x->x_xbufini;
	x->x_ybuffer = x->x_ybufini;
    }
    if (!s || result == LOUD_ARGOK)
    {
	int newsize = (int)bufsize;
	if (newsize > x->x_allocsize)
	{
	    int nrequested = newsize;
	    int allocsize = x->x_allocsize;
	    int oldsize = x->x_bufsize;
	    x->x_xbuffer = grow_withdata(&nrequested, &oldsize,
					 &allocsize, x->x_xbuffer,
					 SCOPE_DEFBUFSIZE, x->x_xbufini,
					 sizeof(*x->x_xbuffer));
	    if (nrequested == newsize)
	    {
		allocsize = x->x_allocsize;
		oldsize = x->x_bufsize;
		x->x_ybuffer = grow_withdata(&nrequested, &oldsize,
					     &allocsize, x->x_ybuffer,
					     SCOPE_DEFBUFSIZE, x->x_ybufini,
					     sizeof(*x->x_ybuffer));
	    }
	    if (nrequested == newsize)
	    {
		x->x_allocsize = allocsize;
		x->x_bufsize = newsize;
	    }
	    else
	    {
		if (x->x_xbuffer != x->x_xbufini)
		    freebytes(x->x_xbuffer,
			      x->x_allocsize * sizeof(*x->x_xbuffer));
		if (x->x_ybuffer != x->x_ybufini)
		    freebytes(x->x_ybuffer,
			      x->x_allocsize * sizeof(*x->x_ybuffer));
		x->x_allocsize = SCOPE_DEFBUFSIZE;
		x->x_bufsize = SCOPE_DEFBUFSIZE;
		x->x_xbuffer = x->x_xbufini;
		x->x_ybuffer = x->x_ybufini;
	    }
	}
	else x->x_bufsize = newsize;
	scope_clear(x, 0);
    }
}

static void scope_range(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float minval = (s ? x->x_minval : SCOPE_DEFMINVAL);
    t_float maxval = (s ? x->x_maxval : SCOPE_DEFMAXVAL);
    loud_floatarg(*(t_pd *)x, (s ? 0 : 5), ac, av, &minval, 0, 0, 0, 0, 0);
    loud_floatarg(*(t_pd *)x, (s ? 1 : 6), ac, av, &maxval, 0, 0, 0, 0, 0);
    /* CHECKME swapping, ignoring if equal */
    if (minval < maxval)
    {
	x->x_minval = minval;
	x->x_maxval = maxval;
    }
    else if (minval > maxval)
    {
	x->x_minval = maxval;
	x->x_maxval = minval;
    }
    else if (!s)
    {
	x->x_minval = SCOPE_DEFMINVAL;
	x->x_maxval = SCOPE_DEFMAXVAL;
    }
}

static void scope_delay(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float delay = (s ? x->x_delay : SCOPE_DEFDELAY);
    int result = loud_floatarg(*(t_pd *)x, (s ? 0 : 7), ac, av, &delay,
			       SCOPE_MINDELAY, 0,
			       LOUD_CLIP | LOUD_WARN, 0, "delay");
    if (!s || result == LOUD_ARGOK)
	x->x_delay = delay;
}

static void scope_trigger(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float trigmode = (s ? x->x_trigmode : SCOPE_DEFTRIGMODE);
    loud_floatarg(*(t_pd *)x, (s ? 0 : 9), ac, av, &trigmode,
		  SCOPE_MINTRIGMODE, SCOPE_MAXTRIGMODE,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN,
		  "trigger mode");
    x->x_trigmode = (int)trigmode;
    if (x->x_trigmode == SCOPE_TRIGLINEMODE)
	x->x_retrigger = 0;
}

static void scope_triglevel(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float triglevel = (s ? x->x_triglevel : SCOPE_DEFTRIGLEVEL);
    loud_floatarg(*(t_pd *)x, (s ? 0 : 10), ac, av, &triglevel, 0, 0, 0, 0, 0);
    x->x_triglevel = triglevel;
}

static void scope_frgb(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float fgred = (s ? x->x_fgred : SCOPE_DEFFGRED);
    t_float fggreen = (s ? x->x_fggreen : SCOPE_DEFFGGREEN);
    t_float fgblue = (s ? x->x_fgblue : SCOPE_DEFFGBLUE);
    t_canvas *cv;
    loud_floatarg(*(t_pd *)x, (s ? 0 : 11), ac, av, &fgred,
		  SCOPE_MINCOLOR, SCOPE_MAXCOLOR,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN, "color");
    loud_floatarg(*(t_pd *)x, (s ? 1 : 12), ac, av, &fggreen,
		  SCOPE_MINCOLOR, SCOPE_MAXCOLOR,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN, "color");
    loud_floatarg(*(t_pd *)x, (s ? 2 : 13), ac, av, &fgblue,
		  SCOPE_MINCOLOR, SCOPE_MAXCOLOR,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN, "color");
    x->x_fgred = (int)fgred;
    x->x_fggreen = (int)fggreen;
    x->x_fgblue = (int)fgblue;
    if (cv = scope_isvisible(x))
    {
        char color[20];
        sprintf(color, "#%2.2x%2.2x%2.2x",
            x->x_fgred, x->x_fggreen, x->x_fgblue);
	//sys_vgui(".x%x.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
	//	 cv, x->x_fgtag, x->x_fgred, x->x_fggreen, x->x_fgblue);
        gui_vmess("gui_scope_configure_fg_color", "xxs",
            cv, x, color);
    }
}

static void scope_brgb(t_scope *x, t_symbol *s, int ac, t_atom *av)
{
    t_float bgred = (s ? x->x_bgred : SCOPE_DEFBGRED);
    t_float bggreen = (s ? x->x_bggreen : SCOPE_DEFBGGREEN);
    t_float bgblue = (s ? x->x_bgblue : SCOPE_DEFBGBLUE);
    t_canvas *cv;
    loud_floatarg(*(t_pd *)x, (s ? 0 : 14), ac, av, &bgred,
		  SCOPE_MINCOLOR, SCOPE_MAXCOLOR,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN, "color");
    loud_floatarg(*(t_pd *)x, (s ? 1 : 15), ac, av, &bggreen,
		  SCOPE_MINCOLOR, SCOPE_MAXCOLOR,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN, "color");
    loud_floatarg(*(t_pd *)x, (s ? 2 : 16), ac, av, &bgblue,
		  SCOPE_MINCOLOR, SCOPE_MAXCOLOR,
		  LOUD_CLIP | LOUD_WARN, LOUD_CLIP | LOUD_WARN, "color");
    x->x_bgred = (int)bgred;
    x->x_bggreen = (int)bggreen;
    x->x_bgblue = (int)bgblue;
    if (cv = scope_isvisible(x))
    {
        char color[20];
        sprintf(color, "#%2.2x%2.2x%2.2x",
            x->x_bgred, x->x_bggreen, x->x_bgblue);
	//sys_vgui(".x%x.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
	//	 cv, x->x_bgtag, x->x_bgred, x->x_bggreen, x->x_bgblue);
        gui_vmess("gui_scope_configure_bg_color", "xxs",
            cv, x, color);
    }
}

static void scope_getrect(t_gobj *z, t_glist *glist,
			  int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_scope *x = (t_scope *)z;
    float x1, y1, x2, y2;
    x1 = text_xpix((t_text *)x, glist);
    y1 = text_ypix((t_text *)x, glist);
    x2 = x1 + x->x_width;
    y2 = y1 + x->x_height;
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

static void scope_displace(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_scope *x = (t_scope *)z;
    t_text *t = (t_text *)z;
    t->te_xpix += dx;
    t->te_ypix += dy;
    if (glist_isvisible(glist))
    {
	t_canvas *cv = scope_getcanvas(x, glist);
	//sys_vgui(".x%x.c move %s %d %d\n", cv, x->x_tag, dx, dy);
	//canvas_fixlinesfor(cv, t);
    }
}

static void scope_displace_wtag(t_gobj *z, t_glist *glist, int dx, int dy)
{
    t_scope *x = (t_scope *)z;
    t_text *t = (t_text *)z;
    t->te_xpix += dx;
    t->te_ypix += dy;
    if (glist_isvisible(glist))
    {
	t_canvas *cv = scope_getcanvas(x, glist);
	//sys_vgui(".x%x.c move %s %d %d\n", cv, x->x_tag, dx, dy);
	canvas_fixlinesfor(cv, t);
    }
}

static void scope_select(t_gobj *z, t_glist *glist, int state)
{
    t_scope *x = (t_scope *)z;
    t_canvas *cv = scope_getcanvas(x, glist);
    t_scopehandle *sh = (t_scopehandle *)x->x_handle;
    if (state)
    {
        gui_vmess("gui_gobj_select", "xx", cv, x);
	//int x1, y1, x2, y2;
	//scope_getrect(z, glist, &x1, &y1, &x2, &y2);

        //sys_vgui(".x%x.c itemconfigure %s -outline blue -width %f -fill %s\n",
        //cv, x->x_bgtag, SCOPE_SELBDWIDTH, SCOPE_SELCOLOR);

        //sys_vgui("canvas %s -width %d -height %d "
        //         "-bg #fedc00 -bd 0 -cursor bottom_right_corner\n",
        //    sh->h_pathname, SCOPEHANDLE_WIDTH, SCOPEHANDLE_HEIGHT);
        //sys_vgui(".x%x.c create window %f %f -anchor nw "
        //         "-width %d -height %d -window %s -tags %s\n",
        //    cv, x2 - (SCOPEHANDLE_WIDTH - SCOPE_SELBDWIDTH),
        //    y2 - (SCOPEHANDLE_HEIGHT - SCOPE_SELBDWIDTH),
        //    SCOPEHANDLE_WIDTH, SCOPEHANDLE_HEIGHT,
        //    sh->h_pathname, x->x_tag);
        //sys_vgui("bind %s <Button> {pd [concat %s _click 1 %%x %%y \\;]}\n",
        //    sh->h_pathname, sh->h_bindsym->s_name);
        //sys_vgui("bind %s <ButtonRelease> "
        //           "{pd [concat %s _click 0 0 0 \\;]}\n",
        //    sh->h_pathname, sh->h_bindsym->s_name);
        //sys_vgui("bind %s <Motion> {pd [concat %s _motion %%x %%y \\;]}\n",
        //    sh->h_pathname, sh->h_bindsym->s_name);
    }
    else
    {
        gui_vmess("gui_gobj_deselect", "xx", cv, x);
        //sys_vgui(".x%x.c itemconfigure %s -outline black -width %f "
        //         "-fill #%2.2x%2.2x%2.2x\n", cv, x->x_bgtag, SCOPE_GRIDWIDTH,
        //x->x_bgred, x->x_bggreen, x->x_bgblue);
        //sys_vgui("destroy %s\n", sh->h_pathname);
    }
}

static void scope_delete(t_gobj *z, t_glist *glist)
{
    canvas_deletelinesfor(glist, (t_text *)z);
}

/* We probably don't need this anymore. We're creating the fg paths
   when we draw the background, so we only need to configure their
   data using the redraw methods. It's no longer used, but we should
   probably test a bit more before removing it... */
static void scope_drawfgmono(t_scope *x, t_canvas *cv,
			     int x1, int y1, int x2, int y2)
{
    int i;
    float dx, dy, xx, yy, sc;
    float *bp;
    dx = (float)(x2 - x1) / (float)x->x_bufsize;
    sc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    sys_vgui(".x%x.c create line \\\n", cv);
    for (i = 0, xx = x1, bp = x->x_xbuffer;
	 i < x->x_bufsize; i++, xx += dx, bp++)
    {
	yy = (y2 - 1) - sc * (*bp - x->x_minval);
#ifndef SCOPE_DEBUG
	if (yy > y2) yy = y2; else if (yy < y1) yy = y1;
#endif
	sys_vgui("%d %d \\\n", (int)xx, (int)yy);
    }
    sys_vgui("-fill #%2.2x%2.2x%2.2x -width %f -tags {%s %s}\n",
	     x->x_fgred, x->x_fggreen, x->x_fgblue,
	     SCOPE_FGWIDTH, x->x_fgtag, x->x_tag);

    /* margin lines:  masking overflows, so that they appear as gaps,
       rather than clipped signal values, LATER rethink */
    sys_vgui(".x%x.c create line %d %d %d %d\
 -fill #%2.2x%2.2x%2.2x -width %f -tags {%s %s}\n",
	     cv, x1, y1, x2, y1, x->x_bgred, x->x_bggreen, x->x_bgblue,
	     1., x->x_fgtag, x->x_tag);
    sys_vgui(".x%x.c create line %d %d %d %d\
 -fill #%2.2x%2.2x%2.2x -width %f -tags {%s %s}\n",
	     cv, x1, y2, x2, y2, x->x_bgred, x->x_bggreen, x->x_bgblue,
	     1., x->x_fgtag, x->x_tag);
}

static void scope_drawfgxy(t_scope *x, t_canvas *cv,
			   int x1, int y1, int x2, int y2)
{
    int nleft = x->x_bufsize;
    float *xbp = x->x_xbuffer, *ybp = x->x_ybuffer;
    char chunk[200 * SCOPE_GUICHUNKXY];  /* LATER estimate */
    char *chunkp = chunk;
    char cmd1[64], cmd2[64];
    float xx, yy, xsc, ysc;
    xx = yy = 0;
    /* subtract 1-pixel margins, see below */
    xsc = ((float)x->x_width - 2.) / (float)(x->x_maxval - x->x_minval);
    ysc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    sprintf(cmd1, ".x%x.c create line", (int)cv);
    sprintf(cmd2, "-fill #%2.2x%2.2x%2.2x -width %f -tags {%s %s}\n ",
	    x->x_fgred, x->x_fggreen, x->x_fgblue,
	    SCOPE_FGWIDTH, x->x_fgtag, x->x_tag);
    /* Not sure whether we need the conditional here or not... */
    if (x->x_bufsize)
    {
        gui_start_vmess("gui_scope_configure_fg_xy", "xx", cv, x);
	gui_start_array();
    }
    while (nleft > SCOPE_GUICHUNKXY)
    {
	int i = SCOPE_GUICHUNKXY;
	while (i--)
	{
	    float oldx = xx, oldy = yy, dx, dy;
	    xx = xsc * (*xbp++ - x->x_minval);
	    yy = y2 - y1 - ysc * (*ybp++ - x->x_minval);
	    /* using 1-pixel margins */
	    dx = (xx > oldx ? 1. : -1.);
	    dy = (yy > oldy ? 1. : -1.);
#ifndef SCOPE_DEBUG
	    if (xx < 0 || xx > (x2 - x1) || yy < 0 || yy > (y2 - y1))
		continue;
#endif
	    sprintf(chunkp, "M%d %d %d %d",
		    (int)(xx - dx), (int)(yy - dy),
		    (int)(xx + dx), (int)(yy + dy));
	    chunkp += strlen(chunkp);
	}
	if (chunkp > chunk)
	    gui_s(chunk);
	chunkp = chunk;
	nleft -= SCOPE_GUICHUNKXY;
    }
    while (nleft--)
    {
	float oldx = xx, oldy = yy, dx, dy;
	xx = xsc * (*xbp++ - x->x_minval);
	yy = (y2 - y1) - ysc * (*ybp++ - x->x_minval);
	/* using 1-pixel margins */
	dx = (xx > oldx ? 1. : -1.);
	dy = (yy > oldy ? 1. : -1.);
#ifndef SCOPE_DEBUG
	if (xx < 0 || xx > (x2 - x1) || yy < 0 || yy > (y2 - y1))
	    continue;
#endif
	sprintf(chunkp, "M%d %d %d %d",
		(int)(xx - dx), (int)(yy - dy),
		(int)(xx + dx), (int)(yy + dy));
	chunkp += strlen(chunkp);
    }
    if (chunkp > chunk)
	gui_s(chunk);
    if (x->x_bufsize)
    {
        gui_end_array();
        gui_end_vmess();
    }
}

static void scope_drawbg(t_scope *x, t_canvas *cv,
			 int x1, int y1, int x2, int y2)
{
    int i;
    float dx, dy, xx, yy;
    char fgcolor[20];
    char bgcolor[20];
    dx = (x2 - x1) * 0.125;
    dy = (y2 - y1) * 0.25;
    sprintf(fgcolor, "#%2.2x%2.2x%2.2x", x->x_fgred, x->x_fggreen, x->x_fgblue);
    sprintf(bgcolor, "#%2.2x%2.2x%2.2x", x->x_bgred, x->x_bggreen, x->x_bgblue);
    //sys_vgui(".x%x.c create rectangle %d %d %d %d\
 -fill #%2.2x%2.2x%2.2x -width %f -tags {%s %s}\n",
    //   cv, x1, y1, x2, y2,
    //   x->x_bgred, x->x_bggreen, x->x_bgblue,
    //   SCOPE_GRIDWIDTH, x->x_bgtag, x->x_tag);
    //for (i = 0, xx = x1 + dx; i < 7; i++, xx += dx)
    //    sys_vgui(".x%x.c create line %f %d %f %d\
 -width %f -tags {%s %s}\n", cv, xx, y1, xx, y2,
    //        SCOPE_GRIDWIDTH, x->x_gridtag, x->x_tag);
    //for (i = 0, yy = y1 + dy; i < 3; i++, yy += dy)
    //    sys_vgui(".x%x.c create line %d %f %d %f\
 -width %f -tags {%s %s}\n", cv, x1, yy, x2, yy,
    //        SCOPE_GRIDWIDTH, x->x_gridtag, x->x_tag);
    /* Here we draw the background, _and_ we create the paths
       for the foreground paths. The paths will get filled with
       data in scope_drawfgxy, etc. This should be cheaper than
       creating and destroying a bunch of DOM objects on every
       redraw. */
    gui_vmess("gui_scope_draw_bg", "xxssiifff",
        glist_getcanvas(cv),
        x,
        fgcolor,
        bgcolor,
        x2 - x1,
        y2 - y1,
        SCOPE_GRIDWIDTH,
        dx,
        dy);
}

static void scope_redrawmono(t_scope *x, t_canvas *cv)
{
    int nleft = x->x_bufsize;
    float *bp = x->x_xbuffer;
    char chunk[32 * SCOPE_GUICHUNKMONO];  /* LATER estimate */
    char *chunkp = chunk;
    int x1, y1, x2, y2;
    float dx, dy, xx, yy, sc;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    dx = (float)(x2 - x1) / (float)x->x_bufsize;
    sc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    //xx = x1;
    xx = 0;
    //sys_vgui(".x%x.c coords %s \\\n", cv, x->x_fgtag);
    if (x->x_bufsize) {
        gui_start_vmess("gui_scope_configure_fg_mono", "xx", cv, x);
        gui_start_array();
        gui_s("M");
    }
    while (nleft > SCOPE_GUICHUNKMONO)
    {
	int i = SCOPE_GUICHUNKMONO;
	while (i--)
	{
	    //yy = (y2 - 1) - sc * (*bp++ - x->x_minval);
	    yy = (y2 - y1 - 1) - sc * (*bp++ - x->x_minval);
#ifndef SCOPE_DEBUG
	    if (yy > (y2 - y1)) yy = y2 - y1; else if (yy < 0) yy = 0;
#endif
	    sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
	    chunkp += strlen(chunkp);
	    xx += dx;
	}
	//strcpy(chunkp, "\\\n");
	gui_s(chunk);
	chunkp = chunk;
	nleft -= SCOPE_GUICHUNKMONO;
    }
    while (nleft--)
    {
	//yy = (y2 - 1) - sc * (*bp++ - x->x_minval);
	yy = (y2 - y1 - 1) - sc * (*bp++ - x->x_minval);
#ifndef SCOPE_DEBUG
	if (yy > (y2 - y1)) yy = y2 - y1; else if (yy < 0) yy = 0;
#endif
	sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
	chunkp += strlen(chunkp);
	xx += dx;
    }
    //strcpy(chunkp, "\n");
    if (x->x_bufsize)
    {
        gui_s(chunk);
        gui_end_array();
        gui_end_vmess();
    }
}

static void scope_drawmono(t_scope *x, t_canvas *cv)
{
    int x1, y1, x2, y2;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    scope_drawbg(x, cv, x1, y1, x2, y2);
    //scope_drawfgmono(x, cv, x1, y1, x2, y2);
}

static void scope_drawxy(t_scope *x, t_canvas *cv)
{
    int x1, y1, x2, y2;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    scope_drawbg(x, cv, x1, y1, x2, y2);
    scope_drawfgxy(x, cv, x1, y1, x2, y2);
}

static void scope_redrawxy(t_scope *x, t_canvas *cv)
{
    int x1, y1, x2, y2;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    //sys_vgui(".x%x.c delete %s\n", cv, x->x_fgtag);
    scope_drawfgxy(x, cv, x1, y1, x2, y2);
}

static void scope_revis(t_scope *x, t_canvas *cv)
{
    //sys_vgui(".x%x.c delete %s\n", cv, x->x_tag);
    gui_vmess("gui_scope_clear_fg", "xx", cv, x);
    if (x->x_xymode)
	scope_drawxy(x, cv);
    else
	scope_drawmono(x, cv);
}

static void scope_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_scope *x = (t_scope *)z;
    t_text *t = (t_text *)z;
    t_canvas *cv = scope_getcanvas(x, glist);
    if (vis)
    {
        int x1, y1, x2, y2;
	scope_getrect(z, glist, &x1, &y1, &x2, &y2);
        gui_vmess("gui_gobj_new", "xxsiii",
            glist_getcanvas(glist),
            x,
            "obj",
            x1,
            y1,
            glist_istoplevel(glist));
	t_scopehandle *sh = (t_scopehandle *)x->x_handle;
#if FORKY_VERSION < 37
	rtext_new(glist, t, glist->gl_editor->e_rtext, 0);
#endif
	sprintf(sh->h_pathname, ".x%x.h%x", (int)cv, (int)sh);
	if (x->x_xymode)
	    scope_drawxy(x, cv);
	else
	    scope_drawmono(x, cv);
        if (glist_isselected(cv, (t_gobj *)x))
            gui_vmess("gui_gobj_select", "xx", cv, x);
    }
    else
    {
#if FORKY_VERSION < 37
	t_rtext *rt = glist_findrtext(glist, t);
	if (rt) rtext_free(rt);
#endif
	//sys_vgui(".x%x.c delete %s\n", cv, x->x_tag);
        gui_vmess("gui_gobj_erase", "xx", glist_getcanvas(glist), x);
	x->x_canvas = 0;
    }
}

static int scope_click(t_gobj *z, t_glist *glist,
		       int xpix, int ypix, int shift, int alt, int dbl,
		       int doit)
{
    t_scope *x = (t_scope *)z;
    x->x_frozen = doit;
    return (CURSOR_RUNMODE_CLICKME);
}

/* CHECKED there is only one copy of state variables,
   the same, whether modified with messages, or in the inspector */
static void scope_save(t_gobj *z, t_binbuf *b)
{
    t_scope *x = (t_scope *)z;
    t_text *t = (t_text *)x;
    binbuf_addv(b, "ssiisiiiiiffififiiiiiii;", gensym("#X"), gensym("obj"),
		(int)t->te_xpix, (int)t->te_ypix,
        atom_getsymbol(binbuf_getvec(t->te_binbuf)),
		x->x_width, x->x_height, x->x_period, 3, x->x_bufsize,
		x->x_minval, x->x_maxval, x->x_delay, 0.,
		x->x_trigmode, x->x_triglevel,
		x->x_fgred, x->x_fggreen, x->x_fgblue,
		x->x_bgred, x->x_bggreen, x->x_bgblue, 0);
}

static t_widgetbehavior scope_widgetbehavior =
{
    scope_getrect,
    scope_displace,
    scope_select,
    0,
    scope_delete,
    scope_vis,
    scope_click,
    scope_displace_wtag
};

static void scope_setxymode(t_scope *x, int xymode)
{
    if (xymode != x->x_xymode)
    {
	t_canvas *cv;
	if (cv = scope_isvisible(x))
	{
	    //sys_vgui(".x%x.c delete %s\n", cv, x->x_fgtag);
            gui_vmess("gui_scope_clear_fg", "xx", cv, x);
	    if (!xymode)
	    {
		int x1, y1, x2, y2;
		scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
		//scope_drawfgmono(x, cv, x1, y1, x2, y2);
                scope_redrawmono(x, cv);
	    }
	}
	x->x_xymode = xymode;
	scope_clear(x, 0);
    }
}

static void scope_tick(t_scope *x)
{
    t_canvas *cv;
    if (!x->x_frozen && (cv = scope_isvisible(x)))
    {
	if (x->x_xymode)
	    scope_redrawxy(x, cv);
	else
	    scope_redrawmono(x, cv);
    }
    scope_clear(x, 1);
}

extern void canvas_apply_setundo(t_canvas *x, t_gobj *y);
static void scopehandle__clickhook(t_scopehandle *sh, t_floatarg f,
    t_floatarg xxx, t_floatarg yyy)
{
    t_scope *x = sh->h_master;
    /* Use constrained dragging. See g_canvas.c clickhook */
    sh->h_constrain = (int)f;
    sh->h_adjust_x = xxx - (((t_object *)x)->te_xpix + x->x_width);
    sh->h_adjust_y = yyy - (((t_object *)x)->te_ypix + x->x_height);
    canvas_apply_setundo(x->x_glist, (t_gobj *)x);
    sh->h_dragon = f;
}

static void scopehandle__motionhook(t_scopehandle *sh,
				    t_floatarg mouse_x, t_floatarg mouse_y)
{
    t_scope *x = (t_scope *)(sh->h_master);
    int width = (sh->h_constrain == CURSOR_EDITMODE_RESIZE_Y) ?
        x->x_width :
        (int)mouse_x - text_xpix((t_text *)x, x->x_glist) - sh->h_adjust_x;
    int height = (sh->h_constrain == CURSOR_EDITMODE_RESIZE_X) ?
        x->x_height :
        (int)mouse_y - text_ypix((t_text *)x, x->x_glist) - sh->h_adjust_y;

    x->x_width =  width < SCOPE_MINWIDTH ? SCOPE_MINWIDTH : width;
    x->x_height = height < SCOPE_MINHEIGHT ? SCOPE_MINHEIGHT : height;

    /* This is just a quick and dirty way to redraw, which has the side
       effect of erasing the waveform until the next tick. For a more elegant
       approach we'd want to call the "revis" function, but that would also
       require an extra gui function for changing the size of the background. */
    if (glist_isvisible(x->x_glist))
    {
        scope_vis((t_gobj *)x, x->x_glist, 0);
        scope_vis((t_gobj *)x, x->x_glist, 1);
    }
}

/* wrapper method for forwarding "scopehandle" data */
static void scope_click_for_resizing(t_scope *x, t_floatarg f, t_floatarg xxx,
    t_floatarg yyy)
{
    t_scopehandle *sh = (t_scopehandle *)x->x_handle;
    scopehandle__clickhook(sh, f, xxx, yyy);
}

/* another wrapper for forwarding "scopehandle" motion data */
static void scope_motion_for_resizing(t_scope *x, t_floatarg xxx,
    t_floatarg yyy)
{
    t_scopehandle *sh = (t_scopehandle *)x->x_handle;
    scopehandle__motionhook(sh, xxx, yyy);
}

static void scope_free(t_scope *x)
{
    if (x->x_clock) clock_free(x->x_clock);
    if (x->x_xbuffer != x->x_xbufini)
	freebytes(x->x_xbuffer, x->x_allocsize * sizeof(*x->x_xbuffer));
    if (x->x_ybuffer != x->x_ybufini)
	freebytes(x->x_ybuffer, x->x_allocsize * sizeof(*x->x_ybuffer));
    if (x->x_handle)
    {
	pd_unbind(x->x_handle, ((t_scopehandle *)x->x_handle)->h_bindsym);
	pd_free(x->x_handle);
    }
}

static void *scope_new(t_symbol *s, int ac, t_atom *av)
{
    t_scope *x = (t_scope *)pd_new(scope_class);
    t_scopehandle *sh;
    t_float width = SCOPE_DEFWIDTH;
    t_float height = SCOPE_DEFHEIGHT;
    char buf[64];
    x->x_glist = canvas_getcurrent();
    x->x_canvas = 0;
    loud_floatarg(*(t_pd *)x, 0, ac, av, &width,
		  SCOPE_MINWIDTH, 0,
		  LOUD_CLIP | LOUD_WARN, 0, "width");
    x->x_width = (int)width;
    loud_floatarg(*(t_pd *)x, 1, ac, av, &height,
		  SCOPE_MINHEIGHT, 0,
		  LOUD_CLIP | LOUD_WARN, 0, "height");
    x->x_height = (int)height;
    scope_period(x, 0, ac, av);
    /* CHECKME 6th argument (default 3 for mono, 1 for xy */
    scope_bufsize(x, 0, ac, av);
    scope_range(x, 0, ac, av);
    scope_delay(x, 0, ac, av);
    /* CHECKME 11th argument (default 0.) */
    scope_trigger(x, 0, ac, av);
    scope_triglevel(x, 0, ac, av);
    scope_frgb(x, 0, ac, av);
    scope_brgb(x, 0, ac, av);
    /* CHECKME last argument (default 0) */

    sprintf(x->x_tag, "all%x", (int)x);
    sprintf(x->x_bgtag, "bg%x", (int)x);
    sprintf(x->x_gridtag, "gr%x", (int)x);
    sprintf(x->x_fgtag, "fg%x", (int)x);
    x->x_xymode = 0;
    x->x_ksr = sys_getsr() * 0.001;  /* redundant */
    x->x_frozen = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_clock = clock_new(x, (t_method)scope_tick);
    scope_clear(x, 0);

    x->x_handle = pd_new(scopehandle_class);
    sh = (t_scopehandle *)x->x_handle;
    sh->h_master = x;
    sprintf(buf, "_h%x", (int)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(buf));
    sprintf(sh->h_outlinetag, "h%x", (int)sh);
    sh->h_dragon = 0;

    x->scale_offset_x = 0;
    x->scale_offset_y = 0;

    return (x);
}

void Scope_tilde_setup(void)
{
    scope_class = class_new(gensym("Scope~"),
			    (t_newmethod)scope_new,
			    (t_method)scope_free,
			    sizeof(t_scope), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)scope_new, gensym("scope~"), A_GIMME, 0);
	class_addcreator((t_newmethod)scope_new, gensym("cyclone/scope~"), A_GIMME, 0);
    sic_setup(scope_class, scope_dsp, scope_float);
    class_addmethod(scope_class, (t_method)scope_bufsize,
		    gensym("bufsize"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_range,
		    gensym("range"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_delay,
		    gensym("delay"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_trigger,
		    gensym("trigger"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_triglevel,
		    gensym("triglevel"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_frgb,
		    gensym("frgb"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_brgb,
		    gensym("brgb"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_click,
		    gensym("click"),
		    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    /* Big hack for receiving edit-mode resize anchor clicks from
       g_editor.c. */
    class_addmethod(scope_class, (t_method)scope_click_for_resizing,
		    gensym("_click_for_resizing"),
		    A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_motion_for_resizing,
		    gensym("_motion_for_resizing"),
		    A_FLOAT, A_FLOAT, 0);
    class_setwidget(scope_class, &scope_widgetbehavior);
    forky_setsavefn(scope_class, scope_save);
    scopehandle_class = class_new(gensym("_scopehandle"), 0, 0,
				  sizeof(t_scopehandle), CLASS_PD, 0);
    class_addmethod(scopehandle_class, (t_method)scopehandle__clickhook,
		    gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scopehandle_class, (t_method)scopehandle__motionhook,
		    gensym("_motion"), A_FLOAT, A_FLOAT, 0);
    fitter_setup(scope_class, 0);
}
