#!/bin/bash

# Make sure the repository is what you want.
# If not, change them (openairinterface5g/openairinterface5g_joon/openairinterface5g_shuffle/.etc)
# Add two lines in the user’s sudoers file (run sudo visudo):
# <yourusername> ALL=(ALL) NOPASSWD: </full/path/to/nr-softmodem>
# <yourusername> ALL=(ALL) NOPASSWD: </full/path/to/nr-uesoftmodem>

# (It’s best to put your custom sudoers rule after all the default configuration and includes, but above the @includedir /etc/sudoers.d line.)

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

### Terminal 4: Check ifconfig for oaitun_ue1
# gnome-terminal -- bash -c "
#     echo '[Terminal 4] Checking for oaitun_ue1 interface ...';
# 
    # Try to get the IP address of oaitun_ue1
#     ip_addr=\$(ifconfig oaitun_ue1 2>/dev/null | grep 'inet ' | awk '{print \$2}');
#     if [ -z \"\$ip_addr\" ]; then
#         echo -e '\e[31m[ERROR] oaitun_ue1 interface is not present or has no IP address.\e[0m';
#     else
#         echo \"oaitun_ue1 IP address: \$ip_addr\";
#     fi
# 
#     echo '[Terminal 4] Check complete. Press ENTER to continue.';
#     read;
#     exec bash
"


