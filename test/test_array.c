#include <cgreen/cgreen.h>
#include <jmm/array.h>

Describe(array);
BeforeEach(array) {}
AfterEach(array) {}

Ensure(array, basic_test) {
  array_s *arr;
  array_alloc(&arr);
  array_init(arr, sizeof(int), 4);

  assert_that(array_is_empty(arr));

  for (int i = 0; i < 8; ++i) {
    array_append(arr, &i);
    assert_that(array_size(arr), is_equal_to(i + 1));
  }

  for (int i = 0; i < 8; ++i) {
    assert_that(array_find(arr, &i), is_equal_to(i));
    assert_that(array_contains(arr, &i));
  }
  for (int i = 8; i < 16; ++i) {
    assert_that(array_find(arr, &i), is_equal_to(array_size(arr)));
    assert_that(array_contains(arr, &i), is_false);
  }

  array_deinit(arr);
  array_dealloc(&arr);
}

TestSuite *array_tests(void) {
  TestSuite *suite = create_test_suite();
  add_test_with_context(suite, array, basic_test);
  return suite;
}