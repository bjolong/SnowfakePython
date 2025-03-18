#pragma once

#include <cmath>
#include "solver_defaults.h"

struct Medium
{
public:
  Medium()
    : _rho(DEFAULT_RHO)
    , _phi(DEFAULT_PHI)
  {
    for (int k = 0; k < 8; k++) _kappa[k] = DEFAULT_KAPPA;
    for (int k = 0; k < 8; k++) _mu[k] = DEFAULT_MU;
    _beta[0] = std::numeric_limits<float>::max();
    _beta[1] = DEFAULT_BETA_01;
    _beta[2] = DEFAULT_BETA_10;
    _beta[3] = DEFAULT_BETA_11;
    _beta[4] = DEFAULT_BETA_20;
    _beta[5] = DEFAULT_BETA_21;
    _beta[6] = DEFAULT_BETA_30;
    _beta[7] = DEFAULT_BETA_31;
  }

  double rho() const { return _rho; }
  double phi() const { return _phi; }
  double kappa() const
  {
    double kappa_all = 0.;
    for (int k = 1; k < 8; k++)
      kappa_all += _kappa[k];
    return kappa_all / 7.;
  }
  double mu() const
  {
    double mu_all = 0.;
    for (int k = 1; k < 8; k++)
      mu_all += _mu[k];
    return mu_all / 7.;
  }
  double beta() const
  {
    double beta_all = 0.;
    for (int k = 1; k < 8; k++)
      beta_all += _mu[k];
    return beta_all / 7.;
  }

  void set_rho(double irho) { _rho = irho; };
  void set_phi(double iphi) { _phi = iphi; };
  void set_kappa(double ikappa)
  {
    for (int k = 0; k < 8; k++)
      _kappa[k] = ikappa;
  }
  void set_mu(double imu)
  {
    for (int k = 0; k < 8; k++)
      _mu[k] = imu;
  }
  void set_beta(double ibeta)
  {
    _beta[0] = std::numeric_limits<float>::max();
    for (int k = 1; k < 8; k++)
      _beta[k] = ibeta;
  }

  double kappa_01() const { return _kappa[1]; }
  double kappa_10() const { return _kappa[2]; }
  double kappa_11() const { return _kappa[3]; }
  double kappa_20() const { return _kappa[4]; }
  double kappa_21() const { return _kappa[5]; }
  double kappa_30() const { return _kappa[6]; }
  double kappa_31() const { return _kappa[7]; }

  double mu_01() const { return _mu[1]; }
  double mu_10() const { return _mu[2]; }
  double mu_11() const { return _mu[3]; }
  double mu_20() const { return _mu[4]; }
  double mu_21() const { return _mu[5]; }
  double mu_30() const { return _mu[6]; }
  double mu_31() const { return _mu[7]; }

  double beta_01() const { return _beta[1]; }
  double beta_10() const { return _beta[2]; }
  double beta_11() const { return _beta[3]; }
  double beta_20() const { return _beta[4]; }
  double beta_21() const { return _beta[5]; }
  double beta_30() const { return _beta[6]; }
  double beta_31() const { return _beta[7]; }

  void set_kappa_01(double ikappa) { _kappa[1] = ikappa; };
  void set_kappa_10(double ikappa) { _kappa[2] = ikappa; };
  void set_kappa_11(double ikappa) { _kappa[3] = ikappa; };
  void set_kappa_20(double ikappa) { _kappa[4] = ikappa; };
  void set_kappa_21(double ikappa) { _kappa[5] = ikappa; };
  void set_kappa_30(double ikappa) { _kappa[6] = ikappa; };
  void set_kappa_31(double ikappa) { _kappa[7] = ikappa; };

  void set_mu_01(double imu) { _mu[1] = imu; };
  void set_mu_10(double imu) { _mu[2] = imu; };
  void set_mu_11(double imu) { _mu[3] = imu; };
  void set_mu_20(double imu) { _mu[4] = imu; };
  void set_mu_21(double imu) { _mu[5] = imu; };
  void set_mu_30(double imu) { _mu[6] = imu; };
  void set_mu_31(double imu) { _mu[7] = imu; };

  void set_beta_01(double ibeta) { _beta[1] = ibeta; };
  void set_beta_10(double ibeta) { _beta[2] = ibeta; };
  void set_beta_11(double ibeta) { _beta[3] = ibeta; };
  void set_beta_20(double ibeta) { _beta[4] = ibeta; };
  void set_beta_21(double ibeta) { _beta[5] = ibeta; };
  void set_beta_30(double ibeta) { _beta[6] = ibeta; };
  void set_beta_31(double ibeta) { _beta[7] = ibeta; };

private:
  double _rho;
  double _phi;

  double _kappa[8];
  double _mu[8];
  double _beta[8];
};
