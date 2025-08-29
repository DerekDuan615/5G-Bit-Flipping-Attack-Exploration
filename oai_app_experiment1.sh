#!/bin/bash

# Add following lines in the user’s sudoers file (run sudo visudo):
# <yourusername> ALL=(ALL) NOPASSWD: </full/path/to/nr-softmodem>
# <yourusername> ALL=(ALL) NOPASSWD: </full/path/to/nr-uesoftmodem>

# Always start in the script directory
cd ~/openairinterface5g_joon_shuffling/ngsim_250724

# Ensure tmux is installed
if ! command -v tmux &> /dev/null; then
    echo -e "\e[31m[ERROR] tmux not installed. Run 'sudo apt-get install tmux' first.\e[0m"
    exit 1
fi

# 1. Find container ID for oai-ext-dn
container_id=$(docker ps --filter "name=oai-ext-dn" --format "{{.ID}}")
if [ -z "$container_id" ]; then
    gnome-terminal -- bash -c "echo -e '\e[31m[ERROR] oai-ext-dn container not running.\e[0m'; exec bash"
    exit 1
fi
gnome-terminal -- bash -c "echo 'oai-ext-dn container ID: $container_id'; exec bash"

# 2. Find oaitun_ue1 IP address
ip_addr=$(ifconfig oaitun_ue1 2>/dev/null | grep 'inet ' | awk '{print $2}')
if [ -z "$ip_addr" ]; then
    gnome-terminal -- bash -c "echo -e '\e[31m[ERROR] oaitun_ue1 interface not present or has no IP address.\e[0m'; exec bash"
    exit 1
fi
gnome-terminal -- bash -c "echo 'oaitun_ue1 IP address: $ip_addr'; exec bash"

# 3. Copy testReceiver.py to the container
sudo docker cp ~/openairinterface5g_joon_shuffling/ngsim_250724/testReceiver.py $container_id:/tmp/testReceiver.py

# 4. Install python3 and python3-pip inside the container
sudo docker exec -it $container_id bash -c "apt-get update && apt-get install -y python3 python3-pip"

# 5. Modify 'specific_ip' in testSender.py
if grep -q "specific_ip" testSender.py; then
    sed -i "s/^specific_ip *= *.*/specific_ip = '$ip_addr'/" testSender.py
    echo "Updated specific_ip in testSender.py to $ip_addr"
else
    gnome-terminal -- bash -c "echo -e '\e[31m[ERROR] specific_ip variable not found in testSender.py\e[0m'; exec bash"
    exit 1
fi

# 6. Start two tmux sessions (windows) in gnome-terminal if not already running
gnome-terminal -- bash -c "tmux new-session -d -s recv; tmux attach -t recv; exec bash" &
gnome-terminal -- bash -c "tmux new-session -d -s send; tmux attach -t send; exec bash" &
sleep 2 # give terminals time to start

# 7. For each attempt, send commands to the same tmux window
for i in 1 2 3
do
    echo "=== Attempt $i ==="
    tmux send-keys -t recv "echo '[Attempt $i] Running testReceiver.py in container...'; sudo docker exec -it $container_id python3 /tmp/testReceiver.py" C-m
    sleep 2
    tmux send-keys -t send "echo '[Attempt $i] Running testSender.py locally...'; sudo python3 testSender.py" C-m
    if [ $i -lt 3 ]; then
        sleep 5
    fi
done

echo "All attempts executed."


