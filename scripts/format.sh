#!/usr/bin/env bash
# 格式化脚本（bash）
# 用法:
#   ./scripts/format.sh        # 递归格式化仓库下所有 .h .hpp .c .cpp .cc .cxx 文件
#   VERBOSE=1 ./scripts/format.sh

set -euo pipefail
VERBOSE=${VERBOSE-0}
EXTENSIONS=".h .hpp .c .cpp .cc .cxx"
EXCLUDE_PATTERN="/build/|/.git/|/.vscode/|/third_party/"

CLANG_FMT=$(command -v clang-format || true)
if [ -z "$CLANG_FMT" ]; then
  echo "clang-format 未找到，请先安装 clang-format 并将其加入 PATH。" >&2
  exit 2
fi

# find files
IFS=' '\n
FILES=$(find . -type f \( -iname "*.h" -o -iname "*.hpp" -o -iname "*.c" -o -iname "*.cpp" -o -iname "*.cc" -o -iname "*.cxx" \) | grep -Ev "$EXCLUDE_PATTERN" || true)

if [ -z "$FILES" ]; then
  echo "未找到需要格式化的源文件。"
  exit 0
fi

COUNT=0
while IFS= read -r f; do
  if [ "$VERBOSE" -eq 1 ]; then
    echo "Formatting: $f"
  fi
  "$CLANG_FMT" -i -style=file "$f" || echo "WARNING: failed to format $f"
  COUNT=$((COUNT+1))
done <<EOF
$FILES
EOF

echo "格式化完成： $COUNT 个文件已处理。"
exit 0
