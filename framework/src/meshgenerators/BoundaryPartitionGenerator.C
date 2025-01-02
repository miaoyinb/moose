//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BoundaryPartitionGenerator.h"

#include "MooseMeshUtils.h"
#include "CastUniquePointer.h"

#include "libmesh/boundary_info.h"

registerMooseObject("MooseApp", BoundaryPartitionGenerator);

InputParameters
BoundaryPartitionGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addClassDescription("Mesh generator which removes side sets");
  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addRequiredParam<std::vector<BoundaryName>>("boundary_names",
                                                     "The boundaries to be partitioned");
  params.addParam<bool>("further_partition_separate_boundaries",
                        false,
                        "Whether to further partition the separate boundaries");

  return params;
}

BoundaryPartitionGenerator::BoundaryPartitionGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _boundary_names(getParam<std::vector<BoundaryName>>("boundary_names")),
    _further_partition_separate_boundaries(getParam<bool>("further_partition_separate_boundaries"))
{
}

std::unique_ptr<MeshBase>
BoundaryPartitionGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);

  std::vector<boundary_id_type> boundary_ids;
  for (const auto & name : _boundary_names)
  {
    auto bid = MooseMeshUtils::getBoundaryID(name, *mesh);
    if (bid == BoundaryInfo::invalid_id)
      paramError("boundary_names", "The boundary '", name, "' was not found in the mesh");

    boundary_ids.push_back(bid);
  }

  auto bc_tuples = mesh->get_boundary_info().build_side_list();

  std::set<std::pair<dof_id_type, unsigned short int>> selected_bc_info;

  for (auto bc_tuple : bc_tuples)
  {
    auto & el = std::get<0>(bc_tuple);
    auto & sl = std::get<1>(bc_tuple);
    auto & bl = std::get<2>(bc_tuple);

    // Check if the bid is in the list of boundary_ids
    if (std::find(boundary_ids.begin(), boundary_ids.end(), bl) != boundary_ids.end())
      selected_bc_info.emplace(std::make_pair(el, sl));
  }

  // build boundary name list
  std::vector<BoundaryName> new_boundary_names;
  for (unsigned int i = 0; i < 32; i++)
  {
    new_boundary_names.push_back(_name + "_boundary_" + std::to_string(i));
  }
  auto new_boundary_ids = MooseMeshUtils::getBoundaryIDs(*mesh, new_boundary_names, true);
  for (unsigned int i = 0; i < new_boundary_ids.size(); i++)
  {
    mesh->get_boundary_info().sideset_name(new_boundary_ids[i]) = new_boundary_names[i];
  }

  for (const auto & side : selected_bc_info)
  {
    auto & el = side.first;
    auto & sl = side.second;

    const Point & side_pt_0 = *mesh->elem_ptr(el)->side_ptr(sl)->node_ptr(0);
    const Point & side_pt_1 = *mesh->elem_ptr(el)->side_ptr(sl)->node_ptr(1);
    const Point & side_pt_2 = *mesh->elem_ptr(el)->side_ptr(sl)->node_ptr(2);

    const Point side_normal = (side_pt_1 - side_pt_0).cross(side_pt_2 - side_pt_1).unit();

    const auto side_index = getClosestDirection(side_normal);

    mesh->get_boundary_info().add_side(el, sl, new_boundary_ids[side_index]);
  }

  if (_further_partition_separate_boundaries)
  {
    // record the current available boundary id
    auto max_boundary_id = MooseMeshUtils::getNextFreeBoundaryID(*mesh);
    // record all the sides info
    const auto sides_info = mesh->get_boundary_info().build_side_list();
    // select the sidesets of interest
    std::vector<std::vector<std::pair<dof_id_type, unsigned short int>>> selected_bc_info;
    selected_bc_info.resize(new_boundary_ids.size());
    for (const auto & bc_tuple : sides_info)
    {
      const auto & el = std::get<0>(bc_tuple);
      const auto & sl = std::get<1>(bc_tuple);
      const auto & bl = std::get<2>(bc_tuple);
      const auto it = std::find(new_boundary_ids.begin(), new_boundary_ids.end(), bl);
      if (it != new_boundary_ids.end())
      {
        const auto index = std::distance(new_boundary_ids.begin(), it);
        selected_bc_info[index].push_back(std::make_pair(el, sl));
      }
    }

    // To facilitate partitioning, we will make lower dimensional blocks based on the selected sides
    dof_id_type max_elem_id = mesh->max_elem_id();
    unique_id_type max_unique_id = mesh->parallel_max_unique_id();
    const auto new_block_id = MooseMeshUtils::getNextFreeSubdomainID(*mesh);
    unsigned int nelem_ct = 0;
    for (unsigned int i = 0; i < selected_bc_info.size(); i++)
    {
      for (auto & [eid, sid] : selected_bc_info[i])
      {
        Elem * elem = mesh->elem_ptr(eid);

        const auto side = sid;

        // Build a non-proxy element from this side.
        std::unique_ptr<Elem> side_elem(elem->build_side_ptr(side, /*proxy=*/false));

        // The side will be added with the same processor id as the parent.
        side_elem->processor_id() = elem->processor_id();

        // Add subdomain ID, TRI and QUAD will need to have different ids
        side_elem->subdomain_id() = new_block_id + (side_elem->n_vertices() - 3) + i * 2;

        // Also assign the side's interior parent, so it is always
        // easy to figure out the Elem we came from.
        side_elem->set_interior_parent(elem);

        // Add id
        nelem_ct++;
        side_elem->set_id(max_elem_id + nelem_ct);
        side_elem->set_unique_id(max_unique_id + nelem_ct);

        // Finally, add the lower-dimensional element to the Mesh.
        mesh->add_elem(side_elem.release());
      }
    }
  }

  mesh->set_isnt_prepared();
  return dynamic_pointer_cast<MeshBase>(mesh);
}

unsigned short
BoundaryPartitionGenerator::getClosestDirection(const Point & pt)
{
  const std::vector<Point> norm_vectors = {
      Point(0.000000, -1.000000, 0.000000),   Point(0.723600, -0.447215, 0.525720),
      Point(-0.276385, -0.447215, 0.850640),  Point(-0.894425, -0.447215, 0.000000),
      Point(-0.276385, -0.447215, -0.850640), Point(0.723600, -0.447215, -0.525720),
      Point(0.276385, 0.447215, 0.850640),    Point(-0.723600, 0.447215, 0.525720),
      Point(-0.723600, 0.447215, -0.525720),  Point(0.276385, 0.447215, -0.850640),
      Point(0.894425, 0.447215, 0.000000),    Point(0.000000, 1.000000, 0.000000),
      Point(0.1876, -0.7947, 0.5774),         Point(0.6071, -0.7947, 0.0000),
      Point(-0.4911, -0.7947, 0.3568),        Point(-0.4911, -0.7947, -0.3568),
      Point(0.1876, -0.7947, -0.5774),        Point(0.9822, -0.1876, 0.0000),
      Point(0.3035, -0.1876, 0.9342),         Point(-0.7946, -0.1876, 0.5774),
      Point(-0.7946, -0.1876, -0.5774),       Point(0.3035, -0.1876, -0.9342),
      Point(0.7946, 0.1876, 0.5774),          Point(-0.3035, 0.1876, 0.9342),
      Point(-0.9822, 0.1876, 0.0000),         Point(-0.3035, 0.1876, -0.9342),
      Point(0.7946, 0.1876, -0.5774),         Point(0.4911, 0.7947, 0.3568),
      Point(-0.1876, 0.7947, 0.5774),         Point(-0.6071, 0.7947, 0.0000),
      Point(-0.1876, 0.7947, -0.5774),        Point(0.4911, 0.7947, -0.3568)};
  std::vector<Real> dot_products(32);
  for (unsigned short i = 0; i < norm_vectors.size(); i++)
  {
    dot_products[i] = pt * norm_vectors[i];
  }
  // return the index of the maximum dot product
  return std::distance(dot_products.begin(),
                       std::max_element(dot_products.begin(), dot_products.end()));
}