#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <chrono>
#include <ctime>
#include <cstdio> // para std::remove
#include <grpcpp/grpcpp.h>
#include "file_processor.grpc.pb.h" // Arquivo gerado pelo protobuf

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerReaderWriter;
using file_processor::FileProcessorService;
using file_processor::FileRequest;
using file_processor::FileResponse;
using file_processor::FileChunk;

class FileProcessorServiceImpl final : public FileProcessorService::Service {
public:
    Status CompressPDF(ServerContext* context,
                      ServerReaderWriter<FileResponse, FileRequest>* stream) override {
        std::string input_file_path = "/tmp/input_compress.pdf";
        std::string output_file_path = "/tmp/output_compress.pdf";

        std::ofstream input_file_stream(input_file_path, std::ios::binary);
        if (!input_file_stream) {
            LogError("CompressPDF", "N/A", "Falha ao criar arquivo temporário de entrada.");
            return Status(grpc::StatusCode::INTERNAL, "Erro ao criar arquivo temporário de entrada.");
        }

        // Receber stream do cliente e salvar no arquivo temporário
        FileRequest request;
        while (stream->Read(&request)) {
            // Cada request pode conter file_content em chunks
            if (request.has_file_content()) {
                input_file_stream.write(request.file_content().content().data(), request.file_content().content().size());
            }
        }
        input_file_stream.close();

        // Executar comando gs (GhostScript) para compressão
        std::string command = "gs -sDEVICE=pdfwrite -dCompatibilityLevel=1.4 -dPDFSETTINGS=/ebook -dNOPAUSE -dQUIET -dBATCH "
                              "-sOutputFile=" + output_file_path + " " + input_file_path;
        int gs_result = std::system(command.c_str());
        
        if (gs_result != 0) {
            LogError("CompressPDF", "N/A", "Falha na compressão PDF. Código de retorno: " + std::to_string(gs_result));
            return Status(grpc::StatusCode::INTERNAL, "Falha ao comprimir PDF.");
        }

        // Enviar o arquivo comprimido de volta como stream
        std::ifstream output_file_stream(output_file_path, std::ios::binary);
        if (!output_file_stream) {
            LogError("CompressPDF", "N/A", "Falha ao abrir arquivo comprimido para envio.");
            return Status(grpc::StatusCode::INTERNAL, "Erro ao abrir arquivo comprimido.");
        }

        const size_t buffer_size = 1024;
        char buffer[buffer_size];
        while (output_file_stream.good()) {
            output_file_stream.read(buffer, buffer_size);
            std::streamsize bytes_read = output_file_stream.gcount();
            if (bytes_read <= 0) break;

            FileResponse response;
            response.set_file_name("compressed_output.pdf");
            FileChunk* chunk = response.mutable_file_content();
            chunk->set_content(buffer, bytes_read);
            response.set_status_message("Compressão concluída com sucesso.");
            response.set_success(true);

            if (!stream->Write(response)) {
                LogError("CompressPDF", "N/A", "Falha ao enviar chunk para cliente.");
                break;
            }
        }

        output_file_stream.close();

        // Limpar arquivos temporários
        std::remove(input_file_path.c_str());
        std::remove(output_file_path.c_str());

        LogSuccess("CompressPDF", "N/A", "Compressão e envio concluídos.");

        return Status::OK;
    }

private:
    void LogError(const std::string& service_name, const std::string& file_name, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_r(&now_c, &now_tm);
        char timestamp[26];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &now_tm);
        std::ofstream log_file("server.log", std::ios::app);
        if (log_file.is_open()) {
            log_file << "[" << timestamp << "] ERROR - Service: " << service_name
                     << ", File: " << file_name << ", Message: " << message << std::endl;
            log_file.close();
        }
        std::cerr << "[" << timestamp << "] ERROR - Service: " << service_name
                  << ", File: " << file_name << ", Message: " << message << std::endl;
    }

    void LogSuccess(const std::string& service_name, const std::string& file_name, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm;
        localtime_r(&now_c, &now_tm);
        char timestamp[26];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &now_tm);
        std::ofstream log_file("server.log", std::ios::app);
        if (log_file.is_open()) {
            log_file << "[" << timestamp << "] SUCCESS - Service: " << service_name
                     << ", File: " << file_name << ", Message: " << message << std::endl;
            log_file.close();
        }
        std::cout << "[" << timestamp << "] SUCCESS - Service: " << service_name
                  << ", File: " << file_name << ", Message: " << message << std::endl;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    FileProcessorServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Servidor gRPC ouvindo em " << server_address << std::endl;
    server->Wait();
}

int main() {
    RunServer();
    return 0;
}

