#include <cgreen/cgreen.h>
#include <stdio.h>

TestSuite *alist_tests();
TestSuite *array_tests();
TestSuite *bb3_tests();
TestSuite *bb3tet_tests();
TestSuite *bb3tri_tests();
TestSuite *bicubic_tests();
TestSuite *bmesh33_tests();  // failing
TestSuite *camera_tests();   // failing
TestSuite *dbl22_tests();
TestSuite *dbl44_tests();
// TestSuite *eik3_tests();
TestSuite *geom_tests();
TestSuite *mesh2_tests();
TestSuite *mesh3_tests();
TestSuite *opt_tests();
// TestSuite *utri_tests();
TestSuite *vec_tests();

int main(int argc, char **argv) {
  int suite_result;
  TestSuite *suite = create_test_suite();
  add_suite(suite, alist_tests());
  add_suite(suite, array_tests());
  add_suite(suite, bb3_tests());
  add_suite(suite, bb3tet_tests());
  add_suite(suite, bb3tri_tests());
  add_suite(suite, bicubic_tests());
  add_suite(suite, bmesh33_tests());
  add_suite(suite, camera_tests());
  add_suite(suite, dbl22_tests());
  add_suite(suite, dbl44_tests());
  // add_suite(suite, eik3_tests());
  add_suite(suite, geom_tests());
  add_suite(suite, mesh2_tests());
  add_suite(suite, mesh3_tests());
  add_suite(suite, opt_tests());
  // add_suite(suite, utri_tests());
  add_suite(suite, vec_tests());

  suite_result = run_test_suite(suite, create_text_reporter());
  destroy_test_suite(suite);
  return suite_result;
}