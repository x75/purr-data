/* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

iemlib1 written by Thomas Musil, Copyright (c) IEM KUG Graz Austria 2000 - 2010 */


#include "m_pd.h"
#include "iemlib.h"
#include <math.h>

/* ---------------- peakenv~ - simple peak-envelope-converter. ----------------- */
/* -- now with double precision; for low-frequency filters it is important to calculate the filter in double precision -- */

typedef struct _peakenv_tilde
{
  t_object x_obj;
  double   x_sr;
  double   x_old_peak;
  double   x_c1;
  double   x_releasetime;
  t_float  x_float_sig_in;
} t_peakenv_tilde;

static t_class *peakenv_tilde_class;

static void peakenv_tilde_reset(t_peakenv_tilde *x)
{
  x->x_old_peak = 0.0;
}

static void peakenv_tilde_ft1(t_peakenv_tilde *x, t_floatarg f)/* release-time in ms */
{
  if(f < 0.0)
    f = 0.0;
  x->x_releasetime = (double)f;
  x->x_c1 = exp(-1.0/(x->x_sr*0.001*x->x_releasetime));
}

static t_int *peakenv_tilde_perform(t_int *w)
{
  t_sample *in = (t_sample *)(w[1]);
  t_sample *out = (t_sample *)(w[2]);
  t_peakenv_tilde *x = (t_peakenv_tilde *)(w[3]);
  int n = (int)(w[4]);
  double peak = x->x_old_peak;
  double c1 = x->x_c1;
  double absolute;
  int i;
  
  for(i=0; i<n; i++)
  {
    absolute = (double)fabs(*in++);
    peak *= c1;
    if(absolute > peak)
      peak = absolute;
    *out++ = (t_sample)peak;
  }
  /* NAN protect */
  //if(IEM_DENORMAL(peak))
  //  peak = 0.0f;
  x->x_old_peak = peak;
  return(w+5);
}

static void peakenv_tilde_dsp(t_peakenv_tilde *x, t_signal **sp)
{
  x->x_sr = (double)sp[0]->s_sr;
  peakenv_tilde_ft1(x, x->x_releasetime);
  dsp_add(peakenv_tilde_perform, 4, sp[0]->s_vec, sp[1]->s_vec, x, (t_int)sp[0]->s_n);
}

static void *peakenv_tilde_new(t_floatarg f)
{
  t_peakenv_tilde *x = (t_peakenv_tilde *)pd_new(peakenv_tilde_class);
  
  if(f <= 0.0)
    f = 0.0;
  x->x_sr = 44100.0;
  peakenv_tilde_ft1(x, f);
  x->x_old_peak = 0.0;
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ft1"));
  outlet_new(&x->x_obj, &s_signal);
  x->x_float_sig_in = 0.0;
  return(x);
}

void peakenv_tilde_setup(void)
{
  peakenv_tilde_class = class_new(gensym("peakenv~"), (t_newmethod)peakenv_tilde_new,
    0, sizeof(t_peakenv_tilde), 0, A_DEFFLOAT, 0);
  CLASS_MAINSIGNALIN(peakenv_tilde_class, t_peakenv_tilde, x_float_sig_in);
  class_addmethod(peakenv_tilde_class, (t_method)peakenv_tilde_dsp, gensym("dsp"), A_CANT, 0);
  class_addmethod(peakenv_tilde_class, (t_method)peakenv_tilde_ft1, gensym("ft1"), A_FLOAT, 0);
  class_addmethod(peakenv_tilde_class, (t_method)peakenv_tilde_reset, gensym("reset"), 0);
}
