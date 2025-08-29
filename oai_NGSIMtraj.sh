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

sleep 45

### Terminal 2: Start gNB (NR Softmodem)
gnome-terminal --title="gNB" --geometry=80x24-0+0 -- bash -c "
    echo '[Terminal 2] Starting gNB (NR Softmodem)';
    cd ~/openairinterface5g_joon_shuffling/cmake_targets/ran_build/build;
    sudo ./nr-softmodem -O ../../../targets/PROJECTS/GENERIC-NR-5GC/CONF/gnb.sa.band78.fr1.106PRB.usrpb210.conf --gNBs.[0].min_rxtxtime 6 --rfsim --sa | tee ~/logs/gNB.log;
    exec bash
"

sleep 5

### Terminal 3: Start UE Softmodem and log output
gnome-terminal --title="UE" --geometry=80x24-0-0 -- bash -c "
    echo '[Terminal 3] Starting UE Softmodem';
    cd ~/openairinterface5g_joon_shuffling/cmake_targets/ran_build/build;
    sudo ./nr-uesoftmodem -r 106 --numerology 1 --band 78 -C 3619200000 --sa --uicc0.imsi 001010000000001 --rfsim | tee ~/logs/UE.log;
    exec bash
"

sleep 5

# Always start in the script directory
cd ~/openairinterface5g_joon_shuffling/ngsim_250724

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

# 3. Copy ego.py and data folder to the container
sudo docker cp ~/openairinterface5g_joon_shuffling/ngsim_250724/ego.py $container_id:/tmp

# 4. Install python3 and python3-pip inside the container
sudo docker exec -it oai-ext-dn bash -c "apt-get update && apt-get install -y python3 python3-pip"

# Wait till the terminal which shows the installation of python3 and python3-pip exits.

# Check Whether the IP address of oaitun_ue1 is the same one as the variable 'specific_address' in preceding.py. If not, then modify the variable value.

# Manually open a terminal (Terminal 4)
# Runs "sudo docker exec -it oai-ext-dn python3 /tmp/ego.py"

# Manually open another terminal (Terminal 5)
# Runs "cd ~/openairinterface5g_joon_shuffling/ngsim_250724;
#       ../cacc-venv/bin/python3 preceding.py;"





