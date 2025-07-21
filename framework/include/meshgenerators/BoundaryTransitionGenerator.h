//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MeshGenerator.h"

/**
 * Convert the elements involved in a set of external boundaries to ensure that the boundary set
 * only contains TRI3 elements.
 */
class BoundaryTransitionGenerator : public MeshGenerator
{
public:
  static InputParameters validParams();

  BoundaryTransitionGenerator(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  /// Mesh that possibly comes from another generator
  std::unique_ptr<MeshBase> & _input;
  ///The boundaries to be converted
  const std::vector<BoundaryName> _boundary_names;
  /// Whether to check if the provided boundaries are external
  const bool _external_boundaries_checking;
  /// The base subdomain ID to shift the original elements because of the element type change
  SubdomainID _sid_shift_base;

  /*
   * Convert the element to a TRI3 element by modifying the mesh.
   */
  void convertElem(ReplicatedMesh & mesh,
                   const dof_id_type & elem_id,
                   const std::vector<unsigned int> & side_indices,
                   const std::vector<std::vector<boundary_id_type>> & elem_side_info);

  /**
   * Convert a HEX8 element to elements with TRI3 surfaces on the given original QUAD4 side(s).
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the HEX8 element to be converted
   * @param side_indices The indices of the QUAD4 sides to be converted to TRI3 sides
   * @param elem_side_info The boundary IDs associated with the sides of the HEX8 element
   */
  void convertHex8Elem(ReplicatedMesh & mesh,
                       const dof_id_type & elem_id,
                       const std::vector<unsigned int> & side_indices,
                       const std::vector<std::vector<boundary_id_type>> & elem_side_info);

  /**
   * Create one PYRAMID5 element based on a side and the centroid of the HEX8 element.
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the HEX8 element to be converted
   * @param side_index The index of the side to be converted
   * @param new_node The new node created at the centroid of the HEX8 element
   * @param side_info The boundary IDs associated with the side of the HEX8 element
   */
  void createUnitPyramid5FromHex8(ReplicatedMesh & mesh,
                                  const dof_id_type & elem_id,
                                  const unsigned int & side_index,
                                  const Node * new_node,
                                  const std::vector<boundary_id_type> & side_info);

  /**
   * Create two TET4 elements based on a side and the centroid of the HEX8 element.
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the HEX8 element to be converted
   * @param side_index The index of the side to be converted
   * @param new_node The new node created at the centroid of the HEX8 element
   * @param side_info The boundary IDs associated with the side of the HEX8 element
   */
  void createUnitTet4FromHex8(ReplicatedMesh & mesh,
                              const dof_id_type & elem_id,
                              const unsigned int & side_index,
                              const Node * new_node,
                              const std::vector<boundary_id_type> & side_info);

  /**
   * Convert a PRISM6 element to elements with TRI3 surfaces on the given original QUAD4 side(s).
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the PRISM6 element to be converted
   * @param side_indices The indices of the QUAD sides to be converted to TRI3 sides
   * @param elem_side_info The boundary IDs associated with the sides of the PRISM6 element
   */
  void convertPrism6Elem(ReplicatedMesh & mesh,
                         const dof_id_type & elem_id,
                         const std::vector<unsigned int> & side_indices,
                         const std::vector<std::vector<boundary_id_type>> & elem_side_info);
  /**
   * Create one or two TET4 elements based on a side and the centroid of the PRISM6 element.
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the PRISM6 element to be converted
   * @param side_index The index of the side to be converted
   * @param new_node The new node created at the centroid of the PRISM6 element
   * @param side_info The boundary IDs associated with the side of the PRISM6 element
   */
  void createUnitTet4FromPrism6(ReplicatedMesh & mesh,
                                const dof_id_type & elem_id,
                                const unsigned int & side_index,
                                const Node * new_node,
                                const std::vector<boundary_id_type> & side_info);

  /**
   * Create a PYRAMID5 element based on on side and the centroid of the PRISM6 element.
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the PRISM6 element to be converted
   * @param side_index The index of the side to be converted
   * @param new_node The new node created at the centroid of the PRISM6 element
   * @param side_info The boundary IDs associated with the side of the PRISM6 element
   */
  void createUnitPyramid5FromPrism6(ReplicatedMesh & mesh,
                                    const dof_id_type & elem_id,
                                    const unsigned int & side_index,
                                    const Node * new_node,
                                    const std::vector<boundary_id_type> & side_info);

  /**
   * Convert a PYRAMID5 element to elements with TRI3 surfaces on the original QUAD4 side.
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the PYRAMID5 element to be converted
   * @param elem_side_info The boundary IDs associated with the sides of the PYRAMID
   */
  void convertPyramid5Elem(ReplicatedMesh & mesh,
                           const dof_id_type & elem_id,
                           const std::vector<std::vector<boundary_id_type>> & elem_side_info);

  /**
   * Retain the extra integer of the original element in a new element.
   * @param mesh The mesh containing the element
   * @param elem_id The ID of the original element
   * @param new_elem_ptr The pointer to the new element that will retain the extra integer
   */
  void retainEEID(ReplicatedMesh & mesh, const dof_id_type & elem_id, Elem * new_elem_ptr);
};
