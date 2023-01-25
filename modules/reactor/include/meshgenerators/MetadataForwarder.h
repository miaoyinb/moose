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
#include "MooseMeshUtils.h"
#include "MeshMetaDataInterface.h"

/**
 * This MetadataForwarder object adds a circular peripheral region to the input mesh.
 */
class MetadataForwarder : public PolygonMeshGeneratorBase
{
public:
  static InputParameters validParams();

  MetadataForwarder(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  ///
  const std::vector<std::string> _type_lib = {"std::vector<unsigned int>",
                                              "double",
                                              "std::vector<double>",
                                              "unsigned short",
                                              "unsigned int",
                                              "bool",
                                              "unsigned long long",
                                              "std::string",
                                              "int",
                                              "libMesh::Point",
                                              "std::vector<unsigned int>",
                                              "std::vector<int>",
                                              "std::vector<unsigned short>",
                                              "std::vector<unsigned long long>",
                                              "std::vector<libMesh::Point>",
                                              "std::vector<std::vector<double>>"};
  /// Name of the mesh generator to get the input mesh
  const MeshGeneratorName _input_name;
  /// Reference to input mesh pointer
  std::unique_ptr<MeshBase> & _input;
  ///
  std::vector<std::string> _md_names;
  ///
  std::vector<unsigned short> _types;

  std::vector<std::string> findMeshMetaData(std::string prefix) const;
};
