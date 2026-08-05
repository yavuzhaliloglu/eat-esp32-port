import serial
import time
import argparse

# --------------------------------------------------------------- BAUD RATE CHECK FUNCTION
def baud_rate_type(value):
    if value not in ["0", "1", "2", "3", "4", "5", "6"]:
        raise argparse.ArgumentTypeError(f"Baud rate must be a string representing a value between 0 and 6. Invalid value: '{value}'")
    return value
# ---------------------------------------------------------------------------------------------------------------------

parser = argparse.ArgumentParser(description="A guide to show readout mode options to choose.")

parser.add_argument("--serial-number", type=str, default="", help="Serial number. Should be 9 characters")
parser.add_argument("--baud-rate", type=baud_rate_type, default="6", help="Baud rate. Should be a value between 0 and 6")
parser.add_argument("-rm", "--readout-mode", action="store_true", help="Readout mode shows content of device")
parser.add_argument("-dm", "--debug-mode", action="store_true", help="Debug mode shows content of sector, records and device info")

args = parser.parse_args()

print("serial number: ", args.serial_number)
print("baud rate: ", args.baud_rate)
print("readout mode arg: ", args.readout_mode)
print("debug mode arg: ", args.debug_mode)

# ------------------------------------------------------------------------------------------------------------

def replaceSerialNumber(byte_data, new_serial_number):
    # if serial number was not provided, return the original bytearray
    if not new_serial_number:
        return byte_data

    # Find the indices of '!' and '\r'
    start_index = byte_data.find(b'?') + 1
    end_index = byte_data.find(b'!')

    if start_index == -1 or end_index == -1:
        raise ValueError("Invalid bytearray format")

    # Convert the new serial number to bytes
    new_serial_bytes = new_serial_number.encode('utf-8')

    # Replace the old serial number with the new one
    byte_data[start_index:end_index] = new_serial_bytes

    print("Serial number implemented successfully!")
    return byte_data

# ------------------------------------------------------------------------------------------------------------

def checkResponseMessage(response_msg):
    response_max_b_rate = int(chr(response_msg[4]))
    print("response max baud rate: ", response_max_b_rate)

    if len(response_msg) < 8:
        print("Invalid response message length!")
        return -1
    
    if not checkSubstrings(response_msg.decode("utf-8")):
        print("Meter Generation cannot found!")
        return -1

    if(response_max_b_rate > 6 or response_max_b_rate < 0):
        print("Invalid baud rate!")
        return -1
    
    if response_msg[0] != 47:
        print("Invalid message format!")
        return -1

    print("Incoming communication response message is valid!")
    return response_max_b_rate

# -------------------------------------------------------------------------------------------------------------

def checkSubstrings(main_string):
    return '<2>' in main_string or '<1>' in main_string

# ---------------------------------------------------------------------------------------------------------------------

def calculateBCC(data, xor):
    for i in data:
        xor ^= i

    return xor

# ---------------------------------------------------------------------------------------------------------------------

def checkInformationResponse(response_msg):
    information_response_head = bytearray(b"\x01P0\x02(")
    information_response_tail = bytearray(b")\x03")
    information_response_len = len(information_response_head) + len(information_response_tail) + 9      # 9 is the length of the serial number

    if(len(response_msg) != information_response_len):
        print("Invalid response message length!")
        return False

    if not information_response_head in response_msg or not information_response_tail in response_msg:
        print("Invalid response message format!")
        return False

    print("Incoming communication response message is valid!")
    return True

# ---------------------------------------------------------------------------------------------------------------------

# variables
seri = serial.Serial("/dev/ttyUSB0", baudrate=300, bytesize=7, parity="E", stopbits=1, timeout=2)
max_baud_rate = b"\x36"
max_baud_rate_integer = int(max_baud_rate.decode("utf-8"))
baud_rates = [300, 600, 1200, 2400, 4800, 9600, 19200]
selected_baud_rate_integer = max_baud_rate_integer
selected_baud_rate = max_baud_rate

communication_req_msg_base = bytearray(b"/?!\r\n")
readout_buffer = bytearray()

# serial number is optional
serial_number = args.serial_number

if(args.baud_rate):
    selected_baud_rate = bytes([ord(args.baud_rate)])
    selected_baud_rate_integer = int(selected_baud_rate.decode("utf-8"))

if(serial_number and len(serial_number) != 9):
    print("Wrong serial number format! Serial number should be 9 characters long!")
    exit(1)

# generate communication request
communication_request_message = replaceSerialNumber(communication_req_msg_base, serial_number)
seri.write(communication_request_message)

# read response
time.sleep(0.25)
communication_request_message_response = bytearray(seri.readline())
time.sleep(0.25)
print(communication_request_message_response)

# check if response is valid
if(len(communication_request_message_response) == 0):
    print("Communication response message cannot be received!")
    exit(1)

response_max_b_rate = checkResponseMessage(communication_request_message_response)
# check response, if not valid, exit
if(response_max_b_rate == -1):
    print("Invalid response message!")
    exit(1)

if args.readout_mode:
    information_message = bytearray(b"\x0600\r\n")
elif args.debug_mode:
    information_message = bytearray(b"\x0604\r\n")
else:
    print("Unsupported readout mode!")
    exit(1)

# put baudrate to information message
information_message[2:2] = selected_baud_rate
seri.write(information_message)
time.sleep(0.25)

seri.close()
seri = serial.Serial(
    "/dev/ttyUSB0",
    baudrate=baud_rates[selected_baud_rate_integer],
    bytesize=7,
    parity="E",
    stopbits=1,
    timeout=2,
)

while True:
    line = bytearray(seri.readline())

    if len(line) == 0:
        break

    readout_buffer += line
    print(line)

if len(readout_buffer) == 0:
    print("Readout buffer is empty!")
    exit(1)

readout_buffer_bcc = readout_buffer.pop()
readout_buffer_bcc_calculated = calculateBCC(readout_buffer, readout_buffer[0])

print("BCC calculated: ", readout_buffer_bcc_calculated)
print("BCC incoming: ", readout_buffer_bcc)

if(readout_buffer_bcc != readout_buffer_bcc_calculated):
    print("BCC check failed!")
else:
    print("BCC check passed!")

# end_connection_bytes = bytearray(b'\x01\x42\x30\x03\x71')
# seri.write(end_connection_bytes)

