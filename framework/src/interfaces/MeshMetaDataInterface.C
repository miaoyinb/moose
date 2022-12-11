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
  return _meta_data_app.hasRestartableMetaData(FindMeshMetaDataAlias(full_name),
                                               MooseApp::MESH_META_DATA);
}

bool
MeshMetaDataInterface::hasMeshMetaDataAliasSet()
{
  const std::string full_name = std::string(SYSTEM) + "/MeshMetaDataAliasSystem";
  return _meta_data_app.hasRestartableMetaData(full_name, MooseApp::MESH_META_DATA);
}

void
MeshMetaDataInterface::AddMeshMetaDataAlias(std::string original_prefix,
                                            std::string original_name,
                                            std::string new_prefix,
                                            std::string new_name)
{
  // The alias information is saved as a RestartableData with the following name
  const std::string full_name = std::string(SYSTEM) + "/MeshMetaDataAliasSystem";
  const std::string full_original_name =
      std::string(SYSTEM) + "/" + original_prefix + "/" + original_name;
  const std::string full_new_name = std::string(SYSTEM) + "/" + new_prefix + "/" + new_name;

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
        std::make_pair(full_new_name, FindMeshMetaDataAlias(full_original_name)));
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
        std::make_pair(full_new_name, FindMeshMetaDataAlias(full_original_name)));
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
MeshMetaDataInterface::FindMeshMetaDataAlias(std::string full_new_name) const
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
