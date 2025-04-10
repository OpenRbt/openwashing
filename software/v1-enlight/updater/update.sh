#!/bin/bash

DECIMAL_SEPARATOR=$(locale decimal_point)
LOG_FILE="/home/$USER/update-logs.log"
MAX_LOG_SIZE=5242880
SERVER_IP_FILE="/home/$USER/server-ip.txt"
CURRENT_WASH_DIR="/home/$USER/wash"
WASH_DIR_LINK="/home/$USER/current_wash"
AUTH_KEYS_FILE="/home/$USER/.ssh/authorized_keys"
INTERFACE="eth0"
PORT="8020"

function log_info {
    echo "$(date +'%Y-%m-%d %H:%M:%S') INFO $1" >> $LOG_FILE
}

function check_log_size {
    if [[ -f "$LOG_FILE" && $(stat -c%s "$LOG_FILE") -gt $MAX_LOG_SIZE ]]; then
        rm $LOG_FILE
    fi
}

function get_mac_address {
    iface=$1
    mac=$(cat /sys/class/net/$iface/address | sed 's/://g')
    echo $mac
}

function get_local_ip {
    iface=$1
    ip=$(ip addr show $iface | grep "inet " | awk '{print $2}' | cut -d/ -f1)
    echo $ip
}

function scan_network {
    local_ip=$1
    base_ip=$(echo $local_ip | cut -d. -f1-3)

    for i in $(seq 0 255); do
        ip="$base_ip.$i"
        response=$(curl -s --connect-timeout "0${DECIMAL_SEPARATOR}5" http://$ip:$PORT/ping)
        if [ $? -eq 0 ]; then
            echo $ip > $SERVER_IP_FILE
            echo $ip
            return
        fi
    done

    echo ""
}

function send_post_request {
    url=$1
    path=$2
    data=$3
    curl -s -X POST -H "Accept: application/json" -H "Content-Type: application/json" \
         -H "User-Agent: diae/0.1" -d "$data" http://$url:$PORT$path
}

function send_get_request {
    url=$1
    path=$2
    curl -s -H "Accept: application/json" -H "Content-Type: application/json" \
         -H "User-Agent: diae/0.1" http://$url:$PORT$path
}

function send_ping {
    url=$1
    data=$2
    while true; do
        curl -s -X POST -H "Accept: application/json" -H "Content-Type: application/json" \
             -H "User-Agent: diae/0.1" -d "$data" http://$url:$PORT/ping
        sleep 1
    done
}

# Main script execution
check_log_size

log_info "Script started"

if [ -L $WASH_DIR_LINK ]; then
    CURRENT_WASH_DIR=$(readlink $WASH_DIR_LINK)
fi

log_info "Current_wash: $CURRENT_WASH_DIR"

mac_address=$(get_mac_address $INTERFACE)

if [ -z "$mac_address" ]; then
    log_info "No valid MAC found"
    exit 1
fi

log_info "MAC: $mac_address"

ip=""

if [ -f "$SERVER_IP_FILE" ]; then
    log_info "Getting server IP from file"
    
    ip=$(cat $SERVER_IP_FILE)

    response=$(curl -s --connect-timeout "0${DECIMAL_SEPARATOR}5" http://$ip:$PORT/ping)

    if [ $? -ne 0 ]; then
        log_info "Server not found by ip received from file"
        ip=""
    fi
fi

if [ -z "$ip" ]; then
    log_info "Getting Server IP via network scan"

    interfaces=$(ls /sys/class/net | grep -v lo)

    for interface in $interfaces; do
        ip=$(get_local_ip $interface)
        
        if [ -z "$ip" ]; then
            log_info "No valid IP found in interface $interface"
            continue
        fi

        log_info "Local IP: $ip in interface $interface"

        ip=$(scan_network $ip)

        if [ -z "$ip" ]; then
            log_info "No server found in interface $interface"
            continue
        fi

        break
    done
fi

if [ -z "$ip" ]; then
    log_info "No server found"
    exit 1
fi

log_info "Server IP: $ip"

public_key=$(send_get_request $ip "/public-key" | jq -r '.publicKey')

if ! grep -q "$public_key" $AUTH_KEYS_FILE; then
    echo "$public_key" >> $AUTH_KEYS_FILE
fi

if [ ! -d "$CURRENT_WASH_DIR" ] || [ ! -d "$CURRENT_WASH_DIR/firmware" ] || [ ! -f "$CURRENT_WASH_DIR/firmware.exe" ] || [ ! -f "$CURRENT_WASH_DIR/firmware/script.lua" ]; then
    log_info "Start pinging"

    send_ping $ip "{\"hash\":\"$mac_address\",\"currentBalance\":0,\"currentProgram\":0}" &

    sleep 2

    log_info "Create task update"
    send_post_request $ip "/tasks/create-by-hash" "{\"hash\":\"$mac_address\",\"type\":\"update\"}"

    log_info "Create task reboot"
    send_post_request $ip "/tasks/create-by-hash" "{\"hash\":\"$mac_address\",\"type\":\"reboot\"}"

    sleep 600
fi

log_info "Script completed"
