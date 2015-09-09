/*
   Copyright 2009 Brain Research Institute, Melbourne, Australia

   Written by J-Donald Tournier, 02/11/09.

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

#ifndef __adapter_extract_h__
#define __adapter_extract_h__

#include "adapter/base.h"

namespace MR
{
  namespace Adapter
  {

    template <class ImageType> class Extract1D : public Base<ImageType>
    {
      public:
        using Base<ImageType>::ndim;
        using Base<ImageType>::spacing;
        using Base<ImageType>::parent;
        typedef typename ImageType::value_type value_type;

        Extract1D (const ImageType& original, const size_t axis, const std::vector<int>& indices) :
          Base<ImageType> (original),
          extract_axis (axis),
          indices (indices),
          trans (original.transform()) {
            reset();

            if (extract_axis < 3) {
              Eigen::Vector3d a (0.0, 0.0, 0.0);
              a[extract_axis] = indices[0] * spacing (extract_axis);
              trans.translation() = trans * a;
            }
          }

        void reset () {
          for (size_t n = 0; n < ndim(); ++n) 
            parent().index(n) = ( n == extract_axis ? indices[0] : 0 );
          current_pos = 0;
        }

        ssize_t size (size_t axis) const {
          return ( axis == extract_axis ? indices.size() : Base<ImageType>::size (axis) );
        }

        const transform_type& transform () const { return trans; } 

        ssize_t index (size_t axis) const { return ( axis == extract_axis ? current_pos : parent().index(axis) ); }
        auto index (size_t axis) -> decltype(Helper::index(*this, axis)) { return { *this, axis }; } 
        void move_index (size_t axis, ssize_t increment) {
          if (axis == extract_axis) {
            ssize_t prev_pos = current_pos < ssize_t (indices.size()) ? indices[current_pos] : 0;
            current_pos += increment;
            if (current_pos < ssize_t (indices.size())) 
              parent().index(axis) += indices[current_pos] - prev_pos;
            else 
              parent().index(axis) = 0;
          }
          else 
            parent().index(axis) += increment;
        }


        friend std::ostream& operator<< (std::ostream& stream, const Extract1D& V) {
          stream << "Extract1D adapter for image \"" << V.name() << "\", position [ ";
          for (size_t n = 0; n < V.ndim(); ++n) 
            stream << V.index(n) << " ";
          stream << "], value = " << V.value();
          return stream;
        }

      private:
        const size_t extract_axis;
        const std::vector<int> indices;
        transform_type trans;
        ssize_t current_pos;

    };










    template <class ImageType> class Extract : public Base<ImageType>
    {
      public:
        using Base<ImageType>::ndim;
        using Base<ImageType>::spacing;
        using Base<ImageType>::parent;
        typedef typename ImageType::value_type value_type;

        Extract (const ImageType& original, const std::vector<std::vector<int>>& indices) :
          Base<ImageType> (original),
          current_pos (ndim()), 
          indices (indices),
          trans (original.transform()) {
            reset();
            trans.translation() = trans * Eigen::Vector3d (
                indices[0][0] * spacing (0), 
                indices[1][0] * spacing (1), 
                indices[2][0] * spacing (2) 
                );
          }

        ssize_t size (size_t axis) const { return indices[axis].size(); }

        const transform_type& transform () const { return trans; }

        void reset () {
          for (size_t n = 0; n < ndim(); ++n) {
            current_pos[n] = 0;
            parent().index(n) = indices[n][0];
          }
        }

        ssize_t index (size_t axis) const { return current_pos[axis]; }
        auto index (size_t axis) -> decltype(Helper::index(*this, axis)) { return { *this, axis }; }
        void move_index (size_t axis, ssize_t increment) {
          ssize_t prev = current_pos[axis];
          current_pos[axis] += increment;
          parent().index (axis) += indices[axis][current_pos[axis]] - indices[axis][prev];
        }

      private:
        std::vector<size_t> current_pos;
        const std::vector<std::vector<int> > indices;
        transform_type trans;
    };

  }
}

#endif


