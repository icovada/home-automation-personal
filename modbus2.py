#!/usr/bin/python3
from pymodbus.constants import Endian
from pymodbus.payload import BinaryPayloadDecoder
from pymodbus.client.sync import ModbusSerialClient
import time
from requests import get
import json


def main(run):
    cl = ModbusSerialClient('rtu', port='/dev/ttyMODBUS', baudrate=38400, parity='N', stopbits=2, timeout=1)
    resp = [cl.read_input_registers(0x000C, 6, unit=1),
            cl.read_input_registers(0x0034, 2, unit=1)]

    if run == 0:
        resp.append(cl.read_input_registers(0x0048, 8, unit=1))

    decoder = [BinaryPayloadDecoder.fromRegisters(x.registers, byteorder=Endian.Big, wordorder=Endian.Big) for x in resp]

    data = {
        "wph1": decoder[0].decode_32bit_float(),
        "wph2": decoder[0].decode_32bit_float(),
        "wph3": decoder[0].decode_32bit_float(),
        "wtot": decoder[1].decode_32bit_float()
    }

    if run == 0:
        data["inwh"] = decoder[2].decode_32bit_float() * 1000
        data["outwh"] = decoder[2].decode_32bit_float() * 1000
        data["invarh"] = decoder[2].decode_32bit_float() * 1000
        data["outvarh"] = decoder[2].decode_32bit_float() * 1000

    r = get("http://192.168.1.2/emoncms/input/post?node=eastroncasa&fulljson=" + json.dumps(data) + "&apikey=5a8504496b87a071442b5d2642123fe9")
    return r


if __name__ == '__main__':
    startup = time.time()
    run = 0
    while time.time() < (startup + 55):
        if time.time() > startup + (run * 10) - 0.005:
            web = main(run)
            assert web.ok
            run = run + 1
        time.sleep(0.1)
