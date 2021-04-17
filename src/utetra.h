#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"
#include "geom.h"
#include "jet.h"
#include "par.h"

typedef struct utetra_spec {
  eik3_s const *eik;
  size_t lhat, l[3];
  state_e state[3];
  dbl xhat[3], x[3][3];
  jet3 jet[3];
} utetra_spec_s;

utetra_spec_s utetra_spec_empty();
utetra_spec_s utetra_spec_from_eik_and_inds(eik3_s const *eik, size_t l,
                                            size_t l0, size_t l1, size_t l2);
utetra_spec_s utetra_spec_from_eik_without_l(eik3_s const *eik, dbl const x[3],
                                             size_t l0, size_t l1, size_t l2);
utetra_spec_s utetra_spec_from_ptrs(mesh3_s const *mesh, jet3 const *jet,
                                    size_t l, size_t l0, size_t l1, size_t l2);

// TODO: we want to create a new module... "utetras"
// maybe... basically, a complex of utetra updates. There's a bunch of
// logic related to doing this updates in eik3 that makes eik3's
// update logic really difficult to follow, but has nothing to do with
// how we extract an update ray from a set of utetras or how to
// prioritize them.

typedef struct utetra utetra_s;

void utetra_alloc(utetra_s **cf);
void utetra_dealloc(utetra_s **cf);
bool utetra_init(utetra_s *u, utetra_spec_s const *spec);
void utetra_deinit(utetra_s *u);
bool utetra_is_degenerate(utetra_s const *u);
void utetra_solve(utetra_s *cf, dbl const *lam);
dbl utetra_get_value(utetra_s const *cf);
void utetra_get_jet(utetra_s const *cf, jet3 *jet);
bool utetra_has_interior_point_solution(utetra_s const *cf);
bool utetra_has_shadow_boundary_solution(utetra_s const *u);
int utetra_cmp(utetra_s const **h1, utetra_s const **h2);
bool utetra_adj_are_optimal(utetra_s const *u1, utetra_s const *u2);
bool utetra_diff(utetra_s const *utetra, mesh3_s const *mesh, size_t const l[3]);
bool utetra_update_ray_is_physical(utetra_s const *utetra, eik3_s const *eik);
int utetra_get_num_interior_coefs(utetra_s const *utetra);
bool utetras_yield_same_update(utetra_s const **u, size_t n);
size_t utetra_get_l(utetra_s const *utetra);
void utetra_set_update_inds(utetra_s *utetra, size_t l[3]);
bool utetra_opt_inc_on_other_utetra(utetra_s const *u1, utetra_s const *u2);
void utetra_get_x(utetra_s const *u, dbl x[3]);
size_t utetra_get_active_inds(utetra_s const *utetra, size_t l[2]);
par3_s utetra_get_parent(utetra_s const *utetra);

#if JMM_TEST
void utetra_step(utetra_s *u);
void utetra_get_lambda(utetra_s const *u, dbl lam[2]);
void utetra_set_lambda(utetra_s *u, dbl const lam[2]);
size_t utetra_get_num_iter(utetra_s const *u);
#endif

#ifdef __cplusplus
}
#endif
