5x5のマスの中で、1～24のタイルを動かして順番をそろえる1人プレイのパズル<BR>
<BR>
エージェントに頼らず、手動でフラッシュする手順：<BR>
エージェントによるデバグ完了後、下記コマンドを実行してESP32C3ターゲットにフラッシュする<BR>
※下記の'COMX'はターゲットを接続したUSBポートに変更する　Windows例：COM7、Mac例：/dev/cu.usbmodem11301<BR>
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 tinygame003.ino<BR>
arduino-cli upload -p COMX --fqbn esp32:esp32:XIAO_ESP32C3 tinygame003.ino<BR>
