#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <as-ops.h>

#define FrustumPlaneCount 6
#define FrustumCornerCount 8

typedef enum frustum_plane_e {
  frustum_plane_left,
  frustum_plane_right,
  frustum_plane_top,
  frustum_plane_bottom,
  frustum_plane_near,
  frustum_plane_far
} frustum_plane_e;

typedef enum frustum_corner_e {
  frustum_corner_near_bottom_left,
  frustum_corner_near_bottom_right,
  frustum_corner_near_top_right,
  frustum_corner_near_top_left,
  frustum_corner_far_bottom_left,
  frustum_corner_far_bottom_right,
  frustum_corner_far_top_right,
  frustum_corner_far_top_left,
} frustum_corner_e;

typedef struct frustum_planes_t {
  as_plane planes[FrustumPlaneCount];
} frustum_planes_t;

typedef struct frustum_corners_t {
  as_point3f corners[FrustumCornerCount];
} frustum_corners_t;

frustum_planes_t build_frustum_planes(
  float aspect_ratio, float vertical_fov, float near, float far);

frustum_corners_t build_frustum_corners(
  float aspect_ratio, float vertical_fov, float near, float far);

#endif // FRUSTUM_H
