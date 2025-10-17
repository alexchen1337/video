#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <zlib.h>
#include <iomanip>
#include <chrono>

namespace fs = std::filesystem;

constexpr size_t DISCORD_MAX_SIZE = 25 * 1024 * 1024; // 25MB for free users
constexpr size_t CHUNK_SIZE = 20 * 1024 * 1024; // 20MB chunks for safety margin
constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
constexpr int COMPRESSION_LEVEL = 6; // zlib compression level (0-9)

#pragma pack(push, 1)
struct ChunkMetadata {
    char magic[8] = {'D', 'C', 'H', 'U', 'N', 'K', 'V', '1'};
    uint32_t chunk_index;
    uint32_t total_chunks;
    uint64_t original_file_size;
    uint64_t uncompressed_chunk_size;
    uint64_t compressed_size;
    uint32_t filename_length;
    uint32_t crc32_checksum;
};
#pragma pack(pop)

struct ChunkData {
    size_t index;
    std::vector<uint8_t> data;
    std::vector<uint8_t> compressed_data;
    bool processed = false;
};

class ThreadPool {
private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

public:
    ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<typename F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        cv.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto& thread : threads) {
            thread.join();
        }
    }
};

std::vector<uint8_t> compress_data(const std::vector<uint8_t>& input) {
    uLongf compressed_size = compressBound(input.size());
    std::vector<uint8_t> compressed(compressed_size);

    int result = compress2(compressed.data(), &compressed_size,
                          input.data(), input.size(), COMPRESSION_LEVEL);

    if (result != Z_OK) {
        throw std::runtime_error("Compression failed");
    }

    compressed.resize(compressed_size);
    return compressed;
}

std::vector<uint8_t> decompress_data(const std::vector<uint8_t>& input, size_t expected_size) {
    std::vector<uint8_t> decompressed(expected_size);
    uLongf decompressed_size = expected_size;

    int result = uncompress(decompressed.data(), &decompressed_size,
                           input.data(), input.size());

    if (result != Z_OK) {
        throw std::runtime_error("Decompression failed");
    }

    decompressed.resize(decompressed_size);
    return decompressed;
}

uint32_t calculate_crc32(const std::vector<uint8_t>& data) {
    return crc32(0L, data.data(), data.size());
}

void compress_file(const std::string& input_path, const std::string& output_dir) {
    auto start_time = std::chrono::high_resolution_clock::now();

    if (!fs::exists(input_path)) {
        throw std::runtime_error("Input file does not exist");
    }

    fs::create_directories(output_dir);

    std::ifstream input(input_path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open input file");
    }

    input.seekg(0, std::ios::end);
    size_t file_size = input.tellg();
    input.seekg(0, std::ios::beg);

    std::string filename = fs::path(input_path).filename().string();
    size_t num_chunks = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    std::cout << "Input file: " << filename << " (" << file_size << " bytes)\n";
    std::cout << "Chunks to create: " << num_chunks << "\n";
    std::cout << "Compressing with " << std::thread::hardware_concurrency() << " threads...\n\n";

    std::vector<ChunkData> chunks(num_chunks);
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    std::mutex io_mutex;

    for (size_t i = 0; i < num_chunks; ++i) {
        chunks[i].index = i;
        size_t bytes_to_read = std::min(CHUNK_SIZE, file_size - (i * CHUNK_SIZE));
        chunks[i].data.reserve(bytes_to_read);

        size_t bytes_read = 0;
        while (bytes_read < bytes_to_read) {
            size_t to_read = std::min(BUFFER_SIZE, bytes_to_read - bytes_read);
            input.read(reinterpret_cast<char*>(buffer.data()), to_read);
            size_t actual_read = input.gcount();
            chunks[i].data.insert(chunks[i].data.end(), buffer.begin(), buffer.begin() + actual_read);
            bytes_read += actual_read;
        }
    }

    input.close();

    ThreadPool pool(std::thread::hardware_concurrency());
    size_t total_original_size = 0;
    size_t total_compressed_size = 0;

    for (size_t i = 0; i < num_chunks; ++i) {
        pool.enqueue([&chunks, i, &io_mutex]() {
            chunks[i].compressed_data = compress_data(chunks[i].data);
            chunks[i].processed = true;

            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << "Chunk " << (i + 1) << " compressed: "
                     << chunks[i].data.size() << " -> "
                     << chunks[i].compressed_data.size() << " bytes ("
                     << std::fixed << std::setprecision(1)
                     << (100.0 - (100.0 * chunks[i].compressed_data.size() / chunks[i].data.size()))
                     << "% reduction)\n";
        });
    }

    pool.~ThreadPool();

    for (size_t i = 0; i < num_chunks; ++i) {
        if (!chunks[i].processed) {
            throw std::runtime_error("Chunk processing failed");
        }

        ChunkMetadata metadata;
        metadata.chunk_index = static_cast<uint32_t>(i);
        metadata.total_chunks = static_cast<uint32_t>(num_chunks);
        metadata.original_file_size = file_size;
        metadata.uncompressed_chunk_size = chunks[i].data.size();
        metadata.compressed_size = chunks[i].compressed_data.size();
        metadata.filename_length = static_cast<uint32_t>(filename.length());
        metadata.crc32_checksum = calculate_crc32(chunks[i].data);

        std::string chunk_filename = output_dir + "/chunk_" +
                                    std::to_string(i + 1) + "_of_" +
                                    std::to_string(num_chunks) + ".dchunk";

        std::ofstream output(chunk_filename, std::ios::binary);
        if (!output) {
            throw std::runtime_error("Cannot create output chunk file");
        }

        output.write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));
        output.write(filename.c_str(), filename.length());
        output.write(reinterpret_cast<const char*>(chunks[i].compressed_data.data()),
                    chunks[i].compressed_data.size());

        total_original_size += chunks[i].data.size();
        total_compressed_size += chunks[i].compressed_data.size() + sizeof(metadata) + filename.length();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\n=== Compression Summary ===\n";
    std::cout << "Total original size: " << total_original_size << " bytes\n";
    std::cout << "Total compressed size: " << total_compressed_size << " bytes\n";
    std::cout << "Size reduction: " << std::fixed << std::setprecision(2)
              << (100.0 - (100.0 * total_compressed_size / total_original_size)) << "%\n";
    std::cout << "Time elapsed: " << duration.count() << " ms\n";
    std::cout << "Output directory: " << fs::absolute(output_dir).string() << "\n";
}

void decompress_file(const std::string& chunks_dir, const std::string& output_path) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::string> chunk_files;
    for (const auto& entry : fs::directory_iterator(chunks_dir)) {
        if (entry.path().extension() == ".dchunk") {
            chunk_files.push_back(entry.path().string());
        }
    }

    if (chunk_files.empty()) {
        throw std::runtime_error("No chunk files found in directory");
    }

    std::sort(chunk_files.begin(), chunk_files.end());

    std::ifstream first_chunk(chunk_files[0], std::ios::binary);
    ChunkMetadata first_metadata;
    first_chunk.read(reinterpret_cast<char*>(&first_metadata), sizeof(first_metadata));

    if (std::string(first_metadata.magic, 8) != "DCHUNKV1") {
        throw std::runtime_error("Invalid chunk file format");
    }

    std::string original_filename(first_metadata.filename_length, '\0');
    first_chunk.read(&original_filename[0], first_metadata.filename_length);
    first_chunk.close();

    std::string final_output_path = output_path.empty() ? original_filename : output_path;

    std::cout << "Decompressing " << first_metadata.total_chunks << " chunks...\n";
    std::cout << "Original filename: " << original_filename << "\n";
    std::cout << "Output file: " << final_output_path << "\n\n";

    std::vector<ChunkData> chunks(first_metadata.total_chunks);
    std::mutex io_mutex;

    for (size_t i = 0; i < chunk_files.size() && i < first_metadata.total_chunks; ++i) {
        std::ifstream chunk_file(chunk_files[i], std::ios::binary);
        if (!chunk_file) {
            throw std::runtime_error("Cannot open chunk file: " + chunk_files[i]);
        }

        ChunkMetadata metadata;
        chunk_file.read(reinterpret_cast<char*>(&metadata), sizeof(metadata));

        std::string filename(metadata.filename_length, '\0');
        chunk_file.read(&filename[0], metadata.filename_length);

        chunks[metadata.chunk_index].index = metadata.chunk_index;
        chunks[metadata.chunk_index].compressed_data.resize(metadata.compressed_size);
        chunk_file.read(reinterpret_cast<char*>(chunks[metadata.chunk_index].compressed_data.data()),
                       metadata.compressed_size);
    }

    ThreadPool pool(std::thread::hardware_concurrency());

    for (size_t i = 0; i < first_metadata.total_chunks; ++i) {
        pool.enqueue([&chunks, i, &io_mutex, &chunk_files]() {
            std::ifstream chunk_file(chunk_files[i], std::ios::binary);
            ChunkMetadata metadata;
            chunk_file.read(reinterpret_cast<char*>(&metadata), sizeof(metadata));

            chunks[i].data = decompress_data(chunks[i].compressed_data, metadata.uncompressed_chunk_size);

            uint32_t calculated_crc = calculate_crc32(chunks[i].data);
            if (calculated_crc != metadata.crc32_checksum) {
                throw std::runtime_error("CRC32 checksum mismatch for chunk " + std::to_string(i));
            }

            chunks[i].processed = true;

            std::lock_guard<std::mutex> lock(io_mutex);
            std::cout << "Chunk " << (i + 1) << " decompressed and verified\n";
        });
    }

    pool.~ThreadPool();

    std::ofstream output(final_output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot create output file");
    }

    std::vector<uint8_t> buffer;
    buffer.reserve(BUFFER_SIZE);

    for (size_t i = 0; i < first_metadata.total_chunks; ++i) {
        if (!chunks[i].processed) {
            throw std::runtime_error("Chunk decompression failed");
        }

        for (const auto& byte : chunks[i].data) {
            buffer.push_back(byte);
            if (buffer.size() >= BUFFER_SIZE) {
                output.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
                buffer.clear();
            }
        }
    }

    if (!buffer.empty()) {
        output.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    }

    output.close();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\n=== Decompression Summary ===\n";
    std::cout << "File reconstructed successfully\n";
    std::cout << "Time elapsed: " << duration.count() << " ms\n";
    std::cout << "Output file: " << fs::absolute(final_output_path).string() << "\n";
}

void print_usage(const char* program_name) {
    std::cout << "Discord File Compressor - Compress large files for Discord upload\n\n";
    std::cout << "Usage:\n";
    std::cout << "  Compress:   " << program_name << " -c <input_file> <output_dir>\n";
    std::cout << "  Decompress: " << program_name << " -d <chunks_dir> [output_file]\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -c video.mp4 chunks/\n";
    std::cout << "  " << program_name << " -d chunks/ restored_video.mp4\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            print_usage(argv[0]);
            return 1;
        }

        std::string mode = argv[1];

        if (mode == "-c" || mode == "--compress") {
            if (argc < 4) {
                std::cerr << "Error: Missing arguments for compress mode\n";
                print_usage(argv[0]);
                return 1;
            }
            compress_file(argv[2], argv[3]);
        }
        else if (mode == "-d" || mode == "--decompress") {
            if (argc < 3) {
                std::cerr << "Error: Missing arguments for decompress mode\n";
                print_usage(argv[0]);
                return 1;
            }
            std::string output = argc > 3 ? argv[3] : "";
            decompress_file(argv[2], output);
        }
        else {
            std::cerr << "Error: Invalid mode '" << mode << "'\n";
            print_usage(argv[0]);
            return 1;
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

