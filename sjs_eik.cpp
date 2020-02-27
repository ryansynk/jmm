#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "def.h"
#include "heap.h"
#include "hermite.h"
#include "jet.h"
#include "sjs_eik.h"

struct heap_wrapper {
  heap * ptr {nullptr};
  heap_wrapper(int capacity,
               std::function<dbl(int)> const & value,
               std::function<void(int,int)> const & setpos) {
    heap_alloc(&ptr);
    heap_init(
      ptr,
      capacity,
      ^(int l) { return value(l); },
      ^(int l, int pos) { setpos(l, pos); }
    );
  }
  ~heap_wrapper() {
    heap_deinit(ptr);
    heap_dealloc(&ptr);
  }
};

struct sjs_wrapper {
  sjs * ptr {nullptr};
  sjs_wrapper(std::array<int, 2> const & shape,
              std::array<dbl, 2> const & xymin,
              dbl h,
              std::function<dbl(dbl, dbl)> const & s,
              std::function<std::tuple<dbl, dbl>(dbl, dbl)> const & grad_s) {
    sjs_alloc(&ptr);
    sjs_init(
      ptr,
      ivec2 {shape[0], shape[1]},
      dvec2 {xymin[0], xymin[1]},
      h,
      ^(dvec2 xy) { return s(xy.x, xy.y); },
      ^(dvec2 xy) {
        auto tmp = grad_s(xy.x, xy.y);
        return dvec2 {std::get<0>(tmp), std::get<1>(tmp)};
      }
    );
  }
  ~sjs_wrapper() {
    sjs_deinit(ptr);
    sjs_dealloc(&ptr);
  }
};

PYBIND11_MODULE (sjs_eik, m) {
  m.doc() = R"pbdoc(
sjs_eik
-------

TODO!
)pbdoc";

  // def.h

  py::enum_<state>(m, "State")
    .value("Far", state::FAR)
    .value("Trial", state::TRIAL)
    .value("Valid", state::VALID)
    .value("Boundary", state::BOUNDARY)
    ;

  // heap.h

  py::class_<heap_wrapper>(m, "Heap")
    .def(py::init<int, std::function<dbl(int)>, std::function<void(int,int)>>())
    .def(
      "insert",
      [] (heap_wrapper & w, int ind) { heap_insert(w.ptr, ind); }
    )
    .def(
      "swim",
      [] (heap_wrapper & w, int ind) { heap_swim(w.ptr, ind); }
    )
    .def(
      "front",
      [] (heap_wrapper const & w) { return heap_front(w.ptr); }
    )
    .def(
      "pop",
      [] (heap_wrapper & w) { heap_pop(w.ptr); }
    )
    .def_property_readonly(
      "size",
      [] (heap_wrapper const & w) { return heap_size(w.ptr); }
    )
    ;

  // hermite.h

  py::enum_<bicubic_variable>(m, "BicubicVariable")
    .value("Lambda", bicubic_variable::LAMBDA)
    .value("Mu", bicubic_variable::MU)
    ;

  py::class_<cubic>(m, "Cubic")
    .def(py::init(
           [] (std::array<dbl, 4> const & a) {
             auto ptr = std::make_unique<cubic>();
             std::copy(std::begin(a), std::end(a), ptr->a);
             return ptr;
           }
         ))
    .def(
      "f",
      [] (cubic const & C, dbl lam) { return cubic_f(&C, lam); }
    )
    .def(
      "df",
      [] (cubic const & C, dbl lam) { return cubic_df(&C, lam); }
    )
    ;

  py::class_<bicubic>(m, "Bicubic")
    .def(py::init(
           [] (std::array<std::array<dbl, 4>, 4> const & data) {
             auto ptr = std::make_unique<bicubic>();
             dbl data_arr[4][4];
             for (int i = 0; i < 4; ++i) {
               for (int j = 0; j < 4; ++j) {
                 data_arr[i][j] = data[i][j];
               }
             }
             bicubic_set_A(ptr.get(), data_arr);
             return ptr;
           }
         ))
    .def_property_readonly(
      "A",
      [] (bicubic const & B) {
        std::array<std::array<dbl, 4>, 4> A;
        for (int i = 0; i < 4; ++i) {
          for (int j = 0; j < 4; ++j) {
            A[i][j] = B.A[i][j];
          }
        }
        return A;
      }
    )
    .def(
      "set_A",
      [] (bicubic & B, std::array<std::array<dbl, 4>, 4> const & data) {
        dbl data_arr[4][4];
        for (int i = 0; i < 4; ++i) {
          for (int j = 0; j < 4; ++j) {
            data_arr[i][j] = data[i][j];
          }
        }
        bicubic_set_A(&B, data_arr);
      }
    )
    .def(
      "restrict",
      [] (bicubic const & B, bicubic_variable var, int edge) {
        return bicubic_restrict(&B, var, edge);
      }
    )
    .def(
      "f",
      [] (bicubic const & B, dbl lambda, dbl mu) {
        return bicubic_f(&B, dvec2 {lambda, mu});
      }
    )
    .def(
      "fx",
      [] (bicubic const & B, dbl lambda, dbl mu) {
        return bicubic_fx(&B, dvec2 {lambda, mu});
      }
    )
    .def(
      "fy",
      [] (bicubic const & B, dbl lambda, dbl mu) {
        return bicubic_fy(&B, dvec2 {lambda, mu});
      }
    )
    .def(
      "fxy",
      [] (bicubic const & B, dbl lambda, dbl mu) {
        return bicubic_fxy(&B, dvec2 {lambda, mu});
      }
    )
    ;

  // jet.h

  py::class_<jet>(m, "Jet")
    .def(py::init<dbl, dbl, dbl, dbl>())
    .def_readwrite("f", &jet::f)
    .def_readwrite("fx", &jet::fx)
    .def_readwrite("fy", &jet::fy)
    .def_readwrite("fxy", &jet::fxy)
    ;

  // sjs_eik.h

  py::class_<sjs_wrapper>(m, "StaticJetScheme")
    .def(py::init<
           std::array<int, 2> const &,
           std::array<dbl, 2> const &,
           dbl,
           std::function<dbl(dbl, dbl)> const &,
           std::function<std::tuple<dbl, dbl>(dbl, dbl)> const &
         >())
    .def(
      "solve",
      [] (sjs_wrapper const & w) { sjs_solve(w.ptr); }
    )
    .def(
      "T",
      [] (sjs_wrapper const & w, dbl x, dbl y) {
        return sjs_T(w.ptr, dvec2 {x, y});
      }
    )
    .def(
      "Tx",
      [] (sjs_wrapper const & w, dbl x, dbl y) {
        return sjs_Tx(w.ptr, dvec2 {x, y});
      }
    )
    .def(
      "Ty",
      [] (sjs_wrapper const & w, dbl x, dbl y) {
        return sjs_Ty(w.ptr, dvec2 {x, y});
      }
    )
    .def(
      "Txy",
      [] (sjs_wrapper const & w, dbl x, dbl y) {
        return sjs_Txy(w.ptr, dvec2 {x, y});
      }
    )
    .def(
      "bicubic",
      [] (sjs_wrapper const & w, int i, int j) {
        return sjs_bicubic(w.ptr, ivec2 {i, j});
      }
    )
    ;

  // vec.h

  py::class_<dvec2>(m, "Dvec2")
    .def(py::init<dbl, dbl>())
    .def_readwrite("x", &dvec2::x)
    .def_readwrite("y", &dvec2::y)
    ;

  py::class_<ivec2>(m, "Ivec2")
    .def(py::init<int, int>())
    .def_readwrite("i", &ivec2::i)
    .def_readwrite("j", &ivec2::j)
    ;

  m.def(
    "ccomb",
    [] (dvec2 const & v0, dvec2 const & v1, dbl t) {
      return dvec2_ccomb(v0, v1, t);
    }
  );

  m.def(
    "dist",
    [] (dvec2 const & v0, dvec2 const & v1) {
      return dvec2_dist(v0, v1);
    }
  );

#ifdef VERSION_INFO
  m.attr("__version__") = VERSION_INFO;
#else
  m.attr("__version__") = "dev";
#endif
}
