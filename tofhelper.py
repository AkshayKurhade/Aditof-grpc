import aditofpython as tof
import numpy as np


class AdiTof:
    def __init__(self):
        self.system = tof.System()
        self.cam_details = tof.CameraDetails()
        self.cameras = []
        self.get_camera_status()
        self.camera1 = self.cameras[0]
        self.camDetails = tof.CameraDetails()
        self.frame = tof.Frame()

    def get_camera_status(self):
        status = self.system.getCameraList(self.cameras)
        # print("system.getCameraList()", status)
        return status

    def get_available_modes(self):
        # camera1 = self.cameras[0]
        modes = []
        status = self.camera1.getAvailableModes(modes)
        # print("system.getAvailableModes()", status)
        # print(modes)
        return modes

    def get_available_frametype(self):
        types = []
        status = self.camera1.getAvailableFrameTypes(types)
        # print("system.getAvailableFrameTypes()", status)
        # print(types)
        return types

    def initialize_camera(self):
        status = self.camera1.initialize()
        self.camera1.setFrameType("depth_ir")
        self.camera1.setMode("near")
        # print("camera1.initialize()", status)
        return status

    def get_camera_details(self):
        return self.cam_details

    def get_frame_camera(self, frametype="depth_ir", mode="near"):
        #self.camera1.setFrameType(frametype)
        #self.camera1.setMode(mode)
        self.camera1.requestFrame(self.frame)
        frame = np.array(self.frame.getData(tof.FrameDataType.Depth), copy=False)
        #frame=frame.flatten()
        return frame
