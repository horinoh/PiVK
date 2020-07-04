# PiVK

## インストール
- [参考](https://blogs.igalia.com/apinheiro/2020/06/v3dv-quick-guide-to-build-and-run-some-demos/)
- アップデートをしておく
    ~~~
    $sudo apt-get update
    $sudo apt-get upgrade
    ~~~
- 開発ツール群インストール
    ~~~
    $sudo apt-get install libxcb-randr0-dev libxrandr-dev \
            libxcb-xinerama0-dev libxinerama-dev libxcursor-dev \
            libxcb-cursor-dev libxkbcommon-dev xutils-dev \
            xutils-dev libpthread-stubs0-dev libpciaccess-dev \
            libffi-dev x11proto-xext-dev libxcb1-dev libxcb-*dev \
            bison flex libssl-dev libgnutls28-dev x11proto-dri2-dev \
            x11proto-dri3-dev libx11-dev libxcb-glx0-dev \
            libx11-xcb-dev libxext-dev libxdamage-dev libxfixes-dev \
            libva-dev x11proto-randr-dev x11proto-present-dev \
            libclc-dev libelf-dev git build-essential mesa-utils \
            libvulkan-dev ninja-build libvulkan1
    ~~~
- meson
    - cmakeのようなもの
    ~~~
    $pip3 install meson
    ~~~
    - PATHは通っていないので通しておく
    ~~~
    PATH=$PATH:/home/pi/.local/bin
    export PATH
    ~~~
- 追加インストールが必要だったもの
    ~~~
    $sudo apt-get install libdrm-dev
    $sudo apt-get install libxshmfence-dev
    $sudo apt-get install libxxf86vm-dev
    ~~~
- ビルド、インストール
    ~~~
    $git clone https://gitlab.freedesktop.org/apinheiro/mesa.git mesa
    $cd mesa
    $meson --prefix /home/pi/local-install --libdir lib -Dplatforms=x11,drm -Dvulkan-drivers=auto -Ddri-drivers= -Dgallium-drivers=v3d,vc4 -Dbuildtype=debug _build
    $ninja -C _build
    $ninja -C _build install
    ~~~
- 環境変数 VK_ICD_FILENAMES を作成
    - LD_LIBRARY_PATH のようなもの
    ~~~
    VK_ICD_FILENAMES=/home/pi/local-install/share/vulkan/icd.d/broadcom_icd.armv7l.json
    export VK_ICD_FILENAMES
    ~~~
## サンプルを動かす
- 追加インストールが必要だったもの
    ~~~
    $sudo apt-get install cmake
    ~~~
- ダウンロード、ビルド
    ~~~
    $sudo apt-get install libassimp-dev
    $git clone --recursive https://github.com/SaschaWillems/Vulkan.git  sascha-willems
    $cd sascha-willems
    $cmake -DCMAKE_BUILD_TYPE=Debug
    $make
    $bin/gears
    ~~~
- アセットを必要とするサンプルを動かす場合
    ~~~
    $python3 download_assets.py
    $bin/scenerendering
    ~~~
## コーディング
- -lvulkan を付けてコンパイルするだけで通るっぽい
    ~~~
    g++ main.cpp -lvulkan
    ~~~
- スワップチェインの作成時に、以下のようなエラーが出るが、サンプルても出ているのでとりあえず放置
    ~~~
    vk: error: v3dv_AllocateMemory: ignored VkStructureType 1000127001:VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO
    vk: error: v3dv_AllocateMemory: ignored VkStructureType 1000072002:VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO
    ~~~
    
### glslang
~~~
$sudo apt-get install glslang-tools
~~~

### glmをサブモジュールとして追加する場合
~~~
$git submodule add https://github.com/g-truc/glm.git glm
~~~
