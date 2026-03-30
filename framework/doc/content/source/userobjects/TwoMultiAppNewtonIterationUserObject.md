# TwoMultiAppNewtonIterationUserObject

!syntax description /UserObjects/TwoMultiAppNewtonIterationUserObject

## Description

`TwoMultiAppNewtonIterationUserObject` solves an inverse problem across two internally-managed
`TransientMultiApp` instances: given a target output value $f(t)$, it finds the scalar parameter
$p$ such that the sub-app output equals the target at each time step.

The two sub-apps run the same input file specified by
[!param](/UserObjects/TwoMultiAppNewtonIterationUserObject/sub_app_input).
The user does not need a `[MultiApps]` block — both `TransientMultiApp` instances are created
automatically with `execute_on = NONE` so that only this UserObject controls their execution.

### Algorithm

At each time step the following procedure is performed:

1. +Backup+ both sub-apps at the start of the time step so every Newton trial starts from the
   same initial condition.
2. +Newton+ iteration** using a finite-difference Jacobian:
   - Sub-app 1 is solved with parameter $p$.
   - Sub-app 2 is solved with parameter $p + \delta p$
     ([!param](/UserObjects/TwoMultiAppNewtonIterationUserObject/delta_parameter)).
   - The derivative $\mathrm{d}f/\mathrm{d}p \approx (y_2 - y_1)/\delta p$ is estimated.
   - The Newton update $p \leftarrow p - (y_1 - f_\text{target}) / (\mathrm{d}f/\mathrm{d}p)$ is applied.
   - Iteration continues until $|y_1 - f_\text{target}| < \max(\texttt{abs\_tol},\ \texttt{rel\_tol} \cdot |f_\text{target}|)$,
     or [!param](/UserObjects/TwoMultiAppNewtonIterationUserObject/max_iterations) is reached
     (a warning is issued in the latter case).
3. +Final solve+ with the converged $p$: both sub-apps are restored to the start-of-timestep
   state and solved once more, then `finishStep()` and `incrementTStep()` are called to properly
   advance their internal clocks before the parent moves to the next time step.

The converged parameter value is stored as restartable data and is used as the initial guess for
the next time step. It can also be written to an optional `Receiver` postprocessor in the main app
via [!param](/UserObjects/TwoMultiAppNewtonIterationUserObject/parameter_postprocessor).

### Communication

The parameter is communicated to each sub-app by directly setting a `Receiver` postprocessor
(named by [!param](/UserObjects/TwoMultiAppNewtonIterationUserObject/param_postprocessor)) on the
sub-app's `FEProblemBase`. The output is read from a postprocessor named by
[!param](/UserObjects/TwoMultiAppNewtonIterationUserObject/output_postprocessor) and reduced across
all MPI ranks so that every processor holds the same value.

This UserObject executes at `TIMESTEP_BEGIN` by default, so the converged sub-app states are
available for any `FROM_MULTIAPP` transfers that also execute at `TIMESTEP_BEGIN`.

## Example input syntax

In this example, the `TwoMultiAppNewtonIterationUserObject` finds the parameter value each time step
such that the sub-app ODE solution matches a target function $u_\text{target}(t) = t^2$.

!listing test/tests/userobjects/two_multiapp_newton_iteration/main.i block=UserObjects

!syntax parameters /UserObjects/TwoMultiAppNewtonIterationUserObject

!syntax inputs /UserObjects/TwoMultiAppNewtonIterationUserObject

!syntax children /UserObjects/TwoMultiAppNewtonIterationUserObject
