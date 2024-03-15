# dds10-thumbnailer-kde

dds10-thumbnailer-kde is a plugin for KDE 6 that creates thumbnail for Direct 
Draw Surface (DDS) images. It supports DX10 version of DDS with BC1/DXT1, 
BC2/DXT3, BC3/DXT5 BC4/ATI1, BC5/ATI2 and BC7 encodings.

If you are looking for the KDE 5 version, check the `plasma5` branch.

## Build and install

Install dependencies:

 - openSUSE: `sudo zypper install cmake kf6-extra-cmake-modules qt6-core-devel qt6-gui-devel kf6-kio-devel`

Build:

```
mkdir build
cd build
cmake -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DBUILD_WITH_QT6=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
sudo cmake --install .
kbuildsycoca6
```

You can then restart Dolphin and enable the plugin in `Configure Dolphin`>`Interface`>`Previews`.
