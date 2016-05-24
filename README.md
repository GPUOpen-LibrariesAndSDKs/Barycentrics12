Barycentrics D3D12 GCN Shader Extension Sample
==============================================

This sample shows how to use the GCN shader extensions for D3D12 to access the barycentric intrinsic instruction in an HLSL pixel shader. 

License
-------

MIT: see `LICENSE.txt` for details.

System requirements
-------------------

* A graphics card with D3D12 support.<sup>[1](#barycentrics12-footnote1)</sup> 
  * For instance, any GCN-based AMD GPU.
* Windows 10 (64-bit recommended).
* Visual Studio 2015 with Visual C++ and the Windows 10 SDK installed. The [free community edition](https://www.visualstudio.com/downloads/download-visual-studio-vs) is sufficient.
  * Note: neither Visual C++ nor the Windows 10 SDK are installed with Visual Studio 2015 by default.
  * For a fresh installation of Visual Studio 2015, choose 'Custom', not 'Typical', and select the required components.
  * For existing installations, you can re-run the installer or select Visual Studio from Programs and Features and click 'Change'.
  * When selecting components to be installed, the option to install the Windows 10 SDK is tucked away under Windows and Web Development -> Universal Windows App Development Tools.
* A graphics driver with GCN shader extension support.
  * For example, AMD Radeon Software Crimson Edition 16.5.2 or later.

Building
--------

Visual Studio files can be found in the `barycentrics12\build` directory.

If you need to regenerate the Visual Studio files, open a command prompt in the `barycentrics12\premake` directory and run `..\..\premake\premake5.exe vs2015` (or `..\..\premake\premake5.exe vs2013` for Visual Studio 2013.)

Sample overview
---------------

This sample renders a triangle zooming in and out. The triangle uses a checker board texture modulated by the barycentric coordinates as RGB colors.


Points of interest
------------------

* This sample uses a driver extension to enable the use of instrinsic instructions.
  * The driver extension is accessed through the AMD GPU Services (AGS) library.
  * For more information on AGS, including samples, visit the AGS SDK repository: https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK 
* The intrinsic instructions require a 5.1 shader model.
* The Root Signature will need to use an extra resource and sampler. These are not real resources/samplers, they are just used to encode the intrinsic instruction.
* The shader compiler should not use the D3DCOMPILE_SKIP_OPTIMIZATION option, otherwise it will not work.
* Other instrinsics are also available: Min, Med, Max ...
* For clarity purposes, the code required to enable the intrinsics extension is #ifdef'ed. 
* The `DEBUG` configuration will automatically enable the debug layers to validate the API usage. Check the source code for details, as this requires the graphics tools to be installed.

Third-party software
--------------------

* Premake is distributed under the terms of the BSD License. See `premake\LICENSE.txt`.

Attribution
-----------

* AMD, the AMD Arrow logo, Radeon, and combinations thereof are either registered trademarks or trademarks of Advanced Micro Devices, Inc. in the United States and/or other countries.
* Microsoft, Direct3D, DirectX, Visual Studio, Visual C++, and Windows are either registered trademarks or trademarks of Microsoft Corporation in the United States and/or other countries.

Notes
-----

<a name="barycentrics12-footnote1">1</a>: While the shader extension sample will run on non-AMD hardware, it will be of limited usefulness, since the purpose of the sample is to demonstrate AMD-specific shader extensions.
