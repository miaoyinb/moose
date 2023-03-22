//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once
#include "PolygonMeshGeneratorBase.h"

/**
 * This MeshLineCutter object is.
 */
class MeshLineCutter : public PolygonMeshGeneratorBase
{
public:
  static InputParameters validParams();

  MeshLineCutter(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  ///
  const MeshGeneratorName _input_name;
  ///
  const std::vector<Real> _cut_line_params;
  /// Reference to input mesh pointer
  std::unique_ptr<MeshBase> & _input;

  /**
   * Removes all the elements on one side of a given line and deforms the elements intercepted by
   * the line to form a flat new boundary
   * @param mesh input mesh to perform line-based elements removing on
   * @param bdry_pars line parameter sets {a, b, c} as in a*x+b*y+c=0
   * @param block_id_to_remove subdomain id used to mark the elements that need to be removed
   * @param subdomain_ids_set all the subdomain ids in the input mesh
   * @param trimming_section_boundary_id ID of the new external boundary formed due to
   * trimming
   * @param external_boundary_id ID of the external boundary of the input mesh
   * @param assign_ext_to_trim whether to assign external_boundary_id to the new boundary formed by
   * removal
   * @param side_to_remove which side of the mesh needs to be removed: true means ax+by+c>0 and
   * false means ax+by+c<0
   */
  void lineRemover(ReplicatedMesh & mesh,
                   const std::vector<Real> bdry_pars,
                   const subdomain_id_type block_id_to_remove,
                   const std::set<subdomain_id_type> subdomain_ids_set,
                   const boundary_id_type trimming_section_boundary_id,
                   const boundary_id_type external_boundary_id = OUTER_SIDESET_ID,
                   const bool assign_ext_to_new = false,
                   const bool side_to_remove = true);

  /**
   * Determines whether a point on XY-plane is on the side of a given line that needs to be removed
   * @param px x coordinate of the point
   * @param py y coordinate of the point
   * @param param_1 parameter 1 (a) in line formula a*x+b*y+c=0
   * @param param_2 parameter 2 (b) in line formula a*x+b*y+c=0
   * @param param_3 parameter 3 (c) in line formula a*x+b*y+c=0
   * @param direction_param which side is the side that needs to be removed
   * @param dis_tol tolerance used in determining side
   * @return whether the point is on the side of the line that needed to be removed
   */
  bool lineSideDeterminator(const Real px,
                            const Real py,
                            const Real param_1,
                            const Real param_2,
                            const Real param_3,
                            const bool direction_param,
                            const Real dis_tol = 1.0E-6);

  /**
   * Calculates the intersection Point of two given straight lines
   * @param param_11 parameter 1 (a) in line formula a*x+b*y+c=0 for the first line
   * @param param_12 parameter 2 (b) in line formula a*x+b*y+c=0 for the first line
   * @param param_13 parameter 3 (c) in line formula a*x+b*y+c=0 for the first line
   * @param param_21 parameter 1 (a) in line formula a*x+b*y+c=0 for the second line
   * @param param_22 parameter 2 (b) in line formula a*x+b*y+c=0 for the second line
   * @param param_23 parameter 3 (c) in line formula a*x+b*y+c=0 for the second line
   * @return intersect point of the two lines
   */
  Point twoLineIntersection(const Real param_11,
                            const Real param_12,
                            const Real param_13,
                            const Real param_21,
                            const Real param_22,
                            const Real param_23);

  /**
   * Fixes degenerate QUAD elements created by the hexagonal mesh trimming by converting them into
   * TRI elements
   * @param mesh input mesh with degenerate QUAD elements that need to be fixed
   * @param subdomain_ids_set all the subdomain ids in the input mesh
   * @param tri_elem_subdomain_name_suffix subdomain name suffix for newly formed triangular elements
   * @param tri_elem_subdomain_shift subdomain id shift for newly formed triangular elements
   * @return whether any elements have been fixed
   */
  bool quasiTriElementsFixer(ReplicatedMesh & mesh,
                             const std::set<subdomain_id_type> subdomain_ids_set,
                             const std::string tri_elem_subdomain_name_suffix = std::string(),
                             const subdomain_id_type tri_elem_subdomain_shift = 0);

  /**
   * Calculates the internal angles of a given 2D element
   * @param elem the element that needs to be investigated
   * @return sizes of all the internal angles, sorted by their size
   */
  std::vector<std::pair<Real, unsigned int>> vertex_angles(Elem & elem) const;
};
