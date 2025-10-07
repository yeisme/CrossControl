#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

#include <dlib/dnn.h>
#include <dlib/serialize.h>

#include "src/modules/HumanRecognition/impl/opencv_dlib/internal/backend_network.h"

int main(int argc, char** argv) {
    const std::filesystem::path root = std::filesystem::path(".");
    const std::vector<std::string> candidates = {
        "resources/models/dlib_face_recognition_resnet_model_v1.dat",
        "resources/models/resnet34_stable_imagenet_1k.dat",
        "resources/models/vit-s-16_stable_imagenet_1k.dat",
    };

    for (const auto& rel : candidates) {
        std::filesystem::path p = root / rel;
        std::cout << "Checking: " << p.string() << " ... ";
        if (!std::filesystem::exists(p)) { std::cout << "NOT FOUND\n"; continue; }
        try {
            HumanRecognition::detail::FaceNet net;
            dlib::deserialize(p.string()) >> net;
            std::cout << "OK: loaded as FaceNet\n";
        } catch (const std::exception& ex) {
            std::cout << "ERROR: " << ex.what() << "\n";
        }
    }
    return 0;
}
