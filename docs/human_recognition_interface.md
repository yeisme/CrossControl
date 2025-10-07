# HumanRecognition 模块接口使用说明

本文档介绍如何实现、注册并使用 HumanRecognition 模块的人脸识别后端接口，包括数据类型、错误码、示例代码和常见注意事项。

## 概览

HumanRecognition 模块将具体识别算法与上层调用分离：上层通过 `HumanRecognition::HumanRecognition` 单例调用统一 API，后端实现需要继承 `HumanRecognition::IHumanRecognitionBackend`。

主要组件：

- 接口：`include/modules/HumanRecognition/ihumanrecognitionbackend.h`
- 类型：`include/modules/HumanRecognition/types.h`
- 工厂注册：`include/modules/HumanRecognition/factory.h`
- 门面：`include/modules/HumanRecognition/humanrecognition.h`

## 关键数据结构

- `HRCode`：操作返回码，常见值包括 `Ok`, `InvalidImage`, `ModelLoadFailed`, `ModelSaveFailed`, `DetectFailed`, `ExtractFeatureFailed`, `CompareFailed`, `TrainFailed`, `PersonExists`, `PersonNotFound`, `UnknownError`。

- `FaceBox`：检测到的人脸框（`QRect rect`）、置信度 `float score`、关键点 `QVector<QPointF> landmarks`。

- `FaceFeature`：特征向量（`QVector<float> values`）、版本 `QString version`、可选范数 `std::optional<float> norm`。

- `RecognitionMatch`：检索结果（`personId`, `personName`, `distance`, `score`）。

- `PersonInfo`：人员记录（`id`, `name`, `metadata`, 可选 `canonicalFeature`）。

- `DetectOptions`：检测参数（`detectLandmarks`, `minScore`, `resizeTo`）。

## 如何实现后端

1. 新建一个继承自 `HumanRecognition::IHumanRecognitionBackend` 的类。
2. 实现纯虚方法：

   - `loadModel(const QString& modelPath)`
   - `saveModel(const QString& modelPath)`
   - `detect(const QImage& image, const DetectOptions& opts, QVector<FaceBox>& outBoxes)`
   - `extractFeature(const QImage& image, const FaceBox& box, FaceFeature& outFeature)`
   - `compare(const FaceFeature& a, const FaceFeature& b, float& outDistance)`
   - `findNearest(const FaceFeature& feature, RecognitionMatch& outMatch)`
   - `registerPerson(const PersonInfo& person)`
   - `removePerson(const QString& personId)`
   - `getPerson(const QString& personId, PersonInfo& outPerson)`
   - `backendName() const`

3. 可选实现：`initialize`, `shutdown`, `train`。

下面给出一个最小后端实现示例（伪代码，放在 `src/modules/HumanRecognition/backends/example_backend.h/.cpp`）：

```cpp
// ExampleBackend.h
#include "ihumanrecognitionbackend.h"

class ExampleBackend : public HumanRecognition::IHumanRecognitionBackend {
public:
    HRCode loadModel(const QString& modelPath) override {
        // 加载模型或准备资源
        return HRCode::Ok;
    }

    HRCode saveModel(const QString& modelPath) override {
        // 保存模型/数据库
        return HRCode::Ok;
    }

    HRCode detect(const QImage& image, const HumanRecognition::DetectOptions& opts,
                  QVector<HumanRecognition::FaceBox>& outBoxes) override {
        // 简单示例：不做真实检测，返回空列表
        outBoxes.clear();
        return HRCode::Ok;
    }

    HRCode extractFeature(const QImage& image, const HumanRecognition::FaceBox& box,
                          HumanRecognition::FaceFeature& outFeature) override {
        // 简单示例：返回固定长度零向量
        outFeature.values = QVector<float>(128, 0.0f);
        outFeature.version = "example.v1";
        outFeature.norm = 0.0f;
        return HRCode::Ok;
    }

    HRCode compare(const HumanRecognition::FaceFeature& a, const HumanRecognition::FaceFeature& b,
                   float& outDistance) override {
        // 计算 L2 距离为示例
        if (a.values.size() != b.values.size()) return HRCode::CompareFailed;
        float d = 0.0f;
        for (int i = 0; i < a.values.size(); ++i) {
            float diff = a.values[i] - b.values[i];
            d += diff * diff;
        }
        outDistance = std::sqrt(d);
        return HRCode::Ok;
    }

    HRCode findNearest(const HumanRecognition::FaceFeature& feature,
                       HumanRecognition::RecognitionMatch& outMatch) override {
        // 示例：无库，直接返回 PersonNotFound
        return HRCode::PersonNotFound;
    }

    HRCode registerPerson(const HumanRecognition::PersonInfo& person) override {
        // 将 person 存入本地 map（示例）
        return HRCode::Ok;
    }

    HRCode removePerson(const QString& personId) override { return HRCode::Ok; }
    HRCode getPerson(const QString& personId, HumanRecognition::PersonInfo& outPerson) override {
        return HRCode::PersonNotFound;
    }

    QString backendName() const override { return QStringLiteral("example"); }
};
```

## 如何注册后端工厂

HumanRecognition 使用工厂函数注册/创建后端。若后端类名为 `ExampleBackend`，请在对应的 .cpp 中静态注册工厂：

```cpp
#include "factory.h"
#include "example_backend.h"

namespace {
    struct RegisterExample {
        RegisterExample() {
            HumanRecognition::registerBackendFactory("example", []() {
                return std::make_unique<ExampleBackend>();
            });
        }
    } r;
}
```

如果后端需要接收 JSON 配置，请使用接收配置的注册函数：

```cpp
HumanRecognition::registerBackendFactory("example", [](const QJsonObject& cfg){
    auto p = std::make_unique<ExampleBackend>();
    p->initialize(cfg);
    return p;
});
```

注册通常放在动态库或可执行文件的静态初始化阶段，以便程序启动时可用。

## 如何在代码中使用 HumanRecognition 门面

HumanRecognition 提供单例 `HumanRecognition::HumanRecognition::instance()`，示例流程如下：

- 初始化/选择后端
- 加载模型
- 对图像进行检测
- 提取特征并查找匹配或比对
- 管理人员库（注册/删除/查询）

示例（伪代码）：

```cpp
#include "modules/HumanRecognition/humanrecognition.h"

using namespace HumanRecognition;

void detectAndIdentify(const QImage& img) {
    auto& hr = HumanRecognition::instance();

    // 可通过可用后端列表选择
    auto backends = hr.availableBackends();
    hr.setBackend(backends.isEmpty() ? QString() : backends[0]);

    // 加载模型
    hr.loadModel("/path/to/model");

    // 检测
    QVector<FaceBox> boxes;
    DetectOptions opts;
    opts.detectLandmarks = true;
    hr.detect(img, opts, boxes);

    for (const auto& box : boxes) {
        FaceFeature feat;
        hr.extractFeature(img, box, feat);

        RecognitionMatch match;
        hr.findNearest(feat, match);
        if (!match.personId.isEmpty()) {
            qDebug() << "Found person:" << match.personName << "distance:" << match.distance;
        }
    }
}
```

## 错误处理与调试

- 所有操作均返回 `HRCode`，请检查返回值并处理错误分支。
- `DetectOptions::minScore` 可用于过滤低置信度结果，避免误识别。
- 人员库操作（register/remove/get）应保证 `PersonInfo.id` 的唯一性。

## 性能与线程

- 检测、特征提取通常是 CPU/GPU 密集型操作。若你的后端使用 GPU，确保在多线程调用时正确管理上下文与设备资源。
- `HumanRecognition` 门面可能在内部持有后端实例，不同线程同时调用时应阅读实现（`include/modules/HumanRecognition/humanrecognition.h`）并在必要时加锁。

## 扩展点

- 支持多后端切换：注册多个后端并通过 `setBackend` 切换。
- 支持批量提取：可在后端实现批量 `extractFeature` 以提高吞吐量。
- 支持自定义度量与后处理：后端可以在 `findNearest` 中返回额外的 `metadata` 或 `score`。

## FAQ

Q: 后端什么时候会被实例化？
A: 工厂注册后，调用 `createBackendInstance` 或 `HumanRecognition::setBackend` 时会实例化。

Q: 如何把我的模型文件放到程序中？
A: 推荐将模型放在资源目录或独立模型目录，通过 `loadModel` 指定路径加载。后端实现负责解析模型文件。

## 参考文件

- include/modules/HumanRecognition/ihumanrecognitionbackend.h
- include/modules/HumanRecognition/types.h
- include/modules/HumanRecognition/factory.h
- include/modules/HumanRecognition/humanrecognition.h
