from scapy.all import sniff, Ether, IP, UDP, Raw
from scapy.all import *
from collections import deque
from threading import Thread
import time
import scapy
import json
import gc
import os
import argparse

watchlst = []
outputlst = deque()
partitionmap = {}

def remove_last_comma(file_name):
    with open(file_name, 'r+') as json_file:
        json_file.seek(0, os.SEEK_END)
        file_size = json_file.tell()
        json_file.seek(file_size - 2)
        last_char = json_file.read(1)
        if last_char == ',':
            json_file.truncate(file_size - 2)
        json_file.close()
        del json_file, file_size, last_char, file_name

def write_to_file(filesize, filepath):
    global outputlst
    my_list = [filepath + "traffic_details1.json", filepath + "traffic_details2.json"]
    index = 0
    for i in my_list:
        with open(i, 'w') as json_file:
            json_file.write('[\n')
            json_file.close()
            del json_file
    # my_list[index]
    while True:
        if os.path.getsize(my_list[index]) <= filesize:
            new_outputlst = outputlst
            # outputlst.clear()
            time.sleep(5)
            with open(my_list[index], 'a') as json_file:
                while new_outputlst:
                    packet = new_outputlst.popleft()
                    json.dump(packet, json_file, indent=4)
                    json_file.write(',')
                    json_file.write('\n')
                json_file.close()
                del json_file, new_outputlst
        else:
            remove_last_comma(my_list[index])
            with open(my_list[index], 'a') as json_file:
                json_file.seek(0, os.SEEK_END)
                json_file.seek(json_file.tell() - 2, os.SEEK_SET)
                json_file.write("\n]")
                json_file.close()
                del json_file
            index = (index + 1) % 2
            with open(my_list[index], 'w') as json_file:
                json_file.write('[\n')
                json_file.close()
                del json_file
            time.sleep(1)
        gc.collect()

def hex_to_ascii(hex_string):
    start_pattern = '07000000'
    end_pattern = '000001'
    
    start_index = hex_string.find(start_pattern) + len(start_pattern)  # +1 to skip the colon after start_pattern
    end_index = hex_string.find(end_pattern, start_index)
    
    if start_index == -1 or end_index == -1:
        return "no_partition"
        # raise ValueError("Input hex string does not contain the required start or end patterns.")
    
    hex_substring = hex_string[start_index:end_index]
    
    ascii_string = ''
    for i in range(0, len(hex_substring), 2):
        hex_byte = hex_substring[i:i+2]
        ascii_char = chr(int(hex_byte, 16))
        if ord(ascii_char) > 127:
            return "no_partition"
        if not ascii_char.isprintable():
            return "no_partition"
        ascii_string += ascii_char
    
    if ascii_string.find("\\u") != -1:
        return "no_partition"
    
    return ascii_string

def dict_callback(packet):
    global partitionmap
    if packet.payload:
        if packet.payload and hasattr(packet.payload, 'load'):
            # print("================================")
            # print("###[ Raw ]###")
            if packet.payload.load[0:4] == b'RTPS':
                partition = packet.payload.load.hex()
                thepartition = partition
                thepartition = hex_to_ascii(thepartition)
                if IP in packet:
                    ipaddr = packet[IP].src
                    if thepartition == "no_partition":
                        return
                    else:
                        if ipaddr not in partitionmap:
                            partitionmap[ipaddr] = thepartition
                            capture_thread = Thread(target=packet_capture_thread, args=(ipaddr,))
                            capture_thread.start()
                            print("start capture, ip = " + ipaddr, ",partition = " + thepartition)



def packet_capture_thread(ipaddr):
    if ipaddr != "10.1.1.148":
        return
    else:
        sniff(iface="eno1", filter=f"src host {ipaddr}", prn=packet_callback)


def packet_callback(packet):
    global outputlst
    global partitionmap

    try:
        print(packet.show())
        # 檢查封包是否包含 IP 和 UDP 層
        if IP in packet:
            ipaddr = packet[IP].src
            timestamp = int(packet.time)

                # 計算總流量
                # frame_length = len(packet)
                # sll_header = 16  # 假設 Linux cooked 捕獲
                # ip_header = 20  # 假設沒有選項的 IPv4 標頭
            try:
                rtps_content = len(packet[UDP].payload)
            except:
                rtps_content = 0

                # total_traffic = frame_length + sll_header + ip_header + rtps_content
                # if UDP in packet:
                #     total_traffic = 1
                # else:
                #     total_traffic = 0

            packet_details = {
                "timestamp": timestamp,
                "total_traffic": len(packet) + rtps_content,
                "dev_partition": partitionmap[ipaddr]
            }
            outputlst.append(packet_details)
    except AttributeError:
        pass

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--savefile", "-s", default="True", help="是否保存文件, default = True")
    parser.add_argument("--interface", "-i", default="any", help="網卡名， default = ALL")
    parser.add_argument("--filter", "-f", default="rtps", help="過濾器封包類型， default = rtps")
    # parser.add_argument("--ipaddress", "-a", default="", help="過濾器ip地址， default = ALL")
    parser.add_argument("--filesize", "-z", default=1000000, help="緩存文件大小的整數值(byte), default = 1000000")
    parser.add_argument("--printable", "-p", default="False", help="是否打印, default = False")
    parser.add_argument("--jsonpath", "-j", default="", help="json文件保存路径, default = \"/\"")
    args = parser.parse_args()
    combinefilter = ""
    count = 0
    if args.filter != "":
        combinefilter = combinefilter + args.filter
        count = count + 1
        
    if args.savefile == "True":
        write_thread = Thread(target=write_to_file, args=(int(args.filesize),args.jsonpath,))
        write_thread.start()
    else:
        pass
    sniff(filter=f"udp", iface="eno1", prn=dict_callback)

if __name__ == "__main__":
    main()

