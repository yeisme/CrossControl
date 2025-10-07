#include <dlib/image_io.h>
#include <dlib/image_transforms.h>
#include <gtest/gtest.h>

#include <QJsonObject>
#include <QString>
#include <filesystem>
#include <optional>
#include <string>

#include "modules/HumanRecognition/types.h"
#include "src/modules/HumanRecognition/impl/opencv_dlib/internal/backend_impl.h"

#ifndef PROJECT_SOURCE_DIR
#define PROJECT_SOURCE_DIR "."
#endif

namespace HumanRecognition::test {

class ImplAccessor {
   public:
    static QString sanitize(const QString& input) {
        ::HumanRecognition::OpenCVDlibBackend::Impl impl;
        return impl.sanitizeTableName(input);
    }

    static QString serializeMetadata(const QJsonObject& obj) {
        ::HumanRecognition::OpenCVDlibBackend::Impl impl;
        return impl.serializeMetadata(obj);
    }

    static QJsonObject deserializeMetadata(const QString& json) {
        ::HumanRecognition::OpenCVDlibBackend::Impl impl;
        return impl.deserializeMetadata(json);
    }

    static QString serializeFeature(const FaceFeature& feature) {
        ::HumanRecognition::OpenCVDlibBackend::Impl impl;
        return impl.serializeFeature(feature);
    }

    static std::optional<FaceFeature> deserializeFeature(const QString& json) {
        ::HumanRecognition::OpenCVDlibBackend::Impl impl;
        return impl.deserializeFeature(json);
    }

    static std::optional<float> computeDistance(const FaceFeature& a, const FaceFeature& b) {
        ::HumanRecognition::OpenCVDlibBackend::Impl impl;
        return impl.computeDistance(a, b);
    }
};

}  // namespace HumanRecognition::test

namespace HumanRecognition::tests {

namespace {
bool computeFeatureFromResources(FaceFeature& outFeature, std::string& outError) {
    const std::filesystem::path root = std::filesystem::path(PROJECT_SOURCE_DIR);
    const std::filesystem::path imagePath = root / "tests/humanrecognition/test1.jpg";
    const std::filesystem::path modelPath =
        root / "resources/models/dlib_face_recognition_resnet_model_v1.dat";

    if (!std::filesystem::exists(imagePath)) {
        outError = "Test image not found at " + imagePath.string();
        return false;
    }
    if (!std::filesystem::exists(modelPath)) {
        outError = "Recognition model not found at " + modelPath.string();
        return false;
    }

    try {
        dlib::matrix<dlib::rgb_pixel> image;
        dlib::load_image(image, imagePath.string());

        dlib::matrix<dlib::rgb_pixel> faceChip;
        faceChip.set_size(150, 150);
        dlib::resize_image(image, faceChip);

        HumanRecognition::detail::FaceNet net;
        dlib::deserialize(modelPath.string()) >> net;

        const dlib::matrix<float, 0, 1> descriptor = net(faceChip);

        outFeature.values.resize(descriptor.size());
        for (long i = 0; i < descriptor.size(); ++i) { outFeature.values[i] = descriptor(i); }
        outFeature.version = QStringLiteral("dlib_resnet_128");
        outFeature.norm = static_cast<float>(dlib::length(descriptor));
        return true;
    } catch (const std::exception& ex) {
        outError = ex.what();
        return false;
    }
}

}  // namespace

// 测试：`sanitizeTableName` 会过滤掉非法字符（只保留字母、数字和下划线）
// 场景：输入包含连字符、井号、感叹号等特殊字符
// 断言：返回的表名中应移除这些字符，仅保留合法字符
TEST(OpenCVDlibBackendImplTest, SanitizeTableNameFiltersDisallowedCharacters) {
    const QString raw = QStringLiteral("person-table#2024!");
    const QString sanitized = test::ImplAccessor::sanitize(raw);
    EXPECT_EQ(QStringLiteral("persontable2024"), sanitized);
}

// 测试：元数据序列化与反序列化的完整性
// 场景：包含简单字段与嵌套对象的 JSON 元数据
// 断言：序列化后再反序列化应当保留原始字段和值
TEST(OpenCVDlibBackendImplTest, SerializeMetadataRoundTripPreservesValues) {
    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), QStringLiteral("Alice"));
    metadata.insert(QStringLiteral("age"), 30);

    QJsonObject nested;
    nested.insert(QStringLiteral("enabled"), true);
    nested.insert(QStringLiteral("threshold"), 0.85);
    metadata.insert(QStringLiteral("settings"), nested);

    const QString json = test::ImplAccessor::serializeMetadata(metadata);
    const QJsonObject restored = test::ImplAccessor::deserializeMetadata(json);

    EXPECT_EQ(metadata.value(QStringLiteral("name")), restored.value(QStringLiteral("name")));
    EXPECT_EQ(metadata.value(QStringLiteral("age")), restored.value(QStringLiteral("age")));

    const QJsonObject restoredNested = restored.value(QStringLiteral("settings")).toObject();
    EXPECT_EQ(nested.value(QStringLiteral("enabled")),
              restoredNested.value(QStringLiteral("enabled")));
    EXPECT_DOUBLE_EQ(nested.value(QStringLiteral("threshold")).toDouble(),
                     restoredNested.value(QStringLiteral("threshold")).toDouble());
}

// 测试：特征向量的序列化/反序列化保持数值与元信息一致
// 场景：构造一个带 version 与 norm 的特征向量
// 断言：反序列化后数值数组、版本字符串与范数应与原始一致
TEST(OpenCVDlibBackendImplTest, SerializeFeatureRoundTripPreservesData) {
    FaceFeature feature;
    feature.values = {0.1f, 0.2f, 0.3f};
    feature.version = QStringLiteral("test_version");
    feature.norm = 0.42f;

    const QString json = test::ImplAccessor::serializeFeature(feature);
    const auto restored = test::ImplAccessor::deserializeFeature(json);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(feature.values.size(), restored->values.size());
    ASSERT_EQ(feature.values.size(), restored->values.size());
    for (int i = 0; i < feature.values.size(); ++i) {
        EXPECT_FLOAT_EQ(feature.values[i], restored->values[i]);
    }
    EXPECT_EQ(feature.version, restored->version);
    ASSERT_TRUE(restored->norm.has_value());
    EXPECT_FLOAT_EQ(feature.norm.value(), restored->norm.value());
}

// 测试：计算两个特征向量间的欧氏距离
// 场景：一个零向量和一个在第一维为 1 的向量
// 断言：距离应为 1.0（标准欧氏距离）
TEST(OpenCVDlibBackendImplTest, ComputeDistanceReturnsEuclideanLength) {
    FaceFeature a;
    a.values = {0.0f, 0.0f, 0.0f};

    FaceFeature b;
    b.values = {1.0f, 0.0f, 0.0f};

    const auto distance = test::ImplAccessor::computeDistance(a, b);
    ASSERT_TRUE(distance.has_value());
    EXPECT_FLOAT_EQ(1.0f, distance.value());
}

// 测试：当两向量长度不一致时返回空（无法比较）
// 场景：一条包含两个元素，另一条只包含一个元素
// 断言：computeDistance 应返回 std::nullopt 表示不可比较
TEST(OpenCVDlibBackendImplTest, ComputeDistanceReturnsNulloptWhenSizesDiffer) {
    FaceFeature a;
    a.values = {0.0f, 1.0f};

    FaceFeature b;
    b.values = {0.0f};

    const auto distance = test::ImplAccessor::computeDistance(a, b);
    EXPECT_FALSE(distance.has_value());
}

// 测试：当任意一个特征为空时，距离计算应返回空（无效输入）
// 场景：两个空的 Feature 结构
// 断言：computeDistance 返回 std::nullopt
TEST(OpenCVDlibBackendImplTest, ComputeDistanceReturnsNulloptWhenEmpty) {
    FaceFeature a;
    FaceFeature b;

    const auto distance = test::ImplAccessor::computeDistance(a, b);
    EXPECT_FALSE(distance.has_value());
}

// 测试：真实模型产出的特征向量能正确序列化/反序列化
// 场景：使用资源目录中的 dlib 模型和 test1.jpg 生成 128 维描述子
// 断言：serializeFeature / deserializeFeature 处理后保持长度和值一致
TEST(OpenCVDlibBackendImplTest, SerializeFeatureRoundTripWithRealModelOutput) {
    FaceFeature feature;
    std::string error;
    if (!computeFeatureFromResources(feature, error)) {
        GTEST_SKIP() << "Skipping test: " << error;
    }

    EXPECT_EQ(128, feature.values.size());
    ASSERT_TRUE(feature.norm.has_value());
    EXPECT_GT(feature.norm.value(), 0.0f);

    const QString json = test::ImplAccessor::serializeFeature(feature);
    const auto restored = test::ImplAccessor::deserializeFeature(json);

    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(feature.values.size(), restored->values.size());
    for (int i = 0; i < feature.values.size(); ++i) {
        EXPECT_NEAR(feature.values[i], restored->values[i], 1e-6f);
    }
    EXPECT_EQ(feature.version, restored->version);
    ASSERT_TRUE(restored->norm.has_value());
    EXPECT_NEAR(feature.norm.value(), restored->norm.value(), 1e-6f);
}

// 测试：真实特征向量与自身距离为 0，与轻度扰动后的向量存在明显差异
// 场景：使用 test1.jpg 生成特征并稍微修改其中一维的数值
// 断言：自比较得到 0 距离，扰动后的距离明显大于 0
TEST(OpenCVDlibBackendImplTest, ComputeDistanceWithRealModelOutput) {
    FaceFeature feature;
    std::string error;
    if (!computeFeatureFromResources(feature, error)) {
        GTEST_SKIP() << "Skipping test: " << error;
    }

    const auto selfDistance = test::ImplAccessor::computeDistance(feature, feature);
    ASSERT_TRUE(selfDistance.has_value());
    EXPECT_NEAR(0.0f, selfDistance.value(), 1e-6f);

    FaceFeature tweaked = feature;
    if (!tweaked.values.isEmpty()) { tweaked.values[0] += 0.1f; }

    const auto diffDistance = test::ImplAccessor::computeDistance(feature, tweaked);
    ASSERT_TRUE(diffDistance.has_value());
    EXPECT_GT(diffDistance.value(), 0.05f);
}

}  // namespace HumanRecognition::tests
