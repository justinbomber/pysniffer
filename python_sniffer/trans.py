import datetime


def format_display_filter(starttime, endtime):
    # Convert the start and end times from Unix timestamps to human-readable format
    start_time_str = datetime.datetime.fromtimestamp(int(starttime)).strftime('%b %d, %Y %H:%M:%S')
    end_time_str = datetime.datetime.fromtimestamp(int(endtime)).strftime('%b %d, %Y %H:%M:%S')

    # Adjust the end time string to account for the last 999999 microseconds
    end_time_str = end_time_str + '.999999'

    # Construct the Wireshark display filter string
    display_filter = f'ip.src == 10.1.1.87 && frame.time >= "{start_time_str}.000000" && frame.time <= "{end_time_str}"'

    return display_filter

def format_sql_query(starttime, endtime):
    # Construct the SQL query string
    sql_query = f'SELECT dev_partition, sum(total_traffic) FROM tb_cam_traffic_info ' \
                f'WHERE starttime >= {starttime} AND endtime <= {endtime} ' \
                f'GROUP BY dev_partition LIMIT 100'

    return sql_query

# Prompt the user for start and end times
while True:
    starttime = input("Enter the starttime (Unix timestamp): ")
    endtime = input("Enter the endtime (Unix timestamp): ")

    # Format and print the output strings
    print(format_display_filter(starttime, endtime))
    print("------------------------------------")
    print(format_sql_query(starttime, endtime))
