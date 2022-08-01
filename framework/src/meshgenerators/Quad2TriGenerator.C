//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "Quad2TriGenerator.h"
#include "MooseMesh.h"
#include "Conversion.h"
#include "MooseMeshUtils.h"
#include "CastUniquePointer.h"

#include "libmesh/elem.h"

registerMooseObject("MooseApp", Quad2TriGenerator);

InputParameters
Quad2TriGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addClassDescription("Convert Quad mesh to Tri mesh.");

  return params;
}

Quad2TriGenerator::Quad2TriGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters), _input(getMesh("input"))
{
}

std::unique_ptr<MeshBase>
Quad2TriGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);
  if (mesh->mesh_dimension() == 3)
    Hex8toPrism6(*mesh);
  MeshTools::Modification::all_tri(*mesh);
  return dynamic_pointer_cast<MeshBase>(mesh);
}

void
Quad2TriGenerator::Hex8toPrism6(MeshBase & mesh)
{
  const dof_id_type n_orig_elem = mesh.n_elem();
  const dof_id_type max_orig_id = mesh.max_elem_id();

  std::vector<Elem *> new_elements;
  unsigned int max_subelems = 2;
  new_elements.reserve(max_subelems * n_orig_elem);

  const bool mesh_has_boundary_data = (mesh.get_boundary_info().n_boundary_conds() > 0);

  std::vector<Elem *> new_bndry_elements;
  std::vector<unsigned short int> new_bndry_sides;
  std::vector<boundary_id_type> new_bndry_ids;

  bool added_new_ghost_point = false;

  for (auto & elem : mesh.element_ptr_range())
  {
    const ElemType etype = elem->type();
    Elem * subelem[2];

    for (unsigned int i = 0; i != max_subelems; ++i)
      subelem[i] = nullptr;

    if (etype == HEX8)
    {
      subelem[0] = new Prism6;
      subelem[1] = new Prism6;
      if ((elem->point(0) - elem->point(2)).norm() < (elem->point(1) - elem->point(3)).norm())
      {
        subelem[0]->set_node(0) = elem->node_ptr(0);
        subelem[0]->set_node(1) = elem->node_ptr(1);
        subelem[0]->set_node(2) = elem->node_ptr(2);
        subelem[0]->set_node(3) = elem->node_ptr(4);
        subelem[0]->set_node(4) = elem->node_ptr(5);
        subelem[0]->set_node(5) = elem->node_ptr(6);

        subelem[1]->set_node(0) = elem->node_ptr(0);
        subelem[1]->set_node(1) = elem->node_ptr(2);
        subelem[1]->set_node(2) = elem->node_ptr(3);
        subelem[1]->set_node(3) = elem->node_ptr(4);
        subelem[1]->set_node(4) = elem->node_ptr(6);
        subelem[1]->set_node(5) = elem->node_ptr(7);
      }

      else
      {
        subelem[0]->set_node(0) = elem->node_ptr(0);
        subelem[0]->set_node(1) = elem->node_ptr(1);
        subelem[0]->set_node(2) = elem->node_ptr(3);
        subelem[0]->set_node(3) = elem->node_ptr(4);
        subelem[0]->set_node(4) = elem->node_ptr(5);
        subelem[0]->set_node(5) = elem->node_ptr(7);

        subelem[1]->set_node(0) = elem->node_ptr(1);
        subelem[1]->set_node(1) = elem->node_ptr(2);
        subelem[1]->set_node(2) = elem->node_ptr(3);
        subelem[1]->set_node(3) = elem->node_ptr(5);
        subelem[1]->set_node(4) = elem->node_ptr(6);
        subelem[1]->set_node(5) = elem->node_ptr(7);
      }
    }
    else if (etype == PRISM6)
    {
      subelem[0] = new Prism6;
      subelem[0]->set_node(0) = elem->node_ptr(0);
      subelem[0]->set_node(1) = elem->node_ptr(1);
      subelem[0]->set_node(2) = elem->node_ptr(2);
      subelem[0]->set_node(3) = elem->node_ptr(3);
      subelem[0]->set_node(4) = elem->node_ptr(4);
      subelem[0]->set_node(5) = elem->node_ptr(5);
    }
    // Be sure the correct IDs are also set for all subelems.
    for (unsigned int i = 0; i != max_subelems; ++i)
      if (subelem[i])
      {
        subelem[i]->processor_id() = elem->processor_id();
        subelem[i]->subdomain_id() = elem->subdomain_id();
      }

    // On a mesh with boundary data, we need to move that data to
    // the new elements.

    // On a mesh which is distributed, we need to move
    // remote_elem links to the new elements.
    bool mesh_is_serial = mesh.is_serial();

    if (mesh_has_boundary_data || mesh_is_serial)
    {
      // Container to key boundary IDs handed back by the BoundaryInfo object.
      std::vector<boundary_id_type> bc_ids;

      for (auto sn : elem->side_index_range())
      {
        mesh.get_boundary_info().boundary_ids(elem, sn, bc_ids);
        for (const auto & b_id : bc_ids)
        {
          if (mesh_is_serial && b_id == BoundaryInfo::invalid_id)
            continue;

          // Make a sorted list of node ids for elem->side(sn)
          std::unique_ptr<Elem> elem_side = elem->build_side_ptr(sn);
          std::vector<dof_id_type> elem_side_nodes(elem_side->n_nodes());
          for (unsigned int esn = 0, n_esn = cast_int<unsigned int>(elem_side_nodes.size());
               esn != n_esn;
               ++esn)
            elem_side_nodes[esn] = elem_side->node_id(esn);
          std::sort(elem_side_nodes.begin(), elem_side_nodes.end());

          for (unsigned int i = 0; i != max_subelems; ++i)
            if (subelem[i])
            {
              for (auto subside : subelem[i]->side_index_range())
              {
                std::unique_ptr<Elem> subside_elem = subelem[i]->build_side_ptr(subside);

                // Make a list of *vertex* node ids for this subside, see if they are all present
                // in elem->side(sn).  Note 1: we can't just compare elem->key(sn) to
                // subelem[i]->key(subside) in the Prism cases, since the new side is
                // a different type.  Note 2: we only use vertex nodes since, in the future,
                // a Hex20 or Prism15's QUAD8 face may be split into two Tri6 faces, and the
                // original face will not contain the mid-edge node.
                std::vector<dof_id_type> subside_nodes(subside_elem->n_vertices());
                for (unsigned int ssn = 0, n_ssn = cast_int<unsigned int>(subside_nodes.size());
                     ssn != n_ssn;
                     ++ssn)
                  subside_nodes[ssn] = subside_elem->node_id(ssn);
                std::sort(subside_nodes.begin(), subside_nodes.end());

                // std::includes returns true if every element of the second sorted range is
                // contained in the first sorted range.
                if (std::includes(elem_side_nodes.begin(),
                                  elem_side_nodes.end(),
                                  subside_nodes.begin(),
                                  subside_nodes.end()))
                {
                  if (b_id != BoundaryInfo::invalid_id)
                  {
                    new_bndry_ids.push_back(b_id);
                    new_bndry_elements.push_back(subelem[i]);
                    new_bndry_sides.push_back(subside);
                  }

                  // If the original element had a RemoteElem neighbor on side 'sn',
                  // then the subelem has one on side 'subside'.
                  if (elem->neighbor_ptr(sn) == remote_elem)
                    subelem[i]->set_neighbor(subside, const_cast<RemoteElem *>(remote_elem));
                }
              }
            }
        } // end for loop over boundary IDs
      }   // end for loop over sides

      // Remove the original element from the BoundaryInfo structure.
      mesh.get_boundary_info().remove(elem);

    } // end if (mesh_has_boundary_data)

    // Determine new IDs for the split elements which will be
    // the same on all processors, therefore keeping the Mesh
    // in sync.  Note: we offset the new IDs by max_orig_id to
    // avoid overwriting any of the original IDs.
    for (unsigned int i = 0; i != max_subelems; ++i)
      if (subelem[i])
      {
        // Determine new IDs for the split elements which will be
        // the same on all processors, therefore keeping the Mesh
        // in sync.  Note: we offset the new IDs by the max of the
        // pre-existing ids to avoid conflicting with originals.
        subelem[i]->set_id(max_orig_id + 6 * elem->id() + i);

        // Prepare to add the newly-created simplices
        new_elements.push_back(subelem[i]);
      }

    // Delete the original element
    mesh.delete_elem(elem);
  } // End for loop over elements

  // Now, iterate over the new elements vector, and add them each to
  // the Mesh.
  {
    std::vector<Elem *>::iterator el = new_elements.begin();
    const std::vector<Elem *>::iterator end = new_elements.end();
    for (; el != end; ++el)
      mesh.add_elem(*el);
  }

  if (mesh_has_boundary_data)
  {
    // If the old mesh had boundary data, the new mesh better have
    // some.  However, we can't assert that the size of
    // new_bndry_elements vector is > 0, since we may not have split
    // any elements actually on the boundary.  We also can't assert
    // that the original number of boundary sides is equal to the
    // sum of the boundary sides currently in the mesh and the
    // newly-added boundary sides, since in 3D, we may have split a
    // boundary QUAD into two boundary TRIs.  Therefore, we won't be
    // too picky about the actual number of BCs, and just assert that
    // there are some, somewhere.
#ifndef NDEBUG
    bool nbe_nonempty = new_bndry_elements.size();
    mesh.comm().max(nbe_nonempty);
    libmesh_assert(nbe_nonempty || mesh.get_boundary_info().n_boundary_conds() > 0);
#endif

    // We should also be sure that the lengths of the new boundary data vectors
    // are all the same.
    libmesh_assert_equal_to(new_bndry_elements.size(), new_bndry_sides.size());
    libmesh_assert_equal_to(new_bndry_sides.size(), new_bndry_ids.size());

    // Add the new boundary info to the mesh
    for (std::size_t s = 0; s < new_bndry_elements.size(); ++s)
      mesh.get_boundary_info().add_side(
          new_bndry_elements[s], new_bndry_sides[s], new_bndry_ids[s]);
  }

  // In a DistributedMesh any newly added ghost node ids may be
  // inconsistent, and unique_ids of newly added ghost nodes remain
  // unset.
  // make_nodes_parallel_consistent() will fix all this.
  if (!mesh.is_serial())
  {
    mesh.comm().max(added_new_ghost_point);

    if (added_new_ghost_point)
      MeshCommunication().make_nodes_parallel_consistent(mesh);
  }

  // Prepare the newly created mesh for use.
  mesh.prepare_for_use(/*skip_renumber =*/false);

  // Let the new_elements and new_bndry_elements vectors go out of scope.
}
