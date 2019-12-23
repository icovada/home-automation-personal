#!/usr/bin/python3
from pymodbus.constants import Endian
from pymodbus.payload import BinaryPayloadDecoder
from pymodbus.client.sync import ModbusSerialClient
import time
from influxdb import InfluxDBClient

client = InfluxDBClient(host='influxdb', port=8086)
client.switch_database('eastron')


def main(run):
    cl = ModbusSerialClient('rtu', port='/dev/ttyMODBUS',
                            baudrate=38400, parity='N', stopbits=2, timeout=1)
    resp = [cl.read_input_registers(0x000C, 6, unit=1),
            cl.read_input_registers(0x0034, 2, unit=1)]

    if run == 0:
        resp.append(cl.read_input_registers(0x0048, 8, unit=1))

    decoder = [BinaryPayloadDecoder.fromRegisters(
        x.registers, byteorder=Endian.Big, wordorder=Endian.Big) for x in resp]

    data = [
        {
            "measurement": "Watt",
            "tags": {
                "entity": "Eastron casa",
                "phase": 1
            },
            "fields": {
                "value": decoder[0].decode_32bit_float()
            }
        },
        {
            "measurement": "Watt",
            "tags": {
                "entity": "Eastron casa",
                "phase": 2
            },
            "fields": {
                "value": decoder[0].decode_32bit_float()
            }
        },
        {
            "measurement": "Watt",
            "tags": {
                "entity": "Eastron casa",
                "phase": 3
            },
            "fields": {
                "value": decoder[0].decode_32bit_float()
            }
        },
        {
            "measurement": "Watt",
            "tags": {
                "entity": "Eastron casa",
                "phase": "all"
            },
            "fields": {
                "value": decoder[1].decode_32bit_float()
            }
        }]

    if run == 0:
        data.append({
            "measurement": "Watth",
            "tags": {
                "entity": "Eastron casa",
                "direction": "in"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        })

        data.append({
            "measurement": "Watth",
            "tags": {
                "entity": "Eastron casa",
                "direction": "out"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        })

        data.append({
            "measurement": "Varh",
            "tags": {
                "entity": "Eastron casa",
                "direction": "in"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        })

        data.append({
            "measurement": "Varh",
            "tags": {
                "entity": "Eastron casa",
                "direction": "out"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        })

    r = client.write_points(data)
    return r


if __name__ == '__main__':
    startup = time.time()
    run = 0
    while time.time() < (startup + 55):
        if time.time() > startup + (run * 10) - 0.005:
            web = main(run)
            assert web
            run = run + 1
        time.sleep(0.1)
