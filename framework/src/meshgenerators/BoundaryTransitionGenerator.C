//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BoundaryTransitionGenerator.h"
#include "CastUniquePointer.h"
#include "MooseMeshUtils.h"

#include "libmesh/cell_pyramid5.h"
#include "libmesh/cell_tet4.h"

registerMooseObject("MooseApp", BoundaryTransitionGenerator);

InputParameters
BoundaryTransitionGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addParam<MeshGeneratorName>("input", "mesh to create the boundary transition layer on.");
  params.addRequiredParam<std::vector<BoundaryName>>(
      "boundary_names", "Boundaries that need to be converted to form a transition layer.");
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
      if (side_type == TRI3)
        continue; // Already TRI3, no need to convert
      else if (side_type == QUAD4)
      {
        if (elems_list.size() && elems_list.back().first == std::get<0>(side_info))
          elems_list.back().second.push_back(std::get<1>(side_info));
        else
          elems_list.push_back(std::make_pair(std::get<0>(side_info),
                                              std::vector<unsigned int>({std::get<1>(side_info)})));
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

  // Now convert the elements
  for (const auto & elem_info : elems_list)
  {
    // Find the involved sidesets of the element so that we can retain them
    std::vector<std::vector<boundary_id_type>> elem_side_info(
        mesh.elem_ptr(elem_info.first)->n_sides());
    auto side_range = sideset_map.equal_range(mesh.elem_ptr(elem_info.first));
    for (auto i = side_range.first; i != side_range.second; ++i)
      elem_side_info[i->second.first].push_back(i->second.second);

    convertElem(mesh, elem_info.first, elem_info.second, elem_side_info);
  }

  // delete the original elements that were converted
  for (const auto & elem_info : elems_list)
    mesh.delete_elem(mesh.elem_ptr(elem_info.first));
  // delete temporary boundary id
  mesh.get_boundary_info().remove_id(uniform_tmp_bid);

  mesh.contract();
  mesh.set_isnt_prepared();

  return std::move(_input);
}

void
BoundaryTransitionGenerator::convertElem(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const std::vector<unsigned int> & side_indices,
    const std::vector<std::vector<boundary_id_type>> & elem_side_info)
{
  const auto & elem_type = mesh.elem_ptr(elem_id)->type();
  switch (elem_type)
  {
    case HEX8:
      convertHex8Elem(mesh, elem_id, side_indices, elem_side_info);
      break;
    case PRISM6:
      convertPrism6Elem(mesh, elem_id, side_indices, elem_side_info);
      break;
    case PYRAMID5:
      convertPyramid5Elem(mesh, elem_id, elem_side_info);
      break;
    default:
      mooseAssert(false,
                  "The provided element type '" + Moose::toString(elem_type) +
                      "' is not supported and is not supposed to be passed to this function. "
                      "Only HEX8, PRISM6 and PYRAMID5 are supported.");
  }
}

void
BoundaryTransitionGenerator::convertHex8Elem(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const std::vector<unsigned int> & side_indices,
    const std::vector<std::vector<boundary_id_type>> & elem_side_info)
{
  // We add a node at the centroid of the HEX8 element
  // With this node, the HEX8 can be converted into 6 PYRAMID5 elements
  // For the PYRAMID5 element, it can further be converted into 2 TET3 elements
  const Point elem_cent = mesh.elem_ptr(elem_id)->true_centroid();
  auto new_node = mesh.add_point(elem_cent);
  for (const auto & i_side : make_range(mesh.elem_ptr(elem_id)->n_sides()))
  {
    if (std::find(side_indices.begin(), side_indices.end(), i_side) != side_indices.end())
      createUnitTet4FromHex8(mesh, elem_id, i_side, new_node, elem_side_info[i_side]);
    else
      createUnitPyramid5FromHex8(mesh, elem_id, i_side, new_node, elem_side_info[i_side]);
  }
}

void
BoundaryTransitionGenerator::createUnitPyramid5FromHex8(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const unsigned int & side_index,
    const Node * new_node,
    const std::vector<boundary_id_type> & side_info)
{
  auto new_elem = std::make_unique<Pyramid5>();
  new_elem->set_node(0, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(3));
  new_elem->set_node(1, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(2));
  new_elem->set_node(2, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(1));
  new_elem->set_node(3, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(0));
  new_elem->set_node(4, const_cast<Node *>(new_node));
  new_elem->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base * 2;
  auto new_elem_ptr = mesh.add_elem(std::move(new_elem));
  retainEEID(mesh, elem_id, new_elem_ptr);
  for (const auto & bid : side_info)
    mesh.get_boundary_info().add_side(new_elem_ptr, 4, bid);
}

void
BoundaryTransitionGenerator::createUnitTet4FromHex8(ReplicatedMesh & mesh,
                                                    const dof_id_type & elem_id,
                                                    const unsigned int & side_index,
                                                    const Node * new_node,
                                                    const std::vector<boundary_id_type> & side_info)
{
  // We want to make sure that the QUAD4 is divided by the diagonal that involves the node with the
  // lowest node id This may help maintain consistency for future applications
  unsigned int lid_index = 0;
  for (const auto & i : make_range(1, 4))
  {
    if (mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(i)->id() <
        mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(lid_index)->id())
      lid_index = i;
  }

  auto new_elem_0 = std::make_unique<Tet4>();
  new_elem_0->set_node(0,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(2 - lid_index % 2, 4)));
  new_elem_0->set_node(1,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(1 - lid_index % 2, 4)));
  new_elem_0->set_node(2,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(0 - lid_index % 2, 4)));
  new_elem_0->set_node(3, const_cast<Node *>(new_node));
  new_elem_0->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base;
  auto new_elem_ptr_0 = mesh.add_elem(std::move(new_elem_0));
  retainEEID(mesh, elem_id, new_elem_ptr_0);

  auto new_elem_1 = std::make_unique<Tet4>();
  new_elem_1->set_node(0,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(3 - lid_index % 2, 4)));
  new_elem_1->set_node(1,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(2 - lid_index % 2, 4)));
  new_elem_1->set_node(2,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(0 - lid_index % 2, 4)));
  new_elem_1->set_node(3, const_cast<Node *>(new_node));
  new_elem_1->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base;
  auto new_elem_ptr_1 = mesh.add_elem(std::move(new_elem_1));
  retainEEID(mesh, elem_id, new_elem_ptr_1);

  for (const auto & bid : side_info)
  {
    mesh.get_boundary_info().add_side(new_elem_ptr_0, 0, bid);
    mesh.get_boundary_info().add_side(new_elem_ptr_1, 0, bid);
  }
}

void
BoundaryTransitionGenerator::convertPrism6Elem(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const std::vector<unsigned int> & side_indices,
    const std::vector<std::vector<boundary_id_type>> & elem_side_info)
{
  // We add a node at the centroid of the PRISM6 element
  // With this node, the PRISM6 can be converted into 3 PYRAMID5 elements and 2 TET4 elements
  // For the PYRAMID5 element, it can further be converted into 2 TET3 elements
  const Point elem_cent = mesh.elem_ptr(elem_id)->true_centroid();
  auto new_node = mesh.add_point(elem_cent);
  for (const auto & i_side : make_range(mesh.elem_ptr(elem_id)->n_sides()))
  {
    if (i_side % 4 == 0 ||
        std::find(side_indices.begin(), side_indices.end(), i_side) != side_indices.end())
      createUnitTet4FromPrism6(mesh, elem_id, i_side, new_node, elem_side_info[i_side]);
    else
      createUnitPyramid5FromPrism6(mesh, elem_id, i_side, new_node, elem_side_info[i_side]);
  }
}

void
BoundaryTransitionGenerator::createUnitTet4FromPrism6(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const unsigned int & side_index,
    const Node * new_node,
    const std::vector<boundary_id_type> & side_info)
{
  // For side 1 and side 4, they are already TRI3, so only one TET is created
  // For side 0, 2, and 3, they are QUAD4, so we create 2 TETs
  // We want to make sure that the QUAD4 is divided by the diagonal that involves
  // the node with the lowest node id This may help maintain consistency for future applications
  bool is_side_quad = (side_index % 4 != 0);
  unsigned int lid_index = 0;
  if (is_side_quad)
    for (const auto & i : make_range(1, 4))
    {
      if (mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(i)->id() <
          mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(lid_index)->id())
        lid_index = i;
    }
  // For a TRI3 side, lid_index is always 0, so the indices are always 2,1,0 here
  auto new_elem_0 = std::make_unique<Tet4>();
  new_elem_0->set_node(0,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(2 - lid_index % 2, 4)));
  new_elem_0->set_node(1,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(1 - lid_index % 2, 4)));
  new_elem_0->set_node(2,
                       mesh.elem_ptr(elem_id)
                           ->side_ptr(side_index)
                           ->node_ptr(MathUtils::euclideanMod(0 - lid_index % 2, 4)));
  new_elem_0->set_node(3, const_cast<Node *>(new_node));
  new_elem_0->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base;
  auto new_elem_ptr_0 = mesh.add_elem(std::move(new_elem_0));
  retainEEID(mesh, elem_id, new_elem_ptr_0);

  Elem * new_elem_ptr_1 = nullptr;
  if (is_side_quad)
  {
    auto new_elem_1 = std::make_unique<Tet4>();
    new_elem_1->set_node(0,
                         mesh.elem_ptr(elem_id)
                             ->side_ptr(side_index)
                             ->node_ptr(MathUtils::euclideanMod(3 - lid_index % 2, 4)));
    new_elem_1->set_node(1,
                         mesh.elem_ptr(elem_id)
                             ->side_ptr(side_index)
                             ->node_ptr(MathUtils::euclideanMod(2 - lid_index % 2, 4)));
    new_elem_1->set_node(2,
                         mesh.elem_ptr(elem_id)
                             ->side_ptr(side_index)
                             ->node_ptr(MathUtils::euclideanMod(0 - lid_index % 2, 4)));
    new_elem_1->set_node(3, const_cast<Node *>(new_node));
    new_elem_1->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base;
    new_elem_ptr_1 = mesh.add_elem(std::move(new_elem_1));
    retainEEID(mesh, elem_id, new_elem_ptr_1);
  }

  for (const auto & bid : side_info)
  {
    mesh.get_boundary_info().add_side(new_elem_ptr_0, 0, bid);
    if (new_elem_ptr_1)
      mesh.get_boundary_info().add_side(new_elem_ptr_1, 0, bid);
  }
}

void
BoundaryTransitionGenerator::createUnitPyramid5FromPrism6(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const unsigned int & side_index,
    const Node * new_node,
    const std::vector<boundary_id_type> & side_info)
{
  // Same as Hex8
  auto new_elem = std::make_unique<Pyramid5>();
  new_elem->set_node(0, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(3));
  new_elem->set_node(1, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(2));
  new_elem->set_node(2, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(1));
  new_elem->set_node(3, mesh.elem_ptr(elem_id)->side_ptr(side_index)->node_ptr(0));
  new_elem->set_node(4, const_cast<Node *>(new_node));
  new_elem->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base * 2;
  auto new_elem_ptr = mesh.add_elem(std::move(new_elem));
  retainEEID(mesh, elem_id, new_elem_ptr);
  for (const auto & bid : side_info)
    mesh.get_boundary_info().add_side(new_elem_ptr, 4, bid);
}

void
BoundaryTransitionGenerator::convertPyramid5Elem(
    ReplicatedMesh & mesh,
    const dof_id_type & elem_id,
    const std::vector<std::vector<boundary_id_type>> & elem_side_info)
{
  // A Pyramid5 element has only one QUAD4 face, so we can convert it to 2 TET4 elements
  unsigned int lid_index = 0;
  for (const auto & i : make_range(1, 4))
  {
    if (mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(i)->id() <
        mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(lid_index)->id())
      lid_index = i;
  }
  auto new_elem_0 = std::make_unique<Tet4>();
  new_elem_0->set_node(
      0,
      mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(MathUtils::euclideanMod(2 - lid_index % 2, 4)));
  new_elem_0->set_node(
      1,
      mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(MathUtils::euclideanMod(1 - lid_index % 2, 4)));
  new_elem_0->set_node(
      2,
      mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(MathUtils::euclideanMod(0 - lid_index % 2, 4)));
  new_elem_0->set_node(3, mesh.elem_ptr(elem_id)->node_ptr(4));
  new_elem_0->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base;
  auto new_elem_ptr_0 = mesh.add_elem(std::move(new_elem_0));
  retainEEID(mesh, elem_id, new_elem_ptr_0);

  auto new_elem_1 = std::make_unique<Tet4>();
  new_elem_1->set_node(
      0,
      mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(MathUtils::euclideanMod(3 - lid_index % 2, 4)));
  new_elem_1->set_node(
      1,
      mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(MathUtils::euclideanMod(2 - lid_index % 2, 4)));
  new_elem_1->set_node(
      2,
      mesh.elem_ptr(elem_id)->side_ptr(4)->node_ptr(MathUtils::euclideanMod(0 - lid_index % 2, 4)));
  new_elem_1->set_node(3, mesh.elem_ptr(elem_id)->node_ptr(4));
  new_elem_1->subdomain_id() = mesh.elem_ptr(elem_id)->subdomain_id() + _sid_shift_base;
  auto new_elem_ptr_1 = mesh.add_elem(std::move(new_elem_1));
  retainEEID(mesh, elem_id, new_elem_ptr_1);

  for (const auto & bid : elem_side_info[0])
    mesh.get_boundary_info().add_side(new_elem_ptr_0, 2 - lid_index % 2, bid);
  for (const auto & bid : elem_side_info[1])
    if (lid_index % 2)
      mesh.get_boundary_info().add_side(new_elem_ptr_1, 2, bid);
    else
      mesh.get_boundary_info().add_side(new_elem_ptr_0, 1, bid);
  for (const auto & bid : elem_side_info[2])
    mesh.get_boundary_info().add_side(new_elem_ptr_1, 2 + lid_index % 2, bid);
  for (const auto & bid : elem_side_info[3])
    if (lid_index % 2)
      mesh.get_boundary_info().add_side(new_elem_ptr_0, 1, bid);
    else
      mesh.get_boundary_info().add_side(new_elem_ptr_1, 3, bid);
  for (const auto & bid : elem_side_info[4])
  {
    mesh.get_boundary_info().add_side(new_elem_ptr_0, 0, bid);
    mesh.get_boundary_info().add_side(new_elem_ptr_1, 0, bid);
  }
}

void
BoundaryTransitionGenerator::retainEEID(ReplicatedMesh & mesh,
                                        const dof_id_type & elem_id,
                                        Elem * new_elem_ptr)
{
  const unsigned int n_eeid = mesh.n_elem_integers();
  for (const auto & i : make_range(n_eeid))
  {
    new_elem_ptr->set_extra_integer(i, mesh.elem_ptr(elem_id)->get_extra_integer(i));
  }
}