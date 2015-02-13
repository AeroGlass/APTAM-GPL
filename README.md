APTAM-GPL: Enhanced Android Port of Oxford’s PTAM-GPL (Parallel Tracking and Mapping re-released under GPLv3)

Oxford’s original PTAM-GPL: https://github.com/Oxford-PTAM/PTAM-GPL

Changes and Improvements:

- Usage and configuration of the Android camera as input video source.
- Support for the OpenCV camera distortion model.
- OpenGL ES 2 support for rendering with fast, shader-based YUV to RGB conversion.
- Experimental support for using the devices inertial sensors to improve tracking accuracy.
- Several minor tweaks to improve speed and stability on Android devices.
- Optional support for NCC during patch matching and further key-frame detectors.
- Saving and loading of the map points and key frames.
