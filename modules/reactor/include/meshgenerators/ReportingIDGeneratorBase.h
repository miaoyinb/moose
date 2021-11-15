//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MooseTypes.h"
#include "libmesh/elem.h"
#include "libmesh/replicated_mesh.h"
#include "libmesh/dof_object.h"

/**
 * A base class that contains common members for reporting ID generators.
 */
class ReportingIDGeneratorBase
{
protected:
  /**
   * assign IDs for each component in pattern in sequential order
   * @param mesh_ptrs input meshes of the cartesian or hexagonalpatterned mesh generator
   * @param pattern 2D vector of the mesh pattern
   * @param use_exclude_id flag to indicate if exclude_id is defined
   * @param exclude_ids flag to indicate if exclude_id is used for each input mesh
   * @return list of reporting IDs for individual mesh elements
   **/
  std::vector<dof_id_type>
  getCellwiseIntegerIDs(const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
                        const std::vector<std::vector<unsigned int>> & pattern,
                        const bool use_exclude_id,
                        const std::vector<bool> & exclude_ids) const;

  /**
   * assign IDs for each input component type
   * @param mesh_ptrs input meshes of the cartesian or hexagonalpatterned mesh generator
   * @param pattern 2D vector of the mesh pattern
   * @return list of reporting IDs for individual mesh elements
   **/
  std::vector<dof_id_type>
  getPatternIntegerIDs(const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
                       const std::vector<std::vector<unsigned int>> & pattern) const;

  /**
   * assign IDs based on user-defined mapping defined in id_pattern
   * @param mesh_ptrs input meshes of the cartesian or hexagonalpatterned mesh generator
   * @param pattern 2D vector of the mesh pattern
   * @param id_pattern user-defined integer ID for each input pattern cell
   * @return list of reporting IDs for individual mesh elements
   **/
  std::vector<dof_id_type>
  getManualIntegerIDs(const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
                      const std::vector<std::vector<unsigned int>> & pattern,
                      const std::vector<std::vector<dof_id_type>> & id_pattern) const;

  /**
   * get list of block IDs in input mesh cells
   * @param mesh_ptrs input meshes of the cartesian or hexagonalpatterned mesh generator
   * @param pattern 2D vector of the mesh pattern
   * @return list of  block IDs in input meshes
   **/
  std::set<SubdomainID>
  getCellBlockIDs(const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
                  const std::vector<std::vector<unsigned int>> & pattern) const;
};
