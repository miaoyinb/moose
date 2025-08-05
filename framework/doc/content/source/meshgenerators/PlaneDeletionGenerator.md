# PlaneDeletionGenerator

!syntax description /Mesh/PlaneDeletionGenerator

## Overview

Allows for deletion of elements that lie on one side of a plane.  The plane can be specified via a point and a vector that is normal to the plane.

A few options are available for determining which elements to delete, which is specified by the [!param](/Mesh/PlaneDeletionGenerator/deletion_criterion) parameter:

- `CENTROID`: Delete elements whose centroids lie "above" (in the direction of the normal vector) the plane.
- `VERTEX_AVG`: Delete elements whose vertex average lies "above" the plane (default).
- `ONE_VERTEX`: Delete elements that have at least one vertex "above" the plane.
- `ALL_VERTICES`: Delete elements that have all vertices "above" the plane.

An optional `new_boundary` parameter can also be specified which will make any newly-created free-surfaces have that boundary ID.

## Example

```
[Mesh]
  [generated]
    type = GeneratedMeshGenerator
    dim = 3
    nx = 4
    ny = 4
    nz = 4
  []
  [deleter]
    type = PlaneDeletionGenerator
    point = '0.5 0.5 0'
    normal = '-1 1 0'
    input = generated
    new_boundary = 6
  []
[]
```

!media media/meshgenerators/plane_deletion_before.png
       caption=The Original Mesh
       style=width:50%;padding:20px;

!media media/meshgenerators/plane_deletion_after.png
       caption=With the elements removed (and showing the new boundary)
       style=width:50%;padding:20px;


!syntax parameters /Mesh/PlaneDeletionGenerator

!syntax inputs /Mesh/PlaneDeletionGenerator

!syntax children /Mesh/PlaneDeletionGenerator
