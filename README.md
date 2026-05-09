# Network Data Collector

A C++ tool designed to collect basic network metrics. This tool is specifically tailored for evaluating internet service provider (ISP) quality by measuring latency jitter, and packet loss across popular web services.

<img width="616" height="309" alt="image" src="https://github.com/user-attachments/assets/da5ad11c-4be8-4a04-b37e-3653f6dd96d1" />

## Features

- **Multi-Site Monitoring**: Automatically tests connectivity to 21 platforms in batches of 5 for speed and efficiency:
  - **Search & Infrastructure**: Google, X (Twitter), Speed.cloudflare.com
  - **Social & Messaging**: TikTok, WhatsApp, Instagram
  - **Streaming & Media**: YouTube, Fast.com (Netflix), Globo.com
  - **E-commerce & AI**: Mercado Livre, UOL, ChatGPT
  - **Gaming**: Steam, Roblox, Fortnite
  - **Music, Finance, Development, Education & Retail**: Spotify, Binance, GitHub, LinkedIn, Coursera, AliExpress
- **Robust Metrics Calculation**:
  - **Manual Latency Sampling**: Calculates Min, Average, and Maximum RTT directly from captured samples to ensure accuracy across different system languages.
  - **Jitter**: High-precision calculation of latency variation between consecutive pings.
  - **Packet Loss**: Tracks reliability with 20 packets per site.
- **Improved Performance**:
  - **Batch Processing**: Executes 5 tests concurrently, reducing total execution time by 3x.
  - **Live Feedback**: Includes a terminal spinner animation to indicate progress during data collection.
- **Smart Status Reporting**:
  - **Online**: Perfect connectivity.
  - **Degraded**: Responses received but with packet loss.
  - **Unreachable**: 100% packet loss/Timeout.
- **Dual Output**:
  - **Live Dashboard**: A clean, formatted terminal interface with proper column alignment.
  - **JSON Export**: Automatically saves structured results to `network_data.json`.

## Requirements

- **Operating System**: Windows 10/11
- **Compiler**: GCC (MinGW-w64)
- **Dependencies**: 
  - `Ws2_32.lib` (Windows Sockets)
  - `Iphlpapi.lib` (IP Helper API)

## Compilation

To compile the project with the application icon and optimizations, use the following commands:

```bash
# Compile the resource file (icon)
windres resource.rc -o resource.o

# Build the executable
g++ main.cpp resource.o -o net_collector.exe -O3 -static -lws2_32 -liphlpapi
```

*Note: The `-static` flag ensures the executable is portable and doesn't require external GCC DLLs.*

## Output Format

The tool generates a `network_data.json` file structured as follows:

```json
[
  {
    "domain": "google.com",
    "status": "online",
    "latency": {
      "min": 15.0,
      "avg": 18.5,
      "max": 25.0
    },
    "jitter": 1.25,
    "packet_loss": 0
  }
]
```

## License

This project is open-source and intended for personal network diagnostic use.
