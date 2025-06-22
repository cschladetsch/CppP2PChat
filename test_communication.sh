#!/bin/bash

# Test script to verify P2P communication

set -e

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}Testing P2P Chat Communication${NC}"
echo -e "${CYAN}==============================${NC}"

# Kill any existing processes
pkill -f "p2pchat" 2>/dev/null || true
fuser -k 9090/tcp 2>/dev/null || true
fuser -k 9091/tcp 2>/dev/null || true
sleep 1

# Start peer 1 in background
echo -e "${YELLOW}Starting Peer 1 on port 9090...${NC}"
./build/Bin/p2pchat --port 9090 > peer1.log 2>&1 &
PEER1_PID=$!
sleep 2

# Start peer 2 in background
echo -e "${YELLOW}Starting Peer 2 on port 9091...${NC}"
./build/Bin/p2pchat --port 9091 > peer2.log 2>&1 &
PEER2_PID=$!
sleep 2

# Start peer 2 with automatic connection to peer 1
echo -e "${YELLOW}Starting Peer 2 on port 9091 and connecting to Peer 1...${NC}"
kill $PEER2_PID 2>/dev/null || true
sleep 1
./build/Bin/p2pchat --port 9091 --connect localhost:9090 > peer2.log 2>&1 &
PEER2_PID=$!
sleep 3

# Let the connection establish
echo -e "${YELLOW}Waiting for connection to establish...${NC}"
sleep 2

# Check logs
echo -e "${CYAN}Peer 1 Log:${NC}"
tail -20 peer1.log
echo -e "\n${CYAN}Peer 2 Log:${NC}"
tail -20 peer2.log

# Cleanup
kill $PEER1_PID $PEER2_PID 2>/dev/null || true
sleep 1

echo -e "\n${GREEN}Test complete!${NC}"