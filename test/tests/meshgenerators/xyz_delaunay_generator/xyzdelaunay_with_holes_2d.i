[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 1
    ny = 1
    nz = 1
  []
  [outer_bdy]
    type = ParsedNodeTransformGenerator
    input = gmg
    x_function = "x"
    y_function = "y"
    z_function = "z+x*y*z"
  []
  [hole_1_3d]
    type = ParsedNodeTransformGenerator
    input = gmg
    x_function = ".25+.125*x"
    y_function = ".25+.125*y"
    z_function = ".25+.125*z"
  []
  [hole_1_block]
    type = LowerDBlockFromSidesetGenerator
    input = hole_1_3d
    sidesets = 'top bottom left right front back'
    new_block_id = 100
    new_block_name = 'surface'
  []
  [hole_1]
    type = BlockToMeshConverterGenerator
    input = hole_1_block
    target_blocks = 'surface'
  []
  [hole_2_3d]
    type = ParsedNodeTransformGenerator
    input = gmg
    x_function = ".75+.125*x"
    y_function = ".75+.125*y"
    z_function = ".75+.125*z"
  []
  [hole_2_block]
    type = LowerDBlockFromSidesetGenerator
    input = hole_2_3d
    sidesets = 'top bottom left right front back'
    new_block_id = 100
    new_block_name = 'surface'
  []
  [hole_2]
    type = BlockToMeshConverterGenerator
    input = hole_2_block
    target_blocks = 'surface'
  []
  [triang]
    type = XYZDelaunayGenerator
    boundary = 'outer_bdy'
    holes = 'hole_1
             hole_2'
    # Let NetGen know interior points are okay
    desired_volume = 100000
  []
[]

[Executioner]
  type = Steady
[]

[Postprocessors]
  [volume]
    type = VolumePostprocessor
  []
[]

[Problem]
  solve = false
[]

[Outputs]
  [output]
    type = CSV
    file_base = 'xyzdelaunay_with_holes_out'
  []
[]
