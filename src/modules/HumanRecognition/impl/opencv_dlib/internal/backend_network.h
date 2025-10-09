#pragma once

#if defined(HAS_OPENCV) && defined(HAS_DLIB)

#include <dlib/dnn.h>

namespace HumanRecognition::detail {

// clang-format off

/**
 * 官方人脸识别网络 anet_type（来自 dlib `dnn_face_recognition_ex.cpp`）。
 * 输入：对齐后的 150x150 RGB 人脸图；输出：128 维度的人脸特征。
 * 结构包含五个残差阶段（含降采样）和最终的全局平均池化 + 全连接层，
 * 可直接加载 `dlib_face_recognition_resnet_model_v1.dat` 预训练权重。
 */

// 标准残差单元，保持空间尺寸不变：F(x) + x。
template <template <int, template <typename> class, int, typename> class block,
          int N, template <typename> class BN, typename SUBNET>
using residual = dlib::add_prev1<block<N, BN, 1, dlib::tag1<SUBNET>>>;

// 带降采样的残差单元：2x2 平均池化作为 skip，卷积分支 stride=2。
template <template <int, template <typename> class, int, typename> class block,
          int N, template <typename> class BN, typename SUBNET>
using residual_down = dlib::add_prev2<dlib::avg_pool<2, 2, 2, 2,
                                        dlib::skip1<dlib::tag2<block<N, BN, 2, dlib::tag1<SUBNET>>>>>>;

// 残差分支的主体：两层 3x3 卷积（第二层 stride 可为 1 或 2），卷积后接 BN+ReLU。
template <int N, template <typename> class BN, int stride, typename SUBNET>
using block = BN<dlib::con<N, 3, 3, 1, 1,
                           dlib::relu<BN<dlib::con<N, 3, 3, stride, stride, SUBNET>>>>>;

// 使用 affine 归一化的残差单元，维持分辨率。
template <int N, typename SUBNET>
using ares = dlib::relu<residual<block, N, dlib::affine, SUBNET>>;

// 负责降采样的残差单元，同时将通道数提升至 N。
template <int N, typename SUBNET>
using ares_down = dlib::relu<residual_down<block, N, dlib::affine, SUBNET>>;

// Level0：输出 256 通道，空间尺寸减半，对应官方网络的最顶层。
template <typename SUBNET>
using FaceLevel0 = ares_down<256, SUBNET>;

// Level1：先降采样，再堆叠两个 256 通道残差单元。
template <typename SUBNET>
using FaceLevel1 = ares<256, ares<256, ares_down<256, SUBNET>>>;

// Level2：通道数降为 128，继续加深表达能力。
template <typename SUBNET>
using FaceLevel2 = ares<128, ares<128, ares_down<128, SUBNET>>>;

// Level3：包含一次降采样与两个 64 通道残差单元。
template <typename SUBNET>
using FaceLevel3 = ares<64, ares<64, ares<64, ares_down<64, SUBNET>>>>;

// Level4：网络入口，保持 32 通道并维持空间尺寸。
template <typename SUBNET>
using FaceLevel4 = ares<32, ares<32, ares<32, SUBNET>>>;

// FaceNet：Stem (7x7 conv + max_pool) -> Level4~0 -> 全局平均池化 -> 128 维嵌入，使用 metric loss。
using FaceNet = dlib::loss_metric<dlib::fc_no_bias<128, dlib::avg_pool_everything<
    FaceLevel0<FaceLevel1<FaceLevel2<FaceLevel3<FaceLevel4<
        dlib::max_pool<3, 3, 2, 2,
                       dlib::relu<dlib::affine<dlib::con<32, 7, 7, 2, 2,
                                                       dlib::input_rgb_image_sized<150>>>>>>>>>>>>>;

// clang-format on

}  // namespace HumanRecognition::detail

#endif  // defined(HAS_OPENCV) && defined(HAS_DLIB)
