//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once
#include "PolygonMeshGeneratorBase.h"

/**
 * This MeshLineCutter object is.
 */
class MeshLineCutter : public PolygonMeshGeneratorBase
{
public:
  static InputParameters validParams();

  MeshLineCutter(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  ///
  const MeshGeneratorName _input_name;
  ///
  const std::vector<Real> _cut_line_params;
  /// Reference to input mesh pointer
  std::unique_ptr<MeshBase> & _input;
};
