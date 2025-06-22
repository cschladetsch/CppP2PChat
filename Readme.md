# C++23 P2P Chat System

A modern peer-to-peer chat application built with C++23, featuring end-to-end encryption, colored terminal output, and vi-like CLI interaction.

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Tests](https://img.shields.io/badge/tests-55%20passing-brightgreen)
![C++](https://img.shields.io/badge/C%2B%2B-23-blue)
![License](https://img.shields.io/badge/license-MIT-blue)

## Features

- **Decentralized P2P Architecture**: Direct peer-to-peer connections without central servers
- **Cryptographic Security**: ECDSA key pairs, digital signatures, and encryption via OpenSSL
- **Modern C++23**: Leverages latest language features for clean, efficient code
- **Colored Terminal UI**: Beautiful colored output with lambda (λ) prompt
- **Vi-like Interface**: Replxx library provides vi keybindings and command history
- **Comprehensive Testing**: 55 unit tests covering all major components
- **Thread-Safe Operations**: Concurrent peer management and network operations
- **Persistent State**: Save and restore peer lists between sessions

## Requirements

- C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 2022+)
- CMake 3.20 or higher
- Boost 1.75+ (system, thread, filesystem, chrono, program_options)
- OpenSSL development libraries
- Internet connection (for automatic dependency fetching)

## Project Structure

```
CppP2PChat/
├── Include/          # Header files
├── Source/           # Implementation files
├── Tests/            # Unit tests
├── external/         # Third-party headers (rang.hpp)
├── build/            # Build output directory
├── CMakeLists.txt    # Build configuration
├── b                 # Quick build script
└── r                 # Run script with tmux
```

## Building

### Quick Build
```bash
./b  # Uses the included build script
```

### Manual Build
```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
```

### Build Options
```bash
cmake -DBUILD_TESTS=OFF ..    # Build without tests
cmake -DCMAKE_BUILD_TYPE=Debug ..  # Debug build
```

All executables are output to `build/Bin/` directory.

## Testing

Run all 55 unit tests:
```bash
cd build
ctest --verbose
```

Run specific test suite:
```bash
./Bin/TestCrypto       # Cryptography tests
./Bin/TestMessage      # Message protocol tests
./Bin/TestPeerManager  # Peer management tests
./Bin/TestNetwork      # Network layer tests
./Bin/TestCliInterface # CLI interface tests
```

## Usage

### Basic Usage
```bash
./build/Bin/p2pchat
```

### Advanced Options
```bash
./build/Bin/p2pchat --port 8081 --connect localhost:8080 --peers-file mypeers.txt
```

### Demo Script
Run two peers in a tmux session:
```bash
./r  # Starts two peers that auto-connect
```

## Commands

### Connection Management
- `connect <address> <port>` - Connect to a peer
- `disconnect <peer_id>` - Disconnect from a peer
- `list` - Show all peers and connection status

### Messaging
- `send <peer_id> <message>` - Send to specific peer
- `broadcast <message>` - Send to all connected peers

### Information
- `info` - Display local peer information
- `help` - Show all available commands
- `quit` or `exit` - Exit the application

### Vi Mode Navigation
- `ESC` - Enter normal mode
- `h,j,k,l` - Vi movement keys
- `i` - Return to insert mode
- `Ctrl+W` - Delete word
- `Ctrl+U` - Delete to beginning of line

## Architecture

### Core Components

- **CryptoManager** - Handles ECDSA key pairs, signatures, and encryption
- **NetworkManager** - Manages TCP connections with Boost.Asio
- **PeerManager** - Thread-safe peer tracking and persistence
- **Message** - Protocol implementation with serialization
- **CLIInterface** - Colored terminal UI with vi-like input

### Message Protocol

```
[Type:1][PayloadSize:4][Timestamp:8][Payload:N]
```

Supported message types:
- TEXT (0x01) - Chat messages
- HANDSHAKE (0x02) - Peer introduction
- PEER_LIST (0x03) - Share known peers
- PING (0x04) - Keepalive
- PONG (0x05) - Keepalive response

## Security

- **Key Generation**: Each peer generates ECDSA keypair on startup
- **Peer Identity**: IDs derived from public key SHA-256 hash
- **Message Signing**: All messages can be digitally signed
- **Encryption**: Point-to-point encryption using ECDH shared secrets
- **No Central Authority**: Fully decentralized trust model

## Dependencies

External dependencies (auto-fetched):
- [Google Test](https://github.com/google/googletest) - Unit testing framework
- [Replxx](https://github.com/AmokHuginnsson/replxx) - Vi-like CLI library
- [rang](https://github.com/agauniyal/rang) - Terminal color library

System dependencies:
- Boost libraries
- OpenSSL
- pthreads

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Add tests for new functionality
4. Ensure all tests pass (`ctest`)
5. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
6. Push to the branch (`git push origin feature/AmazingFeature`)
7. Open a Pull Request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- Boost.Asio for excellent async networking
- OpenSSL for cryptographic primitives
- Google Test for testing framework
- Replxx for vi-like CLI experience
- rang for beautiful terminal colors