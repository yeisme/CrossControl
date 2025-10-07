#pragma once

#if defined(HAS_OPENCV) && defined(HAS_DLIB)

#include <dlib/dnn.h>

namespace HumanRecognition::detail {

// clang-format off

template <template <int, template <typename> class, int, typename> class block,
          int N, template <typename> class BN, typename SUBNET>
using residual = dlib::add_prev1<block<N, BN, 1, dlib::tag1<SUBNET>>>;

template <template <int, template <typename> class, int, typename> class block,
          int N, template <typename> class BN, typename SUBNET>
using residual_down = dlib::add_prev2<dlib::avg_pool<2, 2, 2, 2,
                                        dlib::skip1<dlib::tag2<block<N, BN, 2, dlib::tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET>
using block = BN<dlib::con<N, 3, 3, stride, stride,
                           dlib::relu<BN<dlib::con<N, 3, 3, 1, 1, SUBNET>>>>>;

template <int N, typename SUBNET>
using ares = dlib::relu<residual<block, N, dlib::affine, SUBNET>>;

template <int N, typename SUBNET>
using ares_down = dlib::relu<residual_down<block, N, dlib::affine, SUBNET>>;

using FaceStem = dlib::max_pool<3, 3, 2, 2,
                                dlib::relu<dlib::affine<dlib::con<64, 7, 7, 2, 2,
                                                                dlib::input_rgb_image_sized<150>>>>>;

using FaceStage1 = ares<64, FaceStem>;
using FaceStage1b = ares<64, FaceStage1>;
using FaceStage1c = ares<64, FaceStage1b>;
using FaceStage2 = ares_down<128, FaceStage1c>;
using FaceStage2b = ares<128, FaceStage2>;
using FaceStage2c = ares<128, FaceStage2b>;
using FaceStage3 = ares_down<256, FaceStage2c>;
using FaceStage3b = ares<256, FaceStage3>;
using FaceStage3c = ares<256, FaceStage3b>;

using FaceNet = dlib::loss_metric<
    dlib::fc_no_bias<128, dlib::avg_pool_everything<FaceStage3c>>>;

// clang-format on

}  // namespace HumanRecognition::detail

#endif  // defined(HAS_OPENCV) && defined(HAS_DLIB)
