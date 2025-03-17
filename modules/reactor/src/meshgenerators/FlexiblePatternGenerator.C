//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FlexiblePatternGenerator.h"

// C++ includes
#include <cmath>

registerMooseObject("ReactorApp", FlexiblePatternGenerator);

InputParameters
FlexiblePatternGenerator::validParams()
{
  InputParameters params = FlexiblePatternGeneratorBase::validParams();

  params.addClassDescription("This FlexiblePatternGenerator object is designed to generate a "
                             "mesh with a background region with dispersed unit meshes in "
                             "it and distributed based on a series of flexible patterns.");
  return params;
}

FlexiblePatternGenerator::FlexiblePatternGenerator(const InputParameters & parameters)
  : FlexiblePatternGeneratorBase(parameters)
{
}

// std::unique_ptr<MeshBase>
// FlexiblePatternGenerator::generate()
// {

// }
