# dds10-thumbnailer-kde

dds10-thumbnailer-kde is a plugin for KDE 5 that creates thumbnail for Direct 
Draw Surface (DDS) images. It supports DX10 version of DDS with BC1/DXT1, 
BC2/DXT3, BC3/DXT5 BC4/ATI1, BC5/ATI2 and BC7 encodings.

## Build and install

Install dependencies:

 - Ubuntu: `sudo apt install cmake extra-cmake-modules build-essential qtbase5-dev libkf5kio-dev`
 - openSUSE: `sudo zypper install cmake extra-cmake-modules libqt5core-devel libqt5gui-devel kio-devel`

Build:

```
mkdir build
cd build
cmake -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DCMAKE_INSTALL_PREFIX=`kf5-config --prefix` -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
sudo cmake --install .
kbuildsycoca5
```

You can then restart Dolphin and enable the plugin in `Configure Dolphin`>`General`>`Previews`.
