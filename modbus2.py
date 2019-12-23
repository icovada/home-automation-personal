#!/usr/bin/python3
from pymodbus.constants import Endian
from pymodbus.payload import BinaryPayloadDecoder
from pymodbus.client.sync import ModbusSerialClient
from time import sleep
from influxdb import InfluxDBClient
from apscheduler.schedulers.background import BackgroundScheduler



client = InfluxDBClient(host='influxdb', port=8086)
client.switch_database('eastron')

COUNTER = 0

def modquery():
    cl = ModbusSerialClient('rtu', port='/dev/ttyMODBUS',
                            baudrate=38400, parity='N', stopbits=2, timeout=1)
    resp = [cl.read_input_registers(0x000C, 6, unit=1),
            cl.read_input_registers(0x0034, 2, unit=1)]

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
        },
        {
            "measurement": "Watth",
            "tags": {
                "entity": "Eastron casa",
                "direction": "in"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        },
        {
            "measurement": "Watth",
            "tags": {
                "entity": "Eastron casa",
                "direction": "out"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        },
        {
            "measurement": "Varh",
            "tags": {
                "entity": "Eastron casa",
                "direction": "in"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        },
        {
            "measurement": "Varh",
            "tags": {
                "entity": "Eastron casa",
                "direction": "out"
            },
            "fields": {
                "value": decoder[2].decode_32bit_float() * 1000
            }
        }]

    r = client.write_points(data)


    return r



if __name__ == '__main__':
    sched = BackgroundScheduler()
    sched.start()

    sched.add_job(modquery, trigger='cron', second = '*/2')

    while True:
        sleep(1)