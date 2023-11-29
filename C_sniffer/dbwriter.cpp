#include <iostream>
#include <string>
#include <getopt.h>
#include <pqxx/pqxx>
#include <vector>
#include <ctime>
#include <algorithm>

std::string globalurl = "postgresql://postgres:postgres@localhost/postgres";

std::vector<int> generate_time_window(int start_time, int end_time, int time_window) {
    std::vector<int> result;
    // Generate a list of numbers from start_time to end_time
    for (int i = start_time; i <= end_time; ++i) {
        result.push_back(i);
    }

    std::vector<int> flat_result;
    // Loop through the list and create sublists of length 'time_window'
    for (size_t i = 0; i < result.size(); i += time_window) {
        for (size_t j = i; j < i + time_window && j < result.size(); ++j) {
            flat_result.push_back(result[j]);
        }
    }

    return flat_result;
}

std::vector<std::vector<int>> split_subarray(const std::vector<int>& subarr, int value, bool is_starttime) {
    auto it = std::find(subarr.begin(), subarr.end(), value);
    if (it != subarr.end()) {
        size_t index = std::distance(subarr.begin(), it);
        if (is_starttime && index != 0) {
            return {std::vector<int>(subarr.begin(), subarr.begin() + index),
                    std::vector<int>(subarr.begin() + index, subarr.end())};
        } else if (!is_starttime && index != subarr.size() - 1) {
            return {std::vector<int>(subarr.begin(), subarr.begin() + index + 1),
                    std::vector<int>(subarr.begin() + index + 1, subarr.end())};
        }
    }
    return {subarr};
}

std::vector<std::vector<int>> modify_array(const std::vector<int>& arr, const std::vector<int>& starttimelst, const std::vector<int>& endtimelst) {
    std::vector<std::vector<int>> modified_arr = {arr};

    for (int starttime : starttimelst) {
        std::vector<std::vector<int>> new_arr;
        for (const auto& subarr : modified_arr) {
            auto split_arrays = split_subarray(subarr, starttime, true);
            new_arr.insert(new_arr.end(), split_arrays.begin(), split_arrays.end());
        }
        modified_arr = new_arr;
    }

    for (int endtime : endtimelst) {
        std::vector<std::vector<int>> new_arr;
        for (const auto& subarr : modified_arr) {
            auto split_arrays = split_subarray(subarr, endtime, false);
            new_arr.insert(new_arr.end(), split_arrays.begin(), split_arrays.end());
        }
        modified_arr = new_arr;
    }

    return modified_arr;
}

// 將日期字符串轉換為 Unix 時間戳
time_t to_unix_timestamp(const std::string& date_str) {
    struct tm tm = {};
    if (!strptime(date_str.c_str(), "%Y-%m-%d %H:%M:%S", &tm)) {
        std::cerr << "Date parsing failed for: " << date_str << std::endl;
        return 0;
    }
    return mktime(&tm) - 28800;  // 調整時區
}


int main(int argc, char *argv[]) {
    std::cout << "start...\n";

    int timewindow = 10;
    std::string jsonpath = "";
    std::string databaseurl = "postgresql://postgres:postgres@localhost/postgres";
    bool test_mode = false;

    int opt;
    while ((opt = getopt(argc, argv, "t:j:d:m:")) != -1) {
        switch (opt) {
            case 't':
                timewindow = std::stoi(optarg);
                break;
            case 'j':
                jsonpath = optarg;
                break;
            case 'd':
                databaseurl = optarg;
                break;
            case 'm':
                test_mode = (std::string(optarg) == "True");
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-t timewindow] [-j jsonpath] [-d databaseurl] [-m test_mode]\n";
                return 1;
        }
    }

    if (jsonpath != "") {
        jsonpath += "/";
    }

    // 需要替換的數據庫連接邏輯
    if (test_mode) {
        databaseurl = "postgresql://dds_paas:postgres@10.1.1.200:5433/paasdb";
    }

    try {
        // 使用 URL 初始化連接
        pqxx::connection C(databaseurl);

        if (C.is_open()) {
            std::cout << "Opened database successfully: " << C.dbname() << std::endl;
        } else {
            std::cerr << "Can't open database" << std::endl;
            return 1;
        }

        // 這裡可以添加更多操作，比如創建一個事務對象、執行 SQL 命令等

        // 關閉數據庫連接
        C.disconnect();
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
