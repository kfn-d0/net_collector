#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <regex>
#include <cmath>
#include <iomanip>
#include <chrono>
#include <future>
#include <thread>
#include <atomic>
#include <mutex>

#ifdef _WIN32
    #include <winsock2.h>
    #include <iphlpapi.h>
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "ws2_32.lib")
    #define POPEN _popen
    #define PCLOSE _pclose
#else
    #define POPEN popen
    #define PCLOSE pclose
#endif

std::mutex console_mutex;

struct SiteMetrics {
    std::string domain;
    double min_lat = 0;
    double avg_lat = 0;
    double max_lat = 0;
    double jitter = 0;
    int packet_loss = 0;
    bool unreachable = false;
};

class WSASession {
public:
    WSASession() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }
    ~WSASession() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

std::string escape_json(const std::string& s) {
    std::ostringstream o;
    for (auto c : s) {
        if (c == '"') o << "\\\"";
        else if (c == '\\') o << "\\\\";
        else if ((unsigned char)c <= '\x1f') {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c;
        } else {
            o << c;
        }
    }
    return o.str();
}

bool isValidDomain(const std::string& domain) {
    std::regex domain_regex("^([a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,}$");
    std::regex ip_regex("^([0-9]{1,3}\\.){3}[0-9]{1,3}$");
    return std::regex_match(domain, domain_regex) || std::regex_match(domain, ip_regex);
}

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    
    std::unique_ptr<FILE, int (*)(FILE*)> pipe(POPEN(cmd, "r"), PCLOSE);
    
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void parsePing(const std::string& output, SiteMetrics& metrics) {
    bool is_english = (output.find("Average =") != std::string::npos || output.find("time=") != std::string::npos);
    
    std::regex time_regex;
    if (is_english) {
        time_regex = std::regex("time[=<]([0-9]+(?:\\.[0-9]+)?)\\s*ms");
    } else {
        time_regex = std::regex("tempo[=<]([0-9]+(?:\\.[0-9]+)?)\\s*ms");
    }

    std::vector<double> latencies;
    auto words_begin = std::sregex_iterator(output.begin(), output.end(), time_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        latencies.push_back(std::stod(match[1].str()));
    }

    if (latencies.empty()) {
        metrics.unreachable = true;
        metrics.packet_loss = 100;
        return;
    }

    if (latencies.size() > 1) {
        double total_diff = 0;
        for (size_t i = 1; i < latencies.size(); ++i) {
            total_diff += std::abs(latencies[i] - latencies[i - 1]);
        }
        metrics.jitter = total_diff / (latencies.size() - 1);
    }

    if (!latencies.empty()) {
        double min_val = latencies[0];
        double max_val = latencies[0];
        double sum = 0;
        for (double l : latencies) {
            if (l < min_val) min_val = l;
            if (l > max_val) max_val = l;
            sum += l;
        }
        metrics.min_lat = min_val;
        metrics.max_lat = max_val;
        metrics.avg_lat = sum / latencies.size();
    }

    std::regex loss_regex("([0-9]+)%");
    std::smatch loss_match;
    if (std::regex_search(output, loss_match, loss_regex)) {
        metrics.packet_loss = std::stoi(loss_match[1].str());
    }
}

void printDashboardHeader() {
    std::cout << "\n================================================================" << std::endl;
    std::cout << " NETWORK DATA COLLECTOR v1.1 " << std::endl;
    std::cout << "================================================================" << std::endl;
    std::cout << std::left << std::setw(25) << "Domain" 
              << std::setw(20) << "Lat(min/avg/max)" 
              << std::setw(10) << "Jitter" 
              << std::setw(8) << "Loss" << std::endl;
    std::cout << "---------------------------------------------------------------------------" << std::endl;
}

void printSiteData(const SiteMetrics& m) {
    std::lock_guard<std::mutex> lock(console_mutex);
    std::string lat_str;
    if (m.unreachable) {
        lat_str = "TIMEOUT";
    } else {
        lat_str = std::to_string((int)m.min_lat) + "/" + std::to_string((int)m.avg_lat) + "/" + std::to_string((int)m.max_lat);
    }

    std::cout << std::left << std::setw(25) << m.domain 
              << std::setw(20) << lat_str
              << std::fixed << std::setprecision(1) << std::setw(10) << m.jitter
              << std::setw(8) << (std::to_string(m.packet_loss) + "%") << std::endl;
}

void showSpinner(std::atomic<bool>& done, const std::string& msg) {
    const char spinner[] = {'|', '/', '-', '\\'};
    int i = 0;
    while (!done) {
        {
            std::lock_guard<std::mutex> lock(console_mutex);
            std::cout << "\r" << msg << " " << spinner[i % 4] << "  " << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        i++;
    }
    {
        std::lock_guard<std::mutex> lock(console_mutex);
        std::cout << "\r" << std::string(msg.length() + 10, ' ') << "\r" << std::flush;
    }
}

SiteMetrics processSite(const std::string& domain) {
    SiteMetrics metrics;
    metrics.domain = domain;

    if (!isValidDomain(domain)) {
        metrics.unreachable = true;
        metrics.packet_loss = 100;
        return metrics;
    }

#ifdef _WIN32
    std::string ping_cmd = "ping " + domain + " -n 20";
#else
    std::string ping_cmd = "ping -c 20 " + domain;
#endif

    std::string ping_out = exec(ping_cmd.c_str());
    parsePing(ping_out, metrics);

    return metrics;
}

int main() {
    try {
        WSASession wsa;
        
        std::vector<std::string> sites = {
            "google.com",
            "tiktok.com",
            "web.whatsapp.com",
            "uol.com.br",
            "chatgpt.com",
            "x.com",
            "speed.cloudflare.com",
            "youtube.com",
            "fast.com",
            "www.mercadolivre.com.br",
            "globo.com",
            "instagram.com",
            "steampowered.com",
            "roblox.com",
            "www.fortnite.com",
            "open.spotify.com",
            "www.binance.com",
            "github.com",
            "www.linkedin.com",
            "www.coursera.org",
            "pt.aliexpress.com"
        };

        std::vector<SiteMetrics> results;

        printDashboardHeader();

        for (size_t i = 0; i < sites.size(); i += 5) {
            std::vector<std::future<SiteMetrics>> futures;
            std::atomic<bool> batch_done(false);
            
            std::string batch_msg = "Processing batch " + std::to_string((i / 5) + 1) + " of " + std::to_string((sites.size() + 4) / 5) + "...";
            std::thread spinner_thread(showSpinner, std::ref(batch_done), batch_msg);

            for (size_t j = i; j < i + 5 && j < sites.size(); ++j) {
                futures.push_back(std::async(std::launch::async, processSite, sites[j]));
            }

            std::vector<SiteMetrics> batch_results;
            for (auto& f : futures) {
                batch_results.push_back(f.get());
            }

            batch_done = true;
            if (spinner_thread.joinable()) spinner_thread.join();

            for (const auto& m : batch_results) {
                results.push_back(m);
                printSiteData(m);
            }
        }

        std::cout << "---------------------------------------------------------------------" << std::endl;
        std::cout << "Saving results to network_data.json..." << std::endl;

        std::ofstream file("network_data.json");
        if (!file.is_open()) {
            std::cerr << "Error: Could not create network_data.json" << std::endl;
            return 1;
        }
        file << "[\n";
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            std::string status = r.unreachable ? "unreachable" : (r.packet_loss > 0 ? "degraded" : "online");

            file << "  {\n";
            file << "    \"domain\": \"" << escape_json(r.domain) << "\",\n";
            file << "    \"status\": \"" << status << "\",\n";
            file << "    \"latency\": {\n";
            file << "      \"min\": " << r.min_lat << ",\n";
            file << "      \"avg\": " << r.avg_lat << ",\n";
            file << "      \"max\": " << r.max_lat << "\n";
            file << "    },\n";
            file << "    \"jitter\": " << std::fixed << std::setprecision(2) << r.jitter << ",\n";
            file << "    \"packet_loss\": " << r.packet_loss << "\n";
            file << "  }" << (i == results.size() - 1 ? "" : ",") << "\n";
        }
        file << "]";
        file.close();

        std::cout << "Done! Press Enter to exit." << std::endl;
        std::cin.get();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
