import gc
from datetime import datetime
import time
import os
import json
import pandas as pd
# import pandas_lite as pd
from sqlalchemy import create_engine
import argparse

test_mode = False
timezone = 8
databaseurl = 'postgresql://postgres:admin@paasdb.default:5433/postgres'


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
    # print("Modified array: ", modified_arr)
    return modified_arr

def cal_sort_packet(df, time_window):
    global test_mode
    global timezone
    global databaseurl

    engine = create_engine(f'{databaseurl}')

    # 讀取資料，將時間轉為unix timestamp
    viewdf = pd.read_sql_query("SELECT * FROM vw_dds_devices_for_flow_cal", engine)
    engine.dispose()
    viewdf = viewdf[['ser_id', 'svc_eff_date', 'svc_end_date', 'dev_partition']]
    viewdf['svc_eff_date'] = viewdf['svc_eff_date'].apply(lambda x: int(x.timestamp()) - timezone*3600 if pd.notnull(x) else 0)
    viewdf['svc_end_date'] = viewdf['svc_end_date'].apply(lambda x: int(x.timestamp()) - timezone*3600 if pd.notnull(x) else 0)


    # generate service range list
    date_pairs = []
    for _, row in viewdf.iterrows():
        # 检查 svc_eff_date 和 svc_end_date 是否为 NaN，如果是则赋值为 'None'
        start_date = row['svc_eff_date'] if (pd.notnull(row['svc_eff_date']) and row['svc_eff_date'] != 0) else 'None'
        end_date = row['svc_end_date'] if (pd.notnull(row['svc_end_date']) and row['svc_end_date'] != 0) else 'None'
        date_pairs.append([start_date, end_date])

    # generate_starttime list
    starttimelst = [date for date in viewdf['svc_eff_date'] if date != 0]

    # generate_endtime list
    endtimelst = [date for date in viewdf['svc_end_date'] if date != 0]

    # generate partition list
    partitionlst = [dev for dev in viewdf['dev_partition'] if dev is not None]

    # 根據timewindow大小生成時間範圍列表
    timerange = generate_time_window(df['timestamp'].min(), df['timestamp'].max() , time_window)

    # 根據view的時間範圍切割
    modifytime = modify_array(timerange, starttimelst, endtimelst)
    
    # 根據切割範圍計算流量總合
    outdf = aggregate_traffic(df, modifytime)
    # outdf = aggregate_traffic(df, timerange)

    # 刪除不在partition內的資料 ----->> 可以關掉以適應測試環境
    if not test_mode:
        outdf = outdf[outdf['dev_partition'].isin(partitionlst)]
    
    gc.collect()
    # 返回view table, 結果的table, 服務啟動範圍列表
    return viewdf, outdf, date_pairs

def convert_to_dict_of_arrays(data):
    # 初始化字典來儲存數組
    result = {"timestamp": [], "total_traffic": [], "dev_partition": []}

    # 遍歷列表中的每個字典
    for entry in data:
        # 將每個鍵的值追加到相應的數組
        result["timestamp"].append(entry["timestamp"])
        result["total_traffic"].append(entry["total_traffic"])
        result["dev_partition"].append(entry["dev_partition"])

    return result

def main():
    global test_mode
    global timezone
    global databaseurl
    print("dbwriter  start...")
    parser = argparse.ArgumentParser()

    # set timewindow
    parser.add_argument("--timewindow", "-t", default=10, help="時間窗口的整數值(sec), default = 10")

    # set jsonpath
    parser.add_argument("--jsonpath", "-j", default="", help="json文件保存相對路径，默認同資料夾, default = \"\"")
    
    # set database
    parser.add_argument("--databaseurl", "-d", default='postgresql://postgres:admin@paasdb.default:5433/postgres', help="數據庫URL, default = postgresql://postgres:admin@paasdb.default:5433/postgres")

    # add test_mode
    parser.add_argument("--test_mode", "-m", default="False", help="是否為測試模式, default = False")

    # add timezone
    parser.add_argument("--timezone", "-z", default="8", help="時區, default = Asia/Taipei, UTC+8")

    args = parser.parse_args()

    timezone = int(args.timezone)

    jsonpath = ""
    if args.jsonpath != "":
        jsonpath = args.jsonpath + "/"
    
    if args.test_mode == "True":
        test_mode = True
    elif args.test_mode == "False":
        test_mode = False
    databaseurl = args.databaseurl
    engine = create_engine(f'{databaseurl}')


    # 讀取 JSON 文件
    my_list = [jsonpath + "traffic_details1.json",jsonpath + "traffic_details2.json"]
    index = 0
    change = True

    while True:
        # print(os.path.getsize(my_list[index]))
        time.sleep(2)
        if os.path.getsize(my_list[index]) >= 200:
            change = True
            try:
                with open(my_list[index], 'r') as json_file:
                    jfile = json.load(json_file)
                    # jfile = convert_to_dict_of_arrays(jfile)
                    df_file = pd.DataFrame(jfile)
                    json_file.close()
                    servicetable, outputlst, date_pairs = cal_sort_packet(df_file, int(args.timewindow))

                    # --- 判斷是否drop資料，存入DB ---
                    service_continue = False
                    no_service = False
                    service_isend = False
                    for i in date_pairs:
                        if (i[0] != 'None' and i[1] == 'None'):
                            service_continue = True
                        if (i[0] == 'None' and i[1] == 'None'):
                            no_service = True
                        if (i[0] != 'None' and i[1] != 'None'):
                            service_isend = True

                    if (service_continue):
                        print("service_continue")
                        if not test_mode:
                            try:
                                outputlst.to_sql('tb_cam_traffic_info', engine, if_exists='append', index=False)
                                print("save data to databse successfully.")
                            except Exception as e:
                                print(e)
                                pass
                    elif (service_isend):
                        print("service_isend")
                        startmintime = servicetable[servicetable['svc_eff_date'] != 0]['svc_eff_date'].min()
                        endmaxtime = servicetable[servicetable['svc_end_date'] != 0]['svc_end_date'].max()
                        outputlst = outputlst[(outputlst['starttime'] >= startmintime) & (outputlst['endtime'] <= endmaxtime)]
                        if not test_mode:
                            try:
                                outputlst.to_sql('tb_cam_traffic_info', engine, if_exists='append', index=False)
                                print("save data to databse successfully.")
                            except Exception as e:
                                print(e)
                                pass
                    else:
                        print("no_service, drop data.")
                        pass
                    if (test_mode):
                        print("=================")
                        print(outputlst)
                        print("=================")
                    # ----------------------
                    json_file.close()
            except Exception as e:
                print(e)
                pass
        else:
            change = False
        if change:
            index = (index + 1) % 2
        gc.collect()

if __name__ == "__main__":
    main()
