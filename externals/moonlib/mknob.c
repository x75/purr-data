/* Copyright (c) 1997-1999 Miller Puckette.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution. */

/* mknob.c written by Antoine Rousseau from g_vslider.c.*/
/* g_7_guis.c written by Thomas Musil (c) IEM KUG Graz Austria 2000-2001 */
/* thanks to Miller Puckette, Guenther Geiger and Krzystof Czaja */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "m_pd.h"
#include "g_canvas.h"

#include "g_all_guis.h"
#include <math.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif


#define MKNOB_TANGLE 100
#define MKNOB_DEFAULTH 100
#define MKNOB_DEFAULTSIZE 25
#define MKNOB_MINSIZE 12
#define MKNOB_THICK 3
/* ------------ mknob  ----------------------- */
typedef struct _mknob
{
    t_iemgui x_gui;
    int      x_pos;
    int      x_val;
    int      x_center;
    int      x_thick;
    int      x_lin0_log1;
    int      x_steady;
    double   x_min;
    double   x_max;
    int      x_H;
    double   x_k;
} t_mknob;

t_widgetbehavior mknob_widgetbehavior;
static t_class *mknob_class;

/* widget helper functions */
static void mknob_update_knob(t_mknob *x, t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    float val = (x->x_val + 50.0)/100.0/x->x_H;
    float angle,
        radius = x->x_gui.x_w/2.0*.98,
        miniradius = MKNOB_THICK,
        xp, yp, xc, yc, xpc, ypc;
    int x0,y0,x1,y1;
    //x0=text_xpix(&x->x_gui.x_obj, glist);
    //y0=text_ypix(&x->x_gui.x_obj, glist);
    x0 = 0;
    y0 = 0;
    x1 = x0 + x->x_gui.x_w;
    y1 = y0 + x->x_gui.x_w;
    xc = (x0+x1) / 2.0;
    yc = (y0+y1) / 2.0;

    if (x->x_gui.x_h < 0)
        angle = val * (M_PI * 2.0) + M_PI / 2.0;
    else
        angle = val * (M_PI * 1.5) + 3.0 * M_PI / 4.0;

    xp = xc + radius * cos(angle);
    yp = yc + radius * sin(angle);
    xpc = miniradius * cos(angle - M_PI / 2.0);
    ypc = miniradius * sin(angle - M_PI / 2.0);

    gui_vmess("gui_turn_mknob", "xxffff",
        canvas,
        x,
        xp,
        yp,
        xc,
        yc
    );
}

static void mknob_draw_update(t_mknob *x, t_glist *glist)
{
    if (glist_isvisible(glist))
        mknob_update_knob(x,glist);
}

static void mknob_draw_config(t_mknob *x,t_glist *glist)
{
    t_canvas *canvas = glist_getcanvas(glist);
    char bcol[8], fcol[8], lcol[8];

    sprintf(bcol, "#%6.6x", x->x_gui.x_bcol);
    sprintf(fcol, "#%6.6x", x->x_gui.x_fcol);
    sprintf(lcol, "#%6.6x", x->x_gui.x_lcol);

    gui_vmess("gui_configure_mknob", "xxiss",
        canvas,
        x,
        x->x_gui.x_w,
        bcol,
        fcol
    );
    mknob_update_knob(x,glist);
}

static void mknob_draw_new(t_mknob *x, t_glist *glist)
{
    int xpos = text_xpix(&x->x_gui.x_obj, glist);
    int ypos = text_ypix(&x->x_gui.x_obj, glist);
    t_canvas *canvas = glist_getcanvas(glist);

    gui_vmess("gui_mknob_new", "xxiiiii",
        canvas,
        x,
        xpos,
        ypos,
        glist_istoplevel(glist),
        !iemgui_has_snd(x),
        !iemgui_has_rcv(x)
    );
    mknob_draw_config(x, glist);
}

void mknob_draw(t_mknob *x, t_glist *glist, int mode)
{
    if (mode == IEM_GUI_DRAW_MODE_UPDATE)
        mknob_draw_update(x, glist);
    else if (mode == IEM_GUI_DRAW_MODE_MOVE)
        iemgui_base_draw_move(&x->x_gui);
    else if (mode == IEM_GUI_DRAW_MODE_NEW)
        mknob_draw_new(x, glist);
    else if (mode == IEM_GUI_DRAW_MODE_CONFIG)
        mknob_draw_config(x, glist);
}

/* ------------------------ mknob widgetbehaviour----------------------------- */


static void mknob_getrect(t_gobj *z, t_glist *glist,
                          int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_mknob *x = (t_mknob *)z;

    *xp1 = text_xpix(&x->x_gui.x_obj, glist);
    *yp1 = text_ypix(&x->x_gui.x_obj, glist);
    *xp2 = *xp1 + x->x_gui.x_w;
    *yp2 = *yp1 + x->x_gui.x_w;
}

static void mknob_save(t_gobj *z, t_binbuf *b)
{
    t_mknob *x = (t_mknob *)z;
    int bflcol[3];
    t_symbol *srl[3];

    iemgui_save(&x->x_gui, srl, bflcol);
    binbuf_addv(b, "ssiisiiffiisssiiiiiiiii", gensym("#X"),gensym("obj"),
                (t_int)x->x_gui.x_obj.te_xpix, (t_int)x->x_gui.x_obj.te_ypix,
                atom_getsymbol(binbuf_getvec(x->x_gui.x_obj.te_binbuf)),
                x->x_gui.x_w, x->x_gui.x_h,
                (float)x->x_min, (float)x->x_max,
                x->x_lin0_log1, iem_symargstoint(x),
                srl[0], srl[1], srl[2],
                x->x_gui.x_ldx, x->x_gui.x_ldy,
                iem_fstyletoint(x), x->x_gui.x_fontsize,
                bflcol[0], bflcol[1], bflcol[2],
                x->x_val, x->x_steady);
    binbuf_addv(b, ";");
}

void mknob_check_wh(t_mknob *x, int w, int h)
{
    int H;

    if(w < MKNOB_MINSIZE) w = MKNOB_MINSIZE;
    x->x_gui.x_w = w;

    if(h < -1) h=-1;
    if((h>0)&&(h<20)) h=20;
    x->x_gui.x_h = h;

    H=x->x_gui.x_h;
    if(H<0) H=360;
    if(H==0) H=270;
    x->x_H=H;

    if(x->x_lin0_log1)
        x->x_k = log(x->x_max/x->x_min)/(double)(x->x_H - 1);
    else
        x->x_k = (x->x_max - x->x_min)/(double)(x->x_H - 1);
}

void mknob_check_minmax(t_mknob *x, double min, double max)
{
    int H;

    if(x->x_lin0_log1)
    {
        if((min == 0.0)&&(max == 0.0))
            max = 1.0;
        if(max > 0.0)
        {
            if(min <= 0.0)
                min = 0.01*max;
        }
        else
        {
            if(min > 0.0)
                max = 0.01*min;
        }
    }
    x->x_min = min;
    x->x_max = max;
    if(x->x_min > x->x_max)                /* bugfix */
        x->x_gui.x_reverse = 1;
    else
        x->x_gui.x_reverse = 0;

    if(x->x_lin0_log1)
        x->x_k = log(x->x_max/x->x_min)/(double)(x->x_H - 1);
    else
        x->x_k = (x->x_max - x->x_min)/(double)(x->x_H - 1);
    /*if(x->x_lin0_log1)
    	x->x_k = log(x->x_max/x->x_min)/(double)(MKNOB_TANGLE - 1);
    else
    	x->x_k = (x->x_max - x->x_min)/(double)(MKNOB_TANGLE - 1);*/
}

static void mknob_properties(t_gobj *z, t_glist *owner)
{
    t_mknob *x = (t_mknob *)z;
    char buf[800];
    char *gfx_tag;
    t_symbol *srl[3];

    iemgui_properties(&x->x_gui, srl);
    gfx_tag = gfxstub_new2(&x->x_gui.x_obj.ob_pd, x);

    gui_start_vmess("gui_iemgui_dialog", "s", gfx_tag);
    gui_start_array();

    gui_s("type");             gui_s("mknob");
    /* Since mknob reuses the iemgui dialog code, we just
       use the "width" slot for "size" and the "height" slot
       for the number of steps and re-label it on the GUI side. */
    gui_s("width");            gui_i(x->x_gui.x_w);
    gui_s("height");           gui_i(x->x_gui.x_h);
    gui_s("minimum_range");    gui_f(x->x_min);
    gui_s("maximum_range");    gui_f(x->x_max);
    gui_s("log_scaling");      gui_i(x->x_lin0_log1);
    gui_s("init");             gui_i(x->x_gui.x_loadinit);
    gui_s("steady_on_click");   gui_i(x->x_steady);
    gui_s("send_symbol");      gui_s(srl[0]->s_name);
    gui_s("receive_symbol");   gui_s(srl[1]->s_name);
    gui_s("label");            gui_s(srl[2]->s_name);
    gui_s("x_offset");         gui_i(x->x_gui.x_ldx);
    gui_s("y_offset");         gui_i(x->x_gui.x_ldy);
    gui_s("font_style");       gui_i(x->x_gui.x_font_style);
    gui_s("font_size");        gui_i(x->x_gui.x_fontsize);
    gui_s("background_color"); gui_i(0xffffff & x->x_gui.x_bcol);
    gui_s("foreground_color"); gui_i(0xffffff & x->x_gui.x_fcol);
    gui_s("label_color");      gui_i(0xffffff & x->x_gui.x_lcol);

    gui_end_array();
    gui_end_vmess();
}

static void mknob_set(t_mknob *x, t_floatarg f)    /* bugfix */
{
    double g;

    if(x->x_gui.x_reverse)    /* bugfix */
    {
        if(f > x->x_min)
            f = x->x_min;
        if(f < x->x_max)
            f = x->x_max;
    }
    else
    {
        if(f > x->x_max)
            f = x->x_max;
        if(f < x->x_min)
            f = x->x_min;
    }
    if(x->x_lin0_log1)
        g = log(f/x->x_min)/x->x_k;
    else
        g = (f - x->x_min) / x->x_k;
    x->x_val = (int)(100.0*g + 0.49999);
    x->x_pos = x->x_val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
}

static void mknob_bang(t_mknob *x)
{
    double out;

    if(x->x_lin0_log1)
        out = x->x_min*exp(x->x_k*(double)(x->x_val)*0.01);
    else
        out = (double)(x->x_val)*0.01*x->x_k + x->x_min;
    if((out < 1.0e-10)&&(out > -1.0e-10))
        out = 0.0;
    outlet_float(x->x_gui.x_obj.ob_outlet, out);
    if (iemgui_has_snd(x) && x->x_gui.x_snd->s_thing)
        pd_float(x->x_gui.x_snd->s_thing, out);
}

static void mknob_dialog(t_mknob *x, t_symbol *s, int argc, t_atom *argv)
{
    canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
    t_symbol *srl[3];
    int w = (int)atom_getintarg(0, argc, argv);
    int h = (int)atom_getintarg(1, argc, argv);
    double min = (double)atom_getfloatarg(2, argc, argv);
    double max = (double)atom_getfloatarg(3, argc, argv);
    int lilo = (int)atom_getintarg(4, argc, argv);
    int steady = (int)atom_getintarg(17, argc, argv);
    int sr_flags;

    if (lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if (steady)
        x->x_steady = 1;
    else
        x->x_steady = 0;
    sr_flags = iemgui_dialog(&x->x_gui, argc, argv);
    //x->x_gui.x_h = iemgui_clip_size(h);
    //x->x_gui.x_w = iemgui_clip_size(w);
    mknob_check_wh(x, w, h);
    mknob_check_minmax(x, min, max);
    iemgui_draw_config(&x->x_gui);
    //(*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_CONFIG);
    //(*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_IO + sr_flags);
    iemgui_draw_io(&x->x_gui, sr_flags);
    iemgui_draw_move(&x->x_gui);
    //(*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_MOVE);
    canvas_fixlinesfor(x->x_gui.x_glist, (t_text *)x);
}

static int xm0, ym0, xm, ym;

static void mknob_motion(t_mknob *x, t_floatarg dx, t_floatarg dy)
{
    int old = x->x_val;
    float d=-dy;

    if (abs(dx)>abs(dy)) d=dx;

    if(x->x_gui.x_finemoved)
        x->x_pos += (int)d;
    else
        x->x_pos += 100*(int)d;
    x->x_val = x->x_pos;
    if(x->x_val > (100*x->x_H - 100))
    {
        x->x_val = 100*x->x_H - 100;
        x->x_pos += 50;
        x->x_pos -= x->x_pos%100;
    }
    if(x->x_val < 0)
    {
        x->x_val = 0;
        x->x_pos -= 50;
        x->x_pos -= x->x_pos%100;
    }
    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        mknob_bang(x);
    }
}

static void mknob_motion_circular(t_mknob *x, t_floatarg dx, t_floatarg dy)
{
    int xc=text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist)+x->x_gui.x_w/2;
    int yc=text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist)+x->x_gui.x_w/2;
    int old = x->x_val;
    float alpha;

    xm+=dx;
    ym+=dy;

    alpha=atan2(xm-xc,ym-yc)*180.0/M_PI;

    x->x_pos=(int)(31500-alpha*100.0)%36000;
    if(x->x_pos>31500) x->x_pos=0;
    else if(x->x_pos>(27000-100)) x->x_pos=(27000-100);

    x->x_val=x->x_pos;

    if(old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        mknob_bang(x);
    }
}

static void mknob_motion_fullcircular(t_mknob *x, t_floatarg dx, t_floatarg dy)
{
    int xc=text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist)+x->x_gui.x_w/2;
    int yc=text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist)+x->x_gui.x_w/2;
    int old = x->x_val;
    float alpha;

    xm+=dx;
    ym+=dy;

    alpha=atan2(xm-xc,ym-yc)*180.0/M_PI;

    x->x_pos=(int)(36000-alpha*100.0)%36000;
    /*if(x->x_pos>31500) x->x_pos=0;
    else if(x->x_pos>(27000-100)) x->x_pos=(27000-100);*/

    if (x->x_pos > (36000-100))
    {
        x->x_pos=(36000-100);
    }
    x->x_val=x->x_pos;

    if (old != x->x_val)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        mknob_bang(x);
    }
}

static void mknob_click(t_mknob *x, t_floatarg xpos, t_floatarg ypos,
                        t_floatarg shift, t_floatarg ctrl, t_floatarg alt)
{
    xm0 = xm = xpos;
    ym0 = ym = ypos;
    if (x->x_val > (100*x->x_H - 100))
        x->x_val = 100*x->x_H - 100;
    if (x->x_val < 0)
        x->x_val = 0;
    x->x_pos = x->x_val;
    (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
    mknob_bang(x);

    if (x->x_gui.x_h<0)
        glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
                   (t_glistmotionfn)mknob_motion_fullcircular, 0, xpos, ypos);
    else if (x->x_gui.x_h==0)
        glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
                   (t_glistmotionfn)mknob_motion_circular, 0, xpos, ypos);
    else
        glist_grab(x->x_gui.x_glist, &x->x_gui.x_obj.te_g,
                   (t_glistmotionfn)mknob_motion, 0, xpos, ypos);
}

static int mknob_newclick(t_gobj *z, struct _glist *glist,
                          int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    t_mknob *x = (t_mknob *)z;

    if (doit)
    {
        mknob_click( x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift,
                     0, (t_floatarg)alt);
        if (shift)
            x->x_gui.x_finemoved = 1;
        else
            x->x_gui.x_finemoved = 0;
    }
    return (1);
}

static void mknob_size(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    int w = (int)atom_getintarg(0, ac, av),
        h = x->x_gui.x_h;

    if (ac > 1) h = (int)atom_getintarg(1, ac, av);

    mknob_check_wh(x, w, h);
    iemgui_size((t_iemgui *)x);
}

static void mknob_delta(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_delta((t_iemgui *)x, s, ac, av);
}

static void mknob_pos(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_pos((t_iemgui *)x, s, ac, av);
}

static void mknob_range(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    mknob_check_minmax(x, (double)atom_getfloatarg(0, ac, av),
                       (double)atom_getfloatarg(1, ac, av));
}

static void mknob_color(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_color((t_iemgui *)x, s, ac, av);
}

static void mknob_send(t_mknob *x, t_symbol *s)
{
    iemgui_send((t_iemgui *)x, s);
}

static void mknob_receive(t_mknob *x, t_symbol *s)
{
    iemgui_receive((t_iemgui *)x, s);
}

static void mknob_label(t_mknob *x, t_symbol *s)
{
    iemgui_label((t_iemgui *)x, s);
}

static void mknob_label_pos(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_pos((t_iemgui *)x, s, ac, av);
}

static void mknob_label_font(t_mknob *x, t_symbol *s, int ac, t_atom *av)
{
    iemgui_label_font((t_iemgui *)x, s, ac, av);
}

static void mknob_log(t_mknob *x)
{
    x->x_lin0_log1 = 1;
    mknob_check_minmax(x, x->x_min, x->x_max);
}

static void mknob_lin(t_mknob *x)
{
    x->x_lin0_log1 = 0;
    x->x_k = (x->x_max - x->x_min)/(double)(x->x_gui.x_w - 1);
}

static void mknob_init(t_mknob *x, t_floatarg f)
{
    x->x_gui.x_loadinit = (f==0.0)?0:1;
}

static void mknob_steady(t_mknob *x, t_floatarg f)
{
    x->x_steady = (f==0.0)?0:1;
}

static void mknob_float(t_mknob *x, t_floatarg f)
{
    double out;

    mknob_set(x, f);
    if (x->x_lin0_log1)
        out = x->x_min*exp(x->x_k*(double)(x->x_val)*0.01);
    else
        out = (double)(x->x_val)*0.01*x->x_k + x->x_min;
    if ((out < 1.0e-10)&&(out > -1.0e-10))
        out = 0.0;
    if (x->x_gui.x_put_in2out)
    {
        outlet_float(x->x_gui.x_obj.ob_outlet, out);
        if (iemgui_has_snd(x) && x->x_gui.x_snd->s_thing)
            pd_float(x->x_gui.x_snd->s_thing, out);
    }
}

#define LB_LOAD 0 /* from g_canvas.h */

static void mknob_loadbang(t_mknob *x, t_floatarg action)
{
    if (action == LB_LOAD && x->x_gui.x_loadinit)
    {
        (*x->x_gui.x_draw)(x, x->x_gui.x_glist, IEM_GUI_DRAW_MODE_UPDATE);
        mknob_bang(x);
    }
}

/* we may no longer need h_dragon... */
static void mknob__clickhook(t_scalehandle *sh, int newstate)
{
    t_mknob *x = (t_mknob *)(sh->h_master);
    if (newstate)
    {
        canvas_apply_setundo(x->x_gui.x_glist, (t_gobj *)x);
        if (!sh->h_scale) /* click on a label handle */
            scalehandle_click_label(sh);
    }
    /* We no longer need this "clickhook", as we can handle the dragging
       either in the GUI (for the label handle) or or in canvas_doclick */
    //iemgui__clickhook3(sh,newstate);
    sh->h_dragon = newstate;
}

static void mknob__motionhook(t_scalehandle *sh,
                    t_floatarg mouse_x, t_floatarg mouse_y)
{
    if (sh->h_scale)
    {
        t_mknob *x = (t_mknob *)(sh->h_master);
        int width = x->x_gui.x_w,
            height = x->x_gui.x_h;
        int x1, y1, x2, y2, d;
        x1 = text_xpix(&x->x_gui.x_obj, x->x_gui.x_glist);
        y1 = text_ypix(&x->x_gui.x_obj, x->x_gui.x_glist);
        x2 = x1 + width;
        y2 = y1 + height;

        /* This is convoluted, but I can't think of another
           way to get this behavior... */
        if (mouse_x <= x2)
        {
            if (mouse_y > y2)
                d = mouse_y - y2;
            else if (abs(mouse_y - y2) < abs(mouse_x - x2))
                d = mouse_y - y2;
            else
                d = mouse_x - x2;
        }
        else
        {
            if (mouse_y <= y2)
                d = mouse_x - x2;
            else
                d = maxi(mouse_y - y2, mouse_x - x2);
        }
        sh->h_dragx = d;
        sh->h_dragy = d;
        scalehandle_drag_scale(sh);

        width = maxi(width + d, IEM_GUI_MINSIZE);
        height = width;

        x->x_gui.x_w = width;
        x->x_gui.x_h = height;

        if (glist_isvisible(x->x_gui.x_glist))
        {
            mknob_draw_config(x, x->x_gui.x_glist);
            scalehandle_unclick_scale(sh);
        }

        int properties = gfxstub_haveproperties((void *)x);
        if (properties)
        {
            int new_w = x->x_gui.x_w + sh->h_dragx;
            // This should actually be "size", but we're using the
            // "width" input in dialog_iemgui and just relabelling it
            // as a kluge.
            properties_set_field_int(properties, "width", new_w);
        }
    }
    scalehandle_dragon_label(sh,mouse_x, mouse_y);
}

static void *mknob_new(t_symbol *s, int argc, t_atom *argv)
{
    t_mknob *x = (t_mknob *)pd_new(mknob_class);
    int bflcol[]= {-262144, -1, -1};
    //t_symbol *srl[3];
    int w=MKNOB_DEFAULTSIZE, h=MKNOB_DEFAULTH;
    int fs=8 ,lilo=0, ldx=-2, ldy=-6, f=0, v=0, steady=1;
    //int  iinit=0, ifstyle=0;
    double min=0.0, max=(double)(IEM_SL_DEFAULTSIZE-1);
    //t_iem_init_symargs *init=(t_iem_init_symargs *)(&iinit);
    //t_iem_fstyle_flags *fstyle=(t_iem_fstyle_flags *)(&ifstyle);
    char str[144];

    if(((argc == 17)||(argc == 18))&&IS_A_FLOAT(argv,0)&&IS_A_FLOAT(argv,1)
            &&IS_A_FLOAT(argv,2)&&IS_A_FLOAT(argv,3)
            &&IS_A_FLOAT(argv,4)&&IS_A_FLOAT(argv,5)
            &&(IS_A_SYMBOL(argv,6)||IS_A_FLOAT(argv,6))
            &&(IS_A_SYMBOL(argv,7)||IS_A_FLOAT(argv,7))
            &&(IS_A_SYMBOL(argv,8)||IS_A_FLOAT(argv,8))
            &&IS_A_FLOAT(argv,9)&&IS_A_FLOAT(argv,10)
            &&IS_A_FLOAT(argv,11)&&IS_A_FLOAT(argv,12)&&IS_A_FLOAT(argv,13)
            &&IS_A_FLOAT(argv,14)&&IS_A_FLOAT(argv,15)&&IS_A_FLOAT(argv,16))
    {
        w = (int)atom_getintarg(0, argc, argv);
        h = (int)atom_getintarg(1, argc, argv);
        min = (double)atom_getfloatarg(2, argc, argv);
        max = (double)atom_getfloatarg(3, argc, argv);
        lilo = (int)atom_getintarg(4, argc, argv);
        iem_inttosymargs(&x->x_gui, atom_getintarg(5, argc, argv));
        iemgui_new_getnames(&x->x_gui, 6, argv);
        ldx = (int)atom_getintarg(9, argc, argv);
        ldy = (int)atom_getintarg(10, argc, argv);
        iem_inttofstyle(x, atom_getintarg(11, argc, argv));
        fs = (int)atom_getintarg(12, argc, argv);
        bflcol[0] = (int)atom_getintarg(13, argc, argv);
        bflcol[1] = (int)atom_getintarg(14, argc, argv);
        bflcol[2] = (int)atom_getintarg(15, argc, argv);
        v = (int)atom_getintarg(16, argc, argv);
    }
    else iemgui_new_getnames(&x->x_gui, 6, 0);

    if((argc == 18)&&IS_A_FLOAT(argv,17))
        steady = (int)atom_getintarg(17, argc, argv);

    x->x_gui.x_draw = (t_iemfunptr)mknob_draw;
    x->x_gui.x_glist = (t_glist *)canvas_getcurrent();
    if(x->x_gui.x_loadinit)
        x->x_val = v;
    else
        x->x_val = 0;
    x->x_pos = x->x_val;
    if(lilo != 0) lilo = 1;
    x->x_lin0_log1 = lilo;
    if(steady != 0) steady = 1;
    x->x_steady = steady;
    if (x->x_gui.x_font_style == 1)
    {
        //strcpy(x->x_gui.x_font, "helvetica");
    }
    else if (x->x_gui.x_font_style == 2)
    {
        //strcpy(x->x_gui.x_font, "times");
    }
    else
    {
        x->x_gui.x_font_style = 0;
    }
    if (iemgui_has_rcv(x)) pd_bind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    x->x_gui.x_ldx = ldx;
    x->x_gui.x_ldy = ldy;
    if(fs < 4)
        fs = 4;
    x->x_gui.x_fontsize = fs;
    //x->x_gui.x_h = iemgui_clip_size(h);
    //x->x_gui.x_w = iemgui_clip_size(w);
    mknob_check_wh(x, w, h);
    //mknob_check_width(x, w);
    mknob_check_minmax(x, min, max);
    iemgui_all_colfromload(&x->x_gui, bflcol);
    x->x_thick = 0;
    iemgui_verify_snd_ne_rcv(&x->x_gui);
    outlet_new(&x->x_gui.x_obj, &s_float);
    x->x_gui.x_obj.te_iemgui = 1;

    x->x_gui.x_handle = scalehandle_new((t_object *)x,
        x->x_gui.x_glist, 1, mknob__clickhook, mknob__motionhook);
    x->x_gui.x_lhandle = scalehandle_new((t_object *)x,
        x->x_gui.x_glist, 0, mknob__clickhook, mknob__motionhook);

    return (x);
}

static void mknob_free(t_mknob *x)
{
    if (iemgui_has_rcv(x))
        pd_unbind(&x->x_gui.x_obj.ob_pd, x->x_gui.x_rcv);
    gfxstub_deleteforkey(x);
}

extern void canvas_iemguis(t_glist *gl, t_symbol *guiobjname);

void canvas_mknob(t_glist *gl, t_symbol *s, int argc, t_atom *argv)
{
    canvas_iemguis(gl, gensym("mknob"));
}

void mknob_setup(void)
{
    mknob_class = class_new(gensym("mknob"), (t_newmethod)mknob_new,
                            (t_method)mknob_free, sizeof(t_mknob), 0, A_GIMME, 0);
#ifndef GGEE_mknob_COMPATIBLE
//    class_addcreator((t_newmethod)mknob_new, gensym("mknob"), A_GIMME, 0);
#endif
    class_addbang(mknob_class,mknob_bang);
    class_addfloat(mknob_class,mknob_float);
    //class_addlist(mknob_class, mknob_list);
    class_addmethod(mknob_class, (t_method)mknob_click, gensym("click"),
                    A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(mknob_class, (t_method)mknob_motion, gensym("motion"),
                    A_FLOAT, A_FLOAT, 0);
    class_addmethod(mknob_class, (t_method)mknob_dialog, gensym("dialog"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_loadbang, gensym("loadbang"),
        A_DEFFLOAT, 0);
    class_addmethod(mknob_class, (t_method)mknob_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(mknob_class, (t_method)mknob_size, gensym("size"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_delta, gensym("delta"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_pos, gensym("pos"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_range, gensym("range"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_color, gensym("color"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(mknob_class, (t_method)mknob_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(mknob_class, (t_method)mknob_label, gensym("label"), A_DEFSYM, 0);
    class_addmethod(mknob_class, (t_method)mknob_label_pos, gensym("label_pos"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_label_font, gensym("label_font"), A_GIMME, 0);
    class_addmethod(mknob_class, (t_method)mknob_log, gensym("log"), 0);
    class_addmethod(mknob_class, (t_method)mknob_lin, gensym("lin"), 0);
    class_addmethod(mknob_class, (t_method)mknob_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(mknob_class, (t_method)mknob_steady, gensym("steady"), A_FLOAT, 0);
    /*if(!iemgui_key_sym)
    iemgui_key_sym = gensym("#keyname");*/
    mknob_widgetbehavior.w_getrectfn =    mknob_getrect;
    mknob_widgetbehavior.w_displacefn =   iemgui_displace;
#ifdef PDL2ORK
    mknob_widgetbehavior.w_displacefnwtag =   iemgui_displace_withtag;
#endif
    mknob_widgetbehavior.w_selectfn =     iemgui_select;
    mknob_widgetbehavior.w_activatefn =   NULL;
    mknob_widgetbehavior.w_deletefn =     iemgui_delete;
    mknob_widgetbehavior.w_visfn =        iemgui_vis;
    mknob_widgetbehavior.w_clickfn =      mknob_newclick;
    //mknob_widgetbehavior.w_propertiesfn = mknob_properties;
    //mknob_widgetbehavior.w_savefn =       mknob_save;
    class_setwidget(mknob_class, &mknob_widgetbehavior);

    class_setsavefn(mknob_class, mknob_save);
    class_setpropertiesfn(mknob_class, mknob_properties);
    class_addmethod(canvas_class, (t_method)canvas_mknob, gensym("mknob"),
                    A_GIMME, A_NULL);
}
