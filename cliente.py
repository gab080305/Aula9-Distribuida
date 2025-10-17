import grpc
import file_processor_pb2
import file_processor_pb2_grpc

def compress_pdf(stub, input_file_path, output_file_path):
    def request_generator():
        with open(input_file_path, 'rb') as f:
            while True:
                chunk = f.read(1024)
                if not chunk:
                    break
                yield file_processor_pb2.FileRequest(
                    file_name=input_file_path.split('/')[-1],
                    file_content=file_processor_pb2.FileChunk(content=chunk),
                    compress_pdf_params=file_processor_pb2.CompressPDFRequest()
                )

    response_iterator = stub.CompressPDF(request_generator())

    with open(output_file_path, 'wb') as out_file:
        try:
            for response in response_iterator:
                # Cada response contem um chunk de FileChunk
                out_file.write(response.file_content.content)
                print(f"Recebido chunk: {len(response.file_content.content)} bytes")
        except grpc.RpcError as e:
            print(f"Erro ao receber dados do servidor: {e.details()}")

    print(f"Arquivo comprimido salvo em: {output_file_path}")

def run_client():
    with grpc.insecure_channel('localhost:50051') as channel:
        stub = file_processor_pb2_grpc.FileProcessorServiceStub(channel)
        input_pdf = "input.pdf"  # Coloque seu arquivo PDF aqui
        output_pdf = "compressed_output.pdf"
        compress_pdf(stub, input_pdf, output_pdf)

if __name__ == '__main__':
    run_client()

