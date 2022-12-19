//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MeshGenerator.h"
#include "MooseMesh.h"
#include "MooseApp.h"

InputParameters
MeshGenerator::validParams()
{
  InputParameters params = MooseObject::validParams();

  params.addParam<bool>("show_info",
                        false,
                        "Whether or not to show mesh info after generating the mesh "
                        "(bounding box, element types, sidesets, nodesets, subdomains, etc)");
  params.addParam<std::vector<std::string>>(
      "selected_mesh_metadata_to_retain",
      std::vector<std::string>(),
      "Names of input mesh's metadata that are selected to retain.");
  params.addParam<bool>("retain_all_input_mesh_metadata",
                        false,
                        "Whether to retain all the mesh metadata of the input mesh.");

  params.addParamNamesToGroup(
      "selected_mesh_metadata_to_retain retain_all_input_mesh_metadata", "Mesh Metadata Retainment");

  params.registerBase("MeshGenerator");

  return params;
}

MeshGenerator::MeshGenerator(const InputParameters & parameters)
  : MooseObject(parameters),
    MeshMetaDataInterface(this),
    _mesh(_app.actionWarehouse().mesh()),
    _selected_mesh_metadata_to_retain(
        getParam<std::vector<std::string>>("selected_mesh_metadata_to_retain")),
    _retain_all_input_mesh_metadata(getParam<bool>("retain_all_input_mesh_metadata"))
{
  if (isParamValid("input"))
  {
    const MeshGeneratorName input_name(getParam<MeshGeneratorName>("input"));
    if (_retain_all_input_mesh_metadata)
    {
      if (!_selected_mesh_metadata_to_retain.empty())
        paramError(
            "selected_mesh_metadata_to_retain",
            "This parameter should not be provided if retain_all_input_mesh_metadata is set true.");
      const auto mesh_metadata_names = findMeshMetaData(input_name);
      for (const auto & mmd_name : mesh_metadata_names)
        addMeshMetaDataAlias(input_name, mmd_name, name(), mmd_name);
    }
    else
      for (const auto & mmd_name : _selected_mesh_metadata_to_retain)
      {
        if (!hasMeshProperty(mmd_name, input_name))
          paramError("selected_mesh_metadata_to_retain",
                     "The specified mesh metadata to retain does not exist in the input mesh.");
        addMeshMetaDataAlias(input_name, mmd_name, name(), mmd_name);
      }
  }
  else
  {
    if (_retain_all_input_mesh_metadata)
      paramError("retain_all_input_mesh_metadata",
                 "In the absence of an input mesh, this parameter must not be true.");
    if (!_selected_mesh_metadata_to_retain.empty())
      paramError("selected_mesh_metadata_to_retain",
                 "In the absence of an input mesh, this parameter must be empty.");
  }
}

std::unique_ptr<MeshBase> &
MeshGenerator::getMesh(const std::string & param_name, const bool allow_invalid /* = false */)
{
  const auto valid_param = isParamValid(param_name);
  if (!allow_invalid)
  {
    if (!valid_param)
      mooseError("Failed to get a parameter with the name \"",
                 param_name,
                 "\" when getting a MeshGenerator.",
                 "\n\nKnown parameters:\n",
                 _pars);
    if (!_pars.isType<MeshGeneratorName>(param_name))
      paramError(param_name,
                 "Parameter of type \"",
                 _pars.type(param_name),
                 "\" is not an expected type for getting a MeshGenerator (should be of type "
                 "\"MeshGeneratorName\")");
  }
  else if (!valid_param)
    return _null_mesh;

  return getMeshByName(getParam<MeshGeneratorName>(param_name));
}

std::vector<std::unique_ptr<MeshBase> *>
MeshGenerator::getMeshes(const std::string & param_name)
{
  if (!isParamValid(param_name))
    mooseError("Failed to get a parameter with the name \"",
               param_name,
               "\" when getting MeshGenerators.",
               "\n\nKnown parameters:\n",
               _pars);
  if (!_pars.isType<std::vector<MeshGeneratorName>>(param_name))
    paramError(param_name,
               "Parameter of type \"",
               _pars.type(param_name),
               "\" is not an expected type for getting MeshGenerators (should be of type "
               "\"std::vector<MeshGeneratorName>\")");

  return getMeshesByName(getParam<std::vector<MeshGeneratorName>>(param_name));
}

std::unique_ptr<MeshBase> &
MeshGenerator::getMeshByName(const MeshGeneratorName & mesh_generator_name)
{
  _depends_on.push_back(mesh_generator_name);
  return _app.getMeshGeneratorOutput(mesh_generator_name);
}

std::vector<std::unique_ptr<MeshBase> *>
MeshGenerator::getMeshesByName(const std::vector<MeshGeneratorName> & mesh_generator_names)
{
  std::vector<std::unique_ptr<MeshBase> *> meshes;
  for (const auto & name : mesh_generator_names)
    meshes.push_back(&getMeshByName(name));
  return meshes;
}

std::unique_ptr<MeshBase>
MeshGenerator::buildMeshBaseObject(unsigned int dim)
{
  mooseAssert(_mesh, "Need a MooseMesh object");
  return _mesh->buildMeshBaseObject(dim);
}

std::unique_ptr<ReplicatedMesh>
MeshGenerator::buildReplicatedMesh(unsigned int dim)
{
  mooseAssert(_mesh, "Need a MooseMesh object");
  return _mesh->buildTypedMesh<ReplicatedMesh>(dim);
}

std::unique_ptr<DistributedMesh>
MeshGenerator::buildDistributedMesh(unsigned int dim)
{
  mooseAssert(_mesh, "Need a MooseMesh object");
  return _mesh->buildTypedMesh<DistributedMesh>(dim);
}

std::unique_ptr<MeshBase>
MeshGenerator::generateInternal()
{
  auto mesh = generate();

  if (getParam<bool>("show_info"))
  {
    const auto mesh_info = mesh->get_info(/* verbosity = */ 2);

    // We will prefix all information with "type() 'name()':" because this could potentially
    // output a ton of information and looks a bit better with a prefix
    std::stringstream oss;
    const auto split = MooseUtils::split(mesh_info, "\n");
    if (split.size())
      for (std::size_t i = 0; i < split.size() - 1; ++i) // ignore the last line break
        oss << COLOR_CYAN << "" << type() << " '" << name() << "': " << COLOR_DEFAULT << split[i]
            << std::endl;
    _console << oss.str() << std::flush;
  }

  return mesh;
}

std::unique_ptr<MeshBase> &
MeshGenerator::addMeshSubgenerator(const std::string & generator_name,
                                   const std::string & name,
                                   InputParameters & params)
{
  // In case the user forgot it
  params.set<MooseApp *>("_moose_app") = &_app;

  _app.addMeshGenerator(generator_name, name, params);

  return this->getMeshByName(name);
}
