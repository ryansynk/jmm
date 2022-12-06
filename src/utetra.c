#include <jmm/utetra.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jmm/array.h>
#include <jmm/bb.h>
#include <jmm/eik3.h>
#include <jmm/mat.h>
#include <jmm/mesh3.h>
#include <jmm/opt.h>
#include <jmm/util.h>

#include "macros.h"

#define MAX_NITER 100

struct utetra {
  eik3_s const *eik;

  dbl lam[2]; // Current iterate
  dbl f;
  dbl g[2];
  dbl H[2][2];
  dbl p[2]; // Newton step
  dbl x_minus_xb[3];
  dbl L;

  dbl tol;
  int niter;

  size_t lhat, l[3];

  dbl x[3]; // x[l]
  dbl X[3][3]; // X = [x[l0] x[l1] x[l2]]
  dbl Xt[3][3]; // X'
  dbl XtX[3][3]; // X'*X

  // B-coefs for 9-point triangle interpolation T on base of update
  bb32 T;
};

void utetra_alloc(utetra_s **utetra) {
  *utetra = malloc(sizeof(utetra_s));
}

void utetra_dealloc(utetra_s **utetra) {
  free(*utetra);
  *utetra = NULL;
}

void utetra_init(utetra_s *u, eik3_s const *eik, size_t lhat, uint3 const l) {
  u->eik = eik;

  mesh3_s const *mesh = eik3_get_mesh(eik);

  /* Initialize f with INFINITY---this needs to be done so that `u`
   * `cmp`s correctly with initialized `utetra` (i.e., if we sort an
   * array of `utetra`, uninitialized `utetra` will move to the back
   * so that they can be easily ignored). */
  u->f = INFINITY;

  /* Initialize Newton iteration variables with dummy values */
  u->lam[0] = u->lam[1] = NAN;
  u->g[0] = u->g[1] = NAN;
  u->H[0][0] = u->H[1][0] = u->H[0][1] = u->H[1][1] = NAN;
  u->p[0] = u->p[1] = NAN;
  u->x_minus_xb[0] = u->x_minus_xb[1] = u->x_minus_xb[2] = NAN;
  u->L = NAN;

  u->tol = mesh3_get_face_tol(mesh, l);

  u->lhat = lhat;

  memcpy(u->l, l, sizeof(size_t[3]));

  mesh3_copy_vert(mesh, u->lhat, u->x);
  for (size_t i = 0; i < 3; ++i)
    mesh3_copy_vert(mesh, u->l[i], u->Xt[i]);

  dbl33_transposed(u->Xt, u->X);
  dbl33_mul(u->Xt, u->X, u->XtX);

  /* init jets, depending on how they've been specified */
  jet31t jet[3];
  for (size_t i = 0; i < 3; ++i) {
    jet[i] = eik3_get_jet(eik, l[i]);
    assert(jet31t_is_finite(&jet[i])); /* shouldn't be singular! */
  }

  /* Do BB interpolation to set up the coefficients of T. */
  dbl3 T, DT[3];
  for (int i = 0; i < 3; ++i) {
    T[i] = jet[i].f;
    memcpy(DT[i], jet[i].Df, sizeof(dbl3));
  }
  bb32_init_from_3d_data(&u->T, T, DT, u->Xt);
}

/* Check if the point being updated lies in the plane spanned by by
 * x0, x1, and x2. If it does, the update is degenerate. */
bool utetra_is_degenerate(utetra_s const *u) {
  dbl const *x[4] = {u->x, u->Xt[0], u->Xt[1], u->Xt[2]};
  return points_are_coplanar(x);
}

static void set_lambda_stype_constant(utetra_s *cf, dbl const lam[2]) {
  // TODO: question... would it make more sense to use different
  // vectors for a1 and a2? This choice seems to result in a lot of
  // numerical instability. For now I'm fixing this by replacing sums
  // and dot products involving a1 or a2 with the Neumaier equivalent.
  static dbl a1[3] = {-1, 1, 0};
  static dbl a2[3] = {-1, 0, 1};

  dbl b[3], xb[3], tmp1[3], tmp2[3][3], DL[2], D2L[2][2], DT[2], D2T[2][2];
  dbl tmp3[2];

  cf->lam[0] = lam[0];
  cf->lam[1] = lam[1];

  b[1] = lam[0];
  b[2] = lam[1];
  b[0] = 1 - b[1] - b[2];

  assert(dbl3_valid_bary_coord(b));

  dbl33_dbl3_mul(cf->X, b, xb);
  dbl3_sub(cf->x, xb, cf->x_minus_xb);
  cf->L = dbl3_norm(cf->x_minus_xb);
  assert(cf->L > 0);

  dbl33_dbl3_mul(cf->Xt, cf->x_minus_xb, tmp1);
  dbl3_dbl_div(tmp1, -cf->L, tmp1);

  DL[0] = dbl3_ndot(a1, tmp1);
  DL[1] = dbl3_ndot(a2, tmp1);
  assert(dbl2_isfinite(DL));

  dbl3_outer(tmp1, tmp1, tmp2);
  dbl33_sub(cf->XtX, tmp2, tmp2);
  dbl33_dbl_div(tmp2, cf->L, tmp2);

  dbl33_dbl3_nmul(tmp2, a1, tmp1);
  D2L[0][0] = dbl3_ndot(tmp1, a1);
  D2L[1][0] = D2L[0][1] = dbl3_ndot(tmp1, a2);
  dbl33_dbl3_nmul(tmp2, a2, tmp1);
  D2L[1][1] = dbl3_ndot(tmp1, a2);
  assert(dbl22_isfinite(D2L));

  DT[0] = bb32_df(&cf->T, b, a1);
  DT[1] = bb32_df(&cf->T, b, a2);

  D2T[0][0] = bb32_d2f(&cf->T, b, a1, a1);
  D2T[1][0] = D2T[0][1] = bb32_d2f(&cf->T, b, a1, a2);
  D2T[1][1] = bb32_d2f(&cf->T, b, a2, a2);

  cf->f = cf->L + bb32_f(&cf->T, b);
  assert(isfinite(cf->f));

  dbl2_add(DL, DT, cf->g);
  assert(dbl2_isfinite(cf->g));

  dbl22_add(D2L, D2T, cf->H);
  assert(dbl22_isfinite(cf->H));

  /**
   * Finally, compute Newton step solving the minimization problem:
   *
   *     minimize  y’*H*y/2 + [g - H*x]’*y + [x’*H*x/2 - g’*x + f(x)]
   *   subject to  x >= 0
   *               sum(x) <= 1
   *
   * perturbing the Hessian below should ensure a descent
   * direction. (It would be interesting to see if we can remove the
   * perturbation entirely.)
   */

  // Compute the trace and determinant of the Hessian
  dbl tr = cf->H[0][0] + cf->H[1][1];
  dbl det = cf->H[0][0]*cf->H[1][1] - cf->H[0][1]*cf->H[1][0];
  assert(tr != 0 && det != 0);

  // Conditionally perturb the Hessian
  dbl min_eig_doubled = tr - sqrt(tr*tr - 4*det);
  if (min_eig_doubled < 0) {
    cf->H[0][0] -= min_eig_doubled;
    cf->H[1][1] -= min_eig_doubled;
  }

  // Solve a quadratic program to find the next iterate.
  triqp2_s qp;
  dbl22_dbl2_mul(cf->H, lam, tmp3);
  dbl2_sub(cf->g, tmp3, qp.b);
  memcpy((void *)qp.A, (void *)cf->H, sizeof(dbl)*2*2);
  triqp2_solve(&qp);

  // Compute the projected Newton step from the current iterate and
  // next iterate.
  dbl2_sub(qp.x, lam, cf->p);
}

static void set_lambda(utetra_s *utetra, dbl2 const lam) {
  switch(eik3_get_stype(utetra->eik)) {
  case STYPE_CONSTANT:
    set_lambda_stype_constant(utetra, lam);
    break;
  default:
    assert(false);
  }
}

static void step(utetra_s *cf) {
  dbl const atol = 1e-15, c1 = 1e-4;

  dbl lam1[2], f, c1_times_g_dot_p, beta;

  // Get values for current iterate
  dbl lam[2] = {cf->lam[0], cf->lam[1]};
  dbl p[2] = {cf->p[0], cf->p[1]};
  f = cf->f;

  // Do backtracking line search
  beta = 1;
  c1_times_g_dot_p = c1*dbl2_dot(p, cf->g);
  dbl2_saxpy(beta, p, lam, lam1);
  set_lambda(cf, lam1);
  while (cf->f > f + beta*c1_times_g_dot_p + atol) {
    beta *= 0.9;
    dbl2_saxpy(beta, p, lam, lam1);
    set_lambda(cf, lam1);
  }

  ++cf->niter;
}

/**
 * Do a tetrahedron update starting at `lam`, writing the result to
 * `jet`. If `lam` is `NULL`, then the first iterate will be selected
 * automatically.
 */
void utetra_solve(utetra_s *cf, dbl const *lam) {
  cf->niter = 0;

  if (lam == NULL)
    set_lambda(cf, (dbl[2]) {0, 0}); // see, automatically!
  else
    set_lambda(cf, lam);

  int const max_niter = 100;
  for (int _ = 0; _ < max_niter; ++_) {
    if (dbl2_norm(cf->p) <= cf->tol)
      break;
    step(cf);
  }
}

static void get_b(utetra_s const *cf, dbl b[3]) {
  assert(!isnan(cf->lam[0]) && !isnan(cf->lam[1]));
  b[0] = 1 - cf->lam[0] - cf->lam[1];
  b[1] = cf->lam[0];
  b[2] = cf->lam[1];
}

dbl utetra_get_value(utetra_s const *cf) {
  return cf->f;
}

void get_that_stype_constant(utetra_s const *u, dbl that[3]) {
  dbl3_normalized(u->x_minus_xb, that);
}

static void get_that(utetra_s const *u, dbl that[3]) {
  switch(eik3_get_stype(u->eik)) {
  case STYPE_CONSTANT:
    get_that_stype_constant(u, that);
    break;
  default:
    assert(false);
  }
}

void utetra_get_jet31t(utetra_s const *cf, jet31t *jet) {
  jet->f = cf->f;

  get_that(cf, jet->Df);
}

/**
 * Compute the Lagrange multipliers for the constraint optimization
 * problem corresponding to this type of update
 */
static void get_lag_mults(utetra_s const *cf, dbl alpha[3]) {
  dbl const atol = 5e-15;
  dbl b[3] = {1 - dbl2_sum(cf->lam), cf->lam[0], cf->lam[1]};
  alpha[0] = alpha[1] = alpha[2] = 0;
  // TODO: optimize this
  if (fabs(b[0] - 1) < atol) {
    alpha[0] = 0;
    alpha[1] = -cf->g[0];
    alpha[2] = -cf->g[1];
  } else if (fabs(b[1] - 1) < atol) {
    alpha[0] = cf->g[0];
    alpha[1] = 0;
    alpha[2] = cf->g[0] - cf->g[1];
  } else if (fabs(b[2] - 1) < atol) {
    alpha[0] = cf->g[0];
    alpha[1] = cf->g[0] - cf->g[1];
    alpha[2] = 0;
  } else if (fabs(b[0]) < atol) { // b[1] != 0 && b[2] != 0
    alpha[0] = dbl2_sum(cf->g)/2;
    alpha[1] = 0;
    alpha[2] = 0;
  } else if (fabs(b[1]) < atol) { // b[0] != 0 && b[2] != 0
    alpha[0] = 0;
    alpha[1] = -cf->g[0];
    alpha[2] = 0;
  } else if (fabs(b[2]) < atol) { // b[0] != 0 && b[1] != 0
    alpha[0] = 0;
    alpha[1] = 0;
    alpha[2] = -cf->g[1];
  } else {
    assert(dbl3_valid_bary_coord(b));
  }
}

bool utetra_has_interior_point_solution(utetra_s const *u) {
  /* check whether minimizer is an interior point minimizer */
  dbl alpha[3];
  get_lag_mults(u, alpha);
  return dbl3_maxnorm(alpha) <= 1e-15;
}

bool utetra_is_backwards(utetra_s const *utetra, eik3_s const *eik) {
  mesh3_s const *mesh = eik3_get_mesh(eik);

  dbl3 x0, dx[2];
  mesh3_copy_vert(mesh, utetra->l[0], x0);
  dbl3_sub(mesh3_get_vert_ptr(mesh, utetra->l[1]), x0, dx[0]);
  dbl3_sub(mesh3_get_vert_ptr(mesh, utetra->l[2]), x0, dx[1]);

  dbl3 t;
  dbl3_cross(dx[0], dx[1], t);

  dbl3 dxhat0;
  dbl3_sub(mesh3_get_vert_ptr(mesh, utetra->lhat), x0, dxhat0);

  if (dbl3_dot(dxhat0, t) < 0)
    dbl3_negate(t);

  jet31t const *jet = eik3_get_jet_ptr(eik);

  for (size_t i = 0; i < 3; ++i)
    if (dbl3_dot(t, jet[utetra->l[i]].Df) <= 0)
      return true;

  return false;
}

#if JMM_DEBUG
static bool update_inds_are_set(utetra_s const *utetra) {
  return !(
    utetra->l[0] == (size_t)NO_INDEX ||
    utetra->l[1] == (size_t)NO_INDEX ||
    utetra->l[2] == (size_t)NO_INDEX);
}
#endif

#if JMM_DEBUG
static bool all_inds_are_set(utetra_s const *utetra) {
  return utetra->lhat != (size_t)NO_INDEX && update_inds_are_set(utetra);
}
#endif

bool utetra_ray_is_occluded(utetra_s const *utetra, eik3_s const *eik) {
#if JMM_DEBUG
  assert(all_inds_are_set(utetra));
#endif
  mesh3_s const *mesh = eik3_get_mesh(eik);
  par3_s par = utetra_get_parent(utetra);
  return mesh3_local_ray_is_occluded(mesh, utetra->lhat, &par);
}

int utetra_get_num_interior_coefs(utetra_s const *utetra) {
  dbl const atol = 1e-14;
  dbl b[3];
  get_b(utetra, b);
  return (atol < b[0] && b[0] < 1 - atol)
       + (atol < b[1] && b[1] < 1 - atol)
       + (atol < b[2] && b[2] < 1 - atol);
}

size_t utetra_get_l(utetra_s const *utetra) {
  assert(utetra->lhat != (size_t)NO_INDEX);
  return utetra->lhat;
}

/* Get the triangle of `u`. If `u` is split, this does *not* return
 * the triangle of the split updates (since that would not be
 * well-defined!). */
static tri3 get_tri(utetra_s const *u) {
  tri3 tri;
  memcpy(tri.v, u->Xt, sizeof(dbl[3][3]));
  return tri;
}

size_t utetra_get_active_inds(utetra_s const *utetra, uint3 la) {
#if JMM_DEBUG
  assert(update_inds_are_set(utetra));
#endif

  la[0] = la[1] = la[2] = (size_t)NO_INDEX;

  dbl3 b;
  get_b(utetra, b);

  size_t na = 0;
  for (size_t i = 0; i < 3; ++i)
    if (b[i] > EPS)
      la[na++] = utetra->l[i];
  SORT_UINT3(la);

  return na;
}

par3_s utetra_get_parent(utetra_s const *utetra) {
  par3_s par; get_b(utetra, par.b);
  assert(dbl3_valid_bary_coord(par.b));
  memcpy(par.l, utetra->l, sizeof(size_t[3]));
  return par;
}

void utetra_get_other_inds(utetra_s const *utetra, size_t li, size_t l[2]) {
  assert(utetra->l[0] == li || utetra->l[1] == li || utetra->l[2]);

  for (size_t i = 0, j = 0; i < 3; ++i)
    if (utetra->l[i] != li)
      l[j++] = utetra->l[i];
}

bool utetra_index_is_active(utetra_s const *utetra, size_t l) {
  assert(l == utetra->l[0] || l == utetra->l[1] || l == utetra->l[2]);

  dbl lam0 = 1 - utetra->lam[0] - utetra->lam[1];

  if (l == utetra->l[0])
    return lam0 != 0;
  else if (l == utetra->l[1])
    return utetra->lam[0] != 0;
  else if (l == utetra->l[2])
    return utetra->lam[1] != 0;
  else
    assert(false);
}

bool utetra_has_inds(utetra_s const *u, size_t lhat, uint3 const l) {
  return u->lhat == lhat && uint3_equal(u->l, l);
}

static void get_other_inds(array_s const *utetras, size_t la, size_t i, uint2 l_other) {
  utetra_s const *utetra_other;
  array_get(utetras, i, &utetra_other);
  utetra_get_other_inds(utetra_other, la, l_other);
}

static bool
utetras_bracket_ray_1(utetra_s const *utetra, size_t la, array_s const *utetras) {
  size_t num_utetra_other = array_size(utetras);

  size_t i_current = 0;

  uint2 l_other = {NO_INDEX, NO_INDEX};;
  get_other_inds(utetras, la, i_current, l_other);

  size_t l_start = l_other[0];
  size_t l_current = l_other[1];

  /* Next, we walk around the ring of utetra until we've visited all
   * of them... */
  size_t num_visited = 1;
  while (num_visited++ < num_utetra_other) {
    /* Search through the utetra for the one we should step to next */
    for (size_t i = 0; i < num_utetra_other; ++i) {
      if (i == i_current)
        continue;

      get_other_inds(utetras, la, i, l_other);
      if (l_current != l_other[0] && l_current != l_other[1])
        continue;

      /* Update l_current and i_current and break */
      l_current = l_current == l_other[0] ? l_other[1] : l_other[0];
      i_current = i;
      break;

      /* TODO: actually check whether these vertices form a ring
       * around the active index after projecting onto the normal
       * plane */
      (void)utetra;
    }
  }

  /* We're done walking... check if we're back where we started */
  return l_start == l_current;
}

static void get_unit_normal_for_verts_by_index(mesh3_s const *mesh,
                                               uint3 const l, dbl3 n) {
  dbl3 x0;
  mesh3_copy_vert(mesh, l[0], x0);

  dbl3 dx[2];
  for (size_t i = 0; i < 2; ++i)
    dbl3_sub(mesh3_get_vert_ptr(mesh, l[i + 1]), x0, dx[i]);

  dbl3_cross(dx[0], dx[1], n);
  dbl3_normalize(n);
}

static bool
utetras_bracket_ray_2(utetra_s const *utetra, array_s const *utetras) {
  mesh3_s const *mesh = eik3_get_mesh(utetra->eik);

  size_t l = utetra_get_l(utetra);

  par3_s par = utetra_get_parent(utetra);
  uint3 la, li;
  size_t na = par3_get_active_and_inactive_inds(&par, la, li);
  assert(na == 2);
  SORT_UINT3(la);
  SORT_UINT3(li);

  dbl3 xa, xi;
  mesh3_copy_vert(mesh, la[0], xa);
  mesh3_copy_vert(mesh, li[0], xi);

  for (size_t i = 0; i < array_size(utetras); ++i) {
    utetra_s const *utetra_other;
    array_get(utetras, i, &utetra_other);

    assert(l == utetra_get_l(utetra_other));

    par3_s par_ = utetra_get_parent(utetra_other);

    uint3 la_, li_;
    size_t na_ = par3_get_active_and_inactive_inds(&par_, la_, li_);
    SORT_UINT3(la_);
    SORT_UINT3(li_);

    /* Check if both `utetra` have the same active indices */
    if (na_ != 2 && la_[0] != la[0] && la_[1] != la[1])
      continue;

    /* Get the unit normal for the plane determined by the update
     * index and the active edge */
    dbl3 n;
    get_unit_normal_for_verts_by_index(mesh, (uint3) {l, la[0], la[1]}, n);

    /* Get the inactive vertex for the current other utetra */
    dbl3 xi_;
    mesh3_copy_vert(mesh, li_[0], xi_);

    /* Check whether the two inactive vertices lie on either side of
     * the plane */
    dbl n_dot_xa = dbl3_dot(n, xa);
    dbl dp = dbl3_dot(n, xi) - n_dot_xa;
    dbl dp_ = dbl3_dot(n, xi_) - n_dot_xa;
    if (sgn(dp) != sgn(dp_))
      return true;
  }

  return false;
}

bool utetra_is_bracketed_by_utetras(utetra_s const *utetra, array_s const *utetras) {
  if (array_is_empty(utetras))
    return false;

  par3_s par = utetra_get_parent(utetra);
  uint3 la;
  size_t na = par3_get_active_inds(&par, la);
  assert(na == 1 || na == 2);

  if (na == 1)
    return utetras_bracket_ray_1(utetra, la[0], utetras);
  else if (na == 2)
    return utetras_bracket_ray_2(utetra, utetras);
  else
    assert(false);
}

static dbl get_edge_lam(utetra_s const *u, uint2 const le) {
  assert(uint3_contains_uint2(u->l, le));

  /* Get the barycentric coordinates of the optimum */
  dbl3 b; get_b(u, b);

#if JMM_DEBUG
  { uint3 l_diff;
    size_t n = uint3_diff_uint2(u->l, le, l_diff);
    assert(n == 1);
    assert(fabs(b[uint3_find(u->l, l_diff[0])]) < EPS); }
#endif

  /* Get the index of `le[0]` in `u->l` */
  size_t i0 = uint3_find(u->l, le[0]);

  /* Compute the convex coefficent `lam = 1 - b[i0]` such that `x_opt`
   * is `(1 - lam)*x[le[0]] + lam*x[le[1]]`. */
  dbl lam = 1 - b[i0];

  return lam;
}

size_t get_common_inds(uint3 const la1, uint3 const la2, uint3 le) {
  assert(uint3_is_sorted(la1));
  assert(uint3_is_sorted(la2));

  le[0] = le[1] = le[2] = (size_t)NO_INDEX;

  size_t ne = 0;
  while (la1[ne] == la2[ne] && la1[ne] != (size_t)NO_INDEX) {
    le[ne] = la1[ne];
    ++ne;
  }
  for (size_t i = ne; i < 3; ++i) {
    if ((la1[ne] == (size_t)NO_INDEX) ^ (la2[ne] == (size_t)NO_INDEX)) {
      le[ne] = la1[ne] == (size_t)NO_INDEX ? la2[ne] : la1[ne];
      ++ne;
    }
  }

  return ne;
}

bool utetras_have_same_minimizer(utetra_s const *u1, utetra_s const *u2) {
  assert(u1->eik == u2->eik);

  uint3 la1, la2;
  size_t na1 = utetra_get_active_inds(u1, la1);
  size_t na2 = utetra_get_active_inds(u2, la2);
  assert(na1 < 3 && na2 < 3);

  if (na1 == 1 && na2 == 1)
    return la1[0] == la2[0];

  if (na1 == 2 && na2 == 2 && !uint3_equal(la1, la2))
    return false;

  uint3 le;
  size_t ne = get_common_inds(la1, la2, le);
  assert(ne <= 2);
  if (ne == 0)
    return false;

  /* The effective tolerance used to check the closeness of the two
   * minimizers. Its value depends on which indices are active for
   * each tetrahedron update. */
  dbl edge_tol = mesh3_get_edge_tol(eik3_get_mesh(u1->eik), le);
  edge_tol = fmax(edge_tol, fmax(u1->tol, u2->tol));

  dbl lam1;
  if (na1 == 2)
    lam1 = get_edge_lam(u1, le);
  else
    lam1 = la1[0] == le[0] ? 0 : 1;

  dbl lam2;
  if (na2 == 2)
    lam2 = get_edge_lam(u2, le);
  else
    lam2 = la2[0] == le[0] ? 0 : 1;

  bool same = fabs(lam1 - lam2) <= edge_tol;

  return same;
}

bool utetras_have_same_inds(utetra_s const *u1, utetra_s const *u2) {
  if (u1->lhat != u2->lhat)
    return false;

  size_t l1[3];
  memcpy(l1, u1->l, sizeof(size_t[3]));
  SORT3(l1[0], l1[1], l1[2]);

  size_t l2[3];
  memcpy(l2, u2->l, sizeof(size_t[3]));
  SORT3(l2[0], l2[1], l1[2]);

  return l1[0] == l2[0] && l1[1] == l2[1] && l1[2] == l2[2];
}

#if JMM_TEST
void utetra_step(utetra_s *u) {
  step(u);
}

void utetra_get_lambda(utetra_s const *u, dbl lam[2]) {
  lam[0] = u->lam[0];
  lam[1] = u->lam[1];
}

void utetra_set_lambda(utetra_s *u, dbl const lam[2]) {
  set_lambda(u, lam);
}

size_t utetra_get_num_iter(utetra_s const *u) {
  return u->niter;
}
#endif
