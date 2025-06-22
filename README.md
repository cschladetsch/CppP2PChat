# C++23 P2P Chat System

A peer-to-peer chat application built with C++23, featuring end-to-end encryption, colored output, and vi-like CLI interaction. Uses boost::asio.

## Features

- **Peer-to-Peer Communication**: Direct connections between peers without a central server
- **End-to-End Encryption**: Uses OpenSSL for secure message encryption
- **Vi-like CLI**: Replxx library provides vi keybindings and command history
- **Colored Output**: Rang library for enhanced terminal output
- **Message Types**: Text messages, handshakes, peer lists, and ping/pong
- **Persistent Peer Storage**: Save and load peer information between sessions

## Dependencies

- C++23 compatible compiler
- CMake 3.20+
- Boost 1.75+ (system, thread, filesystem, chrono)
- OpenSSL
- Internet connection (for fetching replxx and rang.hpp)

## Building

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

To build without tests:
```bash
cmake -DBUILD_TESTS=OFF ..
```

## Running Tests

```bash
cd build
ctest --verbose
```

## Usage

Basic usage:
```bash
./p2pchat
```

With options:
```bash
./p2pchat --port 8081 --connect 192.168.1.100:8080 --peers-file mypeers.txt
```

### Command Line Options

- `-h, --help`: Show help message
- `-p, --port <port>`: Local port to listen on (default: 8080)
- `-c, --connect <address:port>`: Connect to a peer on startup
- `-f, --peers-file <file>`: File to save/load peers (default: peers.txt)

### Interactive Commands

Once running, use these commands in the CLI:

- `connect <address> <port>`: Connect to a new peer
- `disconnect <peer_id>`: Disconnect from a peer
- `list`: Show all known peers and their status
- `send <peer_id> <message>`: Send a message to a specific peer
- `broadcast <message>`: Send a message to all connected peers
- `info`: Display local peer information
- `help`: Show available commands
- `quit` or `exit`: Exit the application

### Vi Mode

The CLI supports vi-like keybindings:
- Press `ESC` to enter normal mode
- Use standard vi movement keys (h, j, k, l)
- Press `i` to return to insert mode

## Architecture

- **CryptoManager**: Handles key generation, encryption/decryption, and signatures using OpenSSL
- **NetworkManager**: Manages TCP connections and message routing using Boost.Asio
- **PeerManager**: Tracks peer information and connection states
- **Message**: Protocol implementation for different message types
- **CLIInterface**: User interface with colored output and vi-like interaction

## Security

- Each peer generates an ECDSA key pair on startup
- Peer IDs are derived from public key hashes
- Messages can be encrypted using recipient's public key
- Digital signatures verify message authenticity

## Testing

The project includes unit tests for:
- Cryptographic operations (key generation, signing, verification)
- Message serialization/deserialization
- Peer management operations

Tests are built with Google Test framework.
