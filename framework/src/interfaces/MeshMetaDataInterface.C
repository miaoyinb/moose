//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

// MOOSE includes
#include "MeshMetaDataInterface.h"
#include "MooseApp.h"
#include "MeshGenerator.h"

MeshMetaDataInterface::MeshMetaDataInterface(const MooseObject * moose_object)
  : _meta_data_app(moose_object->getMooseApp())
{
}

MeshMetaDataInterface::MeshMetaDataInterface(MooseApp & moose_app) : _meta_data_app(moose_app) {}

RestartableDataValue &
MeshMetaDataInterface::registerMetaDataOnApp(const std::string & name,
                                             std::unique_ptr<RestartableDataValue> data)
{
  return _meta_data_app.registerRestartableData(
      name, std::move(data), 0, true, MooseApp::MESH_META_DATA);
}

bool
MeshMetaDataInterface::hasMeshProperty(const std::string & data_name,
                                       const std::string & prefix) const
{
  std::string full_name = std::string(SYSTEM) + "/" + prefix + "/" + data_name;
  return _meta_data_app.hasRestartableMetaData(findMeshMetaDataAlias(full_name),
                                               MooseApp::MESH_META_DATA);
}

bool
MeshMetaDataInterface::hasMeshMetaDataAliasSet()
{
  const std::string full_name = std::string(SYSTEM) + "/MeshMetaDataAliasSystem";
  return _meta_data_app.hasRestartableMetaData(full_name, MooseApp::MESH_META_DATA);
}

void
MeshMetaDataInterface::addMeshMetaDataAlias(std::string original_prefix,
                                            std::string original_name,
                                            std::string new_prefix,
                                            std::string new_name)
{
  // The alias information is saved as a RestartableData with the following name
  const std::string full_name = std::string(SYSTEM) + "/MeshMetaDataAliasSystem";
  const std::string full_original_name =
      std::string(SYSTEM) + "/" + original_prefix + "/" + original_name;
  const std::string full_new_name = std::string(SYSTEM) + "/" + new_prefix + "/" + new_name;

  // Check if the new mesh metadata name has already been declared
  if (hasMeshProperty(new_name, new_prefix))
    mooseError("in Mesh Generator ",
               new_prefix,
               ": the mesh metadata ",
               new_name,
               " has already been declared.");
  // Create the mesh metadata alias system if it does not exist yet
  if (!_meta_data_app.hasRestartableMetaData(full_name, MooseApp::MESH_META_DATA))
  {
    auto data_ptr = std::make_unique<RestartableData<std::unordered_map<std::string, std::string>>>(
        full_name, nullptr);
    auto & meta_data_alias_ref =
        static_cast<RestartableData<std::unordered_map<std::string, std::string>> &>(
            _meta_data_app.registerRestartableData(
                full_name, std::move(data_ptr), 0, false, MooseApp::MESH_META_DATA));
    // Add the alias information to the just-created alias system
    // Note that if the original name itself is an alias, we need to track back to the real origin
    meta_data_alias_ref.set().emplace(
        std::make_pair(full_new_name, findMeshMetaDataAlias(full_original_name)));
  }
  // Just add a new line of alias information if the mesh metadata alias system already exists
  else
  {
    const RestartableDataMap & meta_data =
        _meta_data_app.getRestartableDataMap(MooseApp::MESH_META_DATA);
    auto it = meta_data.find(full_name);
    const RestartableDataValuePair & pitch_meta_data = it->second;
    auto & meta_data_alias_ref =
        static_cast<RestartableData<std::unordered_map<std::string, std::string>> &>(
            *pitch_meta_data.value);
    meta_data_alias_ref.set().emplace(
        std::make_pair(full_new_name, findMeshMetaDataAlias(full_original_name)));
  }
}

std::vector<std::string>
MeshMetaDataInterface::findMeshMetaData(std::string prefix) const
{
  std::vector<std::string> res;
  const std::string full_prefix = std::string(SYSTEM) + "/" + prefix + "/";
  const RestartableDataMap & meta_data =
      _meta_data_app.getRestartableDataMap(MooseApp::MESH_META_DATA);
  for (const auto & meta_data_item : meta_data)
  {
    if (meta_data_item.first.size() > full_prefix.size())
      if (meta_data_item.first.compare(0, full_prefix.size(), full_prefix) == 0)
        res.push_back(meta_data_item.first.substr(
            full_prefix.size(), meta_data_item.first.size() - full_prefix.size()));
  }
  return res;
}

std::string
MeshMetaDataInterface::findMeshMetaDataAlias(std::string full_new_name) const
{
  const std::string full_name = std::string(SYSTEM) + "/MeshMetaDataAliasSystem";
  // If the mesh metadata alias system has not yet been created, no metadata has an alias. Just
  // return the input name
  if (!_meta_data_app.hasRestartableMetaData(full_name, MooseApp::MESH_META_DATA))
    return full_new_name;
  const RestartableDataMap & meta_data =
      _meta_data_app.getRestartableDataMap(MooseApp::MESH_META_DATA);
  auto it = meta_data.find(full_name);
  const RestartableDataValuePair & data_ptr = it->second;
  auto & meta_data_alias_ref_tmp =
      static_cast<RestartableData<std::unordered_map<std::string, std::string>> &>(*data_ptr.value);
  auto & meta_data_alias_ref = meta_data_alias_ref_tmp.get();

  auto map_it = meta_data_alias_ref.find(full_new_name);
  // return the input name if it is not registered in the alias system, otherwise return the
  // original name
  if (map_it == meta_data_alias_ref.end())
    return full_new_name;
  else
    return (*map_it).second;
}

void
MeshMetaDataInterface::checkMeshMetadataRetainingSetting(
    const MooseObject * moose_object,
    const bool has_input,
    const bool retain_all_input_mesh_metadata,
    const std::vector<std::string> selected_mesh_metadata_to_retain)
{
  if (!has_input)
  {
    if (retain_all_input_mesh_metadata)
      moose_object->paramError("retain_all_input_mesh_metadata",
                               "In the absence of an input mesh, this parameter must not be true.");
    if (!selected_mesh_metadata_to_retain.empty())
      moose_object->paramError("selected_mesh_metadata_to_retain",
                               "In the absence of an input mesh, this parameter must be empty.");
  }
  else if (retain_all_input_mesh_metadata && !selected_mesh_metadata_to_retain.empty())
    moose_object->paramError(
        "selected_mesh_metadata_to_retain",
        "This parameter should not be provided if retain_all_input_mesh_metadata is set true.");
}

void
MeshMetaDataInterface::retainAllInputMetaData(const MeshGeneratorName input_name,
                                              const MeshGeneratorName current_name)
{
  const auto mesh_metadata_names = findMeshMetaData(input_name);
  for (const auto & mmd_name : mesh_metadata_names)
    addMeshMetaDataAlias(input_name, mmd_name, current_name, mmd_name);
}

void
MeshMetaDataInterface::retainMeshMetaData(
    const MooseObject * moose_object,
    const MeshGeneratorName input_name,
    const bool retain_all_input_mesh_metadata,
    const std::vector<std::string> selected_mesh_metadata_to_retain)
{
  if (retain_all_input_mesh_metadata)
    retainAllInputMetaData(input_name, moose_object->name());
  for (const auto & mmd_name : selected_mesh_metadata_to_retain)
  {
    if (!hasMeshProperty(mmd_name, input_name))
      moose_object->paramError(
          "selected_mesh_metadata_to_retain",
          "The specified mesh metadata to retain does not exist in the input mesh.");
    addMeshMetaDataAlias(input_name, mmd_name, moose_object->name(), mmd_name);
  }
}