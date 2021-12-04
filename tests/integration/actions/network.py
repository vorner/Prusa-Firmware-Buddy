from simulator import Printer, NetworkInterface


def ip_address_get(printer: Printer, interface: NetworkInterface) -> str:
    return printer.network_ip_address_get(interface)


def proxy_http_port_get(printer: Printer) -> int:
    return printer.network_proxy_http_port_get()
