//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GeneralUserObject.h"

class MultiApp;

/**
 * Drives two TransientMultiApps through a Newton iteration to solve an inverse problem:
 * find the parameter p such that output(p) = f_target(t).
 *
 * For each time step:
 *   1. Back up both MultiApps at the start of the time step.
 *   2. Iterate via Newton's method using a finite-difference Jacobian:
 *        - Run MultiApp 1 with parameter p, read output y1.
 *        - Run MultiApp 2 with parameter p + delta_parameter, read output y2.
 *        - Estimate df/dp = (y2 - y1) / delta_parameter.
 *        - Newton update: p -= (y1 - y_target) / (df/dp).
 *      Repeat until |y1 - y_target| < tolerance.
 *   3. Perform a final solve of both MultiApps with the converged p, then call
 *      finishStep() to properly advance time before proceeding to the next time step.
 *
 * The parameter is communicated to each sub-app by setting a Receiver postprocessor
 * value directly on the sub-app's FEProblemBase.  The two MultiApps must have
 * execute_on = NONE so that only this UserObject drives their execution.
 */
class TwoMultiAppNewtonIterationUserObject : public GeneralUserObject
{
public:
  static InputParameters validParams();

  TwoMultiAppNewtonIterationUserObject(const InputParameters & parameters);

  virtual void initialize() override {}
  virtual void execute() override;
  virtual void finalize() override {}

protected:
  /**
   * Set the parameter postprocessor in all local sub-apps of the given MultiApp.
   * Only modifies local apps (hasLocalApp check is applied internally).
   */
  void setSubAppParam(std::shared_ptr<MultiApp> & app, Real value) const;

  /**
   * Read the output postprocessor from the given MultiApp (app index 0).
   * Returns the value on all processors via an MPI sum reduction (safe for
   * cases where the sub-app runs on a subset of ranks).
   */
  Real getSubAppOutput(std::shared_ptr<MultiApp> & app) const;

private:
  /// Auto-generated name of the first internally-created MultiApp (runs with p)
  const MultiAppName _multiapp1_name;
  /// Auto-generated name of the second internally-created MultiApp (runs with p + delta)
  const MultiAppName _multiapp2_name;

  /// Name of the Receiver postprocessor in the sub-apps that accepts the parameter
  const PostprocessorName _param_pp;
  /// Name of the output postprocessor in the sub-apps to compare against the target
  const PostprocessorName _output_pp;

  /// Target output value as a function of time
  const Function & _target_function;

  /// Fixed perturbation delta_p used to estimate df/dp numerically
  const Real _delta_param;
  /// Absolute convergence tolerance on the output residual |y1 - y_target|
  const Real _abs_tol;
  /// Relative convergence tolerance: converged when |y1 - y_target| < rel_tol * |y_target|
  const Real _rel_tol;
  /// Maximum number of Newton iterations per time step
  const unsigned int _max_its;

  /// Current converged parameter value; persists across time steps (Restartable)
  Real & _param_value;

  /// Optional main-app Receiver postprocessor name to publish the converged parameter
  const PostprocessorName _param_output_pp;
};
