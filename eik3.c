#include "eik3.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "array.h"
#include "bb.h"
#include "heap.h"
#include "macros.h"
#include "mat.h"
#include "mesh3.h"
#include "utetra.h"
#include "utri.h"
#include "vec.h"

void par3_init_empty(par3_s *par) {
  par->l[0] = par->l[1] = par->l[2] = NO_PARENT;
  par->b[0] = par->b[1] = par->b[2] = NAN;
}

void par3_set(par3_s *par, size_t const *l, dbl const *b, int n) {
  for (int i = 0; i < n; ++i) {
    par->l[i] = l[i];
    par->b[i] = b[i];
  }
}

int par3_size(par3_s const *par) {
  return (int)(par->l[0] != NO_PARENT)
       + (int)(par->l[1] != NO_PARENT)
       + (int)(par->l[2] != NO_PARENT);
}

struct eik3 {
  mesh3_s const *mesh;
  jet3 *jet;
  state_e *state;
  int *pos;
  par3_s *par;
  heap_s *heap;
  int num_valid;
};

void eik3_alloc(eik3_s **eik) {
  *eik = malloc(sizeof(eik3_s));
}

void eik3_dealloc(eik3_s **eik) {
  free(*eik);
  *eik = NULL;
}

static dbl value(void *ptr, int l) {
  eik3_s *eik = (eik3_s *)ptr;
  assert(l >= 0);
  assert(l < (int)mesh3_nverts(eik->mesh));
  dbl T = eik->jet[l].f;
  return T;
}

static void setpos(void *ptr, int l, int pos) {
  eik3_s *eik = (eik3_s *)ptr;
  eik->pos[l] = pos;
}

void eik3_init(eik3_s *eik, mesh3_s const *mesh) {
  eik->mesh = mesh;

  size_t nverts = mesh3_nverts(mesh);

  eik->jet = malloc(nverts*sizeof(jet3));
  for (size_t l = 0; l < nverts; ++l) {
    eik->jet[l] = (jet3) {.f = INFINITY, .fx = NAN, .fy = NAN, .fz = NAN};
  }

  eik->state = malloc(nverts*sizeof(state_e));
  for (size_t l = 0; l < nverts; ++l) {
    eik->state[l] = FAR;
  }

  eik->pos = malloc(nverts*sizeof(int));
  for (size_t l = 0; l < nverts; ++l) {
    eik->pos[l] = NO_INDEX;
  }

  eik->par = malloc(nverts*sizeof(par3_s));
  for (size_t l = 0; l < nverts; ++l) {
    par3_init_empty(&eik->par[l]);
  }

  /**
   * When we compute the initial heap capacity, we want to estimate
   * the number of nodes that could comprise the expanding numerical
   * front at any one time. We can't know this ahead of time, so we
   * set it to a constant multiple times (# nodes)^(1/d). In this
   * case, d=3. Even if this is an underestimate, well still reduce
   * the number of times the heap needs to be expanded at solve time.
   */
  int capacity = (int) 3*cbrt(nverts);

  heap_alloc(&eik->heap);
  heap_init(eik->heap, capacity, value, setpos, (void *)eik);

  eik->num_valid = 0;
}

void eik3_deinit(eik3_s *eik) {
  free(eik->jet);
  eik->jet = NULL;

  free(eik->state);
  eik->state = NULL;

  free(eik->pos);
  eik->pos = NULL;

  free(eik->par);
  eik->par = NULL;

  heap_deinit(eik->heap);
  heap_dealloc(&eik->heap);
}

static jet3 solve_2pt_bvp(eik3_s const *eik, size_t l, size_t l0) {
  dbl const *x = mesh3_get_vert_ptr(eik->mesh, l);
  dbl const *x0 = mesh3_get_vert_ptr(eik->mesh, l0);
  dbl n0[3];
  dbl3_sub(x, x0, n0);
  dbl L = dbl3_normalize(n0);
  return (jet3) {.f = L, .fx = n0[0], .fy = n0[1], .fz = n0[2]};
}

static void do_1pt_update(eik3_s *eik, size_t l, size_t l0) {
  jet3 jet = solve_2pt_bvp(eik, l, l0);
  assert(jet.f <= eik->jet[l].f);
  eik->jet[l] = jet;
  eik->par[l] = (par3_s) {
    .l = {l0, NO_PARENT, NO_PARENT},
    .b = {1, 0, 0}
  };
}

static bool do_2pt_updates(eik3_s *eik, size_t l, size_t l0, size_t *l1) {
  int nvv0 = mesh3_nvv(eik->mesh, l0);
  size_t *vv0 = malloc(nvv0*sizeof(size_t));
  mesh3_vv(eik->mesh, l0, vv0);

  utri_s *utri;
  utri_alloc(&utri);

  dbl T = INFINITY;
  int min_i1 = NO_INDEX;
  for (int i1 = 0; i1 < nvv0; ++i1) {
    size_t l1 = vv0[i1];
    if (eik->state[l1] != VALID) {
      continue;
    }
    if (eik3_is_point_source(eik, l1)) {
      do_1pt_update(eik, l, l1);
      utri_dealloc(&utri);
      free(vv0);
      return false;
    }
    utri_init_from_eik3(utri, eik, l, l0, l1);
    utri_solve(utri);
    dbl Tnew = utri_get_value(utri);
    if (Tnew < T) {
      T = Tnew;
      min_i1 = i1;
    }
  }

  utri_dealloc(&utri);
  *l1 = vv0[min_i1];
  free(vv0);
  return min_i1 != NO_INDEX;
}

static void get_opposite_cell_edge(mesh3_s const *mesh,
                                   size_t c,
                                   size_t l0, size_t l1,
                                   size_t *l2, size_t *l3) {
  size_t v[4];
  mesh3_cv(mesh, c, v);
  int k = 0;
  for (int i = 0; i < 4; ++i) {
    if (v[i] == l0 || v[i] == l1) {
      continue;
    }
    if (k == 0) {
      *l2 = v[i];
      ++k;
    } else if (k == 1) {
      *l3 = v[i];
      ++k;
    } else {
      assert(false);
    }
  }
}

static void sort_and_orient(size_t *l2, size_t *l3, int n) {
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      // We don't want any duplicate entries among the l2 or l3
      // indices, but this can easily happen. This is the "orient"
      // part of this function.
      if (l2[j] == l2[i] || l3[j] == l3[i]) {
        SWAP(l2[j], l3[j]);
      }
      if (l3[j] == l2[i]) {
        SWAP(l2[i + 1], l2[j]);
        SWAP(l3[i + 1], l3[j]);
      }
    }
  }
}

static int get_l2_and_l3(eik3_s const *eik, size_t l0, size_t l1,
                         size_t **l2_ptr, size_t **l3_ptr) {
  int nec = mesh3_nec(eik->mesh, l0, l1);
  size_t *ec = malloc(nec*sizeof(size_t));
  mesh3_ec(eik->mesh, l0, l1, ec);

  size_t *l2 = *l2_ptr = malloc(nec*sizeof(size_t));
  size_t *l3 = *l3_ptr = malloc(nec*sizeof(size_t));

  for (int i = 0; i < nec; ++i) {
    size_t v[4];
    mesh3_cv(eik->mesh, ec[i], v);
    get_opposite_cell_edge(eik->mesh, ec[i], l0, l1, &l2[i], &l3[i]);
  }

  sort_and_orient(l2, l3, nec);

  free(ec);

  return nec;
}

static void do_tetra_update(eik3_s *eik, size_t l, size_t *L) {
  size_t l0 = L[0], l1 = L[1], l2 = L[2];

  utetra_s *utetra;
  utetra_alloc(&utetra);

  assert(!eik3_is_point_source(eik, l0));

  if (eik3_is_point_source(eik, l1)) {
    do_1pt_update(eik, l, l1);
    goto cleanup;
  }

  if (eik3_is_point_source(eik, l2)) {
    do_1pt_update(eik, l, l2);
    goto cleanup;
  }

  utetra_init_from_eik3(utetra, eik, l, l0, l1, l2);
  if (utetra_is_degenerate(utetra)) {
    goto cleanup;
  }

  dbl lam[2] = {0, 0}, alpha[3];
  utetra_set_lambda(utetra, lam);
  utetra_solve(utetra);

  jet3 jet;
  utetra_get_lag_mults(utetra, alpha);
  if (dbl3_maxnorm(alpha) <= 1e-15) {
    utetra_get_jet(utetra, &jet);
    if (jet.f < eik->jet[l].f) {
      eik->jet[l] = jet;
      memcpy(eik->par[l].l, L, 3*sizeof(size_t));
      utetra_get_bary_coords(utetra, eik->par[l].b);
    }
  }

cleanup:
  utetra_dealloc(&utetra);
}

int compar_utetra(utetra_s const **cf1_handle, utetra_s const **cf2_handle) {
  utetra_s const *cf1 = *cf1_handle;
  utetra_s const *cf2 = *cf2_handle;
  if (cf1 == NULL && cf2 == NULL) {
    return 0;
  } else if (cf2 == NULL) {
    return -1;
  } else if (cf1 == NULL) {
    return 1;
  } else {
    dbl const T1 = utetra_get_value(cf1);
    dbl const T2 = utetra_get_value(cf2);
    if (T1 < T2) {
      return -1;
    } else if (T1 > T2) {
      return 1;
    } else {
      return 0;
    }
  }
}

static bool adjacent_utetra_are_optimal(utetra_s const *cf1, utetra_s const *cf2) {
  dbl const atol = 1e-15;
  dbl lam1[2], lam2[2];
  utetra_get_lambda(cf1, lam1);
  utetra_get_lambda(cf2, lam2);
  return fabs(lam1[0] - lam2[0]) <= atol
    && fabs(lam1[1]) <= atol
    && fabs(lam2[1]) <= atol
    && fabs(utetra_get_value(cf1) - utetra_get_value(cf2)) <= atol;
}

static void do_tetra_updates(eik3_s *eik, size_t l, size_t l0, size_t l1,
                             size_t const *l2, int n) {
  assert(!eik3_is_point_source(eik, l0));
  assert(!eik3_is_point_source(eik, l1));

  utetra_s **utetra = malloc(n*sizeof(utetra_s *));
  memset(utetra, 0x0, n*sizeof(utetra_s *));

  dbl lam[2];

  utetra_s *cf;
  for (int i = 0; i < n; ++i) {
    if (eik->state[l2[i]] != VALID) {
      continue;
    }
    if (eik3_is_point_source(eik, l2[i])) {
      do_1pt_update(eik, l, l2[i]);
      goto cleanup;
    }
    utetra_alloc(&utetra[i]);
    cf = utetra[i];
    utetra_init_from_eik3(cf, eik, l, l0, l1, l2[i]);
    // TODO: more efficient and simpler to check if x, x0, x1, and x2
    // are coplanar *before* calling utetra_init_from_eik3 (then we
    // don't need to dealloc below(
    if (utetra_is_degenerate(cf)) {
      utetra_dealloc(&utetra[i]);
      utetra[i] = NULL;
      continue;
    }
    lam[0] = lam[1] = 0;
    utetra_set_lambda(cf, lam);
    utetra_solve(cf);
  }

  qsort(utetra, n, sizeof(utetra_s *), (compar_t)compar_utetra);

  for (int i = 0; i < n; ++i) {
    cf = utetra[i];
    if (cf == NULL || utetra_get_value(cf) > eik->jet[l].f) {
      break;
    }
    if (utetra_has_interior_point_solution(cf) ||
        (i + 1 < n &&
         utetra[i + 1] != NULL &&
         adjacent_utetra_are_optimal(cf, utetra[i + 1]))) {
      utetra_get_jet(cf, &eik->jet[l]);
      eik->par[l].l[0] = l0;
      eik->par[l].l[1] = l1;
      eik->par[l].l[2] = NO_PARENT;
      utetra_get_bary_coords(cf, eik->par[l].b);
      goto cleanup;
    }
  }

cleanup:
  for (int i = 0; i < n; ++i) {
    if (utetra[i]) {
      utetra_dealloc(&utetra[i]);
    }
  }
  free(utetra);
}

static void update(eik3_s *eik, size_t l, size_t l0) {
  /**
   * The first thing we do is check if we're trying to update from a
   * point source. In this case, we just solve the two-point BVP to
   * high accuracy and return immediately.
  */
  if (eik3_is_point_source(eik, l0)) {
    do_1pt_update(eik, l, l0);
    return;
  }

  size_t l1;
  if (!do_2pt_updates(eik, l, l0, &l1)) {
    return;
  }

  // TODO: not using l3 right now, but might want to use it later if
  // we want to try out "volume updates"
  size_t *l2, *l3;
  int n = get_l2_and_l3(eik, l0, l1, &l2, &l3);

  // TODO: use initial guess for lambda taken from `do_2pt_updates` to
  // use as a warm start in `do_tetra_updates`
  do_tetra_updates(eik, l, l0, l1, l2, n);

  /**
   * We also want to do any updates corresponding to valid faces
   * surrounding l and including l0. These may have been missed when
   * doing the preceding hierarchical update.
   */

  int nvf = mesh3_nvf(eik->mesh, l);
  size_t (*vf)[3] = malloc(3*nvf*sizeof(size_t));
  mesh3_vf(eik->mesh, l, vf);
  for (int i = 0; i < nvf; ++i) {
    if ((l0 == vf[i][0] || l0 == vf[i][1] || l0 == vf[i][2])
        && eik->state[vf[i][0]] == VALID
        && eik->state[vf[i][1]] == VALID
        && eik->state[vf[i][2]] == VALID) {
      do_tetra_update(eik, l, vf[i]);
    }
  }

  free(vf);
  free(l2);
  free(l3);
}

static void do_2pt_bd_update(eik3_s *eik, size_t l, size_t l0, size_t l1) {
  assert(!eik3_is_point_source(eik, l0));

  if (eik->state[l0] != VALID || eik->state[l1] != VALID ||
      !mesh3_bdv(eik->mesh, l0) || !mesh3_bdv(eik->mesh, l1)) {
    return;
  }

  if (eik3_is_point_source(eik, l1)) {
    do_1pt_update(eik, l, l1);
    return;
  }

  utri_s *utri;
  utri_alloc(&utri);
  utri_init_from_eik3(utri, eik, l, l0, l1);
  utri_solve(utri);

  if (utri_get_value(utri) >= eik->jet[l].f)
    goto cleanup;

  utri_get_jet(utri, &eik->jet[l]);
  eik->par[l].l[0] = l0;
  eik->par[l].l[1] = l1;
  eik->par[l].l[2] = NO_PARENT;
  utri_get_bary_coords(utri, eik->par[l].b);
  eik->par[l].b[2] = 0;

cleanup:
  utri_dealloc(&utri);
}
}

/**
 * Updating boundary points is much trickier than updating interior
 * points. We handle this case separately.
 */
static void update_bd(eik3_s *eik, size_t l, size_t l0) {
  if (eik3_is_point_source(eik, l0)) {
    do_1pt_update(eik, l, l0);
    return;
  }

  /**
   * Find all adjacent faces.
   */
  int nvf = mesh3_nvf(eik->mesh, l);
  size_t (*vf)[3] = malloc(3*nvf*sizeof(size_t)), *lf;
  mesh3_vf(eik->mesh, l, vf);

  for (int i = 0; i < nvf; ++i) {
    lf = vf[i];
    do_2pt_bd_update(eik, l, lf[0], lf[1]);
    do_2pt_bd_update(eik, l, lf[1], lf[2]);
    do_2pt_bd_update(eik, l, lf[2], lf[0]);
  }

  /**
   * Do adjacent tetrahedron updates.
   */
  for (int i = 0; i < nvf; ++i) {
    lf = vf[i];
    if ((l0 == lf[0] || l0 == lf[1] || l0 == lf[2])
        && eik->state[lf[0]] == VALID
        && eik->state[lf[1]] == VALID
        && eik->state[lf[2]] == VALID) {
      do_tetra_update(eik, l, lf);
    }
  }

  free(vf);
}

static void adjust(eik3_s *eik, size_t l) {
  assert(eik->state[l] == TRIAL);
  assert(l >= 0);
  assert(l < mesh3_nverts(eik->mesh));

  heap_swim(eik->heap, eik->pos[l]);
}

size_t eik3_peek(eik3_s const *eik) {
  return heap_front(eik->heap);
}

size_t eik3_step(eik3_s *eik) {
  size_t l, l0 = heap_front(eik->heap);
  assert(eik->state[l0] == TRIAL);
  heap_pop(eik->heap);

  assert(isfinite(eik->jet[l0].f));

  eik->state[l0] = VALID;

  ++eik->num_valid;

  // Get i0's neighboring nodes.
  int nnb = mesh3_nvv(eik->mesh, l0);
  size_t *nb = malloc(nnb*sizeof(size_t));
  mesh3_vv(eik->mesh, l0, nb);

  // Set FAR nodes to TRIAL and insert them into the heap.
  for (int i = 0; i < nnb; ++i) {
    if (eik->state[l = nb[i]] == FAR) {
      eik->state[l] = TRIAL;
      heap_insert(eik->heap, l);
    }
  }

  // Update neighboring nodes.
  for (int i = 0; i < nnb; ++i) {
    if (eik->state[l = nb[i]] == TRIAL) {
      if (mesh3_bdv(eik->mesh, l)) {
        update_bd(eik, l, l0);
      } else {
        update(eik, l, l0);
      }
      adjust(eik, l);
    }
  }

  free(nb);

  return l0;
}

void eik3_solve(eik3_s *eik) {
  while (heap_size(eik->heap) > 0) {
    (void)eik3_step(eik);
  }
}

void eik3_add_trial(eik3_s *eik, size_t l, jet3 jet) {
  if (eik->state[l] == VALID) {
    return;
  } else if (eik->pos[l] == NO_INDEX) {
    assert(eik->state[l] == FAR);
    eik->jet[l] = jet;
    eik->state[l] = TRIAL;
    heap_insert(eik->heap, l);
  } else if (jet.f < eik->jet[l].f) {
    assert(eik->state[l] == TRIAL);
    eik->jet[l] = jet;
    adjust(eik, l);
  }
}

void eik3_add_valid(eik3_s *eik, size_t l, jet3 jet) {
  // TODO: need to decide what to do if the state is TRIAL
  if (eik->state[l] == TRIAL) {
    abort();
  }

  eik->jet[l] = jet;
  eik->state[l] = VALID;
}

bool eik3_is_point_source(eik3_s const *eik, size_t l) {
  return eik->state[l] == VALID && isfinite(eik->jet[l].f)
    && isnan(eik->jet[l].fx) && isnan(eik->jet[l].fy) && isnan(eik->jet[l].fz);
}

bool eik3_is_far(eik3_s const *eik, size_t l) {
  return eik->state[l] == FAR;
}

bool eik3_is_trial(eik3_s const *eik, size_t l) {
  return eik->state[l] == TRIAL;
}

bool eik3_is_valid(eik3_s const *eik, size_t l) {
  return eik->state[l] == VALID;
}

bool eik3_is_shadow(eik3_s const *eik, size_t l) {
  return eik->state[l] == SHADOW;
}

mesh3_s const *eik3_get_mesh(eik3_s const *eik) {
  return eik->mesh;
}

jet3 eik3_get_jet(eik3_s const *eik, size_t l) {
  return eik->jet[l];
}

jet3 *eik3_get_jet_ptr(eik3_s const *eik) {
  return eik->jet;
}

state_e *eik3_get_state_ptr(eik3_s const *eik) {
  return eik->state;
}

par3_s eik3_get_par(eik3_s const *eik, size_t l) {
  return eik->par[l];
}
