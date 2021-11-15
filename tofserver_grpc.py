import tofhelper as tof
#import numpy as np
import tofserver_pb2
import tofserver_pb2_grpc
from concurrent import futures
import time
import grpc
import logging


class TofPythonServer(tofserver_pb2_grpc.TofServicer):

    def __init__(self):
        self.tof = tof.AdiTof()

    def GetStatus(self):
        return tofserver_pb2.CamStatus(status=self.tof.get_camera_status())

    def InitializeCamera(self):
        return tofserver_pb2.InitStatus(status=self.tof.initialize_camera())

    def GetFrame(self):
        return tofserver_pb2.Frame(array=self.tof.get_frame_camera())


def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    tofserver_pb2_grpc.add_TofServerServicer_to_server(TofPythonServer(), server)
    server.add_insecure_port('[::]:50021')
    server.start()
    print("Server Initialized.. Waiting for Client")
    server.wait_for_termination()


if __name__ == '__main__':
    logging.basicConfig()
    serve()
