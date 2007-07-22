// __BEGIN_LICENSE__
// 
// Copyright (C) 2006 United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration
// (NASA).  All Rights Reserved.
// 
// Copyright 2006 Carnegie Mellon University. All rights reserved.
// 
// This software is distributed under the NASA Open Source Agreement
// (NOSA), version 1.3.  The NOSA has been approved by the Open Source
// Initiative.  See the file COPYING at the top of the distribution
// directory tree for the complete NOSA document.
// 
// THE SUBJECT SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF ANY
// KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT
// LIMITED TO, ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL CONFORM TO
// SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
// A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT, ANY WARRANTY THAT
// THE SUBJECT SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY THAT
// DOCUMENTATION, IF PROVIDED, WILL CONFORM TO THE SUBJECT SOFTWARE.
// 
// __END_LICENSE__

/// \file TMSQuadTreeGenerator.h
/// 
/// A subclass of ImageQuadTreeGenerator that generates multi-resolution 
/// TMS overlays.
/// 
#ifndef __VW_MOSAIC_TMSQUADTREEGENERATOR_H__
#define __VW_MOSAIC_TMSQUADTREEGENERATOR_H__

#include <iomanip>
#include <vw/Mosaic/QuadTreeGenerator.h>

namespace vw {
namespace mosaic {

  template <class PixelT>
  class TMSQuadTreeGenerator : public vw::mosaic::ImageQuadTreeGenerator<PixelT>
  {
    typedef vw::mosaic::ImageQuadTreeGenerator<PixelT> base_type;

  protected:
    BBox2 total_longlat_bbox;
    int max_lod_pixels;
    int draw_order_offset;
    std::string kml_title;
    mutable std::ostringstream root_node_tags;

  public:
    // Constructor, templatized on the source image type.  The supplied 
    // bounding box sets the size in degrees of the TMS overlay, and has 
    // nothing in particular to do with the source image or pixels.
    template <class ImageT>
    TMSQuadTreeGenerator( std::string const& tree_name,
                          ImageViewBase<ImageT> const& source,
                          BBox2 total_longlat_bbox )
      : vw::mosaic::ImageQuadTreeGenerator<PixelT>( tree_name, source ),
        total_longlat_bbox( total_longlat_bbox )
    {
      base_type::set_crop_images( false );
    }
    
    virtual ~TMSQuadTreeGenerator() {}
    
    virtual std::string compute_image_path( std::string const& name ) const {
      fs::path path( base_type::m_base_dir, fs::native );

      if( name.length() == 1 ) {
        path /= change_extension( path, "" ).leaf();
      }
      else {
        Vector2i pos(0,0);
        for ( int i=1; i<(int)name.length(); ++i ) {
          pos *= 2;
          if( name[i]=='0' ) pos += Vector2i(0,1);
          else if( name[i]=='1' ) pos += Vector2i(1,1);
          else if( name[i]=='3' ) pos += Vector2i(1,0);
        }
        std::ostringstream oss;
        oss << name.length()-2 << "/" << pos.x() << "/" << pos.y();
        path /= oss.str();
      }

      return path.native_file_string();
    }

    virtual void write_meta_file( typename base_type::PatchInfo const& info ) const
    {
    }

  };


  /// A transform functor that relates unprojected lon/lat 
  /// coordinates in degrees to an unprojected pixel space 
  /// cooresponding to a standard global TMS image quad-tree.
  class GlobalTMSTransform : public TransformHelper<GlobalTMSTransform,ConvexFunction,ConvexFunction>
  {
    int resolution;
  public:
    GlobalTMSTransform( int resolution ) : resolution(resolution) {}
    
    // Convert degrees lat/lon to pixel location
    inline Vector2 forward( Vector2 const& p ) const {
      return resolution*Vector2( p.x()+180.0, 270.0-p.y() )/360.0 - Vector2(0.5,0.5);
    }
    
    // Convert pixel location to degrees lat/lon
    inline Vector2 reverse( Vector2 const& p ) const {
      return Vector2( 360.0*(p.x()+0.5)/resolution-180.0, 270.0-360.0*(p.y()+0.5)/resolution );
    }

    template <class TransformT>
    static inline int compute_resolution( TransformT const& tx, Vector2 const& pixel ) {
      Vector2 pos = tx.forward( pixel );
      Vector2 x_vector = tx.forward( pixel+Vector2(1,0) ) - pos;
      Vector2 y_vector = tx.forward( pixel+Vector2(0,1) ) - pos;
      double degrees_per_pixel = (std::min)( norm_2(x_vector), norm_2(y_vector) );
      double pixels_per_circumference = 360.0 / degrees_per_pixel;
      int scale_exponent = (int) ceil( log(pixels_per_circumference)/log(2) );
      int resolution = 1 << scale_exponent;
      return resolution;
    }
  };

} // namespace mosaic
} // namespace vw

#endif // __VW_MOSAIC_TMSQUADTREEGENERATOR_H__
