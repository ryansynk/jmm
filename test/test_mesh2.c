#include <cgreen/cgreen.h>
#include <jmm/mesh2.h>
#include <jmm/util.h>
#include <math.h>

Describe(mesh2);
BeforeEach(mesh2) {}
AfterEach(mesh2) {}

#define SET_UP_CUBE_MESH()                                                 \
  dbl3 verts[8] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 1},             \
                   {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}};            \
  uint3 faces[12] = {{0, 1, 2}, {1, 2, 3}, {0, 1, 4}, {1, 4, 5},           \
                     {0, 2, 4}, {2, 4, 6}, {1, 3, 5}, {3, 5, 7},           \
                     {2, 3, 6}, {3, 6, 7}, {4, 5, 6}, {5, 6, 7}};          \
  dbl3 normals[12] = {{-1, 0, 0}, {-1, 0, 0}, {0, -1, 0}, {0, -1, 0},      \
                      {0, 0, -1}, {0, 0, -1}, {0, 0, 1},  {0, 0, 1},       \
                      {0, 1, 0},  {0, 1, 0},  {1, 0, 0},  {1, 0, 0}};      \
  mesh2_s *mesh;                                                           \
  mesh2_alloc(&mesh);                                                      \
  mesh2_init(mesh, verts, 8, POLICY_COPY, faces, 12, POLICY_COPY, normals, \
             POLICY_COPY);

#define TEAR_DOWN_MESH() \
  mesh2_deinit(mesh);    \
  mesh2_dealloc(&mesh);

Ensure(mesh2, get_num_points_works_for_cube) {
  SET_UP_CUBE_MESH();

  assert_that(mesh2_nverts(mesh), is_equal_to(8));

  TEAR_DOWN_MESH();
}

Ensure(mesh2, get_tri_works) {
  SET_UP_CUBE_MESH();

  tri3 tri = mesh2_get_tri(mesh, 0);
  assert_that(tri.v[0][0], is_equal_to(0));
  assert_that(tri.v[0][1], is_equal_to(0));
  assert_that(tri.v[0][2], is_equal_to(0));
  assert_that(tri.v[1][0], is_equal_to(0));
  assert_that(tri.v[1][1], is_equal_to(0));
  assert_that(tri.v[1][2], is_equal_to(1));
  assert_that(tri.v[2][0], is_equal_to(0));
  assert_that(tri.v[2][1], is_equal_to(1));
  assert_that(tri.v[2][2], is_equal_to(0));

  TEAR_DOWN_MESH();
}

TestSuite *mesh2_tests() {
  TestSuite *suite = create_test_suite();

  add_test_with_context(suite, mesh2, get_num_points_works_for_cube);
  add_test_with_context(suite, mesh2, get_tri_works);

  return suite;
}