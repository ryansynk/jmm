#pragma once

#include "mat.h"
#include "vec.h"

dblz F(dbl x);
int N(dbl beta, int n, int sign);
dbl a(dbl beta, int n, int sign);
dbl phi_in(dbl3 t_in, dbl3 t_e, dbl3 t_o, dbl3 n_o);
dbl phi_out(dbl3 t_out, dbl3 t_e, dbl3 t_o, dbl3 n_o);
dbl beta(dbl3 t_in, dbl3 t_out, dbl3 t_e, dbl3 t_o, dbl3 n_o, int sign);
dbl L(dbl3 x, dbl3 t_e, dbl3 x_e, dbl3 t_in, dbl33 D2T, dbl3 grad, dbl3 t_out);
dbl Di(dbl k, int n, dbl3 t_in, dbl3 t_out, dbl3 t_e, dbl3 t_o, dbl3 n_o,
       int sign_a, int sign_beta);
dbl D(dbl3 x, dbl refl_coef, dbl k, int n, dbl3 t_in, dbl3 t_out, dbl3 t_e,
      dbl3 t_o, dbl3 n_o, int sign_a, int sign_beta);