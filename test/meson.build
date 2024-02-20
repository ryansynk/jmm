#jmm_test_src = [
#    'test_alist.c',
#    'test_array.c'
#    'test_bb3.c',
#    'test_bb3tri.c',
#    'test_bmesh33.c',
#    'test_dbl22.c',
#    'test_eik3.c',
#    'test_mesh2.c',
#    'test_opt.c',
#    'test_utri.c',
#    'test_array.c',
#    'test_bb3tet.c',
#    'test_bicubic.c',
#    'test_camera.c',
#    'test_dbl44.c',
#    'test_geom.c',
#    'test_mesh3.c',
#    'test_utd.c',
#    'test_vec.c'
#]

jmm_test_lib_src = [
    'test_alist.c',
    'test_array.c',
    'test_bb3.c'
]

jmm_test_lib = library(
    'jmm_test',
    jmm_test_lib_src,
    dependencies : [jmm_dep, cgreen_dep, gsl_dep],
    include_directories : jmm_inc
)
#jmm_test_lib_dep = declare_dependency(link_with : jmm_test_lib, include_directories : jmm_inc)

executable('all_tests', 'all_tests.c', link_with : jmm_test_lib, dependencies : cgreen_dep)