#include <cgreen/cgreen.h>
#include <stdio.h>

TestSuite *alist_tests();
TestSuite *array_tests();
TestSuite *bb3_tests();

int main(int argc, char **argv) {
  int suite_result;
  TestSuite *suite = create_test_suite();
  add_suite(suite, alist_tests());
  add_suite(suite, array_tests());
  add_suite(suite, bb3_tests());

  suite_result = run_test_suite(suite, create_text_reporter());
  destroy_test_suite(suite);
  return suite_result;
}