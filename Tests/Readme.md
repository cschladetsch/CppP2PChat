# Tests Directory

This directory contains comprehensive unit tests for all P2P Chat System components.

## Test Files

### TestCrypto.cpp
Tests for cryptographic operations:
- Key pair generation and validation
- Digital signature creation and verification
- Peer ID generation consistency
- Invalid signature detection
- Key uniqueness verification
- Large data signing
- Performance benchmarks

### TestMessage.cpp
Tests for message protocol:
- Serialization and deserialization
- All message type factories
- Edge cases (empty, large, invalid payloads)
- Unicode and binary data handling
- Timestamp accuracy
- Protocol format validation
- Error handling

### TestPeerManager.cpp
Tests for peer management:
- Thread-safe operations
- Peer addition and removal
- Connection state tracking
- Persistence (save/load)
- Concurrent access patterns
- Edge cases (duplicates, invalid data)
- Performance under load

### TestNetwork.cpp
Tests for network operations:
- TCP connection establishment
- Message sending and receiving
- Large message handling
- Rapid connection/disconnection
- Concurrent connections
- Error conditions
- Timeout handling
- Bidirectional communication

### TestCliInterface.cpp
Tests for command-line interface:
- Component lifecycle
- Start/stop operations
- Concurrent stop handling
- Resource cleanup

## Test Coverage

Current test suite includes:
- 55 total tests across all components
- Edge case coverage
- Error condition testing
- Performance benchmarks
- Thread safety validation
- Integration scenarios

## Running Tests

### All Tests
```bash
cd build
ctest --verbose
```

### Individual Test Suites
```bash
./Bin/TestCrypto
./Bin/TestMessage
./Bin/TestPeerManager
./Bin/TestNetwork
./Bin/TestCliInterface
```

### With Debugging
```bash
./Bin/TestCrypto --gtest_filter=CryptoTest.GenerateKeyPair
```

## Test Patterns

### Setup/Teardown
- Each test class uses SetUp() and TearDown()
- Proper resource cleanup
- Isolated test environments

### Assertions
- EXPECT_* for non-critical checks
- ASSERT_* for critical preconditions
- Custom matchers where appropriate

### Thread Safety Testing
- Multiple threads performing operations
- Race condition detection
- Deadlock prevention validation

## Adding New Tests

1. Create test file following naming convention: Test*.cpp
2. Include necessary headers and gtest/gtest.h
3. Use TEST or TEST_F macros
4. Follow existing patterns for consistency
5. Ensure proper cleanup in all paths
6. Add to CMakeLists.txt

## Best Practices

- One logical assertion per test
- Descriptive test names
- Test both success and failure paths
- Use test fixtures for shared setup
- Mock external dependencies when needed
- Keep tests fast and deterministic