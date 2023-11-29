
# capture 及 dbwriter 指令選項

以下是 sniffer 和 dbwriter 這兩個腳本的命令行選項：

## capture.py 用法

| 選項 | 描述 | 預設值 |
| ---- | ---- | ------ |
| `-h`, `--help` | 顯示此幫助訊息 | - |
| `-i`, `--interface` | 指定網路介面名稱 | any |
| `-p`, `--packetcount` | 設定一次處理多少封包 | 15000 |
| `-j`, `--jsonpath` | 設定 JSON 檔案的相對路徑 | 目前目錄 |

## dbwriter.py 用法

| 選項 | 描述 | 預設值 |
| ---- | ---- | ------ |
| `-h`, `--help` | 顯示幫助信息 | - |
| `-t`, `--timewindow` | 時間窗口的整數值(sec) | 10 |
| `-j`, `--jsonpath` | json文件保存路径 | "/" |
| `-d`, `--databaseurl` | 數據庫URL | postgresql://postgres:postgres@localhost/postgres |