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
  return _meta_data_app.hasRestartableMetaData(full_name, MooseApp::MESH_META_DATA);
}

void
MeshMetaDataInterface::checkMeshMetadataForwardingSetting(
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

std::vector<std::string>
MeshMetaDataInterface::identifyMeshMetaData(const MeshGeneratorName prefix) const
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