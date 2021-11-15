//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "HexIDPatternedMeshGenerator.h"

registerMooseObject("ReactorApp", HexIDPatternedMeshGenerator);

InputParameters
HexIDPatternedMeshGenerator::validParams()
{
  InputParameters params = PatternedHexMeshGenerator::validParams();
  params.addRequiredParam<std::string>("id_name", "Reporting_id_name");
  params.addParam<std::vector<MeshGeneratorName>>(
      "exclude_id", "Name of inputs to be excluded in ID generation.");
  MooseEnum option("cell pattern manual", "cell");
  params.addParam<MooseEnum>("assign_type", option, "Type of integer id assignment");
  params.addParam<std::vector<std::vector<dof_id_type>>>(
      "id_pattern",
      "User-defined element IDs. A double-indexed array starting with the upper-left corner");
  params.addClassDescription("This HexIDPatternedMeshGenerator source code is to generate "
                             "patterned hexagonal meshes with Reporting ID");
  return params;
}

HexIDPatternedMeshGenerator::HexIDPatternedMeshGenerator(const InputParameters & parameters)
  : PatternedHexMeshGenerator(parameters),
    _element_id_name(getParam<std::string>("id_name")),
    _assign_type(getParam<MooseEnum>("assign_type")),
    _use_exclude_id(isParamValid("exclude_id"))
{
  if (_use_exclude_id && _assign_type != "cell")
    paramError("exclude_id", "works only when \"assign_type\" is equal 'cell'");
  if (!isParamValid("id_pattern") && _assign_type == "manual")
    paramError("id_pattern", "required when \"assign_type\" is equal to 'manual'");

  if (_assign_type == "manual")
    _id_pattern = getParam<std::vector<std::vector<dof_id_type>>>("id_pattern");
  _exclude_ids.resize(_input_names.size());
  if (_use_exclude_id)
  {
    std::vector<MeshGeneratorName> exclude_id_name =
        getParam<std::vector<MeshGeneratorName>>("exclude_id");
    for (unsigned int i = 0; i < _input_names.size(); ++i)
    {
      _exclude_ids[i] = false;
      for (auto input_name : exclude_id_name)
        if (_input_names[i] == input_name)
        {
          _exclude_ids[i] = true;
          break;
        }
    }
  }
  else
  {
    for (unsigned int i = 0; i < _input_names.size(); ++i)
      _exclude_ids[i] = false;
  }
}

std::unique_ptr<MeshBase>
HexIDPatternedMeshGenerator::generate()
{
  auto mesh = PatternedHexMeshGenerator::generate();

  std::vector<std::unique_ptr<ReplicatedMesh>> meshes;
  meshes.reserve(_input_names.size());
  for (MooseIndex(_input_names) i = 0; i < _input_names.size(); ++i)
  {
    std::unique_ptr<ReplicatedMesh> cell_mesh =
        dynamic_pointer_cast<ReplicatedMesh>(*_mesh_ptrs[i]);
    meshes.push_back(std::move(cell_mesh));
  }

  std::vector<dof_id_type> integer_ids;
  if (_assign_type == "cell")
    integer_ids = getCellwiseIntegerIDs(meshes, _pattern, _use_exclude_id, _exclude_ids);
  else if (_assign_type == "pattern")
    integer_ids = getPatternIntegerIDs(meshes, _pattern);
  else if (_assign_type == "manual")
    integer_ids = getManualIntegerIDs(meshes, _pattern, _id_pattern);

  std::set<SubdomainID> blks = getCellBlockIDs(meshes, _pattern);
  unsigned int duct_boundary_id = *std::max_element(integer_ids.begin(), integer_ids.end()) + 1;

  std::map<SubdomainID, unsigned int> blks_duct = getDuckBlockIDs(mesh, blks);

  unsigned int extra_id_index = mesh->add_elem_integer(_element_id_name);

  unsigned int i = 0;
  unsigned int id = integer_ids[i];
  unsigned old_id = id;
  for (auto elem : mesh->element_ptr_range())
  {
    auto blk = elem->subdomain_id();
    auto it = blks.find(blk);
    if (it == blks.end())
    {
      if (_has_assembly_duct)
      {
        auto it2 = blks_duct.find(blk);
        if (it2 == blks_duct.end())
          elem->set_extra_integer(extra_id_index, old_id);
        else
          elem->set_extra_integer(extra_id_index, duct_boundary_id + it2->second);
      }
      else
        elem->set_extra_integer(extra_id_index, old_id);
    }
    else
    {
      elem->set_extra_integer(extra_id_index, id);
      ++i;
      old_id = id;
      id = integer_ids[i];
    }
  }
  return dynamic_pointer_cast<MeshBase>(mesh);
}

std::map<SubdomainID, unsigned int>
HexIDPatternedMeshGenerator::getDuckBlockIDs(std::unique_ptr<MeshBase> & mesh,
                                             std::set<SubdomainID> & blks) const
{
  unsigned int i = 0;
  std::map<SubdomainID, unsigned int> blks_duct;
  if (_has_assembly_duct)
  {
    for (auto elem : mesh->element_ptr_range())
    {
      auto blk = elem->subdomain_id();
      auto it1 = blks.find(blk);
      if (it1 == blks.end())
      {
        auto it2 = blks_duct.find(blk);
        if (it2 == blks_duct.end())
          blks_duct[blk] = i++;
      }
    }
    blks_duct.erase(blks_duct.begin());
  }
  return blks_duct;
}
