Create configuration images on the phone:
-Start APTAM on the phone.
-Enable Calibration mode in context menu (long press on the screen)
-Restart APTAM

-In config mode press GrabFrame (or any SpaceBar button as in normal APTAM) to capture an image
-See opencvcalib/testdata for an example of good calibration images.
-Do not start any calibration on the device itself, we want to do this with OpenCV!
-When finished disable Calibration mode in the context menu and exit APTAM.

Prepare calibration:
-Copy images from Android/data/at.jku.ptam/files/im-*.bmp to a folder like opencvcalib/testdata
-Create imagelist.txt like in opencvcalib/testdata

Do calibration:
-See commandline.txt for examples how to call cameracalibration.exe in opencvcalib.
-(If cameracalibration.exe does not start you might need to get MSVCP100D.DLL and MSVCR100D.DLL somewhere from the Internet. I will try to include them in future releases.)

Create camera.cfg:
-Open the output file of the calibration (normally out_camera_data.yml from opencvcalib folder).
-Replace Camera.OpenCVParameters in camera.cfg with the new opencv parameters as described in camera.cfg