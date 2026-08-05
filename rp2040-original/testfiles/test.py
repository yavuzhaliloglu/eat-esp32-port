#!/bin/python

import serial
import time
import hashlib
from datetime import datetime

seri = serial.Serial(
    "/dev/ttyUSB0", baudrate=300, bytesize=7, parity="E", stopbits=1, timeout=2
)

current_time = datetime.now()
            
year = current_time.year - 2000
month = current_time.month
day = current_time.day
hour = current_time.hour
minute = current_time.minute
second = current_time.second

year_str = str(year)
month_str = f"{month:02d}"
day_str = f"{day:02d}"
hour_str = f"{hour:02d}"
minute_str = f"{minute:02d}"
second_str = f"{second:02d}"

meeting_message = bytearray(b"/?!\r\n")
seri.write(meeting_message)
time.sleep(0.25)
meeting_response = bytearray(seri.readline())
time.sleep(0.25)
print(meeting_response)
print(len(meeting_response))

max_baud_rate = b"\x36"
mbr_str = max_baud_rate.decode("utf-8")
mbr_int = int(mbr_str)

baud_rates = [300, 600, 1200, 2400, 4800, 9600, 19200]

if meeting_response[0] == 47 and len(meeting_response) > 5:
    # READOUT İÇİN (b"\x0600\r\n") YAPIN, YÜK OKUMA VE DİĞER TESTLER İÇİN (b"\x0601\r\n") YAPIN
    information_message = bytearray(b"\x0600\r\n")
    information_message[2:2] = max_baud_rate
    
    print(information_message)

    seri.write(information_message)
    time.sleep(0.25)

    seri.close()
    seri = serial.Serial(
        "/dev/ttyUSB0",
        baudrate=baud_rates[mbr_int],
        bytesize=7,
        parity="E",
        stopbits=1,
        timeout=2,
    )
    print("port opened again. new baudrate is: ", seri.baudrate)

    if (information_message[3] == 0x30 or information_message[3] == 0x36):
        print("entered readout mode")

        for i in range(10):
            res = bytearray(seri.readline())
            print(res)

    else:
        print("entered programming mode")

        information_response = bytearray(seri.readline())
        print(information_response)
        print(len(information_response))
        bcc_received = information_response.pop()

        bcc = 0x01
        for i in information_response:
            bcc ^= i

        print("information response bcc result: ", hex(bcc))

        if bcc_received == bcc:
            # bcc = 0x01
            # # tüketim sorgusu
            # # TARİH BELİRLEME
            # loadFormat = bytearray(
            #     b"\x01\x52\x32\x02\x50\x2E\x30\x31\x2824-04-25,10:00;24-04-25,16:00\x29\x03"
            # )
            
            bcc = 0x01
            # tüketim sorgusu
            loadFormat = bytearray(
                b"\x01\x52\x32\x02\x50\x2E\x30\x31\x28;\x29\x03"
            )

            for b in loadFormat:
                bcc ^= b
            loadFormat.append(bcc)
            print("data to send device: ", loadFormat)
            seri.write(loadFormat)

            xor_check = 0x02
            for i in range(500):
                data = bytearray(seri.readline())
                if len(data) == 0:
                    break
                print(data)
                if len(data) != 3:
                    for b in data:
                        xor_check ^= b
                else:
                    xor_check ^= data[0]
                    xor_check ^= data[1]
                    print("xor check:", hex(xor_check))

            time.sleep(0.2)

#             bcc_password = 0x01
#             passwordMessage = bytearray(b'\x01\x50\x31\x02\x2812345678\x29\x03')
#             for b in passwordMessage:
#                 bcc_password ^= b
#             passwordMessage.append(bcc_password)
#             print(passwordMessage)
#             seri.write(passwordMessage)
#             print(bytearray(seri.readline()))
# # ---------------------------------------------------------------------------------------------------
#             time_str = hour_str + ":" + minute_str + ":" + second_str
#             time_str = time_str.encode()
            
#             timeset_head = bytearray(b'\x01\x57\x32\x02\x30\x2E\x39\x2E\x31\x28')
#             timeset_date = time_str
#             timeset_tail = bytearray(b'\x29\x03')
            
#             timeset = timeset_head + timeset_date + timeset_tail

#             bcc_time = 0x01
#             for b in timeset:
#                 bcc_time ^=b

#             timeset.append(bcc_time)
#             seri.write(timeset)
#             print(seri.readline())
# # -----------------------------------------------------------------------------------------------------
#             date_str = year_str + "-" + month_str + "-" + day_str
#             date_str = date_str.encode()

#             dateset_head = bytearray(b'\x01\x57\x32\x02\x30\x2E\x39\x2E\x32\x28')
#             dateset_date = date_str
#             dateset_tail = bytearray(b'\x29\x03')
            
#             dateset = dateset_head + dateset_date + dateset_tail
            
#             bcc_date = 0x01
#             for b in dateset:
#                 bcc_date ^= b
            
#             dateset.append(bcc_date)
#             seri.write(dateset)
#             print(seri.readline())
# # ----------------------------------------------------------------------------------------------------

            # bcc_threshold = 0x01
            # threshold = bytearray(b'\x01\x57\x32\x02T.P.1()\x03')
            # for b in threshold:
            #     bcc_threshold ^=b
            # threshold.append(bcc_threshold)
            # seri.write(threshold)
            # for i in range(500):
            #     data = seri.readline()
            #     if len(data) == 0:
            #         break
            #     print(data)

            # bcc_threshold = 0x01
            # threshold = bytearray(b'\x01\x57\x32\x02T.V.1(007)\x03')
            # for b in threshold:
            #     bcc_threshold ^=b
            # threshold.append(bcc_threshold)
            # seri.write(threshold)
            # print(seri.readline())

            # bcc_threshold = 0x01
            # threshold = bytearray(b'\x01\x52\x32\x02T.R.1()\x03')
            # for b in threshold:
            #     bcc_threshold ^=b
            # threshold.append(bcc_threshold)
            # seri.write(threshold)
            # for i in range(500):
            #     data = seri.readline()
            #     if len(data) == 0:
            #         break
            #     print(data)

            # bcc_pw_hint = 0x01
            # pw_hint_message = bytearray(
            #     b"\x01\x52\x32\x02\x39\x36\x2e\x39\x35\x2e\x31\x28\x29\x03"
            # )
            # for b in pw_hint_message:
            #     bcc_pw_hint ^= b
            # pw_hint_message.append(bcc_pw_hint)
            # print(pw_hint_message)
            # seri.write(pw_hint_message)
            # print(bytearray(seri.readline()))

            # production = bytearray(b"\x01R2\x0296.1.3()\x03")
            # bcc = 0x01
            # for b in production:
            #     bcc ^= b
            # production.append(bcc)
            # print(production)
            # seri.write(production)
            # time.sleep(0.25)
            # while True:
            #     res = bytearray(seri.readline())
            #     if len(seri.readline()) == 0:
            #         break
            #     else:
            #         print("message received: ",res)
