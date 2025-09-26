# HumanRecognition 模块说明

本文档用中文详细介绍当前工程中 `HumanRecognition` 模块的设计、实现细节、数据库 schema、API 使用方法、现有 UI（子页面）及已知限制与未来改进建议。本文档对应的代码位置主要在：

- include/modules/HumanRecognition/
  - `humanrecognition.h`（高层 Qt 包装）
  - `iface_human_recognition.h`（抽象接口、数据结构）
  - `opencv_backend.h`（OpenCV 后端声明）
- src/modules/HumanRecognition/
  - `humanrecognition.cpp`（高层实现）
  - `opencv_backend.cpp`（OpenCV 后端实现：检测、嵌入、匹配、持久化）
- UI（示例子页面）
  - `include/widgets/facerecognitionwidget.h`
  - `src/widgets/facerecognitionwidget.cpp`

---

## 1. 总体架构与职责

模块分成三层：

- IHumanRecognitionBackend（抽象接口）

  - 定义后端应实现的能力：检测、人脸嵌入提取、注册（enroll）、单次识别（detect+recognize）、匹配、帧处理、持久化、以及人员信息管理（列出/更新/删除/查询特征）。

- HumanRecognitionService（策略/包装）

  - 持有一个具体的后端实例（例如 OpenCV 后端），对上层（`HumanRecognition`、UI）提供统一代理。

- HumanRecognition（高层 Qt 包装）

  - 用于在应用中直接创建、使用识别服务（包含信号 humanDetected / recognitionCompleted），并提供一组便捷方法供 UI 调用。

- OpenCV 后端（示例实现）
  - 负责具体的检测、嵌入生成、数据库持久化与读取、匹配算法等实现细节。

这样设计的好处是：可以很容易地替换后端（例如用 ONNX、dlib、ArcFace 等）而不改动 UI/业务层。

---

## 2. 关键数据结构

在 `iface_human_recognition.h` 中定义了模块使用的核心类型：

- FaceEmbedding

  - using FaceEmbedding = std::vector<float>;
  - 表示人脸特征向量（当前后端为可变长度的 float 向量）。

- FaceDetectionResult

  - QRect box: 检测到的人脸区域
  - float score: 置信度（目前占位，Haar 不能给出概率）
  - FaceEmbedding embedding: （可选）当前检测的人脸嵌入
  - QString matchedPersonId / matchedPersonName: 如果匹配成功则填充
  - float matchedDistance: 匹配距离（数值语义由后端决定，当前为欧氏距离）

- PersonInfo
  - QString personId: 业务唯一 ID（例如工号或 UUID）
  - QString name: 姓名
  - QString extraJson: 额外信息（以 JSON 字符串保存）

---

## 3. OpenCV 后端实现细节（当前版本）

该实现位于 `src/modules/HumanRecognition/opencv_backend.cpp`，主要要点如下：

### 3.1 人脸检测

- 使用 OpenCV 的 Haar Cascade（cv::CascadeClassifier）进行检测。候选 cascade 文件名为 `haarcascade_frontalface_default.xml`（工程没有强制包含权重文件，运行时需要确保该文件可被加载，或修改路径到本地权重）。
- 检测函数：`detectFaces(const QImage& image)`，返回每个检测框的 `FaceDetectionResult`（目前 score 固定为 0.5，占位）。

### 3.2 人脸嵌入（占位实现）

- 函数：`extractEmbedding(const QImage& faceImage)`
- 当前实现非常简单：
  - 将人脸图像转换为 RGB32 并缩放到 16x16。
  - 按像素逐通道（R,G,B）归一化并 flatten 成 `16*16*3 = 768` 维的 float 向量。
- 这是占位实现，用于演示 pipeline；实际项目应替换为更可靠的 embedding model（例如 ArcFace、FaceNet 的输出）。

### 3.3 注册（enroll）与持久化

- 函数：`enrollPerson(const PersonInfo& info, const std::vector<QImage>& faceImages)`

  - 对每张人脸图像调用 `extractEmbedding()`，将得到的 float 向量二进制化（raw float bytes）放入 SQLite 的 BLOB 字段插入到 `face_features` 表中。
  - 同时在内存缓存 `m_featureCache` 中追加 `CacheItem{emb, personId, name}`，用于加速匹配。

- 数据库 schema（由 `ensureSchema_()` 创建）：

  - 表 `persons`：存储人员基本信息
    - person_id TEXT PRIMARY KEY
    - name TEXT
    - extra TEXT
    - created_at INTEGER
  - 表 `face_features`：存储每一条特征
    - id INTEGER PRIMARY KEY AUTOINCREMENT
    - person_id TEXT NOT NULL
    - name TEXT
    - feature BLOB NOT NULL -- 原始 float bytes
    - dim INTEGER NOT NULL
    - extra TEXT
    - created_at INTEGER
  - 索引：`idx_face_features_person` on face_features(person_id)

- 加载特征：`loadFeatureDatabase()` 会从 `face_features` 读取所有 feature，反序列化为 float 数组并填充 `m_featureCache`。

### 3.4 匹配算法

- `matchEmbedding(const FaceEmbedding& emb)` 使用欧式距离（L2）作为 metric：distance = sqrt(sum((a_i - b_i)^2))。
- 比较所有 `m_featureCache` 条目，找到最小距离的条目；如果 bestDistance <= m_threshold 则视为匹配成功。
- 默认阈值 `m_threshold` 在后端为 0.6f（可通过 `setRecognitionThreshold` 修改）。

### 3.5 人员信息管理

后端实现了以下方法，供 UI/业务层管理人员信息：

- listPersons()：返回 `persons` 表中所有记录（PersonInfo 列表）。
- updatePersonInfo(const PersonInfo& info)：通过 SQLite 的 upsert（INSERT ... ON CONFLICT DO UPDATE）更新或插入人员基本信息。
- deletePerson(const QString& personId)：删除 `face_features` 中与该 personId 相关的所有条目并删除 persons 表对应记录，同时从内存缓存 `m_featureCache` 中移除。
- getPersonFeatures(const QString& personId)：返回指定人员的所有 embedding（反序列化为 FaceEmbedding 列表）。

---

## 4. HumanRecognition 高层 API 快速参考（C++）

位于 `include/modules/HumanRecognition/humanrecognition.h`。

常用方法：

- 初始化/资源：构造 `HumanRecognition hr;`（构造中会创建 `HumanRecognitionService` 并用 OpenCV 后端初始化）
- 检测（仅检测）：

```cpp
HumanRecognition hr;
QImage img = ...;
bool any = hr.detectHumans(img);
```

- 检测并识别（返回每个人脸的匹配信息）：

```cpp
auto results = hr.detectAndRecognize(img);
for (auto& r : results) {
    if (!r.matchedPersonName.isEmpty()) {
        // matched
    }
}
```

- 注册（enroll）：

```cpp
PersonInfo p{QString("user-001"), QString("张三"), QStringLiteral("{\"type\":\"Frequent\"}")};
std::vector<QImage> faces = {face1, face2};
int added = hr.enroll(p, faces);
```

- 人员管理：

```cpp
auto list = hr.listPersons();
hr.updatePersonInfo(p);
hr.deletePerson("user-001");
auto features = hr.getPersonFeatures("user-001");
```

- 保存/加载（持久化读写）

```cpp
hr.save();
hr.load();
```

---

## 5. UI 子页面（FaceRecognitionWidget）说明

项目中包含一个示例子页面 `FaceRecognitionWidget`（`include/widgets/facerecognitionwidget.h` 和 `src/widgets/facerecognitionwidget.cpp`），实现了以下交互：

- Load Image：加载本地图片用于测试与注册。
- Enroll：用当前图片注册人员（简化：使用姓名作为 personId，extraJson 保存类型字段）。
- Match：对当前图片执行 detect+recognize 并把匹配结果打印到日志框。
- Refresh：从 DB 列出已注册的人员。
- Delete：删除选中人员（及其特征）。

这个 UI 旨在作为功能演示与调试工具；正式产品应按需求美化并加入摄像头流、权限控制等。

---

## 6. 调优与开发者注意事项

- Haar cascade 权重文件路径：

  - 当前后端在初始化时尝试加载文件名 `haarcascade_frontalface_default.xml`。若文件不在运行目录，需指定完整路径或把文件放入可访问路径。

- Embedding 质量：

  - 当前 embedding 为占位（16x16 RGB flatten），适合作为 pipeline 占位或 demo，但识别准确率极低。
  - 推荐替换为基于深度学习的嵌入模型（ArcFace/FaceNet/MTCNN+Backbone），并在 `extractEmbedding()` 中调用推理代码以返回固定维度向量（通常 128~512 维）。

- 人脸对齐与预处理：

  - 更准确的识别需要对齐（对眼角/鼻子进行标准化变换）和归一化处理。可在 `extractEmbedding()` 前做关键点检测并对齐。

- 匹配阈值：

  - 当前阈值默认 0.6（针对占位 embedding 无意义），请在替换真实 embedding 后通过验证集调参。

- personId 生成策略：

  - UI 当前以 `name` 作为 id 简化演示。生产环境请使用 UUID 或业务唯一 ID，避免同名冲突。

- 并发与事务：

  - enroll/delete 使用了 SQLite 事务，适用于低并发场景。高并发请使用服务化存储或带行级锁的数据库。

- 隐私与合规：
  - 若存储图片/特征涉及个人隐私，请遵守当地法律法规并在数据库/传输中做好加密与访问控制。当前实现将 float bytes 原封不动写入 DB，必要时应对敏感字段加密或避免存储原图。

---

## 7. 限制与未来改进建议

1. 替换嵌入模型：集成离线或 ONNX 模型以获得真实的人脸特征。
2. 实现人脸关键点检测与对齐：提高嵌入的一致性与匹配精度。
3. 改进检测器：使用更稳健的检测模型（例如 MTCNN、RetinaFace）。
4. 摄像头与实时流：在 `FaceRecognitionWidget` 中以 OpenCV VideoCapture 或 QtMultimedia 集成摄像头预览与实时识别。
5. 批量管理 UI：实现批量导入/批量注册、导出/迁移特征的功能。
6. 单元测试：为后端算法与 DB 接口添加单元测试（小型的 embedding 相似性测试与 DB CRUD 测试）。
7. 安全：对 DB 中敏感信息进行加密与访问控制。

---

## 8. 如何在本仓库中运行/测试（简要）

1. 确保已安装依赖（Qt6、OpenCV, vcpkg 等，参见项目的 CMake 配置）。
2. 配置并生成工程（可在 VSCode 中使用提供的 CMake 任务）：

```powershell
# 在仓库根目录（Windows PowerShell）
# 使用 VSCode 中已定义的任务或手动运行 cmake
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```

3. 运行程序并打开 FaceRecognition 子页面（如果你还没把 widget 挂载到主界面，可以临时在主窗口中创建该 widget 以进行测试）。
4. 在 UI 中使用 Load Image / Enroll / Match / Refresh / Delete 验证功能。

---

## 9. 开发者快速定位

- 检测/嵌入/匹配逻辑：`src/modules/HumanRecognition/opencv_backend.cpp`
- 抽象接口与 Service：`include/modules/HumanRecognition/iface_human_recognition.h`
- Qt 高层包装：`include/modules/HumanRecognition/humanrecognition.h`、`src/modules/HumanRecognition/humanrecognition.cpp`
- 存储门面（获取 QSqlDatabase）：`include/modules/Storage/storage.h`、`src/modules/Storage/*`（注意 Storage::db() 为全局 DB 入口）
- 示例 UI：`src/widgets/facerecognitionwidget.cpp`

---

如果你希望我把这份文档进一步扩展为：

- 包含示例图片和截图（需要你提供图片或运行截图），
- 给出替换为 ONNX/ArcFace 的实现要点（包括如何集成 vcpkg/ONNX Runtime），
- 或者自动生成 API 文档（doxygen 风格），

告诉我你想要的方向，我会继续实现。
