//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BoundaryTransitionGenerator.h"
#include "MooseMeshElementConversionUtils.h"
#include "CastUniquePointer.h"
#include "MooseMeshUtils.h"

registerMooseObject("MooseApp", BoundaryTransitionGenerator);

InputParameters
BoundaryTransitionGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addParam<MeshGeneratorName>("input", "mesh to create the boundary transition layer on.");
  params.addRequiredParam<std::vector<BoundaryName>>(
      "boundary_names", "Boundaries that need to be converted to form a transition layer.");
  params.addParam<unsigned int>(
      "conversion_element_layer_number",
      1,
      "The number of layers of elements to be converted. The farthest layer of the elements from "
      "the given boundary are converted to elements compatible with the remainder of the mesh, "
      "while the other layers of elements are converted into TET4.");
  params.addParam<bool>("external_boundaries_checking",
                        false,
                        "Whether to check if provided boundaries are external.");

  params.addClassDescription("Convert the elements involved in a set of external boundaries to "
                             "ensure that the boundary set only contains TRI3 elements");

  return params;
}

BoundaryTransitionGenerator::BoundaryTransitionGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _boundary_names(getParam<std::vector<BoundaryName>>("boundary_names")),
    _conversion_element_layer_number(getParam<unsigned int>("conversion_element_layer_number")),
    _external_boundaries_checking(getParam<bool>("external_boundaries_checking"))
{
}

std::unique_ptr<MeshBase>
BoundaryTransitionGenerator::generate()
{
  auto replicated_mesh_ptr = dynamic_cast<ReplicatedMesh *>(_input.get());
  if (!replicated_mesh_ptr)
    paramError("input", "Input is not a replicated mesh, which is required");

  ReplicatedMesh & mesh = *replicated_mesh_ptr;

  try
  {
    MooseMeshElementConversionUtils::transitionLayerGenerator(
        mesh, _boundary_names, _conversion_element_layer_number, _external_boundaries_checking);
  }
  catch (const MooseException & e)
  {
    if (((std::string)e.what()).compare(13, 8, "boundary") == 0)
      paramError("boundary_names", e.what());
    else
      paramError("input", e.what());
  }

  return std::move(_input);
}
