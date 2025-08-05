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

  // Find sid shifts
  _sid_shift_base = MooseMeshUtils::getNextFreeSubdomainID(mesh);

  // It would be convenient to have a single boundary id instead of a vector.
  const auto uniform_tmp_bid = MooseMeshUtils::getNextFreeBoundaryID(mesh);

  // Check the boundaries and merge them
  std::vector<boundary_id_type> boundary_ids;
  for (const auto & sideset : _boundary_names)
  {
    if (!MooseMeshUtils::hasBoundaryName(mesh, sideset))
      paramError(
          "boundary_names", "The provided boundary '", sideset, "' was not found within the mesh");
    boundary_ids.push_back(MooseMeshUtils::getBoundaryID(sideset, mesh));
    MooseMeshUtils::changeBoundaryId(mesh, boundary_ids.back(), uniform_tmp_bid, false);
  }

  auto & sideset_map = mesh.get_boundary_info().get_sideset_map();
  auto side_list = mesh.get_boundary_info().build_side_list();

  std::vector<std::pair<dof_id_type, std::vector<unsigned int>>> elems_list;
  std::vector<std::set<dof_id_type>> layered_elems_list;
  layered_elems_list.push_back(std::set<dof_id_type>());
  // Need to collect the list of elements that need to be converted
  if (_external_boundaries_checking && !mesh.is_prepared())
    mesh.find_neighbors();
  for (const auto & side_info : side_list)
  {
    if (std::get<2>(side_info) == uniform_tmp_bid)
    {
      // Check if the involved side is TRI3 or QUAD4
      // We do not limit the element type in the input mesh
      // As long as the involved boundaries only consist of TRI3 and QUAD4 sides,
      // this generator will work
      // As the side element of a quadratic element is still a linear element in libMesh,
      // we need to check the element's default_side_order() first
      if (mesh.elem_ptr(std::get<0>(side_info))->default_side_order() != 1)
        paramError(
            "boundary_names",
            "The provided boundary set contains non-linear side elements, which is not supported.");
      const auto side_type =
          mesh.elem_ptr(std::get<0>(side_info))->side_ptr(std::get<1>(side_info))->type();
      layered_elems_list.back().emplace(std::get<0>(side_info));
      if (_conversion_element_layer_number == 1)
      {
        if (side_type == TRI3)
          continue; // Already TRI3, no need to convert
        else if (side_type == QUAD4)
        {
          if (elems_list.size() && elems_list.back().first == std::get<0>(side_info))
            elems_list.back().second.push_back(std::get<1>(side_info));
          else
            elems_list.push_back(std::make_pair(
                std::get<0>(side_info), std::vector<unsigned int>({std::get<1>(side_info)})));
        }
      }
      else
        mooseAssert(false,
                    "Impossible scenario: a linear side element that is neither TRI3 nor QUAD4.");
      if (_external_boundaries_checking)
        if (mesh.elem_ptr(std::get<0>(side_info))->neighbor_ptr(std::get<1>(side_info)))
          paramError("boundary_names",
                     "The provided boundary contains non-external sides, which is required when "
                     "external_boundaries_checking is enabled.");
    }
  }

  if (_conversion_element_layer_number > 1)
  {
    std::set<dof_id_type> total_elems_set(layered_elems_list.back());

    while (layered_elems_list.back().size() &&
           layered_elems_list.size() < _conversion_element_layer_number)
    {
      layered_elems_list.push_back(std::set<dof_id_type>());
      for (const auto & elem_id : *(layered_elems_list.end() - 2))
      {
        for (const auto & i_side : make_range(mesh.elem_ptr(elem_id)->n_sides()))
        {
          if (mesh.elem_ptr(elem_id)->neighbor_ptr(i_side) != nullptr)
          {
            const auto & neighbor_id = mesh.elem_ptr(elem_id)->neighbor_ptr(i_side)->id();
            if (total_elems_set.find(neighbor_id) == total_elems_set.end())
            {
              layered_elems_list.back().emplace(neighbor_id);
              total_elems_set.emplace(neighbor_id);
            }
          }
        }
      }
    }
  }

  // Remove the last empty layer
  if (layered_elems_list.back().empty())
    layered_elems_list.pop_back();

  if (_conversion_element_layer_number > layered_elems_list.size())
    paramError("input",
               "This is fewer layers of elements in the input mesh than the requested number of "
               "layers to convert.");

  std::vector<std::pair<dof_id_type, bool>> original_elems;
  // construct a list of the element to convert to tet4
  // Convert at most n_layer_conversion layers of elements
  const unsigned int n_layer_conversion = layered_elems_list.size() - 1;
  for (unsigned int i = 0; i < n_layer_conversion; ++i)
    for (const auto & elem_id : layered_elems_list[i])
    {
      // As these elements will become TET4 elements, we need to shift the subdomain ID
      // But we do not need to convert original TET4 elements
      if (mesh.elem_ptr(elem_id)->type() != TET4)
      {
        original_elems.push_back(std::make_pair(elem_id, false));
        mesh.elem_ptr(elem_id)->subdomain_id() += _sid_shift_base;
      }
    }

  const subdomain_id_type block_id_to_remove = _sid_shift_base * 3;

  std::vector<dof_id_type> converted_elems_ids_to_track;
  MooseMeshElementConversionUtils::convert3DMeshToAllTet4(
      mesh, original_elems, converted_elems_ids_to_track, block_id_to_remove, false);

  // Now we need to convert the elements on the transition layer
  // First, we need to identify that the sides that are on the interface with previous layer (all
  // TET layers)
  if (n_layer_conversion)
  {
    for (const auto & elem_id : layered_elems_list[n_layer_conversion])
    {
      for (const auto & i_side : make_range(mesh.elem_ptr(elem_id)->n_sides()))
      {
        if (mesh.elem_ptr(elem_id)->neighbor_ptr(i_side) != nullptr &&
            layered_elems_list[n_layer_conversion - 1].count(
                mesh.elem_ptr(elem_id)->neighbor_ptr(i_side)->id()))
        {
          if (elems_list.size() && elems_list.back().first == elem_id)
            elems_list.back().second.push_back(i_side);
          else
            elems_list.push_back(std::make_pair(elem_id, std::vector<unsigned int>({i_side})));
        }
      }
    }
  }

  // Now convert the elements
  for (const auto & elem_info : elems_list)
  {
    // Find the involved sidesets of the element so that we can retain them
    std::vector<std::vector<boundary_id_type>> elem_side_info(
        mesh.elem_ptr(elem_info.first)->n_sides());
    auto side_range = sideset_map.equal_range(mesh.elem_ptr(elem_info.first));
    for (auto i = side_range.first; i != side_range.second; ++i)
      elem_side_info[i->second.first].push_back(i->second.second);

    MooseMeshElementConversionUtils::convertElem(
        mesh, elem_info.first, elem_info.second, elem_side_info, _sid_shift_base);
  }

  // delete the original elements that were converted
  for (const auto & elem_info : elems_list)
    mesh.delete_elem(mesh.elem_ptr(elem_info.first));
  for (auto elem_it = mesh.active_subdomain_elements_begin(block_id_to_remove);
       elem_it != mesh.active_subdomain_elements_end(block_id_to_remove);
       elem_it++)
    mesh.delete_elem(*elem_it);
  // delete temporary boundary id
  mesh.get_boundary_info().remove_id(uniform_tmp_bid);

  mesh.contract();
  mesh.set_isnt_prepared();

  return std::move(_input);
}
