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

  return params;
}

BoundaryPartitionGenerator::BoundaryPartitionGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _boundary_names(getParam<std::vector<BoundaryName>>("boundary_names"))
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
  for (unsigned short i = 0; i < 32; i++)
  {
    dot_products[i] = pt * norm_vectors[i];
  }
  // return the index of the maximum dot product
  return std::distance(dot_products.begin(),
                       std::max_element(dot_products.begin(), dot_products.end()));
}