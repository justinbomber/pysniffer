
# sniffer.py 和 dbwriter.py 指令選項

以下是 sniffer.py 和 dbwriter.py 這兩個腳本的命令行選項：

## sniffer.py 用法

| 選項 | 描述 | 預設值 |
| ---- | ---- | ------ |
| `-h`, `--help` | 顯示幫助信息並退出 | - |
| `--savefile SAVEFILE`, `-s SAVEFILE` | 是否保存文件至檔案 | True |
| `--interface INTERFACE`, `-i INTERFACE` | 網卡名 | ALL |
| `--filesize FILESIZE`, `-z FILESIZE` | 緩存文件大小的整數值(byte) | 1000000 |
| `--printable PRINTABLE`, `-p PRINTABLE` | 是否打印 | False |
| `--jsonpath JSONPATH`, `-j JSONPATH` | json文件保存路径 | "/" |

## dbwriter.py 用法

| 選項 | 描述 | 預設值 |
| ---- | ---- | ------ |
| `-h`, `--help` | 顯示幫助信息並退出 | - |
| `--timewindow TIMEWINDOW`, `-t TIMEWINDOW` | 時間窗口的整數值(sec) | 10 |
| `--filesize FILESIZE`, `-z FILESIZE` | 緩存文件大小的整數值(byte) | 1000000 |
| `--jsonpath JSONPATH`, `-j JSONPATH` | json文件保存路径 | "/" |
| `--username USERNAME`, `-u USERNAME` | 數據庫用戶名 | dds_paas |
| `--password PASSWORD`, `-w PASSWORD` | 數據庫密碼 | postgres |
| `--host HOST`, `-d HOST` | 數據庫主機地址 | 10.1.1.200 |
| `--port PORT`, `-p PORT` | 數據庫端口 | 5433 |
| `--dbname DBNAME`, `-n DBNAME` | 數據庫名稱 | paasdb |
