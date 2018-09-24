#!/usr/bin/python2
import struct
import pymodbus.client.sync
import binascii
import time
import sys
import requests 
 
def main() :
    cl = pymodbus.client.sync.ModbusSerialClient('rtu', port='/dev/ttyUSB0', baudrate=38400, parity='N',stopbits=2, timeout=0.2)
    respa = cl.read_input_registers(12,6, unit=1)
    respb = cl.read_input_registers(52,2, unit=1)
    try:
        wph1 = str(int(struct.unpack('>f',struct.pack('>HH',*[respa.registers[0],respa.registers[1]]))[0]))
        wph2 = str(int(struct.unpack('>f',struct.pack('>HH',*[respa.registers[2],respa.registers[3]]))[0]))
        wph3 = str(int(struct.unpack('>f',struct.pack('>HH',*[respa.registers[4],respa.registers[5]]))[0]))
        wtot = str(int(struct.unpack('>f',struct.pack('>HH',*[respb.registers[0],respb.registers[1]]))[0]))

        requests.request("put", "http://192.168.1.2/emoncms/input/post.json?node=1&csv="+wph1+","+wph2+","+wph3+","+wtot+"&apikey=5a8504496b87a071442b5d2642123fe9")


    except:
       pass

 
if __name__ == '__main__' :
    startup = time.time()
    run = 0
    while time.time() < (startup + 55):
        if time.time() > startup + (run*10) - 0.005:
            run = run + 1
            main()
        time.sleep(0.1)
