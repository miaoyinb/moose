//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GapMeshAltGenerator.h"

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

#include "libmesh/face_c0polygon.h"

registerMooseObject("MooseApp", GapMeshAltGenerator);

InputParameters
GapMeshAltGenerator::validParams()
{
  InputParameters params = PolygonMeshGeneratorBase::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The input mesh to create the gap based on.");

  params.addRequiredParam<Real>("thickness", "The thickness of the gap to be created.");

  params.addParam<Real>("max_elem_size", "The maximum element size for the generated gap mesh.");

  params.addClassDescription("Create a gap mesh for an 2D XY input mesh.");

  return params;
}

GapMeshAltGenerator::GapMeshAltGenerator(const InputParameters & parameters)
  : PolygonMeshGeneratorBase(parameters),
    _input(getMesh("input")),
    _thickness(getParam<Real>("thickness"))
{
}

std::unique_ptr<MeshBase>
GapMeshAltGenerator::generate()
{
  // Put the boundary mesh in a local pointer
  std::unique_ptr<UnstructuredMesh> mesh =
      dynamic_pointer_cast<UnstructuredMesh>(std::move(_input));

  // MeshedHole is a good tool to extract and sort boundary points
  TriangulatorInterface::MeshedHole bdry_mh(*mesh);

  // Reduce the point list to only contain vertices
  std::vector<Point> reduced_pts_list;
  for (const auto i : make_range(bdry_mh.n_points()))
  {
    if (!isPointsColinear(bdry_mh.point((i - 1 + bdry_mh.n_points()) % bdry_mh.n_points()),
                          bdry_mh.point(i),
                          bdry_mh.point((i + 1) % bdry_mh.n_points())))
      reduced_pts_list.push_back(bdry_mh.point(i));
  }

  // With the vertices, we need to figure out ther normals of each side
  // The normal needs to point outward
  // To make this step easier, we define the polygon as a single element
  // Then the element attribute can be used to get side normals
  auto ply_mesh = buildMeshBaseObject();
  for (const auto & i_pt : make_range(reduced_pts_list.size()))
  {
    auto rpt = reduced_pts_list[i_pt];
    ply_mesh->add_point(rpt, i_pt);
  }
  std::unique_ptr<Elem> polygon = std::make_unique<C0Polygon>(reduced_pts_list.size());
  for (const auto & i : make_range(reduced_pts_list.size()))
    polygon->set_node(i, ply_mesh->node_ptr(i));
  polygon->set_id() = 0;
  ply_mesh->add_elem(std::move(polygon));

  // For each vertex, the shifting direction to form the gap is defined by the normal vectors of the
  // two sides that contain the vertex
  std::map<dof_id_type, std::vector<Point>> node_normal_map;
  for (const auto & i_side : make_range(ply_mesh->elem_ptr(0)->n_sides()))
  {
    const auto & side = ply_mesh->elem_ptr(0)->side_ptr(i_side);
    const Point & side_normal = ply_mesh->elem_ptr(0)->side_vertex_average_normal(i_side);
    // If the map already contains the node key, this is the second side of that vertex we encounter
    // Otherwise, this is the first side of that vertex we encounter so a new entry is created
    if (node_normal_map.count(side->node_ptr(0)->id()))
      node_normal_map[side->node_ptr(0)->id()].push_back(side_normal);
    else
      node_normal_map[side->node_ptr(0)->id()] = {side_normal};
    // We need to do it for both nodes of the side
    if (node_normal_map.count(side->node_ptr(1)->id()))
      node_normal_map[side->node_ptr(1)->id()].push_back(side_normal);
    else
      node_normal_map[side->node_ptr(1)->id()] = {side_normal};
  }

  std::vector<Point> mod_reduced_pts_list(reduced_pts_list);
  for (const auto & [node_id, normal_vecs] : node_normal_map)
  {
    mooseAssert(normal_vecs.size() == 2,
                "Each vertex should be connected to exactly two sides in a polygon.");

    const Point original_pt = *(ply_mesh->node_ptr(node_id));
    const Point move_dir = (normal_vecs.front() + normal_vecs.back()).unit();
    const Real mov_dist =
        _thickness /
        std::sqrt((1.0 + (normal_vecs.front() * normal_vecs.back()) /
                             (normal_vecs.front().norm() * normal_vecs.back().norm())) /
                  2.0);
    // Since we defined the node ids explicitly based on the indices of `reduced_pts_list`
    mod_reduced_pts_list[node_id] = original_pt + move_dir * mov_dist;
  }

  // To ensure no overlapping, we need to check set of four points
  // p1 and p2 should be the pair of points before and after shifting
  // p3 and p4 should be the pair of points after shifting a side
  for (const auto & i_node_1 : make_range(mod_reduced_pts_list.size()))
  {
    const Point & p1 = reduced_pts_list[i_node_1];
    const Point & p2 = mod_reduced_pts_list[i_node_1];
    for (const auto & i_node_2 : make_range(mod_reduced_pts_list.size()))
    {
      if (i_node_2 == i_node_1 || (i_node_2 + 1) % mod_reduced_pts_list.size() == i_node_1)
        continue;
      const Point & p3 = mod_reduced_pts_list[i_node_2];
      const Point & p4 = mod_reduced_pts_list[(i_node_2 + 1) % mod_reduced_pts_list.size()];
      if (fourPointOverlap(p1, p2, p3, p4))
        paramError("thickness",
                   "The specified thickness creates overlapping lines in the gap mesh. "
                   "Please reduce the thickness value.");
    }
  }

  auto ply_mesh_2 = buildMeshBaseObject();

  if (isParamValid("max_elem_size"))
    MooseMeshUtils::buildPolyLineMesh(
        *ply_mesh_2, mod_reduced_pts_list, true, "dummy", "dummy", getParam<Real>("max_elem_size"));
  else
    MooseMeshUtils::buildPolyLineMesh(
        *ply_mesh_2, mod_reduced_pts_list, true, "dummy", "dummy", {1});

  return ply_mesh_2;
}

bool
GapMeshAltGenerator::isPointsColinear(const Point & p1, const Point & p2, const Point & p3) const
{
  const Point v1 = p2 - p1;
  const Point v2 = p3 - p1;
  const Point cross_prod = v1.cross(v2);

  return MooseUtils::absoluteFuzzyEqual(cross_prod.norm(), 0.0);
}

bool
GapMeshAltGenerator::fourPointOverlap(const Point & p1,
                                      const Point & p2,
                                      const Point & p3,
                                      const Point & p4) const
{
  const Real a1 = p2(1) - p1(1);
  const Real b1 = p1(0) - p2(0);
  const Real c1 = p2(0) * p1(1) - p1(0) * p2(1);

  const Real a2 = p4(1) - p3(1);
  const Real b2 = p3(0) - p4(0);
  const Real c2 = p4(0) * p3(1) - p3(0) * p4(1);

  const Real denom = a1 * b2 - a2 * b1;
  // We should not worry about the parallel case here
  // If there is an overlap issue, it will be captured by other line segments
  if (MooseUtils::absoluteFuzzyEqual(denom, 0.0))
    return false;

  const Point intersection_pt =
      Point((b1 * c2 - b2 * c1) / denom, (a2 * c1 - a1 * c2) / denom, 0.0);

  const Real ratio_p1p2 = (intersection_pt - p1) * (p2 - p1) / ((p2 - p1).norm_sq());
  const Real ratio_p3p4 = (intersection_pt - p3) * (p4 - p3) / ((p4 - p3).norm_sq());

  // TODO: consider fuzzy here
  if (ratio_p1p2 > 0.0 && ratio_p1p2 < 1.0 && ratio_p3p4 > 0.0 && ratio_p3p4 < 1.0)
    return true;
  else
    return false;
}