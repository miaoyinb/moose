//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once
#include "FlexiblePatternGeneratorBase.h"

/**
 * This FlexiblePatternGenerator object is designed to generate a complex mesh with a background
 * region with dispersed unit meshes in it and distributed based on a series of flexible patterns.
 */
class FlexiblePatternGenerator : public FlexiblePatternGeneratorBase
{
public:
  static InputParameters validParams();

  FlexiblePatternGenerator(const InputParameters & parameters);

  // std::unique_ptr<MeshBase> generate() override;
};
