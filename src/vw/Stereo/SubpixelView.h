#ifndef __VW_STEREO_AFFINE_SUBPIXEL_VIEW__
#define __VW_STEREO_AFFINE_SUBPIXEL_VIEW__

#include <vw/Image/ImageView.h>
#include <vw/Image/ImageViewRef.h>
#include <vw/Stereo/DisparityMap.h>
#include <vw/Stereo/Correlate.h>


namespace vw {
namespace stereo {

  /// An image view for performing image correlation
  template <class PreprocFilterT>
  class SubpixelView : public ImageViewBase<SubpixelView<PreprocFilterT> > {

    ImageViewRef<PixelDisparity<float> > m_disparity_map;
    ImageViewRef<float> m_left_image; 
    ImageViewRef<float> m_right_image;

    // General Settings
    int m_kern_width, m_kern_height;
    bool m_do_h_subpixel, m_do_v_subpixel;
    //bool m_do_affine_subpixel;
    int m_do_affine_subpixel;
    PreprocFilterT m_preproc_filter;
    bool m_verbose;

  public:
      typedef PixelDisparity<float> pixel_type;
      typedef pixel_type result_type;
      typedef ProceduralPixelAccessor<SubpixelView> pixel_accessor;
        
    template <class DisparityViewT, class InputViewT>
    SubpixelView(DisparityViewT const& disparity_map,
                 InputViewT const& left_image,
                 InputViewT const& right_image,
                 int kern_width, int kern_height,
                 bool do_horizontal_subpixel,
                 bool do_vertical_subpixel,
                 //bool do_affine_subpixel,
                 int do_affine_subpixel,
                 PreprocFilterT preproc_filter,
                 bool verbose) : m_disparity_map(disparity_map),
                                 m_left_image(left_image),
                                 m_right_image(right_image),
                                 m_kern_width(kern_width), m_kern_height(kern_height),
                                 m_do_h_subpixel(do_horizontal_subpixel),
                                 m_do_v_subpixel(do_vertical_subpixel),
                                 m_do_affine_subpixel(do_affine_subpixel),
                                 m_preproc_filter(preproc_filter),
                                 m_verbose(verbose) {
      // Basic assertions
      VW_ASSERT((left_image.impl().cols() == right_image.impl().cols()) &&
                (left_image.impl().rows() == right_image.impl().rows()) &&
                (disparity_map.impl().cols() == right_image.impl().cols()) &&
                (disparity_map.impl().cols() == right_image.impl().cols()),
                ArgumentErr() << "SubpixelView::SubpixelView(): input image dimensions and/or disparity_map dimensions do not agree.\n");
      
      VW_ASSERT((left_image.channels() == 1) && (left_image.impl().planes() == 1) &&
                (right_image.channels() == 1) && (right_image.impl().planes() == 1),
                ArgumentErr() << "SubpixelView::SubpixelView(): multi-channel, multi-plane images not supported.\n");
    }

    // Standard ImageView interface methods
    inline int32 cols() const { return m_left_image.cols(); }
    inline int32 rows() const { return m_left_image.rows(); }
    inline int32 planes() const { return 1; }
    
    inline pixel_accessor origin() const { return pixel_accessor( *this, 0, 0 ); }
    
    inline pixel_type operator()(float x, float y, int32 p = 0) const {
      vw_throw(NoImplErr() << "SubpixelView::operator() is not yet implemented.");
      return PixelDisparity<float>(); // Never reached
    }

    /// \cond INTERNAL
    typedef CropView<ImageView<pixel_type> > prerasterize_type;
    inline prerasterize_type prerasterize(BBox2i bbox) const { 

      // Find the range of disparity values for this patch.
      int num_good;
      BBox2i search_range = disparity::get_disparity_range(crop(m_disparity_map, bbox),num_good,false);

      // The area in the right image that we'll be searching is
      // determined by the bbox of the left image plus the search
      // range.
      BBox2i left_crop_bbox(bbox);
      BBox2i right_crop_bbox(bbox.min() + search_range.min(),
                             bbox.max() + search_range.max());
      
      // The correlator requires the images to be the same size. The
      // search bbox will always be larger than the given left image
      // bbox, so we just make the left bbox the same size as the
      // right bbox.
      left_crop_bbox.max() = left_crop_bbox.min() + Vector2i(right_crop_bbox.width(), right_crop_bbox.height());

      // Finally, we must adjust both bounding boxes to account for
      // the size of the kernel itself.
      right_crop_bbox.min() -= Vector2i(m_kern_width, m_kern_height);
      right_crop_bbox.max() += Vector2i(m_kern_width, m_kern_height);
      left_crop_bbox.min() -= Vector2i(m_kern_width, m_kern_height);
      left_crop_bbox.max() += Vector2i(m_kern_width, m_kern_height);

      // We crop the images to the expanded bounding box and edge
      // extend in case the new bbox extends past the image bounds.
      ImageView<float> left_image_patch, right_image_patch;
      if (m_do_affine_subpixel > 0) { // affine subpixel does its own pre-processing
        left_image_patch = crop(edge_extend(m_left_image,ZeroEdgeExtension()), left_crop_bbox);
        right_image_patch = crop(edge_extend(m_right_image,ZeroEdgeExtension()), right_crop_bbox);
      } else { // parabola subpixel does the same preprocessing as the pyramid correlator
        left_image_patch = m_preproc_filter(crop(edge_extend(m_left_image,ZeroEdgeExtension()), left_crop_bbox));
        right_image_patch = m_preproc_filter(crop(edge_extend(m_right_image,ZeroEdgeExtension()), right_crop_bbox));
      }
      ImageView<PixelDisparity<float> > disparity_map_patch = crop(edge_extend(m_disparity_map, ZeroEdgeExtension()), left_crop_bbox);

      // Adjust the disparities to be relative to the cropped
      // image pixel locations
      for (int v = 0; v < disparity_map_patch.rows(); ++v)
        for (int u = 0; u < disparity_map_patch.cols(); ++u)
          if (!disparity_map_patch(u,v).missing())  {
            disparity_map_patch(u,v).h() -= search_range.min().x();
            disparity_map_patch(u,v).v() -= search_range.min().y();
          }

      //       std::ostringstream ostr;
      //       ostr << "__" << bbox.min().x() << "_" << bbox.min().y() << ".tif";
      //       write_image("left"+ostr.str(), left_image_patch);
      //       write_image("right"+ostr.str(), right_image_patch);
      
      switch (m_do_affine_subpixel){
      case 0 : // Parabola Subpixel
        subpixel_correlation_parabola(disparity_map_patch,
                                      left_image_patch,
                                      right_image_patch,
                                      m_kern_width, m_kern_height,
                                      m_do_h_subpixel, m_do_v_subpixel,
                                      m_verbose);
        break;
      case 1: // Robust Subpixel
        subpixel_correlation_affine_2d(disparity_map_patch,
                                       left_image_patch,
                                       right_image_patch,
                                       m_kern_width, m_kern_height,
                                       m_do_h_subpixel, m_do_v_subpixel,
                                       m_verbose);
        break;
      case 2: // Bayes Subpixel
        subpixel_correlation_affine_2d_bayesian(disparity_map_patch,
                                                left_image_patch,
                                                right_image_patch,
                                                m_kern_width, m_kern_height,
                                                m_do_h_subpixel, m_do_v_subpixel,
                                                m_verbose);
        break;
      case 3: // Bayes EM  Subpixel
        subpixel_correlation_affine_2d_EM(disparity_map_patch,
                                          left_image_patch,
                                          right_image_patch,
                                          m_kern_width, m_kern_height,
                                          m_do_h_subpixel, m_do_v_subpixel,
                                          m_verbose);
        break;
      default:
        vw_throw(ArgumentErr() << "Unknown subpixel correlation type: " << m_do_affine_subpixel << ".");
        break;
      }
      
      // Undo the above adjustment
      for (int v = 0; v < disparity_map_patch.rows(); ++v)
        for (int u = 0; u < disparity_map_patch.cols(); ++u)
          if (!disparity_map_patch(u,v).missing())  {
            disparity_map_patch(u,v).h() += search_range.min().x();
            disparity_map_patch(u,v).v() += search_range.min().y();
          }

      // This may seem confusing, but we must crop here so that the
      // good pixel data is placed into the coordinates specified by
      // the bbox.  This allows rasterize to touch those pixels
      // using the coordinates inside the bbox.  The pixels outside
      // those coordinates are invalid, and they never get accessed.
      return crop(disparity_map_patch, BBox2i(m_kern_width-bbox.min().x(), 
                                              m_kern_height-bbox.min().y(), 
                                              m_left_image.cols(), 
                                              m_left_image.rows() ));
    }
    
    template <class DestT> inline void rasterize(DestT const& dest, BBox2i bbox) const {
      vw::rasterize(prerasterize(bbox), dest, bbox);
    }
    /// \endcond

  };

}} // namespace vw::stereo

#endif // __VW_STEREO_CORRELATOR_VIEW__         
