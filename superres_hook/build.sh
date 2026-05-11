# Android Super Resolution Hook - Build Script
# 使用Android NDK编译Hook库

ANDROID_NDK=${ANDROID_NDK:-/opt/android-ndk}
TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64
API_LEVEL=29

# 目标架构（可修改为armeabi-v7a, x86, x86_64）
ARCH=${ARCH:-arm64}

case $ARCH in
    arm64)
        TARGET=aarch64-linux-android$API_LEVEL
        ABI=arm64-v8a
        ;;
    arm)
        TARGET=armv7a-linux-androideabi$API_LEVEL
        ABI=armeabi-v7a
        ;;
    x86_64)
        TARGET=x86_64-linux-android$API_LEVEL
        ABI=x86_64
        ;;
    x86)
        TARGET=i686-linux-android$API_LEVEL
        ABI=x86
        ;;
esac

CC=$TOOLCHAIN/bin/$TARGET-clang
CXX=$TOOLCHAIN/bin/$TARGET-clang++

echo "Building for $ARCH ($TARGET)"

# 编译标志
CFLAGS="-O2 -fPIC -Wall -Wextra"
LDFLAGS="-shared -llog -lEGL -lGLESv3 -landroid"

# 创建输出目录
OUTPUT_DIR="build/$ABI"
mkdir -p $OUTPUT_DIR

# 编译目标文件
echo "Compiling superres_core.c..."
$CC $CFLAGS -I./include -c src/superres_core.c -o $OUTPUT_DIR/superres_core.o

echo "Compiling superres_hook.c..."
$CC $CFLAGS -I./include -c src/superres_hook.c -o $OUTPUT_DIR/superres_hook.o

# 链接成共享库
echo "Linking libsuperres.so..."
$CC $LDFLAGS $OUTPUT_DIR/superres_core.o $OUTPUT_DIR/superres_hook.o -o $OUTPUT_DIR/libsuperres.so

# 清理中间文件
rm -f $OUTPUT_DIR/*.o

echo "Build completed: $OUTPUT_DIR/libsuperres.so"

# 显示文件信息
if [ -f "$OUTPUT_DIR/libsuperres.so" ]; then
    ls -lh $OUTPUT_DIR/libsuperres.so
    file $OUTPUT_DIR/libsuperres.so
fi
