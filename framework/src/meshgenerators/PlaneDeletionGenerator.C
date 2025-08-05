//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "PlaneDeletionGenerator.h"

#include "libmesh/type_vector.h"
#include "libmesh/point.h"
#include "libmesh/elem.h"

registerMooseObject("MooseApp", PlaneDeletionGenerator);

InputParameters
PlaneDeletionGenerator::validParams()
{
  InputParameters params = ElementDeletionGeneratorBase::validParams();

  params.addClassDescription(
      "Removes elements lying 'above' the plane (in the direction of the normal).");

  params.addRequiredParam<Point>("point", "The point that defines the plane");
  params.addRequiredParam<RealVectorValue>("normal", "The normal that defines the plane");

  MooseEnum delete_criterion("CENTROID VERTEX_AVG ONE_VERTEX ALL_VERTICES", "VERTEX_AVG");
  params.addParam<MooseEnum>("deletion_criterion",
                             delete_criterion,
                             "The criterion for deleting elements. "
                             "CENTROID: delete if the centroid is above the plane; "
                             "ONE_VERTEX: delete if at least one vertex is above the plane; "
                             "ALL_VERTICES: delete if all vertices are above the plane.");
  return params;
}

PlaneDeletionGenerator::PlaneDeletionGenerator(const InputParameters & parameters)
  : ElementDeletionGeneratorBase(parameters),
    _deletion_criterion(
        getParam<MooseEnum>("deletion_criterion").template getEnum<DeletionCriterion>()),
    _point(getParam<Point>("point")),
    _normal(getParam<RealVectorValue>("normal"))

{
  if (!_normal.norm())
    paramError("normal", "Normal vector must have a size!");

  // Make sure it's a unit vector
  _normal /= _normal.norm();
}

bool
PlaneDeletionGenerator::shouldDelete(const Elem * elem)
{
  std::vector<Point> vecs_from_plane_point;

  switch (_deletion_criterion)
  {
    case DeletionCriterion::CENTROID:
      vecs_from_plane_point.push_back(elem->true_centroid() - _point);
      break;
    case DeletionCriterion::VERTEX_AVG:
      vecs_from_plane_point.push_back(elem->vertex_average() - _point);
      break;
    case DeletionCriterion::ONE_VERTEX:
    case DeletionCriterion::ALL_VERTICES:
      for (const auto & i_vertex : make_range(elem->n_vertices()))
        vecs_from_plane_point.push_back(*elem->node_ptr(i_vertex) - _point);
      break;
    default:
      mooseAssert(false, "Unknown deletion criterion");
  }

  if (vecs_from_plane_point.size() == 1)
  {
    auto norm = vecs_from_plane_point.front().norm();

    // If we _perfectly_ hit a centroid... default to deleting the element
    if (!norm)
      return true;

    // Unitize it
    vecs_from_plane_point.front() /= norm;

    return vecs_from_plane_point.front() * _normal > 0;
  }
  else
  {
    std::vector<Real> dot_products(vecs_from_plane_point.size());
    bool should_delete = _deletion_criterion == DeletionCriterion::ONE_VERTEX ? false : true;
    for (const auto & i_vec : make_range(vecs_from_plane_point.size()))
    {
      auto norm = vecs_from_plane_point[i_vec].norm();
      if (!norm)
        dot_products[i_vec] = 0.0;
      else
      {
        vecs_from_plane_point[i_vec] /= norm;
        dot_products[i_vec] = vecs_from_plane_point[i_vec] * _normal;
      }

      if (_deletion_criterion == DeletionCriterion::ONE_VERTEX)
      {
        if (MooseUtils::absoluteFuzzyGreaterThan(dot_products[i_vec], 0))
          return true; // At least one vertex is above the plane
      }
      else if (MooseUtils::absoluteFuzzyLessThan(dot_products[i_vec], 0))
        return false; // At least one vertex is below the plane
    }
    return should_delete;
  }
}
