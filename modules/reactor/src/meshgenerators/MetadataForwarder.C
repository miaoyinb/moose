//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MetadataForwarder.h"

#include "MooseMeshUtils.h"
#include "MooseUtils.h"
#include "LinearInterpolation.h"
#include "FillBetweenPointVectorsTools.h"

// C++ includes
#include <cmath> // for atan2

registerMooseObject("ReactorApp", MetadataForwarder);

InputParameters
MetadataForwarder::validParams()
{
  InputParameters params = PolygonMeshGeneratorBase::validParams();
  params.addRequiredParam<MeshGeneratorName>("input", "The input mesh to be modified.");

  params.addClassDescription("This MetadataForwarder object adds a circular peripheral "
                             "region to the input mesh.");

  return params;
}

MetadataForwarder::MetadataForwarder(const InputParameters & parameters)
  : PolygonMeshGeneratorBase(parameters),
    _input_name(getParam<MeshGeneratorName>("input")),
    _input(getMeshByName(_input_name))
{
  _md_names = findMeshMetaData(_input_name);
  for (const auto & md_name : _md_names)
  {
    const auto name_old =
        std::string(MeshMetaDataInterface::SYSTEM) + "/" + _input_name + "/" + md_name;
    const auto old_type_str =
        _app.getRestartableMetaData(name_old, MooseApp::MESH_META_DATA, 0).type();
    std::cout << "Metadata name is " << md_name << " with type: " << old_type_str << std::endl;

    _types.push_back(std::distance(_type_lib.begin(),
                                   std::find(_type_lib.begin(), _type_lib.end(), old_type_str)));
    switch (_types.back())
    {
      case 0:
        declareMeshProperty<std::vector<unsigned int>>(md_name);
        break;
      case 1:
        declareMeshProperty<double>(md_name);
        break;
      case 2:
        declareMeshProperty<std::vector<double>>(md_name);
        break;
      case 3:
        declareMeshProperty<unsigned short>(md_name);
        break;
      case 4:
        declareMeshProperty<unsigned int>(md_name);
        break;
      case 5:
        declareMeshProperty<bool>(md_name);
        break;
      case 6:
        declareMeshProperty<unsigned long long>(md_name);
        break;
      case 7:
        declareMeshProperty<std::string>(md_name);
      case 8:
        declareMeshProperty<int>(md_name);
        break;
      case 9:
        declareMeshProperty<Point>(md_name);
        break;
      case 10:
        declareMeshProperty<std::vector<unsigned int>>(md_name);
        break;
      case 11:
        declareMeshProperty<std::vector<int>>(md_name);
        break;
      case 12:
        declareMeshProperty<std::vector<unsigned short>>(md_name);
        break;
      case 13:
        declareMeshProperty<std::vector<unsigned long long>>(md_name);
        break;
      case 14:
        declareMeshProperty<std::vector<Point>>(md_name);
        break;
      case 15:
        declareMeshProperty<std::vector<std::vector<double>>>(md_name);
        break;
        // default:
    }
  }
}

std::unique_ptr<MeshBase>
MetadataForwarder::generate()
{
  for (unsigned int i = 0; i != _types.size(); i++)
  {
    switch (_types[i])
    {
      case 0:
        setMeshProperty(_md_names[i],
                        getMeshProperty<std::vector<unsigned int>>(_md_names[i], _input_name));
        break;
      case 1:
        setMeshProperty(_md_names[i], getMeshProperty<double>(_md_names[i], _input_name));
        break;
      case 2:
        setMeshProperty(_md_names[i],
                        getMeshProperty<std::vector<double>>(_md_names[i], _input_name));
        break;
      case 3:
        setMeshProperty(_md_names[i], getMeshProperty<unsigned short>(_md_names[i], _input_name));
        break;
      case 4:
        setMeshProperty(_md_names[i], getMeshProperty<unsigned int>(_md_names[i], _input_name));
        break;
      case 5:
        setMeshProperty(_md_names[i], getMeshProperty<bool>(_md_names[i], _input_name));
        break;
      case 6:
        setMeshProperty(_md_names[i],
                        getMeshProperty<unsigned long long>(_md_names[i], _input_name));
        break;
      case 7:
        setMeshProperty(_md_names[i], getMeshProperty<std::string>(_md_names[i], _input_name));
        break;
      case 8:
        setMeshProperty(_md_names[i], getMeshProperty<int>(_md_names[i], _input_name));
        break;
      case 9:
        setMeshProperty(_md_names[i], getMeshProperty<Point>(_md_names[i], _input_name));
        break;
      case 10:
        setMeshProperty(_md_names[i],
                        getMeshProperty<std::vector<unsigned int>>(_md_names[i], _input_name));
        break;
      case 11:
        setMeshProperty(_md_names[i], getMeshProperty<std::vector<int>>(_md_names[i], _input_name));
        break;
      case 12:
        setMeshProperty(_md_names[i],
                        getMeshProperty<std::vector<unsigned short>>(_md_names[i], _input_name));
        break;
      case 13:
        setMeshProperty(
            _md_names[i],
            getMeshProperty<std::vector<unsigned long long>>(_md_names[i], _input_name));
        break;
      case 14:
        setMeshProperty(_md_names[i],
                        getMeshProperty<std::vector<Point>>(_md_names[i], _input_name));
        break;
      case 15:
        setMeshProperty(
            _md_names[i],
            getMeshProperty<std::vector<std::vector<double>>>(_md_names[i], _input_name));
        break;
        // default:
    }
  }

  return dynamic_pointer_cast<MeshBase>(_input);
}

std::vector<std::string>
MetadataForwarder::findMeshMetaData(std::string prefix) const
{
  std::vector<std::string> res;
  const std::string full_prefix = std::string(SYSTEM) + "/" + prefix + "/";
  const RestartableDataMap & meta_data = _app.getRestartableDataMap(MooseApp::MESH_META_DATA);
  for (const auto & meta_data_item : meta_data)
  {
    if (meta_data_item.first.size() > full_prefix.size())
      if (meta_data_item.first.compare(0, full_prefix.size(), full_prefix) == 0)
        res.push_back(meta_data_item.first.substr(
            full_prefix.size(), meta_data_item.first.size() - full_prefix.size()));
  }
  return res;
}
