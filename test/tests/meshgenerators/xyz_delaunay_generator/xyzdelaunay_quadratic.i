# Second-order (TET10/TET14) tetrahedralization with XYZDelaunayGenerator.
#
# The outer boundary is converted to second order and THEN deformed by a nonlinear
# transform, so its mid-edge nodes lie off the straight-edge midpoint (i.e. it carries
# genuine curvature). XYZDelaunayGenerator inherits that curvature onto the generated
# tets. A second-order hole is stitched in to exercise second-order stitching.

[Mesh]
  # ---- Outer boundary: curved, second order ----
  [gmg]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 2
    ny = 2
    nz = 2
    elem_type = TET4
  []
  [bdy_second]
    type = ElementOrderConversionGenerator
    input = gmg
    conversion_type = SECOND_ORDER
  []
  [outer_bdy]
    type = ParsedNodeTransformGenerator
    input = bdy_second
    x_function = 'x'
    y_function = 'y'
    z_function = 'z + 0.3 * x * y * z'
  []

  # ---- Hole: second order, smaller interior cube ----
  [hgmg]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 1
    ny = 1
    nz = 1
    elem_type = TET4
  []
  [hole_second]
    type = ElementOrderConversionGenerator
    input = hgmg
    conversion_type = SECOND_ORDER
  []
  [hole]
    type = ParsedNodeTransformGenerator
    input = hole_second
    x_function = '0.35 + 0.3 * x'
    y_function = '0.35 + 0.3 * y'
    z_function = '0.35 + 0.3 * z'
  []
  # Give the stitched hole a distinct subdomain id so it differs from the
  # generated tetrahedra (which are subdomain 0).
  [hole_block]
    type = SubdomainIDGenerator
    input = hole
    subdomain_id = 3
  []

  [triang]
    type = XYZDelaunayGenerator
    boundary = 'outer_bdy'
    holes = 'hole_block'
    stitch_holes = 'true'
    desired_volume = 100000
    tet_element_type = TET10
  []
[]

[Executioner]
  type = Steady
[]

[Postprocessors]
  [volume]
    type = VolumePostprocessor
  []
  # The node count is what pins down the element type here. The integrated
  # volume cannot: TET4, TET10 and TET14 all report 1.075 on this mesh, since
  # the boundary deformation z + 0.3*x*y*z is reproduced exactly in volume even
  # by straight-edged tets. Node counts are 69 / 403 / 887 respectively.
  [num_nodes]
    type = NumNodes
  []
  [num_elems]
    type = NumElements
  []
[]

[Problem]
  solve = false
[]

[Outputs]
  [output]
    type = CSV
    file_base = 'xyzdelaunay_quadratic_tet10_out'
  []
[]
