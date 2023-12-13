
# capture 及 dbwriter 指令選項

**以下是 sniffer 和 dbwriter 這兩個腳本的命令行選項：**

- ## capture 

### 1. 簡介：
此模組負責抓取封包，並進行初步的處理後存為json供dbwriter寫入DataBase。

### 2. 編譯環境及依賴包：

- 編譯環境：C++17
- library：
    - pcapplusplus
    - pqxx
- 打包工具：Cmake

### 3. arguments傳入用法：
| 選項 | 描述 | 預設值 |
| ---- | ---- | ------ |
| `-h`, `--help` | 顯示幫助訊息 | - |
| `-i`, `--interface` | 指定網路介面名稱 | any |
| `-p`, `--packetcount` | 設定一次處理多少封包 | 15000 |
| `-j`, `--jsonpath` | 設定 JSON 檔案的相對路徑 | 目前目錄 |
| `-a`, `--ipaddr`         | 指定IP地址 (需與partition同時輸入)| None |
| `-b`,  `--partition`    | 指定partition (需與ip地址同時輸入) | None |
| `-c`, `--connectionurl` | 數據庫URL | postgresql://postgres:admin@140.110.7.17:5433/postgres |
| `-m`, `--testmode` | 測試模式(true/false) | false |
| `-t`, `--threadcount` | 用幾個thread來處理流量封包 | 3 |


- ## dbwriter

### 1. 簡介：
此模組負責將json中抓到的封包流量進行流量的加總及切片後，根據服務啟動的時間區間，將流量存取至DataBase中。

### 2. 編譯環境及依賴包：
- 編譯環境：python 3.11
- pip包:
  - pandas
  - psycopg2
  - psycopg2-binary
  - sqlalchemy
- 打包工具：pyinstaller


### 3. arguments 傳入用法說明：

| 選項 | 描述 | 預設值 |
| ---- | ---- | ------ |
| `-h`, `--help` | 顯示幫助信息 | - |
| `-t`, `--timewindow` | 時間窗口的整數值(sec) | 10 |
| `-j`, `--jsonpath` | json文件保存路径 | "/" |
| `-d`, `--databaseurl` | 數據庫URL | postgresql://postgres:admin@140.110.7.17:5433/postgres |
| `-z`, `--timezone` | 時區校正 | 0 |
| `-m`, `--testmode` | 測試模式(True/False) | False |
