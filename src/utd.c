#include <jmm/utd.h>
#include <math.h>

/* Evaluates the Kouyoumjian transition function for an
 * argument x. Implemented in terms of the modified negative Fresnel
 * integral. See Appendix B of "Introduction to Uniform Geometrical Theory of
 * Diffraction" Equation (A.7) in Potter et. al 2023
 */
dbl x_values[] = {0.3, 0.5, 0.7, 1.0, 1.5, 2.3, 4.0, 5.5};

dblz F_values[] = {
    0.57171324 + 0.27299155 * I, 0.67676271 + 0.26823295 * I,
    0.74395036 + 0.25485662 * I, 0.80952548 + 0.23219939 * I,
    0.87298908 + 0.19820824 * I, 0.92400385 + 0.15765107 * I,
    0.96578828 + 0.10728867 * I, 0.97968559 + 0.08278728 * I,
};

dblz interp_values[] = {
    0.52524735 - 0.023793 * I,   0.33593825 - 0.06688165 * I,
    0.21858373 - 0.0755241 * I,  0.1269272 - 0.0679823 * I,
    0.06376846 - 0.05069646 * I, 0.02457908 - 0.02962494 * I,
    0.00926487 - 0.01633426 * I};

dblz F_p(dbl x) {
  if (x < 0.3) {
    return (sqrt(JMM_PI * x) - 2 * x * cexp(I * (JMM_PI / 4)) -
            (2 / 3) * pow(x, 2) * cexp(-I * (JMM_PI / 4))) *
           cexp(I * (JMM_PI / 4)) * cexp(I * x);
  } else if (x >= 0.3 && x <= 5.5) {
    int i;
    for (i = 0; i < 7; ++i) {
      if (x > x_values[i] && x <= x_values[i + 1]) {
        break;
      }
    }
    return F_values[i] + interp_values[i] * (x - x_values[i]);
  } else {
    return 1.0 + I / (2 * x) - 3 / (4 * pow(x, 2)) -
           (I * 15) / (8 * pow(x, 3)) + 75 / (16 * pow(x, 4));
  }
}

dblz F(dbl x_val) {
  dbl x;
  dblz F_val;
  if (x_val < 0.0) {
    x = -1 * x_val;
    F_val = conj(F_p(x));
    return F_val;
  } else {
    x = x_val;
    F_val = F_p(x);
    return F_val;
  }
}

/* Equation (A.6) in Potter et. al 2023
 */
int N(dbl beta, int n, int sign) {
  int I0[3] = {-1, 0, 1};
  dbl min_val = fabs(beta + sign * JMM_PI - 2 * JMM_PI * n * I0[0]);
  int min_idx = 0;
  for (int i = 1; i < 3; i++) {
    dbl val = fabs(beta + sign * JMM_PI - 2 * JMM_PI * n * I0[i]);
    if (val < min_val) {
      min_val = val;
      min_idx = i;
    }
  }
  return I0[min_idx];
}

/* Equation (A.6) in Potter et. al 2023
 */
dbl a(dbl beta, int n, int sign) {
  int N_ = N(beta, n, sign);
  dbl aval = 2 * cos(0.5 * (2 * JMM_PI * n * N_ - beta)) *
             cos(0.5 * (2 * JMM_PI * n * N_ - beta));
  return aval;
}

dbl phi_in(dbl3 t_in, dbl3 t_e, dbl3 t_o, dbl3 n_o) {
  dbl3 t_aux;
  dbl3_dbl_mul(t_e, dbl3_dot(t_in, t_e), t_aux);
  dbl3 t_in_perp;
  dbl3_sub(t_in, t_aux, t_in_perp);
  dbl3_normalize(t_in_perp);  // calculate using eqn A.2
  dbl phi_in = atan2(dbl3_dot(t_in_perp, n_o), dbl3_dot(t_in_perp, t_o));
  if (phi_in < 0.0) {
    phi_in = phi_in + JMM_PI;
    return phi_in;
  }
  return phi_in;
}

dbl phi_out(dbl3 t_out, dbl3 t_e, dbl3 t_o, dbl3 n_o) {
  dbl3 t_aux;
  dbl3_dbl_mul(t_e, dbl3_dot(t_out, t_e), t_aux);
  dbl3 t_out_perp;
  dbl3_sub(t_out, t_aux, t_out_perp);
  dbl3_normalize(t_out_perp);  // calculate using eqn A.2
  dbl phi_out = atan2(dbl3_dot(t_out_perp, n_o), dbl3_dot(t_out_perp, t_o));
  if (phi_out < 0.0) {
    phi_out = phi_out + JMM_PI;
    return phi_out;
  }
  return phi_out;
}

/* Equation (A.1) in Potter et. al 2023
 */
dbl beta(dbl3 t_in, dbl3 t_out, dbl3 t_e, dbl3 t_o, dbl3 n_o, int sign) {
  if (sign == 1) {
    return phi_out(t_out, t_e, t_o, n_o) + phi_in(t_in, t_e, t_o, n_o);
  } else {
    return phi_out(t_out, t_e, t_o, n_o) - phi_in(t_in, t_e, t_o, n_o);
  }
}

/* Equation (A.4) in Potter et. al 2023
 */
dbl L(dbl3 x, dbl3 t_e, dbl3 x_e, dbl3 t_in, dbl2 rho, dbl rho_e) {
  /* dbl3 t_aux;
 dbl3_dbl_mul(t_in, dbl3_dot(t_in, t_e), t_aux);
 dbl3 q_e;
 dbl3_sub(t_e, t_aux, q_e);
 dbl3_normalize(q_e);
 dbl kappa_qe = dbl3_dbl33_dbl3_dot(q_e, D2T, q_e);
 kappa_qe /= dbl3_norm(grad);
 dbl rho_e = 1.0 / kappa_qe;
 dbl3 lam, abslam;
 size_t perm[3];
 dbl33_eigvals_sym(D2T, lam);
 dbl3_abs(lam, abslam);
 dbl3_argsort(abslam, perm);

 dbl kappa1 = lam[perm[2]],
     kappa2 = lam[perm[1]];   // Should we be dividing by the speed function */
  dbl beta = acos((-1) * dbl3_dot(t_e, t_in));

  dbl rho_diff = dbl3_dist(x, x_e);
  dbl L_ =
      rho_diff * (rho_e + rho_diff) * rho[0] * rho[1] * sin(beta) * sin(beta);
  L_ /= rho_e * (rho[0] + rho_diff) * (rho[1] + rho_diff);
  return L_;
}

/* Equation (A.9) in Potter et. al 2023
 */
dbl Di(dbl k, int n, dbl3 t_in, dbl3 t_out, dbl3 t_e, dbl3 t_o, dbl3 n_o,
       int sign_a, int sign_beta) {
  dbl beta_ = beta(t_in, t_out, t_e, t_o, n_o, sign_beta);
  dbl a_ = a(beta_, n, sign_a);
  // dbl L_ = L(x,t_e,x_e,t_in,D2T,grad,t_o);
  dbl dval =
      -cexp(0.25 * I * JMM_PI) / (2 * n * csqrt(2 * JMM_PI * k) * sin(beta_));
  dval /= tan((JMM_PI + sign_a * beta_) / (2 * n));
  // dval *= F(k * L * a_);
  return dval;
  /* Fix this by looking at what to transport
   */
}

/* Equation (A.8) in Potter et. al 2023
 */
dbl D(dbl3 x, dbl refl_coef, dbl k, int n, dbl3 t_in, dbl3 t_out, dbl3 t_e,
      dbl3 t_o, dbl3 n_o, int sign_a, int sign_beta) {
  dblz D1 = Di(k, n, t_in, t_out, t_e, t_o, n_o, 1, -1);
  dblz D2 = Di(k, n, t_in, t_out, t_e, t_o, n_o, -1, -1);
  dblz D3 = Di(k, n, t_in, t_out, t_e, t_o, n_o, 1, 1);
  dblz D4 = Di(k, n, t_in, t_out, t_e, t_o, n_o, -1, 1);

  return D1 + D2 + refl_coef * (D3 + D4);
}

// def D_from_geometry(k, alpha, no, e, s, sp, t, hess, refl_coef=1):
//     '''Compute the diffraction coefficient for a straight, sound-hard
//     wedge with planar facets from a description of the local
//     geometry. This computes the inputs to `D` and then evaluates it.

// NOTE: old code from utd.py. With any luck we should be able to
// transliterate this more or less directly...

// import logging
// import numpy as np
// import scipy.special

// def _F(x):
//     '''Evaluate the Kouyoumjian transition function for (a vector of)
//     argument(s) `x`. This is implemented in terms of the modified
//     negative Fresnel integral. See Appendix B of "Introduction to
//     Uniform Geometrical Theory of Diffraction" by McNamara, Pistorius,
//     and Malherbe (or equation (4.72) on pp. 184 of the same
//     reference).

//     '''
//     try:
//         sqrt_x = np.sqrt(x)
//     except:
//         import pdb; pdb.set_trace()
//         pass
//     fm = scipy.special.modfresnelm(sqrt_x)[0]
//     return 2*1j*sqrt_x*np.exp(1j*x)*fm

// def _N(beta, n, sign=1):
//     ints = np.outer(np.ones_like(beta), [-1, 0, 1])
//     abs_diff = abs(2*np.pi*n*ints - (beta + sign*np.pi).reshape(-1, 1))
//     argmin = np.argmin(abs_diff, axis=1).reshape(-1, 1)
//     return np.take_along_axis(ints, argmin, axis=1).ravel()

// def _a(beta, n, sign=1):
//     return 2*np.cos((2*n*np.pi*_N(beta, n, sign) - beta)/2)**2

// def _Di(k, n, beta0, phi, phip, Li, sign1=1, sign2=1):
//     if sign1 not in {1, -1}:
//         raise ValueError('sign1 should be +1 or -1')
//     if sign2 not in {1, -1}:
//         raise ValueError('sign2 should be +1 or -1')

//     beta = phi + sign2*phip

//     # See the appendix of Tsingos and Funkhauser or (33) and (34) of
//     # Kouyoumjian and Pathak for an explanation of this. This special
//     # case handles the case when the argument of the cotangent below
//     # is 0.
//     N = _N(beta, n, sign1)
//     eps = beta - sign1*(2*np.pi*n*N - np.pi)
//     tol = np.finfo(np.float64).resolution
//     mask = abs(eps) <= tol

//     Di = np.empty(phi.size, dtype=np.complex128)

//     if mask.sum() > 0:
//         eps_masked = eps[mask]

//         sgn = np.empty(mask.sum(), dtype=np.float64)
//         sgn[eps_masked > 0] = 1
//         sgn[eps_masked <= 0] = -1

//         Di[mask] = np.sqrt(2*np.pi*k*Li[mask])*sgn
//         Di[mask] -= 2*k*Li[mask]*eps_masked*np.exp(1j*np.pi/4)
//         Di[mask] *= n*np.exp(1j*np.pi/4)

//     # Now we compute the rest of the term the normal way
//     tmp1 =
//     -np.exp(-1j*np.pi/4)/(2*n*np.sqrt(2*np.pi*k)*np.sin(beta0[~mask])) tmp2
//     = 1/np.tan((np.pi + sign1*beta[~mask])/(2*n)) tmp3 =
//     _F(k*Li[~mask]*_a(beta[~mask], n, sign=sign1)) Di[~mask] =
//     tmp1*tmp2*tmp3

//     return Di

// def D_from_utd_params(k, n, beta0, phi, phip, Li, refl_coef=1):
//     '''Compute the diffraction coefficient for a straight, sound-hard
//     wedge with planar facets.

//     Parameters
//     ----------
//     k : float [1/m]
//         The wavenumber of the incident wave.
//     n : float [dimensionless]
//         Satisfies alpha = (2 - n)*pi, where alpha is the wedge angle.
//     beta0 : float [rad]
//         The angle that the incident and outgoing rays make with the
//         diffracting edge.
//     phi : float [rad]
//         The angle that the plane spanned by the edge and the
//         diffracted ray makes with the o-face.
//     phip : float [rad]
//         The angle that the plane spanned by the edge and the
//         incident ray makes with the o-face.
//     Li : float [dimensionless]
//         A spreading coefficient used internally to calculate the
//         diffraction coefficient.

//     '''

//     if k <= 0:
//         raise ValueError('k should be positive')
//     if not 0 <= n <= 2:
//         raise ValueError('n should be in the range [0, 2]')

//     D1 = _Di(k, n, beta0, phi, phip, Li, sign1=1, sign2=-1)
//     D2 = _Di(k, n, beta0, phi, phip, Li, sign1=-1, sign2=-1)
//     D3 = _Di(k, n, beta0, phi, phip, Li, sign1=1, sign2=1)
//     D4 = _Di(k, n, beta0, phi, phip, Li, sign1=-1, sign2=1)

//     return D1 + D2 + refl_coef*(D3 + D4)

// def D_from_geometry(k, alpha, no, e, s, sp, t, hess, refl_coef=1):
//     '''Compute the diffraction coefficient for a straight, sound-hard
//     wedge with planar facets from a description of the local
//     geometry. This computes the inputs to `D` and then evaluates it.

//     Parameters
//     ----------
//     k : float [1/m]
//         The wavenumber of the incident wave.
//     alpha : float [rad]
//         The angle of the wedge at the point of diffraction.
//     no : array_like
//         The surface normal for the "o-face" (the face on the far side
//         of the point of incident from the incident ray).
//     e : array_like
//         The unit edge tangent vector, oriented so that `to =
//         np.cross(no, e)` points into the "o-face".
//     s : array_like
//         The unit tangent vector of the diffracted ray at the point of
//         diffraction.
//     sp : array_like
//         The unit tangent vector of the incident ray at the point of
//         diffraction.
//     t : array_like
//         The distance from the point of diffraction.
//     hess : array_like
//         The Hessian of the eikonal at each point.

//     '''

//     # Since `s` will be undefined on the diffracting edge, we want to
//     # mask these values out to avoid bad values while we compute
//     # `D`. The main purpose of this is less confusing debugging.
//     mask = np.logical_not(np.isnan(s).any(1))

//     # We also want to mask out nodes where s or sp are exactly aligned
//     # with e. This should only happen far away from the diffracting
//     # edge where s or sp get aligned with e coincidentally; i.e., in a
//     # disconnected component... (TODO: verify this...)
//     mask[(s == e).all(1)] = False
//     mask[(s == -e).all(1)] = False
//     mask[(sp == e).all(1)] = False
//     mask[(sp == -e).all(1)] = False

//     s_masked = s[mask]
//     sp_masked = sp[mask]
//     hess_masked = hess[mask]
//     t_masked = t[mask]

//     if not np.isfinite(sp_masked.ravel()).all():
//         raise RuntimeError('missing some entries of sp after masking')

//     if not np.isfinite(hess_masked.ravel()).all():
//         raise RuntimeError('missing some entries of hess after masking')

//     if not np.isfinite(t_masked.ravel()).all():
//         raise RuntimeError('missing some entries of t after masking')

//     if (t_masked < 0).any():
//         raise RuntimeError('passed negative distances (t)')

//     n = 2 - alpha/np.pi
//     beta0 = np.arccos(np.clip(s_masked@e, -1, 1))

//     to = np.cross(no, e)
//     to /= np.linalg.norm(to)

//     # NOTE: when we compute phi and phip below, we just clamp them to
//     # the desired range. There might be cases where these angles fall
//     # outside [0, n*pi), but this should only happen as a result of
//     # numerical error, so what we're doing here should be OK.

//     tol = np.finfo(np.float64).resolution

//     log = logging.getLogger('utd.py')

//     st = s_masked - np.outer(s_masked@e, e)
//     st /= np.sqrt(np.sum(st**2, axis=1)).reshape(-1, 1)
//     phi = np.pi - (np.pi - np.arccos(st@to))*np.sign(st@no)
//     if np.isnan(phi).any():
//         raise RuntimeError('computed bad angles (phi)')

//     if phi.min() < -tol:
//         log.warn('out of range angle (phi: by %g)', abs(phi.min()))
//     if 2*np.pi - alpha + tol < phi.max():
//         log.warn('out of range angle (phi: by %g)',
//                  abs(2*np.pi - alpha - phi.max()))
//     phi = np.clip(phi, 0, n*np.pi)

//     spt = sp_masked - np.outer(sp_masked@e, e)
//     spt /= np.sqrt(np.sum(spt**2, axis=1)).reshape(-1, 1)
//     phip = np.pi - (np.pi - np.arccos(-spt@to))*np.sign(-spt@no)
//     if np.isnan(phip).any():
//         raise RuntimeError('computed bad angles (phip)')

//     if phip.min() < -tol:
//         log.warn('out of range angle (phip: by %g)', abs(phip.min()))
//     if 2*np.pi - alpha + tol < phip.max():
//         log.warn('out of range angle (phip: by %g)',
//                  abs(2*np.pi - alpha - phip.max()))
//     phip = np.clip(phip, 0, n*np.pi)

//     # Compute the radius of curvature in the "edge-fixed plane of
//     # incidence"
//     qe = e - (sp_masked.T*(sp_masked@e)).T
//     qe /= np.sqrt(np.sum(qe**2, axis=1)).reshape(-1, 1)
//     kappae = np.maximum(
//         np.finfo(np.float64).resolution,
//         np.einsum('ijk,ij,ik->i', hess_masked, qe, qe)
//     )
//     rhoe = 1/kappae

//     q1 = no - (sp_masked.T*(sp_masked@no)).T
//     q1 /= np.sqrt(np.sum(q1**2, axis=1)).reshape(-1, 1)
//     kappa1 = np.maximum(
//         np.finfo(np.float64).resolution,
//         np.einsum('ijk,ij,ik->i', hess_masked, q1, q1)
//     )
//     rho1 = 1/kappa1

//     q2 = np.cross(q1, to)
//     q2 /= np.sqrt(np.sum(q2**2, axis=1)).reshape(-1, 1)
//     kappa2 = np.maximum(
//         np.finfo(np.float64).resolution,
//         np.einsum('ijk,ij,ik->i', hess_masked, q2, q2)
//     )
//     rho2 = 1/kappa2

//     Li = t_masked*(rhoe + t_masked)*rho1*rho2
//     Li /= rhoe*(rho1 + t_masked)*(rho2 * t_masked)
//     Li *= np.sin(beta0)**2

//     if (Li < 0).any():
//         raise RuntimeError('computed negative spreading factors (Li)')

//     D = np.empty_like(t, dtype=np.complex128)
//     D[~mask] = 1
//     D[mask] = D_from_utd_params(k, n, beta0, phi, phip, Li, refl_coef)

//     return D
