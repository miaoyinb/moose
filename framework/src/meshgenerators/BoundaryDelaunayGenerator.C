//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BoundaryDelaunayGenerator.h"

#include "MooseMeshUtils.h"
#include "CastUniquePointer.h"

#include "libmesh/boundary_info.h"
#include "libmesh/poly2tri_triangulator.h"

registerMooseObject("MooseApp", BoundaryDelaunayGenerator);

InputParameters
BoundaryDelaunayGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addClassDescription("Mesh generator which removes side sets");
  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addRequiredParam<std::vector<BoundaryName>>("boundary_names", "The boundaries to be used");

  return params;
}

BoundaryDelaunayGenerator::BoundaryDelaunayGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getParam<MeshGeneratorName>("input")),
    _boundary_names(getParam<std::vector<BoundaryName>>("boundary_names"))
{
  declareMeshForSub("input");

  {
    auto params = _app.getFactory().getValidParams("LowerDBlockFromSidesetGenerator");
    params.set<MeshGeneratorName>("input") = _input;
    params.set<SubdomainID>("new_block_id") = 100;
    params.set<std::vector<BoundaryName>>("sidesets") = _boundary_names;

    addMeshSubgenerator("LowerDBlockFromSidesetGenerator", _name + "_2d_block", params);
  }

  {
    auto params = _app.getFactory().getValidParams("BlockToMeshConverterGenerator");
    params.set<MeshGeneratorName>("input") = _name + "_2d_block";
    params.set<std::vector<SubdomainName>>("target_blocks") = {(SubdomainName)("100")};

    addMeshSubgenerator("BlockToMeshConverterGenerator", _name + "_2d_mesh", params);
  }

  {
    auto params = _app.getFactory().getValidParams("SideSetsAroundSubdomainGenerator");
    params.set<MeshGeneratorName>("input") = _name + "_2d_mesh";
    params.set<std::vector<BoundaryName>>("new_boundary") = {
        (BoundaryName)(_name + "_2d_mesh_ext")};
    params.set<std::vector<SubdomainName>>("block") = {(SubdomainName)("0")};

    addMeshSubgenerator("SideSetsAroundSubdomainGenerator", _name + "_2d_mesh_ext", params);
  }

  _2d_mesh = &getMeshByName(_name + "_2d_mesh_ext");
}

std::unique_ptr<MeshBase>
BoundaryDelaunayGenerator::generate()
{
  auto & mesh_in = *_2d_mesh;

  mesh_in->prepare_for_use();
  // Centroid
  const Point centroid = MooseMeshUtils::meshCentroidCalculator(*mesh_in);
  // std::cout << "Centroid " << centroid << std::endl;

  Point mesh_norm = Point(0.0, 0.0, 0.0);
  Real mesh_area = 0.0;
  // Check all the elements' normal vectors
  for (const auto & elem :
       as_range(mesh_in->active_local_elements_begin(), mesh_in->active_local_elements_end()))
  {
    const Real elem_area = elem->volume();
    mesh_norm += elemNormal(*elem) * elem_area;
    mesh_area += elem_area;
  }
  mesh_norm /= mesh_area;
  mesh_norm = mesh_norm.unit();
  // std::cout << "Mesh normal " << mesh_norm << std::endl;

  // Move the mesh to the centroid
  MeshTools::Modification::translate(*mesh_in, -centroid(0), -centroid(1), -centroid(2));
  const Real theta = std::acos(mesh_norm(2)) / M_PI * 180.0;
  const Real phi =
      (MooseUtils::absoluteFuzzyLessThan(mesh_norm(2), 1.0) ? std::atan2(mesh_norm(1), mesh_norm(0))
                                                            : 0.0) /
      M_PI * 180.0;
  // TO figure out euler angles based on the normal vector
  MeshTools::Modification::rotate(*mesh_in, 90.0 - phi, theta, 0.0);

  // // Store all the nodes z(x,y)
  // std::vector<Point> mod_pts;
  // std::vector<Real> mod_vals;
  // for (const auto & node : mesh_in->node_ptr_range())
  // {
  //   mod_pts.push_back(Point((*node)(0), (*node)(1), 0.0));
  //   mod_vals.push_back((*node)(2));
  // }
  // // Using a inverse power interpolation based on initial nodes seems imperfect
  // // This is mainly due to the non-uniform distribution of the nodes
  // // Thinking about identify the element that not node belonging to and use the four vertices to
  // // interpolate the z(x,y)
  // auto z_xy_func = std::make_unique<InverseDistanceInterpolation<3>>(
  //     this->comm(), std::min(32, (int)mesh_in->n_nodes()), 3, 0, -1);
  // std::vector<std::string> field_vars{"f"};
  // z_xy_func->set_field_variables(field_vars);
  // z_xy_func->get_source_points() = mod_pts;
  // z_xy_func->get_source_vals() = mod_vals;
  // z_xy_func->prepare_for_use();

  // Clone the mesh for future use
  auto mesh_in_xyz = dynamic_pointer_cast<MeshBase>(mesh_in->clone());

  // "project" the mesh to the xy-plane
  for (const auto & node : mesh_in->node_ptr_range())
    (*node)(2) = 0;

  auto mesh_in_xy = dynamic_pointer_cast<MeshBase>(mesh_in->clone());

  std::unique_ptr<UnstructuredMesh> mesh =
      dynamic_pointer_cast<UnstructuredMesh>(std::move(mesh_in));

  Poly2TriTriangulator poly2tri(*mesh);
  poly2tri.triangulation_type() = TriangulatorInterface::PSLG;
  // poly2tri.set_outer_boundary_ids(
  //     {static_cast<unsigned long>(MooseMeshUtils::getBoundaryID(_name + "_2d_mesh_ext",
  //     *mesh))});
  poly2tri.set_interpolate_boundary_points(0);
  poly2tri.set_refine_boundary_allowed(false);
  poly2tri.set_verify_hole_boundaries(false);
  poly2tri.desired_area() = 0;
  poly2tri.minimum_angle() = 0; // Not yet supported
  poly2tri.smooth_after_generating() = false;
  poly2tri.set_auto_area_function(this->comm(), 8, 2, 0.0, -1.0);
  poly2tri.triangulate();

  // // Move the nodes back to the original z(x,y)
  // for (const auto & node : mesh->node_ptr_range())
  // {
  //   std::vector<Point> target_pts;
  //   std::vector<Number> target_vals;
  //   target_pts.push_back((*node));
  //   target_vals.resize(1);
  //   z_xy_func->interpolate_field_data(z_xy_func->field_variables(), target_pts, target_vals);
  //   (*node)(2) = target_vals[0];
  // }

  for (const auto & node : mesh->node_ptr_range())
  {
    bool node_mod = false;
    // Try to find the element in mesh_in_xy that contains the new node
    for (const auto & elem : as_range(mesh_in_xy->active_local_elements_begin(),
                                      mesh_in_xy->active_local_elements_end()))
    {
      if (elem->contains_point(Point((*node)(0), (*node)(1), 0.0)))
      {
        // Element id
        const auto elem_id = elem->id();
        // element in xyz_in_xyz
        const Elem & elem_xyz = *mesh_in_xyz->elem_ptr(elem_id);

        const Point elem_normal = elemNormal(elem_xyz);
        const Point & elem_p = *mesh_in_xyz->elem_ptr(elem_id)->node_ptr(0);

        (*node)(2) = elem_p(2) - (((*node)(0) - elem_p(0)) * elem_normal(0) +
                                  ((*node)(1) - elem_p(1)) * elem_normal(1)) /
                                     elem_normal(2);
        node_mod = true;
        break;
      }
    }
    if (!node_mod)
      mooseError("Node not found in mesh_in_xy");
  }

  // TO figure out euler angles based on the normal vector
  MeshTools::Modification::rotate(*mesh, 0.0, -theta, phi - 90.0);
  // Move the mesh to the centroid
  MeshTools::Modification::translate(*mesh, centroid(0), centroid(1), centroid(2));

  return std::move(mesh);
}

Point
BoundaryDelaunayGenerator::elemNormal(const Elem & elem)
{
  const Point & p0 = *elem.node_ptr(0);
  const Point & p1 = *elem.node_ptr(1);
  const Point & p2 = *elem.node_ptr(2);

  return ((p2 - p1).cross(p0 - p1)).unit();
}