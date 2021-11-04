#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>
#include "buffer.pb.h"


using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using payload::ClientRequest;
using payload::ServerResponse;

class TofClient{
    public:
        TofClient(std::shared_ptr<Channel> channel): stub_(TimeOfFlight::NewStub(channel)){}


        void GetFrames(){
            ServerResponse response;
            ClientContext context;
            ClientRequest request;
            request->set_func_name("GetFrame");
            std::unique_ptr<ClientReader<ServerResponse> > reader(
        stub_->RequestStream(&context,request ));
        while (reader->Read(&response)){
            frame=response.bytes_payload();
            std::cout<<frame<<std::endl;
        }
        }


}
