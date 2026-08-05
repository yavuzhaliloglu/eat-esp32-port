import serial
from bitarray import bitarray
import time
import hashlib


def calculateBCC(data, xor):
    for i in data:
        xor ^= i

    return xor & 0x7F


def bitArrayToByte(lsb_arr):
    lsb_arr_list = lsb_arr.tolist()
    lsb_byte = 0

    for i in range(len(lsb_arr)):
        lsb_byte = (lsb_byte << 1) | lsb_arr_list[i]

    return lsb_byte


def conversionTo7Bit(data_arr):
    lsb_buffer = bitarray()
    dest_arr = bytearray()

    for index, byte in enumerate(data_arr):
        lsb = byte & 0x01
        byte = byte >> 1

        lsb_buffer.append(lsb)
        dest_arr.append(byte)

        if len(lsb_buffer) == 7 or index == len(data_arr) - 1:
            lsb_buffer = bitarray(reversed(lsb_buffer))
            lsb_byte = bitArrayToByte(lsb_buffer)

            dest_arr.append(lsb_byte)
            lsb_buffer = bitarray()

    return dest_arr


def messageLengthConversion(msg_len):
    lsb_buffer = bitarray()
    message_len_array = bytearray()

    for byte in msg_len.to_bytes(3, byteorder="big"):
        lsb = byte & 0x01
        byte = byte >> 1

        lsb_buffer.append(lsb)
        message_len_array.append(byte)

    lsb_buffer = bitarray(reversed(lsb_buffer))
    len_lsb_byte = bitArrayToByte(lsb_buffer)

    message_len_array.append(len_lsb_byte)

    return message_len_array

def printArrayHexBinary(arr):
    for i in arr:
        print("hexadecimal: ", hex(i), "\tbinary: ", bin(i))

    print("\n")


seri = serial.Serial(
    "/dev/ttyUSB0", baudrate=300, bytesize=7, parity="E", stopbits=1, timeout=2
)

md5_hash = hashlib.md5()

meeting_message = bytearray(b"/?!\r\n")
seri.write(meeting_message)
time.sleep(0.25)
meeting_response = bytearray(seri.readline())
time.sleep(0.25)
print(meeting_response)

max_baud_rate = b"\x36"
mbr_str = max_baud_rate.decode("utf-8")
mbr_int = int(mbr_str)

baud_rates = [300, 600, 1200, 2400, 4800, 9600, 19200]

if meeting_response[0] == 47 and len(meeting_response) > 5:
    information_message = bytearray(b"\x0601\r\n")
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

    if information_message[3] == 0x30:
        print("entered readout mode")

        for i in range(4):
            res = bytearray(seri.readline())
            print(res)

    else:
        print("entered programming mode")

        information_response = bytearray(seri.readline())
        print(information_response)
        bcc_received = information_response.pop()

        bcc = 0x01
        for i in information_response:
            bcc ^= i

        print("information response bcc result: ", hex(bcc))
        
        if bcc_received == bcc:
            bcc_pw = 0x01
            pw_msg = bytearray(b"\x01\x50\x31\x02(12345678)\x03")

            for byte in pw_msg:
                bcc_pw ^= byte
            pw_msg.append(bcc_pw)

            print("pw msg : ", pw_msg)
            # send to pw message and wait for ack message
            seri.write(pw_msg)
            pw_res = bytearray(seri.readline())
            time.sleep(0.25)

            if pw_res[0] == 0x06:
                print("ACK message received.")

            # initlialize the program info
            bcc_repogram = 0x01
            reprogram_info = bytearray(b"\x01\x57\x32\x02O.T.A()\x03")
            for byte in reprogram_info:
                bcc_repogram ^= byte
            reprogram_info.append(bcc_repogram)

            print("repogram info : ", reprogram_info)
            # send to reporgram info message and wait for ack message
            seri.write(reprogram_info)
            rpg_res = bytearray(seri.readline())
            time.sleep(0.25)

            if rpg_res[0] == 0x06:
                print("ACK message received.")

                # open the file
                file = open(
                    "/home/yavuz/pico/freertos-pico/blink/build/blink.bin", "rb"
                )
                reprogram_bin = bytearray(file.read())

                # reprogram_bin = bytearray(b"\x01\x23\x45")
                # print("\nreprogram_bin: ")
                # printArrayHexBinary(reprogram_bin)

                # initialize the binary header
                binary_header = bytearray()

                # add vector address to start program to binary header
                start_address = 0x10009100.to_bytes(8,"little")
                binary_header = start_address + binary_header

                # calculate the md5 checksum for the binary and add the binary header array (16 Bytes)
                md5_csum = hashlib.md5(reprogram_bin).digest()
                csum_bytearray = bytearray(md5_csum)
                print("\nMD5 Checksum calculated!")
                binary_header = csum_bytearray + binary_header

                # add the binary len to the binary header array (4 Bytes)
                binary_len = len(reprogram_bin)
                binary_header = (
                    bytearray(reversed(bytearray(binary_len.to_bytes(4, "big"))))
                    + binary_header
                )
                print("\nbinary len: ", binary_len)

                # add epoch unix time value (4 Bytes)
                current_epoch = int(time.time())
                print("\nepoch value: ", current_epoch)
                binary_header = (
                    bytearray(reversed(current_epoch.to_bytes(4, "big")))
                    + binary_header
                )
                print("\nbinary header after adding epoch:")
                printArrayHexBinary(binary_header)

                # add padding to binary header (should be 256 bytes)
                binary_header = binary_header + bytearray(256 - len(binary_header))
                print("\nlength of binary header", len(binary_header))

                # add binary header to binary file
                reprogram_bin = binary_header + reprogram_bin

                # convert new binary file to 7-bit format
                reprogram_msg2 = conversionTo7Bit(reprogram_bin)

                # send file
                print("sending data...")
                seri.write(reprogram_msg2)
                print("data sent.")
