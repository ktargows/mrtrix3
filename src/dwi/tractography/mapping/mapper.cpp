/*
    Copyright 2011 Brain Research Institute, Melbourne, Australia

    Written by Robert E. Smith, 2011.

    This file is part of MRtrix.

    MRtrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MRtrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "dwi/tractography/mapping/mapper.h"


namespace MR {
namespace DWI {
namespace Tractography {
namespace Mapping {




void TrackMapperBase::voxelise (const std::vector< Point<float> >& tck, SetVoxel& voxels) const
{
  Voxel vox;
  for (std::vector< Point<float> >::const_iterator i = tck.begin(); i != tck.end(); ++i) {
    vox = round (transform.scanner2voxel (*i));
    if (check (vox, info))
      voxels.insert (vox);
  }
}



void TrackMapperBase::voxelise (const std::vector< Point<float> >& tck, SetVoxelDEC& voxels) const
{

  std::vector< Point<float> >::const_iterator prev = tck.begin();
  const std::vector< Point<float> >::const_iterator last = tck.end() - 1;

  VoxelDEC vox;
  for (std::vector< Point<float> >::const_iterator i = tck.begin(); i != last; ++i) {
    vox = round (transform.scanner2voxel (*i));
    if (check (vox, info)) {
      const Point<float> dir ((*(i+1) - *prev).normalise());
      SetVoxelDEC::iterator existing_vox = voxels.find (vox);
      if (existing_vox == voxels.end()) {
        vox.set_dir (dir);
        voxels.insert (vox);
      } else {
        (*existing_vox) += dir;
      }
    }
    prev = i;
  }

  vox = round (transform.scanner2voxel (*last));
  if (check (vox, info)) {
    const Point<float> dir ((*last - *prev).normalise());
    SetVoxelDEC::iterator existing_vox = voxels.find (vox);
    if (existing_vox == voxels.end()) {
      vox.set_dir (dir);
      voxels.insert (vox);
    } else {
      (*existing_vox) += dir;
    }
  }

  for (SetVoxelDEC::iterator i = voxels.begin(); i != voxels.end(); ++i)
    i->normalise();

}


void TrackMapperBase::add_to_set (SetVoxel& out, const Voxel& v, const Point<float>& d, const float l = 1.0) const
{
  out.insert (v, l);
}
void TrackMapperBase::add_to_set (SetVoxelDEC& out, const Voxel& v, const Point<float>& d, const float l = 1.0) const
{
  out.insert (v, d, l);
}
void TrackMapperBase::add_to_set (SetDixel& out, const Voxel& v, const Point<float>& d, const float l = 1.0) const
{
  assert (dixel_plugin);
  const size_t bin = (*dixel_plugin) (d);
  out.insert (v, bin, l);
}
void TrackMapperBase::add_to_set (SetVoxelTOD& out, const Voxel& v, const Point<float>& d, const float l = 1.0) const
{
  assert (tod_plugin);
  Math::Vector<float> sh;
  (*tod_plugin) (sh, d);
  out.insert (v, sh, l);
}






void TrackMapperTWI::set_factor (const std::vector< Point<float> >& tck, SetVoxelExtras& out) const
{

  size_t count = 0;

  switch (contrast) {

    case TDI:         out.factor = 1.0; break;
    case ENDPOINT:    out.factor = 1.0; break;
    case LENGTH:
    case INVLENGTH:
      out.factor = 0.0;
      for (size_t i = 1; i != tck.size(); ++i)
        out.factor += dist(tck[i], tck[i-1]);
      if (contrast == INVLENGTH)
        out.factor = 1.0f / out.factor;
      break;

    case SCALAR_MAP:
    case SCALAR_MAP_COUNT:
    case FOD_AMP:
    case CURVATURE:

      factors.clear();
      factors.reserve (tck.size());
      load_factors (tck); // This should call the overloaded virtual function for TrackMapperImage

      switch (track_statistic) {

        case T_SUM:
          out.factor = 0.0;
          for (std::vector<float>::const_iterator i = factors.begin(); i != factors.end(); ++i) {
            if (std::isfinite (*i))
              out.factor += *i;
          }
          break;

        case T_MIN:
          out.factor = INFINITY;
          for (std::vector<float>::const_iterator i = factors.begin(); i != factors.end(); ++i) {
            if (std::isfinite (*i))
              out.factor = MIN (out.factor, *i);
          }
          break;

        case T_MEAN:
          out.factor = 0.0;
          for (std::vector<float>::const_iterator i = factors.begin(); i != factors.end(); ++i) {
            if (std::isfinite (*i)) {
              out.factor += *i;
              ++count;
            }
          }
          out.factor = (count ? (out.factor / float(count)) : 0.0);
          break;

        case T_MAX:
          out.factor = -INFINITY;
          for (std::vector<float>::const_iterator i = factors.begin(); i != factors.end(); ++i) {
            if (std::isfinite (*i))
              out.factor = MAX (out.factor, *i);
          }
          break;

        case T_MEDIAN:
          if (factors.empty()) {
            out.factor = 0.0;
          } else {
            nth_element (factors.begin(), factors.begin() + (factors.size() / 2), factors.end());
            out.factor = *(factors.begin() + (factors.size() / 2));
          }
          break;

        case T_MEAN_NONZERO:
          out.factor = 0.0;
          for (std::vector<float>::const_iterator i = factors.begin(); i != factors.end(); ++i) {
            if (std::isfinite (*i) && *i) {
              out.factor += *i;
              ++count;
            }
          }
          out.factor = (count ? (out.factor / float(count)) : 0.0);
          break;

        case GAUSSIAN:
          throw Exception ("Gaussian track-wise statistic temporarily disabled");
/*
          gaussian_smooth_factors (tck);
          out.factor = 0.0;
          for (std::vector<float>::const_iterator i = factors.begin(); i != factors.end(); ++i) {
            if (*i) {
              out.factor = 1.0;
              break;
            }
          }
          break;
*/
        case ENDS_MIN:
          assert (factors.size() == 2);
          out.factor = (Math::abs(factors[0]) < Math::abs(factors[1])) ? factors[0] : factors[1];
          break;

        case ENDS_MEAN:
          assert (factors.size() == 2);
          out.factor = 0.5 * (factors[0] + factors[1]);
          break;

        case ENDS_MAX:
          assert (factors.size() == 2);
          out.factor = (Math::abs(factors[0]) > Math::abs(factors[1])) ? factors[0] : factors[1];
          break;

        case ENDS_PROD:
          assert (factors.size() == 2);
          if ((factors[0] < 0.0 && factors[1] < 0.0) || (factors[0] > 0.0 && factors[1] > 0.0))
            out.factor = factors[0] * factors[1];
          else
            out.factor = 0.0;
          break;

        default:
          throw Exception ("FIXME: Undefined / unsupported track statistic in TrackMapperTWI::get_factor()");

      }
      break;

    default:
      throw Exception ("FIXME: Undefined / unsupported contrast mechanism in TrackMapperTWI::get_factor()");

  }

  if (contrast == SCALAR_MAP_COUNT)
    out.factor = (out.factor ? 1.0 : 0.0);

  if (!std::isfinite (out.factor))
    out.factor = 0.0;

}






void TrackMapperTWI::voxelise (const std::vector< Point<float> >& tck, SetVoxel& voxels) const
{

  if (contrast == ENDPOINT) {

    Voxel vox = round (transform.scanner2voxel (tck.front()));
    if (check (vox, info))
      voxels.insert (vox);

    vox = round (transform.scanner2voxel (tck.back()));
    if (check (vox, info))
      voxels.insert (vox);

  } else {

    TrackMapperBase::voxelise (tck, voxels);

  }

}



void TrackMapperTWI::voxelise (const std::vector< Point<float> >& tck, SetVoxelDEC& voxels) const
{

  if (contrast == ENDPOINT) {

    VoxelDEC vox = round (transform.scanner2voxel (tck.front()));
    if (check (vox, info)) {
      vox.set_dir (tck[0] - tck[1]);
      voxels.insert (vox);
    }

    vox = round (transform.scanner2voxel (tck.back()));
    if (check (vox, info)) {
      vox.set_dir (tck[tck.size() - 1] - tck[tck.size() - 2]);
      voxels.insert (vox);
    }

  } else {

    TrackMapperBase::voxelise (tck, voxels);

  }

}



/*
void TrackMapperTWI::voxelise (const std::vector< Point<float> >& tck, SetVoxelFactor& voxels) const
{

  VoxelFactor vox;
  for (size_t i = 0; i != tck.size(); ++i) {
    vox = round (transform.scanner2voxel (tck[i]));
    if (check (vox, info)) {

      // Get a linearly-interpolated value from factors[] based upon factors[] being
      //   generated with non-interpolated data, and index 'i' here representing interpolated data
      const float ideal_index = float(i) / float(get_upsample_factor());
      const size_t lower_index = MAX(floor (ideal_index), 0);
      const size_t upper_index = MIN(ceil  (ideal_index), tck.size() - 1);
      const float mu = ideal_index - lower_index;
      const float factor = (mu * factors[upper_index]) + ((1.0 - mu) * factors[lower_index]);

      // Change here from base classes: need to explicitly check whether this voxel has been visited
      SetVoxelFactor::iterator v = voxels.find (vox);
      if (v == voxels.end()) {
        vox.set_factor (factor);
        voxels.insert (vox);
      } else {
        vox = *v;
        vox.add_contribution (factor);
        voxels.erase (v);
        voxels.insert (vox);
      }

    }
  }

}



void TrackMapperTWI::voxelise (const std::vector< Point<float> >& tck, SetVoxelDECFactor& voxels) const
{

  Point<float> prev = tck.front();
  const Point<float>& last = tck[tck.size() - 1];

  VoxelDECFactor vox;
  for (unsigned int i = 0; i != tck.size() - 1; ++i) {
    vox = round (transform.scanner2voxel (tck[i]));
    if (check (vox, info)) {

      const float ideal_index = float(i) / float(get_upsample_factor());
      const size_t lower_index = MAX(floor (ideal_index), 0);
      const size_t upper_index = MIN(ceil  (ideal_index), tck.size() - 1);
      const float mu = ideal_index - lower_index;
      const float factor = (mu * factors[upper_index]) + ((1.0 - mu) * factors[lower_index]);

      SetVoxelDECFactor::iterator existing_vox = voxels.find (vox);
      if (existing_vox == voxels.end()) {
        vox.set_factor (factor);
        vox.set_dir (tck[i+1] - prev);
        voxels.insert (vox);
      } else {
        existing_vox->add_dir (tck[i+1] - prev);
        existing_vox->add_contribution (factor);
      }
    }
    prev = tck[i];
  }

  vox = round (transform.scanner2voxel (last));
  if (check (vox, info)) {

    const float ideal_index = float(tck.size() - 1) / float(get_upsample_factor());
    const size_t lower_index = MAX(floor (ideal_index), 0);
    const size_t upper_index = MIN(ceil  (ideal_index), tck.size() - 1);
    const float mu = ideal_index - lower_index;
    const float factor = (mu * factors[upper_index]) + ((1.0 - mu) * factors[lower_index]);

    SetVoxelDECFactor::iterator existing_vox = voxels.find (vox);
    if (existing_vox == voxels.end()) {
      vox.set_dir (last - prev);
      vox.set_factor (factor);
      voxels.insert (vox);
    } else {
      existing_vox->add_dir (last - prev);
      existing_vox->add_contribution (factor);
    }
  }

  for (SetVoxelDECFactor::iterator i = voxels.begin(); i != voxels.end(); ++i)
    i->norm_dir();

}
*/





void TrackMapperTWI::add_scalar_image (const std::string& path)
{
  if (image_plugin)
    throw Exception ("Cannot add more than one associated image to TWI");
  if (contrast != SCALAR_MAP && contrast != SCALAR_MAP_COUNT)
    throw Exception ("Cannot add a scalar image to TWI unless the contrast depends on it");
  image_plugin = new TWIScalarImagePlugin (path, track_statistic);
}

void TrackMapperTWI::add_fod_image (const std::string& path)
{
  if (image_plugin)
    throw Exception ("Cannot add more than one associated image to TWI");
  if (contrast != FOD_AMP)
    throw Exception ("Cannot add an FOD image to TWI unless the FOD_AMP contrast is used");
  image_plugin = new TWIFODImagePlugin (path);
}





void TrackMapperTWI::load_factors (const std::vector< Point<float> >& tck) const
{

  if (contrast == SCALAR_MAP || contrast == SCALAR_MAP_COUNT) {
    assert (image_plugin);
    image_plugin->load_factors (tck, factors);
    return;
  }
  if (contrast == FOD_AMP) {
    assert (image_plugin);
    image_plugin->load_factors (tck, factors);
    return;
  }
  if (contrast != CURVATURE)
    throw Exception ("Unsupported contrast in function TrackMapperTWI::load_factors()");

  std::vector< Point<float> > tangents;
  tangents.reserve (tck.size());

  // Would like to be able to manipulate the length over which the tangent calculation is affected
  // However don't want to just take a pair of distant points and get the tangent that way; would rather
  //   find a way to 'smooth' the curvature in a non-scalar fashion i.e. inverted curvature cancels
  // Ideally would like to get a curvature measurement & azimuth at each point; these can be averaged
  //   using a Gaussian kernel
  // But how to define azimuth & make it consistent between points?
  // Average principal normal vectors using a gaussian kernel, re-determine the curvature

  // Need to know the distance along the spline between every point and every other point
  // Start by logging the length of each step
  std::vector<float> step_sizes;
  step_sizes.reserve (tck.size());

  for (size_t i = 0; i != tck.size(); ++i) {
    Point<float> this_tangent;
    if (i == 0)
      this_tangent = ((tck[1]   - tck[0]  ).normalise());
    else if (i == tck.size() - 1)
      this_tangent = ((tck[i]   - tck[i-1]).normalise());
    else
      this_tangent = ((tck[i+1] - tck[i-1]).normalise());
    if (this_tangent.valid())
      tangents.push_back (this_tangent);
    else
      tangents.push_back (Point<float> (0.0, 0.0, 0.0));
    if (i)
      step_sizes.push_back (dist(tck[i], tck[i-1]));
  }

  // For those tangents that are invalid, fill with valid factors from neighbours
  for (size_t i = 0; i != tangents.size(); ++i) {
    if (tangents[i] == Point<float> (0.0, 0.0, 0.0)) {

      if (i == 0) {
        size_t j;
        for (j = 1; (j < tck.size() - 1) && (tangents[j] != Point<float> (0.0, 0.0, 0.0)); ++j);
        tangents[i] = tangents[j];
      } else if (i == tangents.size() - 1) {
        size_t k;
        for (k = i - 1; k && (tangents[k] != Point<float> (0.0, 0.0, 0.0)); --k);
        tangents[i] = tangents[k];
      } else {
        size_t j, k;
        for (j = 1; (j < tck.size() - 1) && (tangents[j] != Point<float> (0.0, 0.0, 0.0)); ++j);
        for (k = i - 1; k && (tangents[k] != Point<float> (0.0, 0.0, 0.0)); --k);
        tangents[i] = (tangents[j] + tangents[k]).normalise();
      }

    }
  }

  // Produce a matrix of spline distances between points
  Math::Matrix<float> spline_distances (tck.size(), tck.size());
  spline_distances = 0.0f;
  for (size_t i = 0; i != tck.size(); ++i) {
    for (size_t j = 0; j <= i; ++j) {
      for (size_t k = i+1; k != tck.size(); ++k) {
        spline_distances (j, k) += step_sizes[i];
        spline_distances (k, j) += step_sizes[i];
      }
    }
  }

  // Smooth both the tangent vectors and the principal normal vectors according to a Gaussuan kernel
  // Remember: tangent vectors are unit length, but for principal normal vectors length must be preserved!

  std::vector< Point<float> > smoothed_tangents;
  smoothed_tangents.reserve (tangents.size());

  static const float gaussian_theta = CURVATURE_TRACK_SMOOTHING_FWHM / (2.0 * sqrt (2.0 * log (2.0)));
  static const float gaussian_denominator = 2.0 * gaussian_theta * gaussian_theta;

  for (size_t i = 0; i != tck.size(); ++i) {

    Point<float> this_tangent (0.0, 0.0, 0.0);
    std::pair<float, Point<float> > this_normal (std::make_pair (0.0, Point<float>(0.0, 0.0, 0.0)));

    for (size_t j = 0; j != tck.size(); ++j) {
      const float distance = spline_distances (i, j);
      const float this_weight = exp (-distance * distance / gaussian_denominator);
      this_tangent += (tangents[j] * this_weight);
    }

    smoothed_tangents.push_back (this_tangent.normalise());

  }

  for (size_t i = 0; i != tck.size(); ++i) {

    float tangent_dot_product, length;
    if (i == 0) {
      tangent_dot_product = smoothed_tangents[ 1 ].dot (smoothed_tangents[ 0 ]);
      length = spline_distances (0, 1);
    } else if (i == tck.size() - 1) {
      tangent_dot_product = smoothed_tangents[ i ].dot (smoothed_tangents[i-1]);
      length = spline_distances (i, i-1);
    } else {
      tangent_dot_product = smoothed_tangents[i+1].dot (smoothed_tangents[i-1]);
      length = spline_distances (i+1, i-1);
    }

    if (tangent_dot_product >= 1.0f)
      factors.push_back (0.0);
    else
      factors.push_back (Math::acos (tangent_dot_product) / length);

  }

}


/*
void TrackMapperTWI::gaussian_smooth_factors (const std::vector< Point<float> >& tck) const
{

  std::vector<float> unsmoothed (factors);

  for (size_t i = 0; i != unsmoothed.size(); ++i) {

    double sum = 0.0, norm = 0.0;

    if (std::isfinite (unsmoothed[i])) {
      sum  = unsmoothed[i];
      norm = 1.0; // Gaussian is unnormalised -> e^0 = 1
    }

    float distance = 0.0;
    for (size_t j = i; j--; ) { // Decrement AFTER null test, so loop runs with j = 0
      distance += dist(tck[j], tck[j+1]);
      if (std::isfinite (unsmoothed[j])) {
        const float this_weight = exp (-distance * distance / gaussian_denominator);
        norm += this_weight;
        sum  += this_weight * unsmoothed[j];
      }
    }
    distance = 0.0;
    for (size_t j = i + 1; j < unsmoothed.size(); ++j) {
      distance += dist(tck[j], tck[j-1]);
      if (std::isfinite (unsmoothed[j])) {
        const float this_weight = exp (-distance * distance / gaussian_denominator);
        norm += this_weight;
        sum  += this_weight * unsmoothed[j];
      }
    }

    if (norm)
      factors[i] = (sum / norm);
    else
      factors[i] = 0.0;

  }

}
*/






}
}
}
}




