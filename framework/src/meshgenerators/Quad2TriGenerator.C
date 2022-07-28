//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "Quad2TriGenerator.h"
#include "MooseMesh.h"
#include "Conversion.h"
#include "MooseMeshUtils.h"
#include "CastUniquePointer.h"

#include "libmesh/elem.h"

registerMooseObject("MooseApp", Quad2TriGenerator);

InputParameters
Quad2TriGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addClassDescription(
      "Convert Quad mesh to Tri mesh.");

  return params;
}

Quad2TriGenerator::Quad2TriGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters), _input(getMesh("input"))
{
}

std::unique_ptr<MeshBase>
Quad2TriGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);
  MeshTools::Modification::all_tri(*mesh);
  return dynamic_pointer_cast<MeshBase>(mesh);
}
