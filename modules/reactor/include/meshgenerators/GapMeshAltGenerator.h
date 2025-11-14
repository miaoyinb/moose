//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "PolygonMeshGeneratorBase.h"

/**
 * Generates a triangulation in the XY plane, based on an input mesh
 * defining the outer boundary (as well as explicitly required
 * interior Steiner points) and an optional set of input meshes
 * defining inner hole boundaries.
 */
class GapMeshAltGenerator : public PolygonMeshGeneratorBase
{
public:
  static InputParameters validParams();

  GapMeshAltGenerator(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  /// Input mesh defining the boundary
  std::unique_ptr<MeshBase> & _input;

  /// The thickness of the gap to be created
  const Real & _thickness;

  bool isPointsColinear(const Point & p1, const Point & p2, const Point & p3) const;

  bool
  fourPointOverlap(const Point & p1, const Point & p2, const Point & p3, const Point & p4) const;
};
