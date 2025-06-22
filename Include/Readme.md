# Include Directory

This directory contains all header files for the P2P Chat System.

## Header Files

### CliInterface.hpp
Defines the command-line interface class that provides:
- Vi-like input handling using Replxx
- Colored output using rang
- Command parsing and dispatch
- Thread-safe display queuing
- Integration with NetworkManager and PeerManager

### Crypto.hpp
Cryptographic functionality wrapper around OpenSSL:
- ECDSA key pair generation
- Digital signature creation and verification
- Peer ID generation from public keys
- Shared secret derivation using ECDH
- Future support for message encryption/decryption

### Message.hpp
Message protocol definition and serialization:
- Message types enum (TEXT, HANDSHAKE, PEER_LIST, PING, PONG)
- Binary serialization format
- Factory methods for creating specific message types
- Timestamp handling
- Payload management

### Network.hpp
Network layer implementation using Boost.Asio:
- NetworkManager class for managing connections
- Session class for individual peer connections
- Asynchronous TCP operations
- Message routing and broadcasting
- Connection lifecycle management

### PeerManager.hpp
Peer information storage and management:
- PeerInfo structure definition
- Thread-safe peer storage
- Connection status tracking
- Peer persistence (save/load)
- Local peer information management

## Usage

All headers are designed to be included from the project root:

```cpp
#include "CliInterface.hpp"
#include "Crypto.hpp"
#include "Message.hpp"
#include "Network.hpp"
#include "PeerManager.hpp"
```

## Design Principles

1. **Clear Separation**: Each header focuses on a single responsibility
2. **Forward Declarations**: Used where possible to reduce compilation dependencies
3. **RAII**: Resource management follows RAII principles
4. **Thread Safety**: Classes that may be accessed from multiple threads use appropriate synchronization
5. **Modern C++**: Utilizes C++23 features where beneficial

## Dependencies

- Boost.Asio (Network.hpp)
- OpenSSL (Crypto.hpp)
- Replxx (CliInterface.hpp)
- Standard Library containers and algorithms