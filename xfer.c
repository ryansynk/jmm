#include "xfer.h"

#include <assert.h>

#include "bb.h"
#include "grid3.h"
#include "index.h"
#include "mesh3.h"
#include "vec.h"

typedef struct {
  mesh3_s const *mesh;
  grid3_s const *grid;
  dbl *y;
  grid3_s subgrid;
  int offset[3];
  dbl c[20];
  size_t lc;
} xfer_tetra_wkspc_t;

static void xfer_tetra(int const subgrid_ind[3], xfer_tetra_wkspc_t *wkspc) {
  dbl point[3];
  grid3_get_point(&wkspc->subgrid, subgrid_ind, point);

  ivec3 dim = ivec3_from_int3(wkspc->grid->dim);
  ivec3 grid_ind = ivec3_add(
    ivec3_from_int3(subgrid_ind), ivec3_from_int3(wkspc->offset));

  if (!grid3_inbounds(wkspc->grid, &grid_ind.data[0])) {
    return;
  }

  dbl b[4];
  if (mesh3_dbl3_in_cell(wkspc->mesh, wkspc->lc, point, b)) {
    size_t l = ind2l3(dim, grid_ind);
    dbl y = bb3tet(wkspc->c, b);
    mesh3_dbl3_in_cell(wkspc->mesh, wkspc->lc, point, b);
    if (isfinite(wkspc->y[l])) {
      assert(fabs(y - wkspc->y[l])/fabs(fmax(y, wkspc->y[l])) <= 1e-12);
    }
    wkspc->y[l] = y;
  }
}

void xfer(mesh3_s const *mesh, jet3 const *jet, grid3_s const *grid, dbl *y) {
  // Initialize values on grid to NaN.
  for (size_t i = 0; i < grid3_size(grid); ++i) {
    y[i] = NAN;
  }

  /**
   * Traverse the cells of the tetrahedron mesh, restrict the grid so
   * that it just covers each cell, and then evaluate the Bezier
   * tetrahedron at each grid node if it contains the grid node.
   */
  xfer_tetra_wkspc_t wkspc  = {.mesh = mesh, .grid = grid, .y = y};
  for (wkspc.lc = 0; wkspc.lc < mesh3_ncells(mesh); ++wkspc.lc) {
    rect3 bbox;
    mesh3_get_cell_bbox(mesh, wkspc.lc, &bbox);
    wkspc.subgrid = grid3_restrict_to_rect(grid, &bbox, wkspc.offset);
    bb3tet_for_cell(mesh, jet, wkspc.lc, wkspc.c);
    grid3_map(&wkspc.subgrid, (grid3_map_func_t)xfer_tetra, &wkspc);
  }
}
