import gc
import time
import os
import json
from collections import defaultdict
import pandas as pd
from sqlalchemy import create_engine

username = 'dds_paas'
password = 'postgres'
host = '10.1.1.200'
port = '5433'
dbname = 'paasdb'

engine = create_engine(f'postgresql://{username}:{password}@{host}:{port}/{dbname}')
try:
    with engine.connect() as conn:
        print("connection success!")
except Exception as e:
    print(f"connection failed: {e}")

pd.set_option('display.max_rows', None)
pd.set_option('display.max_columns', None)

def generate_time_window(start_time, end_time, time_window):
    result = []
    # Generate a list of numbers from start_time to end_time
    flat_list = [i for i in range(start_time, end_time + 1)]
    # Loop through the list and create sublists of length 'time_window'
    for i in range(0, len(flat_list), time_window):
        # Add the sublist to the result list
        result.append(flat_list[i:i + time_window])
    
    return result

def aggregate_traffic(df, time_groups):
    # 初始化結果列表
    aggregated_data = []
    
    # 遍歷時間組
    for group in time_groups:
        # 根据時間組過濾DataFrame
        df_group = df[df['timestamp'].isin(group)]
        # 按設備ID進行分組並計算總流量
        group_traffic = df_group.groupby('dev_partition')['total_traffic'].sum().reset_index()
        # 將開始和結束時間添加到分組結果中
        group_traffic['starttime'] = min(group)
        group_traffic['endtime'] = max(group)
        # 將當前組的結果添加到列表中
        aggregated_data.append(group_traffic)

    # 合併所有分組結果到一個DataFrame
    aggregated_df = pd.concat(aggregated_data, ignore_index=True)
    new_order = ['dev_partition', 'starttime', 'endtime', 'total_traffic']
    aggregated_df = aggregated_df[new_order]
    
    return aggregated_df

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

def drop_out_of_range_rows(df, start_min_time, end_max_time):
    filtered_df = None

    # 根據時間範圍大小作判斷，刪除不在時間範圍內的資料
    if ((df['starttime'] >= start_min_time) and (df['endtime'] <= end_max_time)):
        filtered_df = df
    elif ((df['starttime'] >= start_min_time) and (df['endtime'] >= end_max_time)):
        if (df['starttime'] >= end_max_time):
            pass
        else:
            filtered_df = df[(df['starttime'] >= start_min_time) & (df['endtime'] <= end_max_time)]
    elif ((df['starttime'] <= start_min_time) and (df['endtime'] <= end_max_time)):
        filtered_df = df[(df['starttime'] >= start_min_time) & (df['endtime'] <= end_max_time)]
    elif ((df['starttime'] <= start_min_time) and (df['endtime'] >= end_max_time)):
        filtered_df = df[(df['starttime'] >= start_min_time) & (df['endtime'] <= end_max_time)]

    return filtered_df

def cal_sort_packet(df, time_window):

    # 讀取資料，將時間轉為unix timestamp
    df = pd.read_sql_query("SELECT * FROM vw_dds_devices_for_flow_cal", engine)
    df = df[['ser_id', 'svc_eff_date', 'svc_end_date', 'dev_partition']]
    df['svc_eff_date'] = df['svc_eff_date'].apply(lambda x: int(x.timestamp()) if pd.notnull(x) else None)
    df['svc_end_date'] = df['svc_end_date'].apply(lambda x: int(x.timestamp()) if pd.notnull(x) else None)

    # generate service range list
    date_pairs = []
    for _, row in df.iterrows():
        start_date = row['svc_eff_date']
        end_date = row['svc_end_date'] if row['svc_end_date'] is not None else 'None'
        date_pairs.append([start_date, end_date])

    # generate_starttime list
    starttimelst = [date for date in df['svc_eff_date'] if date is not None]

    # generate_endtime list
    endtimelst = [date for date in df['svc_end_date'] if date is not None]

    # generate partition list
    partitionlst = [dev for dev in df['dev_partition'] if dev is not None]

    # 根據timewindow大小生成時間範圍列表
    timerange = generate_time_window(df['timestamp'].min(), df['timestamp'].max() , time_window)

    # 根據view的時間範圍切割
    modifytime = modify_array(timerange, starttimelst, endtimelst)
    
    # 根據切割範圍計算流量總合
    outdf = aggregate_traffic(df, modifytime)

    # 刪除不在partition內的資料 ----->> 可以關掉以適應測試環境
    outdf = outdf[outdf['dev_partition'].isin(partitionlst)]
    
    gc.collect()
    # 返回view table, 結果的table, 服務啟動範圍列表
    return df, outdf, date_pairs

# 讀取 JSON 文件
my_list = ["traffic_details1.json","traffic_details2.json"]
index = 0

while True:
    if os.path.getsize(my_list[index]) >= 100000:
        time.sleep(3)
        try:
            with open(my_list[index], 'r') as json_file:
                jfile = json.load(json_file)
                df_file = pd.DataFrame(jfile)
                json_file.close()
                servicetable, outputlst, date_pairs = cal_sort_packet(df_file, 10)

                # --- 判斷是否drop資料，存入DB ---
                service_continue = False
                no_service = False
                service_isend = False
                for i in date_pairs:
                    if (i[1] == 'None' and i[0] != 'None'):
                        service_continue = True
                    if (i[1] == 'None' and i[0] == 'None'):
                        no_service = True
                    if (i[1] != 'None' and i[0] != 'None'):
                        service_isend = True

                if (service_continue):
                    print(outputlst)
                    outputlst.to_sql('tb_cam_traffic_info', engine, if_exists='append', index=False)
                elif (service_isend):
                    startmintime = servicetable['svc_eff_date'].min()
                    endmaxtime = servicetable['svc_end_date'].max()
                    outputlst = drop_out_of_range_rows(outputlst, startmintime, endmaxtime)
                    print(outputlst)
                    if (outputlst == None):
                        continue
                    else:
                        outputlst.to_sql('tb_cam_traffic_info', engine, if_exists='append', index=False)
                else:
                    continue
                # ----------------------
                json_file.close()
            with open(my_list[index], 'r') as json_file:
                json_file.close()
        except Exception as e:
            pass
    else:
        time.sleep(1)
        index = (index + 1) % 2
    gc.collect()

