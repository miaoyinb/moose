//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MeshGenerator.h"
#include "FunctionParserUtils.h"

#include "libmesh/meshfree_interpolation.h"

class Boundary2DDelaunayGenerator : public MeshGenerator, public FunctionParserUtils<false>
{
public:
  static InputParameters validParams();

  Boundary2DDelaunayGenerator(const InputParameters & parameters);

  virtual std::unique_ptr<MeshBase> generate() override;

protected:
  ///The input mesh name
  std::unique_ptr<MeshBase> & _input;
  ///The boundaries to be removed
  const std::vector<BoundaryName> _boundary_names;
  /// Whether to use automatic desired area function
  const bool _use_auto_area_func;

  /// Background size for automatic desired area function
  const Real _auto_area_func_default_size;

  /// Background size's effective distance for automatic desired area function
  const Real _auto_area_func_default_size_dist;

  /// Maximum number of points to use for the inverse distance interpolation for automatic area function
  const unsigned int _auto_area_function_num_points;

  /// Power of the polynomial used in the inverse distance interpolation for automatic area function
  const Real _auto_area_function_power;

  /// Maximum number of iterations to correct the nodes based on the level set function
  const unsigned int _max_level_set_correction_iterations;

  /// function parser object describing the level set
  SymFunctionPtr _func_level_set;

  /**
   *
   */
  Point elemNormal(const Elem & elem);

  /**
   *
   */
  Point meshNormal2D(const MeshBase & mesh);

  /**
   * Evaluate the level set function at a given point.
   * @param point The point at which the level set function is to be evaluated
   * @return the value of the level set function at the given point
   */
  Real levelSetEvaluator(const Point & point);

  void levelSetCorrection(Node & node);
};
