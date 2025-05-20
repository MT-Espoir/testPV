### 1: Xác minh cài đặt WiFi hiện tại

1. Trên MQTT Gửi tin nhắn JSON sau đến chủ đề `test291104/control`:

{"show_wifi": true}

### 2: Cập nhật thông tin xác thực WiFi
{
"wifi_config": true,
"ssid": "Min",
"password": "123456789"
}

### 3: Xóa thông tin xác thực WiFi
{"clear_wifi": true}
