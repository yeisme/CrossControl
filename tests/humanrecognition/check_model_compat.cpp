#include <dlib/dnn.h>
#include <dlib/revision.h>
#include <dlib/serialize.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<png.h>)
#include <png.h>
#define HAVE_PNG_H 1
#endif
#if __has_include(<jpeglib.h>)
#include <jpeglib.h>
#define HAVE_JPEG_H 1
#endif
#if __has_include(<libavcodec/version.h>)
#include <libavcodec/version.h>
#define HAVE_FFMPEG_VERSION_H 1
#endif
#else
// fallback: nothing
#endif

#include "src/modules/HumanRecognition/impl/opencv_dlib/internal/backend_network.h"

static void print_dlib_info() {
#ifdef DLIB_MAJOR_VERSION
    std::cout << "dlib version: " << DLIB_MAJOR_VERSION << "." << DLIB_MINOR_VERSION;
#ifdef DLIB_PATCH_VERSION
    std::cout << "." << DLIB_PATCH_VERSION;
#endif
#ifdef DLIB_VERSION
    std::cout << " (" << DLIB_VERSION << ")";
#endif
    std::cout << "\n";
#else
    std::cout << "dlib version: (unknown - revision header not available)\n";
#endif
#ifdef DLIB_USE_CUDA
    std::cout << "dlib built with CUDA support\n";
    try {
        int n = dlib::cuda::get_num_devices();
        std::cout << "  CUDA devices: " << n << "\n";
    } catch (...) { std::cout << "  CUDA device query failed\n"; }
#else
    std::cout << "dlib built WITHOUT CUDA support\n";
#endif

    // dlib feature macros
#ifdef DLIB_USE_FFMPEG
    std::cout << "dlib: FFmpeg support: ON\n";
#else
    std::cout << "dlib: FFmpeg support: OFF\n";
#endif
#ifdef DLIB_PNG_SUPPORT
    std::cout << "dlib: PNG support: ON\n";
#else
    std::cout << "dlib: PNG support: OFF\n";
#endif
#ifdef DLIB_JPEG_SUPPORT
    std::cout << "dlib: JPEG support: ON\n";
#else
    std::cout << "dlib: JPEG support: OFF\n";
#endif
#ifdef DLIB_USE_BLAS
    std::cout << "dlib: BLAS support: ON\n";
#else
    std::cout << "dlib: BLAS support: OFF\n";
#endif
#ifdef DLIB_USE_LAPACK
    std::cout << "dlib: LAPACK support: ON\n";
#else
    std::cout << "dlib: LAPACK support: OFF\n";
#endif

    // Try to print header/library versions for supported libs when headers are available
#ifdef HAVE_PNG_H
    std::cout << "libpng header: PNG_LIBPNG_VER_STRING=" << PNG_LIBPNG_VER_STRING << "\n";
#endif
#ifdef HAVE_JPEG_H
    std::cout << "libjpeg header: JPEG_LIB_VERSION=" << JPEG_LIB_VERSION << "\n";
#endif
#ifdef HAVE_FFMPEG_VERSION_H
    // LIBAVCODEC_VERSION_MAJOR is available in newer FFmpeg headers
#ifdef LIBAVCODEC_VERSION_MAJOR
    std::cout << "libavcodec version major=" << LIBAVCODEC_VERSION_MAJOR << "\n";
#endif
#endif

    // Environment / runtime hints
    const char* cuda_path = std::getenv("CUDA_PATH");
    if (cuda_path) {
        std::cout << "CUDA_PATH=" << cuda_path << "\n";
    } else {
        std::cout << "CUDA_PATH not set\n";
    }
}

int main(int argc, char** argv) {
    std::filesystem::path modelPath;
    if (argc >= 2) {
        modelPath = std::filesystem::path(argv[1]);
    } else {
        // fallback candidates
        const std::filesystem::path root = std::filesystem::path(".");
        const std::vector<std::string> candidates = {
            "resources/models/taguchi_face_recognition_resnet_model_v1.dat",
        };
        for (const auto& rel : candidates) {
            std::filesystem::path p = root / rel;
            if (std::filesystem::exists(p)) {
                modelPath = p;
                break;
            }
        }
    }

    print_dlib_info();

    if (modelPath.empty()) {
        std::cerr << "No model path provided and no candidate models found in resources/models/.\n";
        std::cerr << "Usage: CheckModelCompat.exe <path-to-model.dat>\n";
        return 2;
    }

    std::cout << "Checking model: " << modelPath.string() << "\n";
    if (!std::filesystem::exists(modelPath)) {
        std::cerr << "Model file does not exist: " << modelPath.string() << "\n";
        return 3;
    }

    try {
        HumanRecognition::detail::FaceNet net;
        dlib::deserialize(modelPath.string()) >> net;
        std::cout << "OK: loaded as FaceNet\n";
        // Try a small forward pass with a dummy input to validate shape/IO
        dlib::matrix<dlib::rgb_pixel> img(150, 150);
        for (long r = 0; r < img.nr(); ++r)
            for (long c = 0; c < img.nc(); ++c) img(r, c) = dlib::rgb_pixel(128, 128, 128);
        auto descriptor = net(img);
        std::cout << "Forward OK: descriptor size=" << descriptor.size() << "\n";
    } catch (const std::exception& ex) {
        std::cout << "ERROR loading/using model: " << ex.what() << "\n";
        return 4;
    }

    return 0;
}
