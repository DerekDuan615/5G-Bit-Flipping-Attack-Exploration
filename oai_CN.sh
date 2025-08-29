#!/bin/bash

### Terminal 1: Start Core Network (Docker Compose)
gnome-terminal -- bash -c "
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
    echo '[Terminal 1] Core Network check complete. Press ENTER to continue.';
    read;
    exec bash
"



# "GNOME" is GNU Network Object Model Environment. It is a terminal emulator application, a graphical program which creates a window for you to interact with a command-line shell.
# When you click the "terminal" icon in your Ubuntu machine, you are starting the GNOME application.
# So, it is usually pre-installed on any Ubuntu machine.

# (1) Open a new GNOME terminal and starts a bash shell.
#     GNOME terminal provides the window and interface,
#     bash shell is the interpreter of your commands.
#     Usually bash shell is your default interpreter.
#     "-c" option means "command"
# (2) "echo" is the command which is used by bash.
#     It prints out specified text or variable values on the command line.
# (3) "read" command in bash waits for user input.
#     It pauses the script's execution and waits for you to type sth. and press ENTER
# (4) "exec bash" replaces the current shell and gives you a fresh Bash session,
#     so the terminal stays open for interactive use after everything above completes.
