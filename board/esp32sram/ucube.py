src = ['board.c']

component = aos_board_component('board_esp32sram', 'esp32', src)

if aos_global_config.get('hci_h4', 0):
    component.add_global_macros('CONFIG_BLE_HCI_H4_UART_PORT=1')


platform_options="wifi=1"
supported_targets="bluetooth.bleadv bluetooth.bleperipheral coapapp helloworld linkkit_gateway linkkitapp mqttapp"
linux_only_targets="athostapp blink http2app meshapp otaapp tinyengine_app tls udataapp yts"
