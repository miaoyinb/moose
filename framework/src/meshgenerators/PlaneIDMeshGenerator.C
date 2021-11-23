//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "PlaneIDMeshGenerator.h"
#include "CastUniquePointer.h"

#include "libmesh/elem.h"

registerMooseObject("MooseApp", PlaneIDMeshGenerator);

InputParameters
PlaneIDMeshGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addRequiredParam<std::vector<Real>>("plane_coordinates", "Coordinates of axial planes");
  params.addParam<std::vector<unsigned int>>("num_ids_per_plane", "Number of unique ids per plane");
  MooseEnum plane_axis("x y z", "z");
  params.addParam<MooseEnum>("plane_axis", plane_axis, "Axis of plane");
  params.addRequiredParam<std::string>("id_name", "Name of Integer ID set");
  params.addParam<Real>("tolerance", 1.0E-4, "Tolerance for plane coordinate check");
  params.addClassDescription(
      "This PlaneIDMeshGenerator source code is to assigns plane extra ID for existing 3D meshes");

  return params;
}

PlaneIDMeshGenerator::PlaneIDMeshGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _axis_index(getParam<MooseEnum>("plane_axis")),
    _element_id_name(getParam<std::string>("id_name"))
{
  if (!isParamValid("num_ids_per_plane"))
    _planes = getParam<std::vector<Real>>("plane_coordinates");
  else
  {
    _planes.clear();
    std::vector<Real> base_planes = getParam<std::vector<Real>>("plane_coordinates");
    std::vector<unsigned int> sublayers = getParam<std::vector<unsigned int>>("num_ids_per_plane");
    if (base_planes.size() != sublayers.size() + 1)
      paramError("plane_coordinates", "Sizes of 'z_planes' and '_sublayers' disagree");
    _planes.push_back(base_planes[0]);
    for (unsigned int i = 0; i < sublayers.size(); ++i)
    {
      Real layer_size = (base_planes[i + 1] - base_planes[i]) / (Real)sublayers[i];
      for (unsigned int j = 0; j < sublayers[i]; ++j)
        _planes.push_back(_planes.back() + layer_size);
    }
  }
}

std::unique_ptr<MeshBase>
PlaneIDMeshGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);
  if (mesh->mesh_dimension() < _axis_index + 1)
    paramError("plane_axis", "PlaneIDMeshGenerator must operate on a proper layer axis");
  unsigned int eid = 0;
  if (!mesh->has_elem_integer(_element_id_name))
    eid = mesh->add_elem_integer(_element_id_name);
  else
    eid = mesh->get_elem_integer_index(_element_id_name);

  Real tol = getParam<Real>("tolerance");
  for (auto & elem : mesh->active_element_ptr_range())
  {
    int layer_id = getPlaneID(elem->centroid());
    if (layer_id == -1 || layer_id == (int)_planes.size())
      mooseError("The axial layers do not cover element at ", elem->centroid());
    for (unsigned int i = 0; i < elem->n_nodes(); ++i)
    {
      const Point & p = elem->point(i) - (elem->point(i) - elem->centroid()) * tol;
      if (getPlaneID(p) != layer_id)
        mooseError("Element at ", elem->centroid(), " is cut by the axial layers");
    }
    elem->set_extra_integer(eid, layer_id);
  }
  return dynamic_pointer_cast<MeshBase>(mesh);
}

int
PlaneIDMeshGenerator::getPlaneID(const Point & p) const
{
  if (_planes.size() == 0)
    return -1;
  else if (p(_axis_index) < _planes[0])
    return -1;
  else if (p(_axis_index) > _planes.back())
    return _planes.size();
  int id = 0;
  for (unsigned int layer_id = 0; layer_id < _planes.size() - 1; ++layer_id)
  {
    if (_planes[layer_id] < p(_axis_index) && _planes[layer_id + 1] >= p(_axis_index))
    {
      id = layer_id;
      break;
    }
  }
  return id;
}
