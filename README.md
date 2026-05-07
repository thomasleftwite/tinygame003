エージェントによるデバグ完了後、下記コマンドを実行してESP32C3ターゲットにフラッシュする
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 tinygame003.ino
arduino-cli upload -p COMX --fqbn esp32:esp32:XIAO_ESP32C3 tinygame003.ino
