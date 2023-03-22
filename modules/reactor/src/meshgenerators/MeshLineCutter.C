//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MeshLineCutter.h"
#include "MooseMeshCuttingUtils.h"

// C++ includes
#include <cmath>

registerMooseObject("ReactorApp", MeshLineCutter);

InputParameters
MeshLineCutter::validParams()
{
  InputParameters params = PolygonMeshGeneratorBase::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The input mesh that needs to be trimmed.");
  params.addRequiredParam<std::vector<Real>>("cut_line_params", "Cutting Line Parameters");

  params.addClassDescription("This MeshLineCutter object is.");

  return params;
}

MeshLineCutter::MeshLineCutter(const InputParameters & parameters)
  : PolygonMeshGeneratorBase(parameters),
    _input_name(getParam<MeshGeneratorName>("input")),
    _cut_line_params(getParam<std::vector<Real>>("cut_line_params")),
    _input(getMeshByName(_input_name))
{
}

std::unique_ptr<MeshBase>
MeshLineCutter::generate()
{
  auto replicated_mesh_ptr = dynamic_cast<ReplicatedMesh *>(_input.get());
  if (!replicated_mesh_ptr)
    paramError("input", "Input is not a replicated mesh, which is required");

  ReplicatedMesh & mesh = *replicated_mesh_ptr;

  std::set<subdomain_id_type> subdomain_ids_set;
  mesh.subdomain_ids(subdomain_ids_set);
  const subdomain_id_type max_subdomain_id = *subdomain_ids_set.rbegin();
  const subdomain_id_type block_id_to_remove = max_subdomain_id + 1;

  MooseMeshCuttingUtils::lineRemover(
      mesh, _cut_line_params, block_id_to_remove, subdomain_ids_set, 12345);

  if (MooseMeshCuttingUtils::quasiTriElementsFixer(mesh, subdomain_ids_set))
    mesh.prepare_for_use();
  return std::move(_input);
}
