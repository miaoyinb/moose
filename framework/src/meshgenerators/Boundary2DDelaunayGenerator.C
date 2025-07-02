//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "Boundary2DDelaunayGenerator.h"

#include "MooseMeshUtils.h"
#include "CastUniquePointer.h"

#include "libmesh/boundary_info.h"
#include "libmesh/poly2tri_triangulator.h"

registerMooseObject("MooseApp", Boundary2DDelaunayGenerator);

InputParameters
Boundary2DDelaunayGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addClassDescription("Mesh generator which removes side sets");
  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addRequiredParam<std::vector<BoundaryName>>("boundary_names", "The boundaries to be used");
  // XYDelaunay parameters
  params.addParam<bool>("use_auto_area_func",
                        false,
                        "Use the automatic area function for the triangle meshing region.");
  params.addParam<Real>(
      "auto_area_func_default_size",
      0,
      "Background size for automatic area function, or 0 to use non background size");
  params.addParam<Real>("auto_area_func_default_size_dist",
                        -1.0,
                        "Effective distance of background size for automatic area "
                        "function, or negative to use non background size");
  params.addParam<unsigned int>("auto_area_function_num_points",
                                10,
                                "Maximum number of nearest points used for the inverse distance "
                                "interpolation algorithm for automatic area function calculation.");
  params.addRangeCheckedParam<Real>(
      "auto_area_function_power",
      1.0,
      "auto_area_function_power>0",
      "Polynomial power of the inverse distance interpolation algorithm for automatic area "
      "function calculation.");

  return params;
}

Boundary2DDelaunayGenerator::Boundary2DDelaunayGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _boundary_names(getParam<std::vector<BoundaryName>>("boundary_names")),
    _use_auto_area_func(getParam<bool>("use_auto_area_func")),
    _auto_area_func_default_size(getParam<Real>("auto_area_func_default_size")),
    _auto_area_func_default_size_dist(getParam<Real>("auto_area_func_default_size_dist")),
    _auto_area_function_num_points(getParam<unsigned int>("auto_area_function_num_points")),
    _auto_area_function_power(getParam<Real>("auto_area_function_power"))
{
}

std::unique_ptr<MeshBase>
Boundary2DDelaunayGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);

  // Generate a new block id if one isn't supplied.
  SubdomainID new_block_id = MooseMeshUtils::getNextFreeSubdomainID(*mesh);
  try
  {
    MooseMeshUtils::createSubdomainFromSidesets(
        mesh, _boundary_names, new_block_id, SubdomainName(), type());
  }
  catch (MooseException & e)
  {
    paramError("sidesets", e.what());
  }

  return mesh;
}

Point
Boundary2DDelaunayGenerator::elemNormal(const Elem & elem)
{
  const Point & p0 = *elem.node_ptr(0);
  const Point & p1 = *elem.node_ptr(1);
  const Point & p2 = *elem.node_ptr(2);

  return ((p2 - p1).cross(p0 - p1)).unit();
}
