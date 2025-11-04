//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GapMeshGenerator.h"

#include "CastUniquePointer.h"
#include "MooseMeshUtils.h"
#include "MooseUtils.h"

#include "libmesh/elem.h"
#include "libmesh/enum_to_string.h"
#include "libmesh/int_range.h"
#include "libmesh/mesh_modification.h"
#include "libmesh/mesh_serializer.h"
#include "libmesh/mesh_triangle_holes.h"
#include "libmesh/parsed_function.h"
#include "libmesh/poly2tri_triangulator.h"
#include "libmesh/unstructured_mesh.h"
#include "DelimitedFileReader.h"

registerMooseObject("MooseApp", GapMeshGenerator);

InputParameters
GapMeshGenerator::validParams()
{
  InputParameters params = PolygonMeshGeneratorBase::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The input mesh to create the gap based on.");

  params.addRequiredParam<Real>("thickness", "The thickness of the gap to be created.");

  params.addClassDescription("Create a gap mesh for an 2D XY input mesh.");

  return params;
}

GapMeshGenerator::GapMeshGenerator(const InputParameters & parameters)
  : PolygonMeshGeneratorBase(parameters),
    _input(getMesh("input")),
    _thickness(getParam<Real>("thickness"))
{
}

std::unique_ptr<MeshBase>
GapMeshGenerator::generate()
{
  // Put the boundary mesh in a local pointer
  std::unique_ptr<UnstructuredMesh> mesh =
      dynamic_pointer_cast<UnstructuredMesh>(std::move(_input));

  TriangulatorInterface::MeshedHole bdry_mh(*mesh);

  std::vector<Point> reduced_pts_list;

  for (const auto i : make_range(bdry_mh.n_points()))
  {
    if (!isPointsColinear(bdry_mh.point((i - 1 + bdry_mh.n_points()) % bdry_mh.n_points()),
                          bdry_mh.point(i),
                          bdry_mh.point((i + 1) % bdry_mh.n_points())))
      reduced_pts_list.push_back(bdry_mh.point(i));
  }

  auto ply_mesh = buildMeshBaseObject();

  MooseMeshUtils::buildPolyLineMesh(*ply_mesh, reduced_pts_list, true, "dummy", "dummy", 1);

  std::unique_ptr<UnstructuredMesh> ply_mesh_u =
      dynamic_pointer_cast<UnstructuredMesh>(std::move(ply_mesh));

  // Generate a very simple triangulation mesh so that we can get the outward normal vectors
  libMesh::Poly2TriTriangulator poly2tri(*ply_mesh_u);
  poly2tri.triangulation_type() = libMesh::TriangulatorInterface::PSLG;

  poly2tri.set_interpolate_boundary_points(0);
  poly2tri.set_refine_boundary_allowed(false);
  poly2tri.set_verify_hole_boundaries(false);
  poly2tri.desired_area() = 0;
  poly2tri.minimum_angle() = 0; // Not yet supported
  poly2tri.smooth_after_generating() = false;
  poly2tri.triangulate();

  // The mesh now only contain one side set that corresponds to the outer boundary with an ID of 0
  auto bdry_list(ply_mesh_u->get_boundary_info().build_side_list());

  std::map<dof_id_type, std::vector<Point>> node_normal_map;

  for (const auto & bside : bdry_list)
  {
    const auto & side = ply_mesh_u->elem_ptr(std::get<0>(bside))->side_ptr(std::get<1>(bside));
    const Point side_normal =
        ply_mesh_u->elem_ptr(std::get<0>(bside))->side_vertex_average_normal(std::get<1>(bside));
    if (node_normal_map.count(side->node_ptr(0)->id()))
      node_normal_map[side->node_ptr(0)->id()].push_back(side_normal);
    else
      node_normal_map[side->node_ptr(0)->id()] = {side_normal};
    if (node_normal_map.count(side->node_ptr(1)->id()))
      node_normal_map[side->node_ptr(1)->id()].push_back(side_normal);
    else
      node_normal_map[side->node_ptr(1)->id()] = {side_normal};
  }

  for (const auto & [node_id, normal_vecs] : node_normal_map)
  {
    const Point original_pt = *(ply_mesh_u->node_ptr(node_id));
    const Point move_dir = (normal_vecs.front() + normal_vecs.back()).unit();
    const Real mov_dist =
        _thickness /
        std::sqrt((1.0 + (normal_vecs.front() * normal_vecs.back()) /
                             (normal_vecs.front().norm() * normal_vecs.back().norm())) /
                  2.0);
    // Do we need fuzzy here?
    for (auto & rpt: reduced_pts_list)
      if (rpt == original_pt)
        rpt = original_pt + move_dir * mov_dist;
  }

  auto ply_mesh_2 = buildMeshBaseObject();

  MooseMeshUtils::buildPolyLineMesh(*ply_mesh_2, reduced_pts_list, true, "dummy", "dummy", 1);

  return ply_mesh_2;
}

bool
GapMeshGenerator::isPointsColinear(const Point & p1, const Point & p2, const Point & p3) const
{
  const Point v1 = p2 - p1;
  const Point v2 = p3 - p1;
  const Point cross_prod = v1.cross(v2);

  return MooseUtils::absoluteFuzzyEqual(cross_prod.norm(), 0.0);
}