import grpc
import numpy as np
import tofserver_pb2
import tofserver_pb2_grpc
from concurrent import futures
import time
import grpc
import logging
import cv2

np.set_printoptions(suppress=True)

def getstatus(stub):
    new_func = "GetStatus()"
    #print(func_name)
    status=stub.GetStatus(tofserver_pb2.ClientRequest(func_name=new_func))
    print(status)

def initialize_camera(stub):
    func_name = 'InitializeCamera()'
    #print(tofserver_pb2.ClientRequest(func_name=func_name))
    status=stub.InitializeCamera(tofserver_pb2.ClientRequest(func_name=func_name))
    print(status)

def get_frame(stub):
    func_name = 'GetFrame()'
    frame=stub.GetFrame(tofserver_pb2.ClientRequest(func_name=func_name))
    frame=np.array(list(frame.array))
    frame=np.uint16(frame.reshape(480,640))
    # Uncomment the next line to save the frame to file
    #np.savetxt("frame.csv",frame,delimiter=',',fmt='%i')
    cv2.imshow('test',frame)
    cv2.waitKey(1)
    #cv2.destroyAllWindows()
    


def run():
    
    with grpc.insecure_channel('10.0.0.23:50021') as channel:
        stub = tofserver_pb2_grpc.TofServerStub(channel)

        #getstatus(stub)
        initialize_camera(stub)
        while(1):
        
            get_frame(stub)
        

if __name__ == '__main__':
    run()

