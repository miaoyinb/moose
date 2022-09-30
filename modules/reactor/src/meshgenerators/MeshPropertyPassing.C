//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MeshPropertyPassing.h"
#include "MooseMeshUtils.h"

// C++ includes
#include <cmath> // provides round, not std::round (see http://www.cplusplus.com/reference/cmath/round/)

registerMooseObject("ReactorApp", MeshPropertyPassing);

InputParameters
MeshPropertyPassing::validParams()
{
  InputParameters params = PolygonMeshGeneratorBase::validParams();
  params.addRequiredParam<MeshGeneratorName>("input", "The input mesh to be modified.");
  params.addClassDescription("This MeshPropertyPassing object.");

  return params;
}

MeshPropertyPassing::MeshPropertyPassing(const InputParameters & parameters)
  : PolygonMeshGeneratorBase(parameters),
    _input_name(getParam<MeshGeneratorName>("input")),
    _input(getMeshByName(_input_name))
{
}

std::unique_ptr<MeshBase>
MeshPropertyPassing::generate()
{
  return std::move(_input);
}
