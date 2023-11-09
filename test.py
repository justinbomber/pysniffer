import pyshark
import time
from collections import deque
from threading import Thread
import json
import gc
import os

watchlst = []
outputlst = deque(maxlen=15000)
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
partitionmap = {}

def packet_callback(packet):
    global outputlst

    try:
        if packet.rtps is not None:
            if 'issuedata' in packet.rtps.field_names:
                partition = packet.rtps.issuedata[0:50]
                partition = hex_to_ascii(partition)
                ipaddr = packet.ip.src

                if partition == "no_partition":
                    return
                else:
                    if ipaddr not in partitionmap:
                        partitionmap[ipaddr] = partition
                    
                timestamp = int(float(packet.sniff_timestamp))
                length = int(packet.length)
                packet_details = {
                    "timestamp": timestamp,
                    "total_traffic": length,
                    # "dev_id": partitionmap[ipaddr]
                    "dev_partition": partitionmap[ipaddr] 
                }
                outputlst.append(packet_details)
                # storage.append(packet)
                # print(packet_details)
                del timestamp, length, packet_details
    except AttributeError:
        pass
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

def write_to_file():
    global outputlst
    my_list = ["traffic_details1.json", "traffic_details2.json"]
    index = 0
    for i in my_list:
        with open(i, 'w') as json_file:
            json_file.write('[\n')
            json_file.close()
            del json_file
    # my_list[index]
    while True:
        if os.path.getsize(my_list[index]) <= 100000:
            new_outputlst = outputlst
            outputlst.clear()
            time.sleep(5)
            with open(my_list[index], 'a') as json_file:
                while new_outputlst:
                    item = new_outputlst.popleft()
                    json.dump(item, json_file, indent=4)
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



write_thread = Thread(target=write_to_file)
write_thread.start()
while True:
    capture = pyshark.LiveCapture(interface="eno1", display_filter = "rtps")
    try:
        capture.apply_on_packets(packet_callback)
    except Exception as e:
        del capture
        gc.collect()
