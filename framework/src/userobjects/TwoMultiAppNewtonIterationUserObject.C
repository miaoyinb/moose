//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "TwoMultiAppNewtonIterationUserObject.h"

#include "Executioner.h"
#include "FEProblemBase.h"
#include "Factory.h"
#include "Function.h"
#include "MultiApp.h"

registerMooseObject("MooseApp", TwoMultiAppNewtonIterationUserObject);

InputParameters
TwoMultiAppNewtonIterationUserObject::validParams()
{
  InputParameters params = GeneralUserObject::validParams();
  params.addClassDescription(
      "Drives two TransientMultiApps through a Newton iteration to find the parameter value "
      "that yields a target output (given as a function of time). For each time step, the two "
      "sub-apps are run with p and p+delta_parameter to estimate df/dp numerically; Newton "
      "updates p until convergence, then a final solve is performed before advancing time.");

  params.addRequiredParam<FileName>(
      "sub_app_input",
      "Input file for the two internally-created TransientMultiApp instances. "
      "Both sub-apps run the same input file.");

  params.addRequiredParam<PostprocessorName>(
      "param_postprocessor",
      "Name of the Receiver postprocessor in the sub-apps that receives the parameter value.");
  params.addRequiredParam<PostprocessorName>(
      "output_postprocessor",
      "Name of the postprocessor in the sub-apps whose value is compared against the target.");

  params.addRequiredParam<FunctionName>(
      "target_function",
      "Function f(t) specifying the target output value at each time step.");

  params.addRequiredParam<Real>("initial_parameter",
                                "Initial guess for the parameter (used for the first time step).");

  params.addParam<Real>("delta_parameter",
                        1e-4,
                        "Fixed perturbation delta_p applied to the second MultiApp to estimate "
                        "df/dp numerically.  Should be small relative to the expected parameter "
                        "magnitude but large enough to avoid numerical cancellation.");
  params.addParam<Real>("abs_tol",
                        1e-8,
                        "Absolute convergence tolerance: iteration stops when "
                        "|output - target| < abs_tol.");
  params.addParam<Real>("rel_tol",
                        1e-6,
                        "Relative convergence tolerance: iteration stops when "
                        "|output - target| < rel_tol * |target|.");
  params.addParam<unsigned int>("max_iterations",
                                50,
                                "Maximum number of Newton iterations per time step.  A warning "
                                "is issued if this limit is reached without convergence.");

  params.addParam<PostprocessorName>(
      "parameter_postprocessor",
      "Optional name of a Receiver postprocessor in the main app to which the "
      "converged parameter value is written after each time step.");

  // Run before the main-app solve so the converged sub-app states are available
  // for any FROM_MULTIAPP transfers that execute at TIMESTEP_BEGIN.
  params.set<ExecFlagEnum>("execute_on") = EXEC_TIMESTEP_BEGIN;

  return params;
}

TwoMultiAppNewtonIterationUserObject::TwoMultiAppNewtonIterationUserObject(
    const InputParameters & parameters)
  : GeneralUserObject(parameters),
    _multiapp1_name(name() + "_app1"),
    _multiapp2_name(name() + "_app2"),
    _param_pp(getParam<PostprocessorName>("param_postprocessor")),
    _output_pp(getParam<PostprocessorName>("output_postprocessor")),
    _target_function(getFunction("target_function")),
    _delta_param(getParam<Real>("delta_parameter")),
    _abs_tol(getParam<Real>("abs_tol")),
    _rel_tol(getParam<Real>("rel_tol")),
    _max_its(getParam<unsigned int>("max_iterations")),
    _param_value(declareRestartableData<Real>("param_value", getParam<Real>("initial_parameter"))),
    _param_output_pp(isParamValid("parameter_postprocessor")
                         ? getParam<PostprocessorName>("parameter_postprocessor")
                         : PostprocessorName(""))
{
  if (_delta_param == 0.0)
    paramError("delta_parameter", "delta_parameter must be nonzero.");

  // Create the two internally-managed TransientMultiApp instances so the user
  // does not need a [MultiApps] block. Both run the same sub-app input file
  // with execute_on = NONE so only this UserObject drives their execution.
  const std::vector<FileName> input_files = {getParam<FileName>("sub_app_input")};
  for (const auto & app_name : {_multiapp1_name, _multiapp2_name})
  {
    InputParameters mp = _app.getFactory().getValidParams("TransientMultiApp");
    mp.set<std::vector<FileName>>("input_files") = input_files;
    mp.set<ExecFlagEnum>("execute_on") = EXEC_NONE;
    _fe_problem.addMultiApp("TransientMultiApp", app_name, mp);
  }
}

void
TwoMultiAppNewtonIterationUserObject::execute()
{
  auto multiapp1 = _fe_problem.getMultiApp(_multiapp1_name);
  auto multiapp2 = _fe_problem.getMultiApp(_multiapp2_name);

  const Real y_target = _target_function.value(_t, libMesh::Point());

  // Save sub-app states at the beginning of this time step so every Newton
  // trial starts from the same initial condition.
  multiapp1->backup();
  multiapp2->backup();

  Real p = _param_value;
  bool converged = false;

  for (unsigned int iter = 0; iter < _max_its; ++iter)
  {
    // Restore both sub-apps to their start-of-timestep state before each trial
    // (skip the very first iteration since we just backed up and have not yet
    // modified any state).
    if (iter > 0)
    {
      multiapp1->restore();
      multiapp2->restore();
    }

    // Inject the current iterate into sub-app 1 (p) and sub-app 2 (p + delta).
    setSubAppParam(multiapp1, p);
    setSubAppParam(multiapp2, p + _delta_param);

    // Solve both sub-apps for this time step without advancing their time
    // counters so that subsequent iterations can start from the same state.
    const bool ok1 = multiapp1->solveStep(_dt, _t, /*auto_advance=*/false);
    const bool ok2 = multiapp2->solveStep(_dt, _t, /*auto_advance=*/false);

    if (!ok1 || !ok2)
    {
      if (!ok1)
        mooseInfoRepeated(name(),
                          ": '",
                          _multiapp1_name,
                          "' failed to converge during Newton iteration ",
                          iter + 1,
                          " at t = ",
                          _t,
                          ". Cutting timestep.");
      if (!ok2)
        mooseInfoRepeated(name(),
                          ": '",
                          _multiapp2_name,
                          "' failed to converge during Newton iteration ",
                          iter + 1,
                          " at t = ",
                          _t,
                          ". Cutting timestep.");
      multiapp1->restore();
      multiapp2->restore();
      getMooseApp().getExecutioner()->fixedPointSolve().failStep();
      return;
    }

    const Real y1 = getSubAppOutput(multiapp1);
    const Real y2 = getSubAppOutput(multiapp2);

    const Real residual = y1 - y_target;

    // Check convergence.
    const Real tol = std::max(_abs_tol, _rel_tol * std::abs(y_target));
    if (std::abs(residual) <= tol)
    {
      converged = true;
      break;
    }

    // Finite-difference estimate of df/dp.
    const Real dy_dp = (y2 - y1) / _delta_param;

    if (std::abs(dy_dp) < 1e-15 * (std::abs(y1) + std::abs(y2) + 1.0))
    {
      mooseWarning(name(),
                   ": the estimated derivative df/dp = ",
                   dy_dp,
                   " is too small to perform a Newton update at t = ",
                   _t,
                   ". Cutting timestep.");
      multiapp1->restore();
      multiapp2->restore();
      getMooseApp().getExecutioner()->fixedPointSolve().failStep();
      return;
    }

    // Newton update.
    p -= residual / dy_dp;
  }

  if (!converged)
    mooseWarning(name(),
                 ": Newton iteration did not converge within ",
                 _max_its,
                 " iterations at t = ",
                 _t,
                 ". Proceeding with the best estimate p = ",
                 p,
                 ".");

  // Store the converged (or best) parameter for the next time step.
  _param_value = p;

  // Publish to the optional main-app Receiver postprocessor.
  if (!_param_output_pp.empty())
    _fe_problem.setPostprocessorValueByName(_param_output_pp, p);

  // -------------------------------------------------------------------------
  // Final solve: restore both sub-apps and run with the converged p.
  //
  // IMPORTANT: use auto_advance=false here just like the Newton trial solves.
  // If auto_advance=true were used, solveStep() would internally call
  // endStep()+postStep() to advance time, and then finishStep() would call
  // them a second time — double-advancing the sub-app clock.  On the next
  // parent time step the sub-app would see its time >= target_time and silently
  // skip the solve entirely, so the output would appear frozen.
  //
  // Instead we replicate the Picard pattern used by FixedPointSolve:
  //   solveStep(..., false) — compute solution, do NOT advance time
  //   incrementTStep(target) — increment the sub-app step counter
  //   finishStep()           — call endStep()+postStep() exactly once
  // -------------------------------------------------------------------------
  multiapp1->restore();
  multiapp2->restore();

  setSubAppParam(multiapp1, p);
  setSubAppParam(multiapp2, p);

  const bool final_ok1 = multiapp1->solveStep(_dt, _t, /*auto_advance=*/false);
  const bool final_ok2 = multiapp2->solveStep(_dt, _t, /*auto_advance=*/false);

  if (!final_ok1)
    mooseWarning(
        name(), ": '", _multiapp1_name, "' failed during the final solve at t = ", _t, ". Cutting timestep.");
  if (!final_ok2)
    mooseWarning(
        name(), ": '", _multiapp2_name, "' failed during the final solve at t = ", _t, ". Cutting timestep.");
  if (!final_ok1 || !final_ok2)
  {
    multiapp1->restore();
    multiapp2->restore();
    getMooseApp().getExecutioner()->fixedPointSolve().failStep();
    return;
  }

  // Advance time: calls endStep()+postStep() exactly once per sub-app.
  // MUST come before incrementTStep() so that endStep() sets _time = t_new
  // before incrementStepOrReject() captures _time_old = _time.
  multiapp1->finishStep();
  multiapp2->finishStep();

  // Increment step counters: _time_old = _time (now = t_new after finishStep).
  multiapp1->incrementTStep(_t);
  multiapp2->incrementTStep(_t);
}

void
TwoMultiAppNewtonIterationUserObject::setSubAppParam(std::shared_ptr<MultiApp> & app,
                                                     Real value) const
{
  for (unsigned int i = 0; i < app->numGlobalApps(); ++i)
    if (app->hasLocalApp(i))
      app->appProblemBase(i).setPostprocessorValueByName(_param_pp, value);
}

Real
TwoMultiAppNewtonIterationUserObject::getSubAppOutput(std::shared_ptr<MultiApp> & app) const
{
  // Collect the output postprocessor value from the local sub-app (app index 0
  // by convention; for multiple global apps this sums contributions from all
  // root processors, which is only meaningful for numGlobalApps() == 1).
  Real value = 0.0;
  for (unsigned int i = 0; i < app->numGlobalApps(); ++i)
    if (app->hasLocalApp(i) && app->isRootProcessor())
      value += app->appPostprocessorValue(i, _output_pp);

  // Sum across all MPI ranks so every processor holds the same value.
  _communicator.sum(value);
  return value;
}
