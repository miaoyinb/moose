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
  [2d_block]
    type = LowerDBlockFromSidesetGenerator
    input = outer_bdy
    sidesets = 'top bottom left right front back'
    new_block_id = 100
    new_block_name = 'surface'
  []
  [2d_mesh]
    type = BlockToMeshConverterGenerator
    input = 2d_block
    target_blocks = 'surface'
  []
  [triang]
    type = XYZDelaunayGenerator
    boundary = '2d_mesh'
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
    file_base = 'xyzdelaunay_mesh_generator_out'
  []
[]
