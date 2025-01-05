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

#include "libmesh/meshfree_interpolation.h"


class BoundaryDelaunayGenerator : public MeshGenerator
{
public:
  static InputParameters validParams();

  BoundaryDelaunayGenerator(const InputParameters & parameters);

  virtual std::unique_ptr<MeshBase> generate() override;

protected:
  ///The input mesh name
  const MeshGeneratorName _input;
  ///The boundaries to be removed
  const std::vector<BoundaryName> _boundary_names;
  ///
  std::unique_ptr<MeshBase> * _2d_mesh;
  /**
   *
   */
  Point elemNormal(const Elem & elem);
};
