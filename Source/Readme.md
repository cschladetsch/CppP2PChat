# Source Directory

This directory contains all implementation files for the P2P Chat System.

## Source Files

### Main.cpp
The application entry point that:
- Parses command-line arguments
- Initializes all core components
- Starts the CLI interface
- Manages graceful shutdown

### CliInterface.cpp
Command-line interface implementation:
- Vi-like input handling with Replxx
- Command parsing and execution
- Colored output with lambda prompt
- Message display queue management
- Thread-safe console operations

### Crypto.cpp
Cryptographic operations implementation:
- ECDSA key pair generation using secp256k1 curve
- Digital signature creation and verification
- SHA-256 hash generation for peer IDs
- ECDH shared secret derivation
- OpenSSL context management

### Message.cpp
Message protocol implementation:
- Binary serialization/deserialization
- Message factory methods
- Timestamp handling
- Protocol format enforcement
- Error handling for malformed messages

### Network.cpp
Network layer implementation:
- Asynchronous TCP server and client
- Session management for peer connections
- Message routing and broadcasting
- Connection lifecycle handling
- Boost.Asio integration

### PeerManager.cpp
Peer information management:
- Thread-safe peer storage
- Connection state tracking
- Peer persistence to disk
- Local peer information
- Peer discovery support

## Implementation Details

### Thread Safety
- All public methods in PeerManager use mutex protection
- NetworkManager uses strand for async operation serialization
- CLIInterface uses mutex for display queue

### Error Handling
- Exceptions for critical errors
- Error codes for network operations
- Graceful degradation for non-critical failures

### Resource Management
- RAII for all resources
- Smart pointers for ownership
- Automatic cleanup in destructors

## Coding Standards

- Modern C++23 features used throughout
- Consistent naming: PascalCase for methods, camelCase for variables
- Clear separation of concerns
- Comprehensive error handling
- No raw pointers or manual memory management

## Dependencies

Each source file includes only necessary headers:
- Standard library headers
- Boost headers as needed
- OpenSSL headers in Crypto.cpp
- Project headers from Include directory