//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ReportingIDGeneratorBase.h"

std::vector<dof_id_type>
ReportingIDGeneratorBase::getCellwiseIntegerIDs(
    const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
    const std::vector<std::vector<unsigned int>> & pattern,
    const bool use_exclude_id,
    const std::vector<bool> & exclude_ids) const
{
  std::vector<dof_id_type> integer_ids;
  dof_id_type id = 0;
  for (MooseIndex(pattern) i = 0; i < pattern.size(); ++i)
  {
    for (MooseIndex(pattern[i]) j = 0; j < pattern[i].size(); ++j)
    {
      ReplicatedMesh & cell_mesh = *meshes[pattern[i][j]];
      unsigned int n_cell_elem = cell_mesh.n_elem();
      bool exclude_id = false;
      if (use_exclude_id)
        if (exclude_ids[pattern[i][j]])
          exclude_id = true;
      if (!exclude_id)
      {
        for (unsigned int k = 0; k < n_cell_elem; ++k)
          integer_ids.push_back(id);
        ++id;
      }
      else
        for (unsigned int k = 0; k < n_cell_elem; ++k)
          integer_ids.push_back(DofObject::invalid_id);
    }
  }
  return integer_ids;
}

std::vector<dof_id_type>
ReportingIDGeneratorBase::getPatternIntegerIDs(
    const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
    const std::vector<std::vector<unsigned int>> & pattern) const
{
  std::vector<dof_id_type> integer_ids;
  for (MooseIndex(pattern) i = 0; i < pattern.size(); ++i)
  {
    for (MooseIndex(pattern[i]) j = 0; j < pattern[i].size(); ++j)
    {
      ReplicatedMesh & cell_mesh = *meshes[pattern[i][j]];
      unsigned int n_cell_elem = cell_mesh.n_elem();
      for (unsigned int k = 0; k < n_cell_elem; ++k)
        integer_ids.push_back(pattern[i][j]);
    }
  }
  return integer_ids;
}

std::vector<dof_id_type>
ReportingIDGeneratorBase::getManualIntegerIDs(
    const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
    const std::vector<std::vector<unsigned int>> & pattern,
    const std::vector<std::vector<dof_id_type>> & id_pattern) const
{
  std::vector<dof_id_type> integer_ids;
  for (MooseIndex(pattern) i = 0; i < pattern.size(); ++i)
  {
    for (MooseIndex(pattern[i]) j = 0; j < pattern[i].size(); ++j)
    {
      unsigned int id = id_pattern[i][j];
      ReplicatedMesh & cell_mesh = *meshes[pattern[i][j]];
      unsigned int n_cell_elem = cell_mesh.n_elem();
      for (unsigned int k = 0; k < n_cell_elem; ++k)
        integer_ids.push_back(id);
    }
  }
  return integer_ids;
}

std::set<SubdomainID>
ReportingIDGeneratorBase::getCellBlockIDs(
    const std::vector<std::unique_ptr<ReplicatedMesh>> & meshes,
    const std::vector<std::vector<unsigned int>> & pattern) const
{
  std::set<SubdomainID> blks;
  for (MooseIndex(pattern) i = 0; i < pattern.size(); ++i)
  {
    for (MooseIndex(pattern[i]) j = 0; j < pattern[i].size(); ++j)
    {
      ReplicatedMesh & cell_mesh = *meshes[pattern[i][j]];
      for (auto elem : cell_mesh.element_ptr_range())
      {
        auto blk = elem->subdomain_id();
        auto it = blks.find(blk);
        if (it == blks.end())
          blks.insert(blk);
      }
    }
  }
  return blks;
}
