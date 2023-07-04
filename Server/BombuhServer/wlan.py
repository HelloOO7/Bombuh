import network

def create_ap(ssid: str, password: str) -> network.WLAN:
    wlan = network.WLAN(network.AP_IF)
    wlan.config(essid=ssid, password=password, security=network.AUTH_WPA2_PSK)
    wlan.active(True)
    return wlan