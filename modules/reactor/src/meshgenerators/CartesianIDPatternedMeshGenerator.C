//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "CartesianIDPatternedMeshGenerator.h"

registerMooseObject("ReactorApp", CartesianIDPatternedMeshGenerator);

InputParameters
CartesianIDPatternedMeshGenerator::validParams()
{
  InputParameters params = PatternedMeshGenerator::validParams();
  MooseEnum option("cell pattern manual", "cell");
  params.addRequiredParam<std::string>("id_name", "Name of Integer ID set");
  params.addParam<std::vector<MeshGeneratorName>>(
      "exclude_id", "Name of inputs to be excluded in ID generation.");
  params.addParam<MooseEnum>("assign_type", option, "Type of integer id assignment");
  params.addParam<std::vector<std::vector<dof_id_type>>>(
      "id_pattern",
      "User-defined element IDs. A double-indexed array starting with the upper-left corner");
  params.addClassDescription("This CartesianIDPatternedMeshGenerator source code is to generate "
                             "patterned Cartesian meshes with Reporting ID");
  return params;
}

CartesianIDPatternedMeshGenerator::CartesianIDPatternedMeshGenerator(
    const InputParameters & parameters)
  : PatternedMeshGenerator(parameters),
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
    for (unsigned int i = 0; i < _input_names.size(); ++i)
      _exclude_ids[i] = false;
}

std::unique_ptr<MeshBase>
CartesianIDPatternedMeshGenerator::generate()
{
  auto mesh = PatternedMeshGenerator::generate();
  // assumes that the entire mesh has elements of each individual mesh sequentially ordered.
  std::vector<dof_id_type> integer_ids;
  if (_assign_type == "cell")
    integer_ids = getCellwiseIntegerIDs(_meshes, _pattern, _use_exclude_id, _exclude_ids);
  else if (_assign_type == "pattern")
    integer_ids = getPatternIntegerIDs(_meshes, _pattern);
  else if (_assign_type == "manual")
    integer_ids = getManualIntegerIDs(_meshes, _pattern, _id_pattern);

  unsigned int extra_id_index = 0;
  if (!mesh->has_elem_integer(_element_id_name))
    extra_id_index = mesh->add_elem_integer(_element_id_name);
  else
    extra_id_index = mesh->get_elem_integer_index(_element_id_name);
  unsigned int i = 0;
  for (auto & elem : mesh->element_ptr_range())
    elem->set_extra_integer(extra_id_index, integer_ids[i++]);
  return mesh;
}
