# LSyMAssetGenerator Dependency Setup

When building and running the `LSyMAssetGenerator` application, you might encounter an error like this when trying to load fonts or images:
`Error reading file C:\Windows\Fonts\arial.ttf: read error (Could not find plugin to read objects from file "C:\Windows\Fonts\arial.ttf".)`

## The Problem
OpenSceneGraph dynamically loads plugins (like `osgdb_freetype.dll` or `osgdb_freetyped.dll`) at runtime from the `osgPlugins-<version>` folder. However, these plugins depend on 3rd-party libraries (such as Freetype, Zlib, Libpng, etc.) provided by your package manager (e.g., vcpkg). 

On Windows, the OS will attempt to find these 3rd-party DLL dependencies in the same directory as the **executable** (`build/bin`), not in the plugin directory. Because the 3rd-party DLLs are placed inside the `osgPlugins-<version>` folder instead of `bin/`, the OS fails to load the plugin, resulting in the "Could not find plugin" error.

## The Solution
To fix this, you need to manually copy the 3rd-party dependency DLLs from the plugins directory into the main binary directory alongside your executable.

### Step 1: Build with the Package Manager
If you see that the `osgPlugins-3.6.5` folder doesn't exist, it means you didn't configure CMake to find your 3rd-party libraries (Freetype, libpng, zlib, etc.). 
Make sure you run CMake with your vcpkg toolchain file so that the plugins are actually built:
`cmake -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ..`
Once configured and compiled, the `osgPlugins-3.6.5` folder will be generated in your `build/bin/` folder.

### Step 2: Build the Required Configuration
By default, CMake might only build the **Debug** configuration. If you try to run the **Release** executable (`LSyMAssetGenerator.exe`), it won't find the Release dependencies because they haven't been built or copied yet!

To ensure the Release dependencies are generated, build the specific configuration you intend to run. For Release, run:
`cmake --build . --config Release`

### Step 3: What to Copy
1. Navigate to your build's plugin directory (e.g., `D:\OpenSceneGraph\build\bin\osgPlugins-3.6.5`).
2. Select all `.dll` files that **DO NOT** start with `osgdb_`. For example, copy files like:
   - `freetype.dll` (or `freetyped.dll` for Debug)
   - `libpng16.dll` (or `libpng16d.dll` for Debug)
   - `z.dll` (or `zd.dll` for Debug)
   - `brotlidec.dll`
   - `brotlicommon.dll`
   - `bz2.dll` (or `bz2d.dll` for Debug)
3. Paste these files directly into your build's binary directory (e.g., `D:\OpenSceneGraph\build\bin`), right next to `LSyMAssetGenerator.exe`.

Do **NOT** move the `osgdb_*.dll` files out of the `osgPlugins-<version>` folder. OpenSceneGraph specifically looks for them inside that folder. You only need to copy the 3rd-party dependencies.

Once copied, your application will be able to successfully load the plugins and read assets like `arial.ttf`.
