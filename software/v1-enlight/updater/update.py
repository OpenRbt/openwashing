import os
import socket
import fcntl
import struct
import aiohttp
import asyncio
import requests
from requests.adapters import HTTPAdapter
import json
import netifaces as ni
import threading
import time
from loguru import logger

def get_mac_address(interface="eth0", outSize=6):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    info = fcntl.ioctl(s.fileno(), 0x8927, struct.pack('256s', interface[:15].encode('utf-8')))
    s.close()

    mac = ''.join(['%02x' % b for b in info[18:24]])[:outSize * 2]
    return mac

def get_local_ip(interface='wlan0'):
    try:
        ip_info = ni.ifaddresses(interface)
        local_ip = ip_info[ni.AF_INET][0]['addr']
        return local_ip
    except KeyError:
        logger.info("IP-адрес не найден для интерфейса: " + interface)
        raise
    except ValueError:
        logger.info("Некорректное название интерфейса: " + interface)
        raise
    except Exception as e:
        logger.info("Ошибка: " + str(e))
        raise
    

async def scan_network_async(local_ip):
    base_ip_parts = local_ip.split('.')
    base_ip = '.'.join(base_ip_parts[:3]) + '.'

    logger.info("Base IP: " + base_ip)

    tasks = []
    async with aiohttp.ClientSession() as session:
        for i in range(1, 255):
            req_ip = base_ip + str(i)
            task = asyncio.create_task(send_get_request_async(session, req_ip, path='/ping'))
            tasks.append(task)

        for future in asyncio.as_completed(tasks):
            ip = await future
            if ip:
                logger.info("Found server at: " + ip)
                return ip

    logger.info("No server found in the network block.")
    return ''

def send_post_request(url, port='8020', path='/', data={}):
    headers = {
        'Accept': 'application/json',
        'Content-Type': 'application/json',
        'charsets': 'utf-8',
        'User-Agent': 'diae/0.1'
    }

    response = requests.post(f'http://{url}:{port}{path}', headers=headers, data=json.dumps(data))
    logger.info(response)
    return response

def send_get_request(url, port='8020', path='/'):

    headers = {
        'Accept': 'application/json',
        'Content-Type': 'application/json',
        'charsets': 'utf-8',
        'User-Agent': 'diae/0.1'
    }

    response = requests.get(f'http://{url}:{port}{path}', headers=headers,)
    return response.json()

async def send_get_request_async(session, url, port='8020', path='/'):
    try:
        full_url = f'http://{url}:{port}{path}'
        async with session.get(full_url) as response:
            if response.status == 200:
                return url
    except Exception as e:
        logger.info(str(e))
    return None


def send_ping(url, port='8020', path='/', data={}):
    headers = {
        'Accept': 'application/json',
        'Content-Type': 'application/json',
        'charsets': 'utf-8',
        'User-Agent': 'diae/0.1'
    }
    while True:
        response = requests.post(f'http://{url}:{port}{path}', data=json.dumps(data), headers=headers)
        print(response.text)
        time.sleep(1)

if __name__ == '__main__':    

    logger.add('/home/pi/.log/logs.log', compression='zip', rotation='5 MB', retention=5, level='INFO')

    current_wash = "/home/pi/wash"

    if os.path.islink("/home/pi/current_wash"):
        current_wash = os.readlink("/home/pi/current_wash")
    
    logger.info("current_wash: " + current_wash)

    mac_address = get_mac_address()
    interfaces = ni.interfaces()

    logger.info(interfaces)

    hosts = []
    for interface in interfaces:
        if interface != 'lo':
            try:
                hosts.append(get_local_ip(interface))
            except Exception as e:
                pass

    ip = ''
    loop = asyncio.get_event_loop()

    for host in hosts:
        ip = loop.run_until_complete(scan_network_async(host))
        if ip:
            break

    public_key = send_get_request(ip, path='/public-key')['publicKey']

    with open("/home/pi/.ssh/authorized_keys", "r") as keys_file:
        file_content = keys_file.read()

    if file_content.find(public_key) == -1:
        with open("/home/pi/.ssh/authorized_keys", "a+") as keys_file:
            keys_file.write(public_key)

    if not os.path.exists(current_wash) or not os.path.exists(current_wash + "/firmware") or not os.path.exists(current_wash + "/firmware.exe") or not os.path.exists(current_wash + "/firmware/script.lua"):

        logger.info('Start pinging')

        thread = threading.Thread(target=send_ping, args=(ip, '8020', '/ping', {
            'hash': mac_address,
            'currentBalance': 0,
            'currentProgram': 0,
        }))
        thread.daemon = True
        thread.start()

        time.sleep(2)

        logger.info('create task update')

        send_post_request(ip, path='/tasks/create-by-hash', data={
            'hash': mac_address,
            'type': 'update'
        })

        logger.info('create task reboot')

        send_post_request(ip, path='/tasks/create-by-hash', data={
            'hash': mac_address,
            'type': 'reboot'
        })

        time.sleep(10 * 60)