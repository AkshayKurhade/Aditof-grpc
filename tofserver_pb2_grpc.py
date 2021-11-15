# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

import tofserver_pb2 as tofserver__pb2


class TofServerStub(object):
    """Missing associated documentation comment in .proto file."""

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.GetStatus = channel.unary_unary(
                '/payload.TofServer/GetStatus',
                request_serializer=tofserver__pb2.ClientRequest.SerializeToString,
                response_deserializer=tofserver__pb2.CamStatus.FromString,
                )
        self.InitializeCamera = channel.unary_unary(
                '/payload.TofServer/InitializeCamera',
                request_serializer=tofserver__pb2.ClientRequest.SerializeToString,
                response_deserializer=tofserver__pb2.InitStatus.FromString,
                )
        self.GetFrame = channel.unary_unary(
                '/payload.TofServer/GetFrame',
                request_serializer=tofserver__pb2.ClientRequest.SerializeToString,
                response_deserializer=tofserver__pb2.Frame.FromString,
                )


class TofServerServicer(object):
    """Missing associated documentation comment in .proto file."""

    def GetStatus(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def InitializeCamera(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def GetFrame(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_TofServerServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'GetStatus': grpc.unary_unary_rpc_method_handler(
                    servicer.GetStatus,
                    request_deserializer=tofserver__pb2.ClientRequest.FromString,
                    response_serializer=tofserver__pb2.CamStatus.SerializeToString,
            ),
            'InitializeCamera': grpc.unary_unary_rpc_method_handler(
                    servicer.InitializeCamera,
                    request_deserializer=tofserver__pb2.ClientRequest.FromString,
                    response_serializer=tofserver__pb2.InitStatus.SerializeToString,
            ),
            'GetFrame': grpc.unary_unary_rpc_method_handler(
                    servicer.GetFrame,
                    request_deserializer=tofserver__pb2.ClientRequest.FromString,
                    response_serializer=tofserver__pb2.Frame.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'payload.TofServer', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class TofServer(object):
    """Missing associated documentation comment in .proto file."""

    @staticmethod
    def GetStatus(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/payload.TofServer/GetStatus',
            tofserver__pb2.ClientRequest.SerializeToString,
            tofserver__pb2.CamStatus.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def InitializeCamera(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/payload.TofServer/InitializeCamera',
            tofserver__pb2.ClientRequest.SerializeToString,
            tofserver__pb2.InitStatus.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def GetFrame(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/payload.TofServer/GetFrame',
            tofserver__pb2.ClientRequest.SerializeToString,
            tofserver__pb2.Frame.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)
