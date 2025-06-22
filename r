#!/bin/bash

# λ P2P Chat Demo Runner - builds and starts two peers in tmux

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${MAGENTA}${BOLD}λ${NC} ${CYAN}P2P Chat Demo Runner${NC}"
echo -e "${BLUE}═══════════════════════${NC}"

# Kill any existing p2pchat processes
echo -e "${YELLOW}→ Cleaning up existing processes...${NC}"
pkill -f "p2pchat" 2>/dev/null || true
sleep 1

# Build the application
echo -e "${YELLOW}→ Building application...${NC}"
./b

# Check if build was successful
if [ ! -f "build/Bin/p2pchat" ]; then
    echo -e "${RED}✗ Build failed! Executable not found.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Build successful!${NC}"

# Kill any existing tmux session
tmux kill-session -t p2pchat 2>/dev/null || true

# Kill processes using our ports
echo -e "${YELLOW}→ Freeing up ports...${NC}"
fuser -k 9090/tcp 2>/dev/null || true
fuser -k 9091/tcp 2>/dev/null || true
sleep 1

# Use different ports to avoid conflicts
PORT1=9090
PORT2=9091

# Create a new tmux session with two panes
echo -e "${GREEN}→ Starting tmux session with two peers...${NC}"
echo -e "${CYAN}  • Peer 1: Port ${BOLD}$PORT1${NC}"
echo -e "${CYAN}  • Peer 2: Port ${BOLD}$PORT2${NC} ${GREEN}(auto-connecting to Peer 1)${NC}"

# Create new session with first peer
tmux new-session -d -s p2pchat -n peers

# Start first peer on port 9090
tmux send-keys -t p2pchat:peers "cd $(pwd)" C-m
tmux send-keys -t p2pchat:peers "clear" C-m
tmux send-keys -t p2pchat:peers "printf '\\033[35m\\033[1mλ\\033[0m \\033[36mPeer 1\\033[0m - Port \\033[1m$PORT1\\033[0m\\n'" C-m
tmux send-keys -t p2pchat:peers "printf '\\033[34m═══════════════════\\033[0m\\n'" C-m
tmux send-keys -t p2pchat:peers "./build/Bin/p2pchat --port $PORT1" C-m

# Split window vertically for second peer
tmux split-window -h -t p2pchat:peers

# Wait a moment for first peer to start
sleep 2

# Start second peer on port 9091 and connect to first peer
tmux send-keys -t p2pchat:peers "cd $(pwd)" C-m
tmux send-keys -t p2pchat:peers "clear" C-m
tmux send-keys -t p2pchat:peers "printf '\\033[35m\\033[1mλ\\033[0m \\033[36mPeer 2\\033[0m - Port \\033[1m$PORT2\\033[0m\\n'" C-m
tmux send-keys -t p2pchat:peers "printf '\\033[34m═══════════════════\\033[0m\\n'" C-m
tmux send-keys -t p2pchat:peers "./build/Bin/p2pchat --port $PORT2 --connect localhost:$PORT1" C-m

# Wait for connection
sleep 2

# Send initial commands to establish connection
echo -e "${YELLOW}→ Verifying peer connection...${NC}"
sleep 1
# Peer 2 should already be connected via --connect flag, but let's verify
tmux send-keys -t p2pchat:peers.1 "connect localhost $PORT1" C-m
sleep 1
tmux send-keys -t p2pchat:peers.0 "list" C-m
sleep 0.5
tmux send-keys -t p2pchat:peers.1 "list" C-m

# Set pane titles
tmux select-pane -t p2pchat:peers.0 -T "λ Peer 1 (Port $PORT1)"
tmux select-pane -t p2pchat:peers.1 -T "λ Peer 2 (Port $PORT2)"

# Enable pane borders and titles
tmux set -t p2pchat pane-border-status top
tmux set -t p2pchat pane-border-format "#{pane_title}"

# Attach to the session
echo ""
echo -e "${GREEN}${BOLD}✓ Ready!${NC} Attaching to tmux session..."
echo ""
echo -e "${YELLOW}${BOLD}Navigation:${NC}"
echo -e "  ${CYAN}•${NC} Switch panes: ${BOLD}Ctrl+B${NC} then ${BOLD}← →${NC}"
echo -e "  ${CYAN}•${NC} Detach: ${BOLD}Ctrl+B${NC} then ${BOLD}D${NC}"
echo -e "  ${CYAN}•${NC} Kill session: ${BOLD}Ctrl+B${NC} then ${BOLD}:kill-session${NC}"
echo ""
echo -e "${YELLOW}${BOLD}Chat Commands:${NC}"
echo -e "  ${CYAN}connect${NC} <address> <port> - Connect to a peer"
echo -e "  ${CYAN}list${NC} - List connected peers"
echo -e "  ${CYAN}send${NC} <peer_id> <message> - Send to specific peer"
echo -e "  ${CYAN}broadcast${NC} <message> - Send to all peers"
echo -e "  ${CYAN}info${NC} - Show local peer info"
echo -e "  ${CYAN}help${NC} - Show all commands"
echo -e "  ${CYAN}quit${NC} - Exit the application"
echo ""
echo -e "${MAGENTA}${BOLD}λ${NC} ${GREEN}Starting chat session...${NC}"
echo ""

tmux attach-session -t p2pchat