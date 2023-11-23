import pyshark
import time
from collections import deque
from threading import Thread
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
    start_pattern = '07:00:00:00'
    end_pattern = '00:00:01'
    
    start_index = hex_string.find(start_pattern) + len(start_pattern) + 1  # +1 to skip the colon after start_pattern
    end_index = hex_string.find(end_pattern, start_index)
    
    if start_index == -1 or end_index == -1:
        return "no_partition"
        # raise ValueError("Input hex string does not contain the required start or end patterns.")
    
    hex_substring = hex_string[start_index:end_index].replace(':', '')
    
    ascii_string = ''
    for i in range(0, len(hex_substring), 2):
        hex_byte = hex_substring[i:i+2]
        ascii_char = chr(int(hex_byte, 16))
        ascii_string += ascii_char

    return ascii_string

def dict_callback(packet):
    global partitionmap
    if packet.rtps is not None:
        partition = packet.rtps.issuedata[:100]
        partition = hex_to_ascii(partition)
        ipaddr = packet.ip.src

        if 'issuedata' in packet.rtps.field_names:
            if partition == "no_partition":
                return
            else:
                if ipaddr not in partitionmap:
                    partitionmap[ipaddr] = partition
                    thefilter = "rtps && ip.src == " + ipaddr
                    capture_thread = Thread(target=packet_capture_thread, args=(thefilter,))
                    capture_thread.start()
                    print("start capture, ip = " + ipaddr, ",partition = " + partition)

def packet_callback(packet):
    global outputlst
    global partitionmap
    print(packet)

    try:
        ipaddr = packet.ip.src
        if ipaddr in partitionmap:
            timestamp = int(float(packet.sniff_timestamp))

            frame_length = int(packet.length)
            sll_header = 16
            ip_header = 20
            rtps_content  = int(packet.udp.length)

            total_traffic = frame_length + sll_header + ip_header + rtps_content
           
            packet_details = {
                "timestamp": timestamp,
                "total_traffic": total_traffic,
                "dev_partition": partitionmap[ipaddr]
            }
        else:
            return

        outputlst.append(packet_details)
    except AttributeError:
        pass

def packet_capture_thread(combinefilter):
    global partitionmap
    while True:
        capture = pyshark.LiveCapture(interface="any", display_filter = combinefilter)
        try:
            capture.apply_on_packets(packet_callback)
        except Exception as e:
            del capture
            gc.collect()

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

    while True:
        dictupdater = pyshark.LiveCapture(interface="any", display_filter = combinefilter)
        try:
            dictupdater.apply_on_packets(dict_callback)
            break
        except Exception as e:
            del dictupdater
            gc.collect()

if __name__ == "__main__":
    main()
