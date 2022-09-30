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
#include "MooseEnum.h"

/**
 * This MeshPropertyPassing object
 */
class MeshPropertyPassing : public PolygonMeshGeneratorBase
{
public:
  static InputParameters validParams();

  MeshPropertyPassing(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  /// Input mesh to be modified
  const MeshGeneratorName _input_name;
  /// Reference to input mesh pointer
  std::unique_ptr<MeshBase> & _input;
};
