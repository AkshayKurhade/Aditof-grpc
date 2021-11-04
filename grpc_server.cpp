
#include "../server/server.h"
#include "aditof/aditof.h"
//#include "sdk/include/aditof/sensor_enumerator_factory.h""
#include "aditof/sensor_enumerator_interface.h"
#include "buffer.pb.h"

#include "../../sdk/src/connections/target/v4l_buffer_access_interface.h"
#include "../../sdk/src/connections/utils/connection_validator.h"

#include <csignal>
#include <glog/logging.h>
#include <iostream>
#include <linux/videodev2.h>
#include <map>
#include <string>
#include <sys/time.h>
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using payload::ServerResponse;
using payload::ClientRequest;


using namespace google::protobuf::io;

static int interrupted = 0;

/* Available sensors */
static std::vector<std::shared_ptr<aditof::StorageInterface>> storages;
static std::vector<std::shared_ptr<aditof::TemperatureSensorInterface>>
    temperatureSensors;
bool sensors_are_created = false;

/* Server only works with one depth sensor */
std::vector<std::shared_ptr<aditof::DepthSensorInterface>> camDepthSensor;
std::vector<std::shared_ptr<aditof::V4lBufferAccessInterface>>
    sensorV4lBufAccess;
aditof::FrameDetails frameDetailsCache[2];
std::vector<uint16_t *> sensorsFrameBuffers;

static payload::ClientRequest buff_recv;
static payload::ServerResponse buff_send;
static std::map<std::string, api_Values> s_map_api_Values;
static void Initialize();
void invoke_sdk_api(payload::ClientRequest buff_recv);
static bool Client_Connected = false;
static bool no_of_client_connected = false;
bool latest_sent_msg_is_was_buffered = false;

struct clientData {
    bool hasFragments;
    std::vector<char> data;
};

static void cleanup_sensors() {
    for (auto &storage : storages) {
        storage->close();
    }
    storages.clear();
    for (auto &sensor : temperatureSensors) {
        sensor->close();
    }
    temperatureSensors.clear();
    for (auto &depthSensor : camDepthSensor) {
        depthSensor.reset();
    }
    camDepthSensor.clear();
    for (auto &depthSensor : sensorV4lBufAccess) {
        depthSensor.reset();
    }
    sensorV4lBufAccess.clear();

    for (int i = 0; i < sensorsFrameBuffers.size(); ++i) {
        if (sensorsFrameBuffers[i]) {
            delete[] sensorsFrameBuffers[i];
        }
    }
    sensorsFrameBuffers.clear();

    sensors_are_created = false;
}


void RunServer() {
  std::string server_address("0.0.0.0:54021");
  TimeOfFlight service();

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  server->Wait();
}

class TimeOfFlight final : public TimeOfFlight::Service{
    public:
        Status RequestStream(ServerContext* context, const ClientRequest* clientrequest, 
        ServerWriter<ServerResponse>* writer)
        )override{
            buff_recv= clientrequest;
            invoke_sdk_api(buff_recv, writer);
            return Status::OK;
        } 
    private:
        void invoke_sdk_api(payload::ClientRequest buff_recv, ServerWriter<ServerResponse>* writer ) {
            buff_send.Clear();
            buff_send.set_server_status(::payload::ServerStatus::REQUEST_ACCEPTED);

            DLOG(INFO) << buff_recv.func_name() << " function";

            switch (s_map_api_Values[buff_recv.func_name()]) {

            case FIND_SENSORS: {
                std::vector<std::shared_ptr<aditof::DepthSensorInterface>> depthSensors;

                if (!sensors_are_created) {
                    auto sensorsEnumerator =
                        aditof::SensorEnumeratorFactory::buildTargetSensorEnumerator();
                    if (!sensorsEnumerator) {
                        std::string errMsg =
                            "Failed to create a target sensor enumerator";
                        LOG(WARNING) << errMsg;
                        buff_send.set_message(errMsg);
                        buff_send.set_status(static_cast<::payload::Status>(
                            aditof::Status::UNAVAILABLE));
                        writer->Write(buff_send);
                        break;
                    }

                    sensorsEnumerator->searchSensors();
                    sensorsEnumerator->getDepthSensors(depthSensors);
                    sensorsEnumerator->getStorages(storages);
                    sensorsEnumerator->getTemperatureSensors(temperatureSensors);
                    sensors_are_created = true;

                    // Create buffers to be used by depth sensors when reuqesting frame from them
                    sensorsFrameBuffers.resize(depthSensors.size());
                    for (int i = 0; i < sensorsFrameBuffers.size(); ++i) {
                        sensorsFrameBuffers[i] = nullptr;
                    }
                }

                /* Add information about available sensors */

                // Depth sensor
                if (depthSensors.size() < 1) {
                    buff_send.set_message("No depth sensors are available");
                    buff_send.set_status(::payload::Status::UNREACHABLE);
                    break;
                }

                auto pbSensorsInfo = buff_send.mutable_sensors_info();
                int depth_sensor_id = 0;
                for (const auto &depthSensor : depthSensors) {
                    camDepthSensor.emplace_back(depthSensor);
                    sensorV4lBufAccess.emplace_back(
                        std::dynamic_pointer_cast<aditof::V4lBufferAccessInterface>(
                            depthSensor));
                    std::string name;
                    depthSensor->getName(name);
                    auto pbImagerInfo = pbSensorsInfo->add_image_sensors();
                    pbImagerInfo->set_name(name);
                    pbImagerInfo->set_id(depth_sensor_id);
                    ++depth_sensor_id;
                }

                // Storages
                int storage_id = 0;
                for (const auto &storage : storages) {
                    std::string name;
                    storage->getName(name);
                    auto pbStorageInfo = pbSensorsInfo->add_storages();
                    pbStorageInfo->set_name(name);
                    pbStorageInfo->set_id(storage_id);
                    ++storage_id;
                }

                // Temperature sensors
                int temp_sensor_id = 0;
                for (const auto &sensor : temperatureSensors) {
                    std::string name;
                    sensor->getName(name);
                    auto pbTempSensorInfo = pbSensorsInfo->add_temp_sensors();
                    pbTempSensorInfo->set_name(name);
                    pbTempSensorInfo->set_id(temp_sensor_id);
                    ++temp_sensor_id;
                }

                buff_send.set_status(
                    static_cast<::payload::Status>(aditof::Status::OK));
                break;
                writer->Write(buff_send);
            }

            case OPEN: {
                int index = buff_recv.sensors_info().image_sensors(0).id();
                aditof::Status status = camDepthSensor[index]->open();
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case START: {
                int index = buff_recv.sensors_info().image_sensors(0).id();
                aditof::Status status = camDepthSensor[index]->start();
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case STOP: {
                int index = buff_recv.sensors_info().image_sensors(0).id();
                aditof::Status status = camDepthSensor[index]->stop();
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case GET_AVAILABLE_FRAME_TYPES: {
                std::vector<aditof::FrameDetails> frameDetails;
                int index = buff_recv.sensors_info().image_sensors(0).id();
                aditof::Status status =
                    camDepthSensor[index]->getAvailableFrameTypes(frameDetails);
                if (!index) {
                    for (auto detail : frameDetails) {
                        auto type = buff_send.add_available_frame_types();
                        type->set_width(detail.width);
                        type->set_height(detail.height);
                        type->set_type(detail.type);
                        type->set_full_data_width(detail.fullDataWidth);
                        type->set_full_data_height(detail.fullDataHeight);
                    }
                } else {
                    for (auto detail : frameDetails) {
                        auto type = buff_send.add_available_frame_types();
                        type->set_width(detail.rgbWidth);
                        type->set_height(detail.rgbHeight);
                        type->set_type(detail.type);
                    }
                }
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case SET_FRAME_TYPE: {
                aditof::FrameDetails details;
                int index = buff_recv.sensors_info().image_sensors(0).id();
                aditof::Status status;

                details.width = buff_recv.frame_type().width();
                details.height = buff_recv.frame_type().height();
                details.type = buff_recv.frame_type().type();
                details.fullDataWidth = buff_recv.frame_type().full_data_width();
                details.fullDataHeight = buff_recv.frame_type().full_data_height();
                details.rgbWidth = buff_recv.frame_type().rgb_width();
                details.rgbHeight = buff_recv.frame_type().rgb_height();
                status = camDepthSensor[index]->setFrameType(details);
                frameDetailsCache[index] = details;

                if (sensorsFrameBuffers[index]) {
                    delete[] sensorsFrameBuffers[index];
                }
                auto width = details.width;
                auto height = details.height;
                if (index == 1) {
                    width = details.rgbWidth;
                    height = details.rgbHeight;
                }
                sensorsFrameBuffers[index] =
                    new uint16_t[width * height * sizeof(uint16_t)];

                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case PROGRAM: {
                size_t programSize = static_cast<size_t>(buff_recv.func_int32_param(0));
                const uint8_t *pdata = reinterpret_cast<const uint8_t *>(
                    buff_recv.func_bytes_param(0).c_str());
                aditof::Status status = camDepthSensor[0]->program(pdata, programSize);
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case GET_FRAME: {
                int index = buff_recv.sensors_info().image_sensors(0).id();
                aditof::BufferInfo bufferInfo;

                int frameHeight;
                int frameWidth;

                if (!index) {
                    frameWidth = frameDetailsCache[index].width;
                    frameHeight = frameDetailsCache[index].height;
                } else {
                    frameWidth = frameDetailsCache[index].rgbWidth;
                    frameHeight = frameDetailsCache[index].rgbHeight;
                }

                aditof::Status status = camDepthSensor[index]->getFrame(
                    sensorsFrameBuffers[index], &bufferInfo);

        #ifdef JETSON
                buff_send.add_int32_payload(0);
                if (!index) {
                    buff_send.add_bytes_payload(sensorsFrameBuffers[index],
                                                frameWidth * frameHeight *
                                                    sizeof(uint16_t) * 2);
                } else {
                    buff_send.add_bytes_payload(sensorsFrameBuffers[index],
                                                frameWidth * frameHeight *
                                                    sizeof(uint16_t));
                }
        #else
                buff_send.add_int32_payload(1);
                buff_send.add_bytes_payload(sensorsFrameBuffers[index],
                                            frameWidth * frameHeight *
                                                sizeof(uint16_t));
        #endif
                buff_send.set_status(payload::Status::OK);
                writer->Write(buff_send);
                break;
            }

            case READ_AFE_REGISTERS: {
                size_t length = static_cast<size_t>(buff_recv.func_int32_param(0));
                const uint16_t *address = reinterpret_cast<const uint16_t *>(
                    buff_recv.func_bytes_param(0).c_str());
                uint16_t *data = new uint16_t[length];
                aditof::Status status =
                    camDepthSensor[0]->readAfeRegisters(address, data, length);
                if (status == aditof::Status::OK) {
                    buff_send.add_bytes_payload(data, length * sizeof(uint16_t));
                }
                delete[] data;
                buff_send.set_status(static_cast<::payload::Status>(status));
                
                writer->Write(buff_send);
                break;
            }

            case WRITE_AFE_REGISTERS: {
                size_t length = static_cast<size_t>(buff_recv.func_int32_param(0));
                const uint16_t *address = reinterpret_cast<const uint16_t *>(
                    buff_recv.func_bytes_param(0).c_str());
                const uint16_t *data = reinterpret_cast<const uint16_t *>(
                    buff_recv.func_bytes_param(1).c_str());
                aditof::Status status =
                    camDepthSensor[0]->writeAfeRegisters(address, data, length);
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case STORAGE_OPEN: {
                aditof::Status status;
                std::string msg;
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= storages.size()) {
                    buff_send.set_message("The ID of the storage is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    writer->Write(buff_send);
                    break;
                }

                void *sensorHandle;
                status = camDepthSensor[0]->getHandle(&sensorHandle);
                if (status != aditof::Status::OK) {
                    buff_send.set_message("Failed to obtain handle from depth sensor "
                                        "needed to open storage");
                    buff_send.set_status(::payload::Status::GENERIC_ERROR);
                    writer->Write(buff_send);
                    break;
                }

                status = storages[index]->open(sensorHandle);

                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case STORAGE_READ: {
                aditof::Status status;
                std::string msg;
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= storages.size()) {
                    buff_send.set_message("The ID of the storage is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    break;
                }

                uint32_t address = static_cast<uint32_t>(buff_recv.func_int32_param(1));
                size_t length = static_cast<size_t>(buff_recv.func_int32_param(2));
                std::unique_ptr<uint8_t[]> buffer(new uint8_t[length]);
                status = storages[index]->read(address, buffer.get(), length);
                if (status == aditof::Status::OK) {
                    buff_send.add_bytes_payload(buffer.get(), length);
                }
                buff_send.set_status(static_cast<::payload::Status>(status));
                break;
            }

            case STORAGE_WRITE: {
                aditof::Status status;
                std::string msg;
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= storages.size()) {
                    buff_send.set_message("The ID of the storage is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    break;
                }

                uint32_t address = static_cast<uint32_t>(buff_recv.func_int32_param(1));
                size_t length = static_cast<size_t>(buff_recv.func_int32_param(2));
                const uint8_t *buffer = reinterpret_cast<const uint8_t *>(
                    buff_recv.func_bytes_param(0).c_str());

                status = storages[index]->write(address, buffer, length);
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case STORAGE_CLOSE: {
                aditof::Status status;
                std::string msg;
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= storages.size()) {
                    buff_send.set_message("The ID of the storage is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    break;
                }

                status = storages[index]->close();

                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case TEMPERATURE_SENSOR_OPEN: {
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= temperatureSensors.size()) {
                    buff_send.set_message(
                        "The ID of the temperature sensor is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    break;
                }

                void *sensorHandle;
                aditof::Status status = camDepthSensor[0]->getHandle(&sensorHandle);
                if (status != aditof::Status::OK) {
                    buff_send.set_message("Failed to obtain handle from depth sensor "
                                        "needed to open temperature sensor");
                    buff_send.set_status(::payload::Status::GENERIC_ERROR);
                    break;
                }

                status = temperatureSensors[index]->open(sensorHandle);

                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case TEMPERATURE_SENSOR_READ: {
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= temperatureSensors.size()) {
                    buff_send.set_message(
                        "The ID of the temperature sensor is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    break;
                }

                float temperature;
                aditof::Status status = temperatureSensors[index]->read(temperature);
                if (status == aditof::Status::OK) {
                    buff_send.add_float_payload(temperature);
                }
                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case TEMPERATURE_SENSOR_CLOSE: {
                size_t index = static_cast<size_t>(buff_recv.func_int32_param(0));

                if (index < 0 || index >= temperatureSensors.size()) {
                    buff_send.set_message(
                        "The ID of the temperature sensor is invalid");
                    buff_send.set_status(::payload::Status::INVALID_ARGUMENT);
                    break;
                }

                aditof::Status status = temperatureSensors[index]->close();

                buff_send.set_status(static_cast<::payload::Status>(status));
                writer->Write(buff_send);
                break;
            }

            case HANG_UP: {
                if (sensors_are_created) {
                    cleanup_sensors();
                }
                //writer->Write(buff_send);
                break;
            }

            case GET_CONNECTION_STRING: {
                buff_send.set_message(
                    aditof::getVersionString(aditof::ConnectionType::NETWORK));
                buff_send.set_status(
                    static_cast<::payload::Status>(aditof::Status::OK));
                break;
            }

            case GET_CAMERA_TYPE: {
                auto sensorsEnumerator =
                    aditof::SensorEnumeratorFactory::buildTargetSensorEnumerator();
                if (!sensorsEnumerator) {
                    std::string errMsg = "Failed to create a target sensor enumerator";
                    LOG(WARNING) << errMsg;
                    buff_send.set_message(errMsg);
                    buff_send.set_status(
                        static_cast<::payload::Status>(aditof::Status::UNAVAILABLE));
                    break;
                writer->Write(buff_send);
                }

                aditof::CameraType tofCameraType;
                aditof::Status status =
                    sensorsEnumerator->getCameraTypeOnTarget(tofCameraType);
                ::payload::CameraType msgCameraType;
                if (status == aditof::Status::OK) {
                    switch (tofCameraType) {
                    case aditof::CameraType::AD_96TOF1_EBZ:
                        msgCameraType = ::payload::CameraType::AD_96TOF1_EBZ;
                        break;
                    case aditof::CameraType::AD_FXTOF1_EBZ:
                        msgCameraType = ::payload::CameraType::AD_FXTOF1_EBZ;
                        break;
                    case aditof::CameraType::SMART_3D_CAMERA:
                        msgCameraType = ::payload::CameraType::SMART_3D_CAMERA;
                        break;
                    }
                    buff_send.set_camera_type(msgCameraType);
                    writer->Write(buff_send);
                }

                buff_send.set_status(
                    static_cast<::payload::Status>(aditof::Status::OK));
                writer->Write(buff_send);
                break;
            }

            default: {
                std::string msgErr = "Function not found";
                std::cout << msgErr << "\n";

                buff_send.set_message(msgErr);
                buff_send.set_server_status(::payload::ServerStatus::REQUEST_UNKNOWN);
                writer->Write(buff_send);
                break;
            }
            } // switch

            buff_recv.Clear();
        }
        void Initialize() {
            s_map_api_Values["FindSensors"] = FIND_SENSORS;
            s_map_api_Values["Open"] = OPEN;
            s_map_api_Values["Start"] = START;
            s_map_api_Values["Stop"] = STOP;
            s_map_api_Values["GetAvailableFrameTypes"] = GET_AVAILABLE_FRAME_TYPES;
            s_map_api_Values["SetFrameType"] = SET_FRAME_TYPE;
            s_map_api_Values["Program"] = PROGRAM;
            s_map_api_Values["GetFrame"] = GET_FRAME;
            s_map_api_Values["ReadAfeRegisters"] = READ_AFE_REGISTERS;
            s_map_api_Values["WriteAfeRegisters"] = WRITE_AFE_REGISTERS;
            s_map_api_Values["StorageOpen"] = STORAGE_OPEN;
            s_map_api_Values["StorageRead"] = STORAGE_READ;
            s_map_api_Values["StorageWrite"] = STORAGE_WRITE;
            s_map_api_Values["StorageClose"] = STORAGE_CLOSE;
            s_map_api_Values["TemperatureSensorOpen"] = TEMPERATURE_SENSOR_OPEN;
            s_map_api_Values["TemperatureSensorRead"] = TEMPERATURE_SENSOR_READ;
            s_map_api_Values["TemperatureSensorClose"] = TEMPERATURE_SENSOR_CLOSE;
            s_map_api_Values["HangUp"] = HANG_UP;
            s_map_api_Values["GetVersionString"] = GET_CONNECTION_STRING;
            s_map_api_Values["GetCameraType"] = GET_CAMERA_TYPE;
        }   
}


int main() {
  
  RunServer();

  return 0;
}
