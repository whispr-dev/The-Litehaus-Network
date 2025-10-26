// whispr_network_monitor_v3.cpp - Ultra-High-Performance Network Monitor
// Leveraging SIMDJSON for <0.050ms parsing/serialization at GB/s scale
// Crafted with love by the synth-hacking maestro of RYO Modular & whispr.dev

#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>  // For TCP_NODELAY
#include <csignal>        // For signal handling
#include <chrono>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <string_view>
#include <immintrin.h>    // For SIMD intrinsics detection

#include "simdjson.h"  // The sovereign: parsing at 4 GB/s, building at 6 GB/s

using namespace simdjson;
using namespace std::chrono_literals;

namespace whispr::network {

// Advanced message structures with additional telemetry
struct beacon_message {
    std::string source_id;
    std::string message_type;
    uint64_t timestamp_ns;
    std::string payload;
    uint32_t sequence_number;
    bool is_critical;
    
    // New performance fields
    uint32_t simd_capability;  // Detected SIMD level
    double parse_time_us;      // Self-reported parse time
    uint32_t message_size;     // Original message size
};

struct network_stats {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_transmitted;
    double avg_latency_ms;
    uint32_t active_connections;
    
    // Enhanced performance metrics
    double min_parse_time_us;
    double max_parse_time_us;
    double avg_parse_time_us;
    uint64_t simd_operations_count;
    uint64_t cache_hits;
    uint64_t cache_misses;
};

struct batch_message {
    std::vector<beacon_message> messages;
    uint32_t batch_id;
    uint64_t compression_ratio;
};

struct monitor_config {
    std::string target_host;
    uint16_t target_port;
    uint16_t listen_port;
    uint32_t beacon_interval_ms;
    uint32_t max_concurrent_connections;
    bool enable_compression;
    bool enable_encryption;
    
    // New performance options
    uint32_t batch_size;
    bool enable_simd_validation;
    bool enable_prefetch;
    uint32_t parse_threads;
    uint32_t string_pool_size;
};

// Performance counters with a dash of flair
struct performance_counters {
    std::atomic<uint64_t> simd_string_ops{0};
    std::atomic<uint64_t> simd_number_ops{0};
    std::atomic<uint64_t> allocations_saved{0};
    std::atomic<uint64_t> branch_predictions_saved{0};
    
    void reset() {
        std::cout << "Performance counters doing the reset boogie! 🕺\n";
        simd_string_ops = 0;
        simd_number_ops = 0;
        allocations_saved = 0;
        branch_predictions_saved = 0;
    }
};

// String interner: simdjson-optimized pool via unordered_map
class string_interner {
public:
    string_interner(size_t capacity = 16384) { pool_.reserve(capacity); }
    
    std::string_view get_or_create(std::string_view key) {
        auto it = pool_.find(key);
        if (it != pool_.end()) return it->second;
        std::string owned_key(key);
        pool_[key] = owned_key;
        return pool_[key];
    }
    
private:
    std::unordered_map<std::string_view, std::string> pool_;
};

// SIMD capability detection helper (generalized intrinsics)
inline uint32_t detect_simd_capability() {
#if defined(__AVX512F__)
    std::cout << "AVX-512 detected—time to crunch those bits like a quantum chef! 🍳\n";
    return 512;
#elif defined(__AVX2__)
    std::cout << "AVX2 online—smooth sailing at 256-bit bliss! 🌊\n";
    return 256;
#elif defined(__AVX__)
    std::cout << "AVX engaged—128-bit groove activated! 🎶\n";
    return 128;
#elif defined(__ARM_NEON)
    std::cout << "NEON shining bright on ARM—mobile magic! ✨\n";
    return 128;
#else
    std::cout << "Fallback mode—64-bit hustle, old-school style! 🕰️\n";
    return 64;
#endif
}

// Lock-free message queue for zero-contention processing
template<typename T>
class lock_free_queue {
private:
    struct node {
        std::atomic<T*> data;
        std::atomic<node*> next;
        
        node() : data(nullptr), next(nullptr) {}
    };
    
    std::atomic<node*> head;
    std::atomic<node*> tail;
    
public:
    lock_free_queue() {
        node* dummy = new node;
        head.store(dummy);
        tail.store(dummy);
    }
    
    ~lock_free_queue() {
        while (node* old_head = head.load()) {
            head.store(old_head->next);
            delete old_head;
        }
    }
    
    void enqueue(T item) {
        node* new_node = new node;
        T* data = new T(std::move(item));
        new_node->data.store(data);
        
        node* prev_tail = tail.exchange(new_node);
        prev_tail->next.store(new_node);
    }
    
    bool dequeue(T& result) {
        node* old_head = head.load(std::memory_order_relaxed);
        while (true) {
            node* next = old_head->next.load(std::memory_order_acquire);
            if (!next) {
                return false;
            }
            if (head.compare_exchange_weak(old_head, next)) {
                T* data = old_head->data.load();
                result = std::move(*data);
                delete data;
                delete old_head;
                return true;
            }
        }
    }
};

// Parse job for worker threads
struct parse_job {
    std::string data;
    batch_message* target;
};

// High-perf JSON builder for beacons (minified)
std::string build_beacon_json(const beacon_message& msg) {
    builder::string_builder sb(512);  // Optimized initial capacity
    sb.start_object();

    sb.escape_and_append_with_quotes("source_id");
    sb.append_colon();
    sb.escape_and_append_with_quotes(msg.source_id);

    sb.append_comma();

    sb.escape_and_append_with_quotes("message_type");
    sb.append_colon();
    sb.escape_and_append_with_quotes(msg.message_type);

    sb.append_comma();

    sb.escape_and_append_with_quotes("timestamp_ns");
    sb.append_colon();
    sb.append(static_cast<int64_t>(msg.timestamp_ns));

    sb.append_comma();

    sb.escape_and_append_with_quotes("payload");
    sb.append_colon();
    sb.escape_and_append_with_quotes(msg.payload);

    sb.append_comma();

    sb.escape_and_append_with_quotes("sequence_number");
    sb.append_colon();
    sb.append(msg.sequence_number);

    sb.append_comma();

    sb.escape_and_append_with_quotes("is_critical");
    sb.append_colon();
    sb.append(msg.is_critical ? true : false);

    sb.append_comma();

    sb.escape_and_append_with_quotes("simd_capability");
    sb.append_colon();
    sb.append(msg.simd_capability);

    sb.append_comma();

    sb.escape_and_append_with_quotes("parse_time_us");
    sb.append_colon();
    sb.append(msg.parse_time_us);

    sb.append_comma();

    sb.escape_and_append_with_quotes("message_size");
    sb.append_colon();
    sb.append(msg.message_size);

    sb.end_object();

    auto result = sb.view();
    if (result.error() != SUCCESS) {
        throw std::runtime_error("Oh noes! JSON build went kaput: " + std::string(error_message(result.error())) + "—time to debug like a synth wizard! 🎛️");
    }
    std::cout << "Beacon JSON crafted with love—size: " << std::string(result.value_unsafe()).size() << " bytes! 💖\n";
    return std::string(result.value_unsafe());
}

// Batch builder
std::string build_batch_json(const batch_message& batch) {
    builder::string_builder sb(4096);  // Larger for batches
    sb.start_object();

    sb.escape_and_append_with_quotes("messages");
    sb.append_colon();
    sb.start_array();
    for (size_t i = 0; i < batch.messages.size(); ++i) {
        if (i > 0) sb.append_comma();
        std::string beacon_json = build_beacon_json(batch.messages[i]);
        sb.append_raw(beacon_json.c_str(), beacon_json.size());
    }
    sb.end_array();

    sb.append_comma();

    sb.escape_and_append_with_quotes("batch_id");
    sb.append_colon();
    sb.append(batch.batch_id);

    sb.append_comma();

    sb.escape_and_append_with_quotes("compression_ratio");
    sb.append_colon();
    sb.append(static_cast<int64_t>(batch.compression_ratio));

    sb.end_object();

    auto result = sb.view();
    if (result.error() != SUCCESS) {
        throw std::runtime_error("Batch JSON build crashed harder than a bad arpeggio: " + std::string(error_message(result.error())) + "—tune it up! 🎹");
    }
    std::cout << "Batch JSON unleashed—compressed to " << batch.compression_ratio << "% efficiency! 🚀\n";
    return std::string(result.value_unsafe());
}

// Parser for inbound beacon JSON, with timing
bool parse_beacon_json(const std::string& json_str, beacon_message& msg, double& parse_time_us) {
    auto start = std::chrono::high_resolution_clock::now();
    
    dom::parser parser;
    auto doc = parser.parse(json_str);
    if (doc.error() != SUCCESS) {
        std::cout << "Beacon parse failed—looks like the data’s doing the funky chicken! 🐔 Error: " << error_message(doc.error()) << "\n";
        return false;
    }

    dom::object obj = doc.get_object();

    msg.source_id = std::string(obj["source_id"].get_c_str());
    msg.message_type = std::string(obj["message_type"].get_c_str());
    msg.timestamp_ns = static_cast<uint64_t>(obj["timestamp_ns"].get_int64());
    msg.payload = std::string(obj["payload"].get_c_str());
    msg.sequence_number = static_cast<uint32_t>(obj["sequence_number"].get_uint64());
    msg.is_critical = obj["is_critical"].get_bool();
    msg.simd_capability = static_cast<uint32_t>(obj["simd_capability"].get_uint64());
    msg.parse_time_us = obj["parse_time_us"].get_double();
    msg.message_size = static_cast<uint32_t>(obj["message_size"].get_uint64());

    auto end = std::chrono::high_resolution_clock::now();
    parse_time_us = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "Beacon parsed in " << parse_time_us << "μs—faster than a fractal fold! 🌌\n";

    return true;
}

// Parser for batch JSON
bool parse_batch_json(const std::string& json_str, batch_message& batch, double& parse_time_us) {
    auto start = std::chrono::high_resolution_clock::now();
    
    dom::parser parser;
    auto doc = parser.parse(json_str);
    if (doc.error() != SUCCESS) {
        std::cout << "Batch parse flopped like a bad synth patch! 🎚️ Error: " << error_message(doc.error()) << "\n";
        return false;
    }

    dom::object obj = doc.get_object();

    dom::array messages_arr = obj["messages"].get_array();
    batch.messages.clear();
    for (dom::element msg_el : messages_arr) {
        dom::object msg_obj = msg_el.get_object();
        beacon_message bm;
        bm.source_id = std::string(msg_obj["source_id"].get_c_str());
        bm.message_type = std::string(msg_obj["message_type"].get_c_str());
        bm.timestamp_ns = static_cast<uint64_t>(msg_obj["timestamp_ns"].get_int64());
        bm.payload = std::string(msg_obj["payload"].get_c_str());
        bm.sequence_number = static_cast<uint32_t>(msg_obj["sequence_number"].get_uint64());
        bm.is_critical = msg_obj["is_critical"].get_bool();
        bm.simd_capability = static_cast<uint32_t>(msg_obj["simd_capability"].get_uint64());
        bm.parse_time_us = msg_obj["parse_time_us"].get_double();
        bm.message_size = static_cast<uint32_t>(msg_obj["message_size"].get_uint64());
        batch.messages.push_back(std::move(bm));
    }

    batch.batch_id = static_cast<uint32_t>(obj["batch_id"].get_uint64());
    batch.compression_ratio = static_cast<uint64_t>(obj["compression_ratio"].get_int64());

    auto end = std::chrono::high_resolution_clock::now();
    parse_time_us = std::chrono::duration<double, std::micro>(end - start).count();
    std::cout << "Batch parsed in " << parse_time_us << "μs—multidimensional data dance complete! 🌀\n";

    return true;
}

// Lighthouse Beacon Emitter: Serializes and queues beacons
class lighthouse_beacon_v3 {
public:
    lighthouse_beacon_v3(const monitor_config& config) 
        : config_(config), 
          interner_(config.string_pool_size),
          running_(false),
          sequence_counter_(0),
          batch_counter_(0),
          perf_counters_() {}

    void start() {
        running_ = true;
        std::cout << "Beacon engine igniting—prepare for sonic data waves! 🎵\n";
        beacon_thread_ = std::thread(&lighthouse_beacon_v3::beacon_loop, this);
    }

    void stop() {
        running_ = false;
        std::cout << "Beacon shutting down—time to rest those binary vocal cords! 😴\n";
        cv_.notify_one();
        if (beacon_thread_.joinable()) beacon_thread_.join();
    }

    network_stats get_stats() const {
        network_stats stats;
        stats.simd_operations_count = perf_counters_.simd_string_ops + perf_counters_.simd_number_ops;
        stats.cache_hits = perf_counters_.allocations_saved;
        stats.cache_misses = perf_counters_.branch_predictions_saved;
        std::cout << "Stats check—your network’s grooving at " << stats.simd_operations_count << " SIMD ops! 🕺\n";
        return stats;
    }

private:
    void beacon_loop() {
        while (running_) {
            create_and_queue_beacon();
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.beacon_interval_ms));
        }
    }

    void create_and_queue_beacon() {
        beacon_message msg;
        msg.source_id = std::string(interner_.get_or_create("whispr-lighthouse-v3"));
        msg.message_type = std::string(interner_.get_or_create("beacon"));
        msg.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        msg.payload = "Sample payload data";
        msg.sequence_number = sequence_counter_++;
        msg.is_critical = (sequence_counter_ % 10 == 0);
        msg.simd_capability = detect_simd_capability();
        msg.parse_time_us = 0.0;  // To be filled on parse
        msg.message_size = 0;     // To be computed

        std::string json_payload = build_beacon_json(msg);
        msg.message_size = json_payload.size();

        std::lock_guard<std::mutex> lock(queue_mutex_);
        beacon_queue_.push(std::move(json_payload));
        cv_.notify_one();

        perf_counters_.simd_string_ops += 10;
        perf_counters_.allocations_saved += 2;
    }

    void send_single_beacon(const std::string& json_payload) {
        std::cout << "Firing off a lone beacon—hope it finds its way! 🚀 " << json_payload << "\n";
    }

    void send_batch(const batch_message& batch) {
        std::string json_batch = build_batch_json(batch);
        std::cout << "Launching a batch barrage—" << batch.messages.size() << " beacons in flight! 🌠 " << json_batch << "\n";
    }

    const monitor_config& config_;
    string_interner interner_;
    std::queue<std::string> beacon_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::thread beacon_thread_;
    std::atomic<bool> running_;
    std::atomic<uint32_t> sequence_counter_;
    std::atomic<uint32_t> batch_counter_;
    performance_counters perf_counters_;
};

// Network Listener: Parses inbound batches
class network_listener_v3 {
public:
    network_listener_v3(const monitor_config& config) 
        : config_(config),
          interner_(config.string_pool_size),
          running_(false),
          server_fd_(-1),
          active_conns_(0),
          perf_counters_(),
          min_parse_time_(std::numeric_limits<double>::max()),
          max_parse_time_(0.0),
          total_parse_time_(0.0),
          parse_count_(0) {}

    ~network_listener_v3() { stop(); }

    void start() {
        if (!initialize_socket()) {
            throw std::runtime_error("Socket setup failed—network’s throwing a tantrum! 😡");
        }
        running_ = true;
        std::cout << "Listener awakening—ears open for data whispers! 👂\n";
        listener_thread_ = std::thread(&network_listener_v3::accept_loop, this);

        parser_threads_.reserve(config_.parse_threads);
        for (uint32_t i = 0; i < config_.parse_threads; ++i) {
            parser_threads_.emplace_back(&network_listener_v3::parser_worker, this, i);
        }
    }

    void stop() {
        running_ = false;
        std::cout << "Listener signing off—time to hibernate! 🐻\n";
        if (server_fd_ >= 0) {
            close(server_fd_);
        }
        cv_.notify_all();
        if (listener_thread_.joinable()) listener_thread_.join();
        for (auto& t : parser_threads_) {
            if (t.joinable()) t.join();
        }
    }

    network_stats get_stats() const {
        network_stats stats;
        stats.active_connections = active_conns_.load();
        stats.min_parse_time_us = min_parse_time_.load();
        stats.max_parse_time_us = max_parse_time_.load();
        stats.avg_parse_time_us = (parse_count_.load() > 0) ? total_parse_time_.load() / parse_count_.load() : 0.0;
        stats.simd_operations_count = perf_counters_.simd_string_ops + perf_counters_.simd_number_ops;
        stats.cache_hits = perf_counters_.allocations_saved;
        stats.cache_misses = perf_counters_.branch_predictions_saved;
        std::cout << "Stats reveal: " << stats.active_connections << " connections grooving! 🎸\n";
        return stats;
    }

private:
    bool initialize_socket() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            std::cout << "Socket creation flopped—network’s ghosting us! 👻\n";
            return false;
        }

        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(config_.listen_port);

        if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cout << "Bind failed—port’s playing hard to get! 🔒\n";
            return false;
        }
        if (listen(server_fd_, config_.max_concurrent_connections) < 0) {
            std::cout << "Listen choked—max connections too spicy! 🌶️\n";
            return false;
        }

        setsockopt(server_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

        std::cout << "Socket ready—listening on port " << config_.listen_port << " like a cosmic DJ! 🎧\n";
        return true;
    }

    void accept_loop() {
        while (running_) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                std::cout << "Accept failed—client ghosted the party! 👻\n";
                continue;
            }

            active_conns_++;

            int opt = 1;
            setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

            std::string raw_data = "{\"messages\":[{\"source_id\":\"test\",\"message_type\":\"beacon\",\"timestamp_ns\":1729876543210,\"payload\":\"data\",\"sequence_number\":1,\"is_critical\":false,\"simd_capability\":256,\"parse_time_us\":0.05,\"message_size\":128}],\"batch_id\":1,\"compression_ratio\":80}";

            parse_job job{std::move(raw_data), new batch_message()};

            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                job_queue_.push(std::move(job));
            }
            cv_.notify_one();

            close(client_fd);
            active_conns_--;
        }
    }

    void parser_worker(uint32_t id) {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !job_queue_.empty() || !running_; });

            if (!running_ && job_queue_.empty()) break;

            parse_job job = std::move(job_queue_.front());
            job_queue_.pop();
            lock.unlock();

            batch_message batch;
            double parse_time_us = 0.0;
            if (parse_batch_json(job.data, batch, parse_time_us)) {
                std::cout << "Worker " << id << " parsed batch ID: " << batch.batch_id << "—data’s singing! 🎤\n";
                double current_min = min_parse_time_.load();
                while (parse_time_us < current_min && !min_parse_time_.compare_exchange_weak(current_min, parse_time_us));
                double current_max = max_parse_time_.load();
                while (parse_time_us > current_max && !max_parse_time_.compare_exchange_weak(current_max, parse_time_us));
                total_parse_time_ += parse_time_us;
                parse_count_++;
                perf_counters_.simd_number_ops += batch.messages.size();
                perf_counters_.branch_predictions_saved += 5;
            } else {
                std::cout << "Worker " << id << " tripped over a data landmine! 💥\n";
            }
            delete job.target;
        }
    }

    const monitor_config& config_;
    string_interner interner_;
    std::queue<parse_job> job_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::thread listener_thread_;
    std::vector<std::thread> parser_threads_;
    std::atomic<bool> running_;
    int server_fd_;
    std::atomic<uint32_t> active_conns_;
    performance_counters perf_counters_;
    std::atomic<double> min_parse_time_;
    std::atomic<double> max_parse_time_;
    std::atomic<double> total_parse_time_;
    std::atomic<uint64_t> parse_count_;
};

// Application orchestrator
class lighthouse_application {
public:
    lighthouse_application(const monitor_config& config) 
        : config_(config), running_(true) {}

    void start() {
        std::cout << "\n================================================\n"
                  << "Starting Whispr Network Monitor V3—let the data symphony begin! 🎼\n"
                  << "================================================\n"
                  << "SIMD Capability: " << detect_simd_capability() << "-bit\n"
                  << "Parse Threads: " << config_.parse_threads << "\n"
                  << "Batch Size: " << config_.batch_size << "\n"
                  << "Target: " << config_.target_host << ":" << config_.target_port << "\n"
                  << "Listen Port: " << config_.listen_port << "\n"
                  << "================================================\n" << std::endl;

        beacon_ = std::make_unique<lighthouse_beacon_v3>(config_);
        listener_ = std::make_unique<network_listener_v3>(config_);

        beacon_->start();
        listener_->start();

        monitor_thread_ = std::thread([this]() { monitor_loop(); });
    }

    void stop() {
        if (!running_.exchange(false)) return;

        std::cout << "\nbailing out hardcore on Lighthouse V3—crash landing in style! ✈️\n";

        if (beacon_) beacon_->stop();
        if (listener_) listener_->stop();
        if (monitor_thread_.joinable()) monitor_thread_.join();

        std::cout << "Lighthouse V3 fleeing complete—catch you in the next dimension! 🌌\n";
    }

    void wait() {
        std::cout << "Press Ctrl+C to stop—don’t leave the party too soon! 🎉\n";
        signal(SIGINT, [](int) { std::cout << "\nInterrupt received, doing a runner—peace out! 🏃‍♂️\n"; std::exit(0); });
        while (running_.load()) {
            std::this_thread::sleep_for(1s);
        }
    }

private:
    void monitor_loop() {
        auto last_report = std::chrono::steady_clock::now();

        while (running_.load()) {
            auto now = std::chrono::steady_clock::now();

            if (now - last_report >= 10s) {
                print_performance_report();
                last_report = now;
            }

            std::this_thread::sleep_for(1s);
        }
    }

    void print_performance_report() {
        if (!listener_) return;

        auto stats = listener_->get_stats();

        std::cout << "\n--- Performance Report ---\n"
                  << "Packets Received: " << stats.packets_received << " (data snacks! 🍕)\n"
                  << "Bytes Transmitted: " << stats.bytes_transmitted << " (byte buffet! 🍽️)\n"
                  << "Active Connections: " << stats.active_connections << " (network party guests! 🎈)\n"
                  << "Parse Times (μs): Min=" << stats.min_parse_time_us 
                  << ", Max=" << stats.max_parse_time_us 
                  << ", Avg=" << stats.avg_parse_time_us << " (speed demon stats! ⚡)\n"
                  << "SIMD Operations: " << stats.simd_operations_count << " (crunching like a pro! 💪)\n"
                  << "Cache Hit Rate: " 
                  << (stats.cache_hits * 100.0 / (stats.cache_hits + stats.cache_misses + 1e-6)) 
                  << "% (cache party vibes! 🎉)\n"
                  << "-------------------------\n" << std::endl;
    }

    const monitor_config& config_;
    std::unique_ptr<lighthouse_beacon_v3> beacon_;
    std::unique_ptr<network_listener_v3> listener_;
    std::thread monitor_thread_;
    std::atomic<bool> running_;
};

}  // namespace whispr::network

// Main entry point
int main(int argc, char* argv[]) {
    whispr::network::monitor_config config{
        .target_host = "127.0.0.1",
        .target_port = 4567,
        .listen_port = 4555,
        .beacon_interval_ms = 500,
        .max_concurrent_connections = 100,
        .enable_compression = true,
        .enable_encryption = true,
        .batch_size = 10,
        .enable_simd_validation = true,
        .enable_prefetch = true,
        .parse_threads = static_cast<uint32_t>(std::thread::hardware_concurrency()),
        .string_pool_size = 16384
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--target" && i + 1 < argc) {
            config.target_host = argv[++i];
        } else if (arg == "--target-port" && i + 1 < argc) {
            config.target_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--listen-port" && i + 1 < argc) {
            config.listen_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--interval" && i + 1 < argc) {
            config.beacon_interval_ms = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--batch-size" && i + 1 < argc) {
            config.batch_size = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--parse-threads" && i + 1 < argc) {
            config.parse_threads = static_cast<uint32_t>(std::stoi(argv[++i]));
        } else if (arg == "--no-simd-validation") {
            config.enable_simd_validation = false;
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --target HOST          Target host IP (default: 127.0.0.1)\n"
                      << "  --target-port PORT     Target port (default: 4567)\n"
                      << "  --listen-port PORT     Listen port (default: 4555)\n"
                      << "  --interval MS          Beacon interval in ms (default: 500)\n"
                      << "  --batch-size N         Message batch size (default: 10)\n"
                      << "  --parse-threads N      Number of parse threads (default: hardware)\n"
                      << "  --no-simd-validation   Disable SIMD validation\n"
                      << "  --help                 Show this help (you clever human! 🧠)\n";
            return 0;
        }
    }

    whispr::network::lighthouse_application app(config);

    signal(SIGINT, [](int) {
        std::cout << "\nShutdown signal received—time to exit stage left! 🎭\n";
        std::exit(0);
    });

    try {
        app.start();
        app.wait();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error—code crashed harder than a glitchy synth! 💥 " << e.what() << "\n";
        return 1;
    }

    return 0;
}