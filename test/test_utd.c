#include <cgreen/cgreen.h>
#include <jmm/utd.h>

Describe(utd);
BeforeEach(utd) {}
AfterEach(utd) {}

Ensure(utd, test_F) {
  dbl x = 0.0;
  dblz z = F(x);
}

Ensure(utd, test_N) {
  int n = 0;
  int sign = 1;
  dbl beta = 0.0;
  int x = N(beta, n, sign);
}

Ensure(utd, test_a) {
  dbl beta = 0.0;
  int n = 0;
  int sign = 1;
  dbl x = a(beta, n, sign);
}

Ensure(utd, test_phi_in) {
  dbl3 t_in = {0.0, 1.0, 0.0};
  dbl3 t_e = {1.0, 0.0, 0.0};
  dbl3 t_o = {1.0, 0.0, 0.0};
  dbl3 n_o = {1.0, 0.0, 0.0};
  dbl phi = phi_in(t_in, t_e, t_o, n_o);
}

Ensure(utd, test_phi_out) {
  dbl3 t_out = {0.0, 1.0, 0.0};
  dbl3 t_e = {1.0, 0.0, 0.0};
  dbl3 t_o = {1.0, 0.0, 0.0};
  dbl3 n_o = {1.0, 0.0, 0.0};
  dbl phi = phi_out(t_out, t_e, t_o, n_o);
}

Ensure(utd, test_beta) {
  dbl3 t_in = {0.0, 1.0, 0.0};
  dbl3 t_out = {0.0, 1.0, 0.0};
  dbl3 t_e = {1.0, 0.0, 0.0};
  dbl3 t_o = {1.0, 0.0, 0.0};
  dbl3 n_o = {1.0, 0.0, 0.0};
  int sign = 1;
  dbl b = beta(t_in, t_out, t_e, t_o, n_o, sign);
}

Ensure(utd, test_L) {
  dbl3 x = {1.0, 0.0, 0.0};
  dbl3 t_e = {0.0, 1.0, 0.0};
  dbl3 x_e = {1.0, 0.0, 0.0};
  dbl3 t_in = {1.0, 0.0, 0.0};
  dbl33 hess = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  dbl3 grad = {1.0, 0.0, 0.0};
  dbl l = L(x, t_e, x_e, t_in, hess, grad);
}

Ensure(utd, test_Di) {
  dbl k = 1.0;
  int n = 1;
  dbl3 t_in = {0.0, 1.0, 0.0};
  dbl3 t_out = {0.0, 1.0, 0.0};
  dbl3 t_e = {1.0, 0.0, 0.0};
  dbl3 t_o = {1.0, 0.0, 0.0};
  dbl3 n_o = {1.0, 0.0, 0.0};
  int sign_a = 1;
  int sign_beta = 1;
  dbl d1 = Di(k, n, t_in, t_out, t_e, t_o, n_o, sign_a, sign_beta);
}

Ensure(utd, test_D) {
  dbl3 x = {1.0, 0.0, 0.0};
  dbl refl_coef = 1.0;
  dbl k = 1.0;
  int n = 1;
  dbl3 t_in = {0.0, 1.0, 0.0};
  dbl3 t_out = {0.0, 1.0, 0.0};
  dbl3 t_e = {1.0, 0.0, 0.0};
  dbl3 t_o = {1.0, 0.0, 0.0};
  dbl3 n_o = {1.0, 0.0, 0.0};
  int sign_a = 1;
  int sign_beta = 1;

  dbl d = D(x, refl_coef, k, n, t_in, t_out, t_e, t_o, n_o, sign_a, sign_beta);
}

TestSuite *utd_tests() {
  TestSuite *suite = create_test_suite();
  add_test_with_context(suite, utd, test_F);
  add_test_with_context(suite, utd, test_N);
  add_test_with_context(suite, utd, test_a);
  add_test_with_context(suite, utd, test_phi_in);
  add_test_with_context(suite, utd, test_phi_out);
  add_test_with_context(suite, utd, test_beta);
  add_test_with_context(suite, utd, test_L);
  add_test_with_context(suite, utd, test_Di);
  add_test_with_context(suite, utd, test_D);
  return suite;
}