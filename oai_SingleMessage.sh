#!/bin/bash

# Add following lines in the user’s sudoers file (run sudo visudo):
# <yourusername> ALL=(ALL) NOPASSWD: </full/path/to/nr-softmodem>
# <yourusername> ALL=(ALL) NOPASSWD: </full/path/to/nr-uesoftmodem>
# <yourusername> ALL=(ALL) NOPASSWD: </usr/bin/docker cp *, /usr/bin/docker exec *, /usr/bin/docker ps, /usr/bin/python3>
# (if your docker and python3 are installed in another folder other than /usr/bin, change the paths correspondingly)


### Terminal 1: Start Core Network (Docker Compose)
gnome-terminal --title="Core Network" --geometry=200x24+0+0 -- bash -c "
    echo '[Terminal 1] Make sure Core Network is down';
    cd ~/oai-cn5g;
    docker compose down;
    sleep 10;
    containers_to_check='oai-upf oai-ext-dn ims oai-smf oai-amf oai-ausf oai-udm oai-udr oai-nrf mysql'
    check_down_count=0
    max_down_checks=3
    while [ \$check_down_count -lt \$max_down_checks ]; do
        not_stopped=''
        for cname in \$containers_to_check; do
            if docker ps --format '{{.Names}}' | grep -wq \"\$cname\"; then
                not_stopped=\"\$not_stopped \$cname\"
            fi
        done
        if [ -z \"\$not_stopped\" ]; then
            echo '[Terminal 1] All core network containers are stopped.'
            break
        else
            ((check_down_count++))
            if [ \$check_down_count -lt \$max_down_checks ]; then
                echo \"[Terminal 1] Waiting for the following containers to stop:\$not_stopped\"
                sleep 5
            fi
        fi
    done
    if [ ! -z \"\$not_stopped\" ]; then
        echo -e '\e[31m[ERROR] Containers still running after docker compose down:\$not_stopped\e[0m'
        echo '[Terminal 1] Cannot start Core Network because containers are still running. Press ENTER to continue.'
        read
        exec bash
    fi

    echo '[Terminal 1] Starting Core Network';
    cd ~/oai-cn5g;
    docker compose up -d;
    sleep 15;
    docker ps;
    check_count=0
    max_checks=3
    while [ \$check_count -lt \$max_checks ]; do
        all_healthy=true
        unhealthy_list=''
        for cname in \$containers_to_check; do
            status=\$(docker ps --filter \"name=^/\$cname\$\" --format '{{.Status}}')
            if [[ ! \$status =~ \(healthy\) ]]; then
                all_healthy=false
                unhealthy_list=\"\$unhealthy_list \$cname\"
            fi
            if [[ \$status =~ health:\ starting ]]; then
                all_healthy=false
                unhealthy_list=\"\$unhealthy_list \$cname\"
            fi
        done
        if \$all_healthy; then
            echo '[Terminal 1] All containers are healthy.'
            break
        else
            ((check_count++))
            if [ \$check_count -lt \$max_checks ]; then
                echo \"[Terminal 1] Waiting for containers to become healthy:\$unhealthy_list\"
                sleep 10
            fi
        fi
    done
    if ! \$all_healthy; then
        echo -e '\e[31m[ERROR] These containers are not healthy after retries:\$unhealthy_list\e[0m'
    fi
    echo '[Terminal 1] Core Network check complete.';
    exec bash
"

sleep 50

### Terminal 2: Start gNB (NR Softmodem)
gnome-terminal -- bash -c "
    echo '[Terminal 2] Starting gNB (NR Softmodem)';
    cd ~/openairinterface5g_joon_shuffling/cmake_targets/ran_build/build;
    sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 --rfsim --sa | tee ~/logs/gNB.log;
    exec bash
"
sleep 5

### Terminal 3: Start UE Softmodem and log output
gnome-terminal -- bash -c "
    echo '[Terminal 3] Starting UE Softmodem';
    cd ~/openairinterface5g_joon_shuffling/cmake_targets/ran_build/build;
    sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --sa --uicc0.imsi 001010000000001 --rfsim | tee ~/logs/UE.log;
    exec bash
"

sleep 5


# Always start in the script directory
cd ~/openairinterface5g_joon_shuffling/ngsim_250724

# Ensure tmux is installed
if ! command -v tmux &> /dev/null; then
    gnome-terminal -- bash -c "echo -e '\e[31m[ERROR] tmux not installed. Run (sudo apt-get install tmux) first.\e[0m'; exec bash"
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
sudo docker exec -it oai-ext-dn bash -c "apt-get update && apt-get install -y python3 python3-pip"

# 5. Modify 'specific_ip' in testSender.py
if grep -q "specific_ip" testSender.py; then
    sed -i "s/^specific_ip *= *.*/specific_ip = '$ip_addr'/" testSender.py
    echo "Updated specific_ip in testSender.py to $ip_addr"
else
    gnome-terminal -- bash -c "echo -e '\e[31m[ERROR] specific_ip variable not found in testSender.py\e[0m'; exec bash"
    exit 1
fi

# 6. Kill previous tmux sessions if they exist
tmux kill-session -t recv 2>/dev/null
tmux kill-session -t send 2>/dev/null

# 7. Start two tmux sessions (windows) in gnome-terminal if not already running
gnome-terminal -- bash -c "echo '[Terminal 4]'; tmux new-session -d -s recv; tmux attach -t recv; exec bash" &
gnome-terminal -- bash -c "echo '[Terminal 5]'; tmux new-session -d -s send; tmux attach -t send; exec bash" &
sleep 2 

# 8. For each attempt, send commands to the same tmux window
for i in 1 2 3
do
    echo "=== Attempt $i ==="
    tmux send-keys -t recv "echo '[Attempt $i] Running testReceiver.py in container...'; sudo docker exec -it oai-ext-dn python3 /tmp/testReceiver.py" C-m
    sleep 2
    tmux send-keys -t send "echo '[Attempt $i] Running testSender.py locally...'; sudo python3 testSender.py" C-m
    if [ $i -lt 3 ]; then
        sleep 5
    fi
done

# 9. Stop the receiver and sender processes (simulate Ctrl+C in both tmux windows)
tmux send-keys -t recv C-c
tmux send-keys -t send C-c

# 10. Experiment Finished
gnome-terminal -- bash -c "echo 'All attempts executed. Experiment is finished.'; exec bash"
pkill -SIGINT -f nr-softmodem
pkill -SIGINT -f nr-uesoftmodem



