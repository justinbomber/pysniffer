import gc
import time
import os
import json
from collections import defaultdict
import pandas as pd
from sqlalchemy import create_engine

def generate_time_window(start_time, end_time, time_window):
    result = []
    flat_list = [i for i in range(start_time, end_time + 1)]
    while len(flat_list) % time_window != 0:
        flat_list.append(0)
    for i in range(0, len(flat_list), time_window):
        result.append(flat_list[i:i + time_window])
    del flat_list
    
    return result

def modify_array(arr, starttimelst, endtimelst):
    # Helper function to split the subarray based on a value if it exists within it
    def split_subarray(subarr, value, is_starttime):
        # If it's a starttime split, we want to split before the value, otherwise after
        index_offset = 0 if is_starttime else 1
        try:
            index = subarr.index(value)
            # Return the split parts only if the value is not at the ends as specified
            if is_starttime and index != 0:
                return [subarr[:index]] + [subarr[index:]]
            elif not is_starttime and index != len(subarr) - 1:
                return [subarr[:index+1]] + [subarr[index+1:]]
        except ValueError:
            pass
        # If the value was not found or at the ends, return the original subarray
        return [subarr]

    # Initialize the modified array with the original array
    modified_arr = arr.copy()

    # Process start times
    for starttime in starttimelst:
        new_arr = []
        for subarr in modified_arr:
            new_arr.extend(split_subarray(subarr, starttime, is_starttime=True))
        modified_arr = new_arr

    # Process end times
    for endtime in endtimelst:
        new_arr = []
        for subarr in modified_arr:
            new_arr.extend(split_subarray(subarr, endtime, is_starttime=False))
        modified_arr = new_arr

    # Return the modified array
    return modified_arr

def calculate_packet_sum(df, time_window, grouped_timestamps):
    result = defaultdict(lambda: defaultdict(int))

    for group in grouped_timestamps:
        temp_df = df[df['timestamp'].isin(group)]
        for _, row in temp_df.iterrows():
            key = (row['dev_id'])
            timestamp = group[0] + time_window -1
            result[key][timestamp] += row['total_traffic']

    return result

def defaultdict_to_dataframe(output, time_window):
    # 將 defaultdict 转换为列表，其中每个元素都是一个字典
    data = []
    for key, value in output.items():
        for timestamp, total_traffic in value.items():
            # 計算 starttime 和 endtime
            starttime = timestamp
            endtime = starttime + (time_window - 1)
            data.append({'dev_id': key, 'starttime': starttime, 'endtime': endtime, 'total_traffic': total_traffic})

    # 將列表轉换为 DataFrame
    output_df = pd.DataFrame(data)

    # 刪除包含 NaN 值的行
    output_df = output_df.dropna()

    # 重新設置索引
    output_df = output_df.reset_index(drop=True)

    return output_df

def split_dataframe(df, time_windows):
    df_with_zeros = pd.DataFrame(columns=df.columns)
    df_remaining = pd.DataFrame(columns=df.columns)

    for window in time_windows:
        for time_point in window:
            if time_point == 0:
                # 添加整個時間窗口到包含0的DataFrame
                df_with_zeros = pd.concat([df_with_zeros, df[df['timestamp'].isin(window)]])
                break
        else:
            # 添加整個時間窗口到剩下的DataFrame
            df_remaining = pd.concat([df_remaining, df[df['timestamp'].isin(window)]])

    return df_with_zeros.reset_index(drop=True), df_remaining.reset_index(drop=True)

def cal_sort_packet(df, time_window, last_input = None):

    if last_input is None:
        df = df
    else:
        df = pd.concat([df, last_input]).reset_index(drop=True)


    # 生成時間範圍列表
    start_time = time.time()
    print("============================")
    print(df['timestamp'].min())
    print(df['timestamp'].max())
    print("============================")
    timerange = generate_time_window(df['timestamp'].min(), df['timestamp'].max() , time_window)
    for i in range(len(timerange)):
        print(timerange[i])
    print("============================")
    modifytime = modify_array(timerange, [df["timestamp"].min()+4], [df["timestamp"].max() -2, df["timestamp"].max() - 7])
    for i in range(len(modifytime)):
        print(modifytime[i])
    print("============================")
    end_time = time.time()
    runtime = end_time - start_time
    print(f"The function took {runtime} seconds to run.")
    # time.sleep(1)
    # 將時間範圍切割
    start_time = time.time()
    next_input, this_input = split_dataframe(df, timerange)
    end_time = time.time()
    runtime = end_time - start_time
    print(f"The function took {runtime} seconds to run.")
    # time.sleep(1)
    # 計算每個時間窗口的總包數
    start_time = time.time()
    output = calculate_packet_sum(this_input, len(timerange[0]), timerange)
    end_time = time.time()
    runtime = end_time - start_time
    print(f"The function took {runtime} seconds to run.")
    # time.sleep(1)
    # 轉換為 DataFrame
    start_time = time.time()
    finaloutput = defaultdict_to_dataframe(output, time_window)
    end_time = time.time()
    runtime = end_time - start_time
    print(f"The function took {runtime} seconds to run.")
    # time.sleep(1)
    del output, this_input, timerange, df, time_window
    gc.collect()

    # return dataframe, this time doesn't finish, already finish.
    return next_input, finaloutput

# 讀取 JSON 文件
my_list = ["traffic_details1.json","traffic_details2.json"]
index = 0
last_input = None

username = 'dds_paas'
password = 'postgres'
host = '10.1.1.200'
port = '5433'
dbname = 'paasdb'

# 創建數據庫引擎
engine = create_engine(f'postgresql://{username}:{password}@{host}:{port}/{dbname}')
try:
    with engine.connect() as conn:
        print("connection success!")
except Exception as e:
    print(f"connection failed: {e}")




pd.set_option('display.max_rows', None)
while True:
    # print(my_list[index], ":", os.path.getsize(my_list[index]))
    if os.path.getsize(my_list[index]) >= 100000:
        time.sleep(3)
        try:
            with open(my_list[index], 'r') as json_file:
                jfile = json.load(json_file)
                df_file = pd.DataFrame(jfile)
                json_file.close()
                last_input, outputlst = cal_sort_packet(df_file, 5, last_input)
                # print(outputlst)
                outputlst.to_sql('tb_cam_traffic_info', engine, if_exists='append', index=False)
                # print("DataFrame is saved to PostgreSQL database.")
            with open(my_list[index], 'w') as json_file:
                json_file.close()
        except Exception as e:
            pass
    else:
        time.sleep(1)
        index = (index + 1) % 2
        # print(index, ":", my_list[index])
    gc.collect()

# pd.set_option('display.max_rows', None)
