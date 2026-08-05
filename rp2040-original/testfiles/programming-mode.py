import serial
import time
import argparse
from datetime import datetime, timedelta
from serial.serialutil import SerialException
import matplotlib.pyplot as plt
import struct

# --------------------------------------------------------------- BAUD RATE CHECK FUNCTION
def baud_rate_type(value):
    if value not in ["0", "1", "2", "3", "4", "5", "6"]:
        raise argparse.ArgumentTypeError(f"Baud rate must be a string representing a value between 0 and 6. Invalid value: '{value}'")
    return value
# ----------------------------------------------------------------------------------------

parser = argparse.ArgumentParser(description="A guide to show programming mode options and it's parameters to choose.")

parser.add_argument("--serial_number", type=str, default="", help="Serial number. Should be 9 characters")
parser.add_argument("--baud-rate", type=baud_rate_type, default="6", help="Baud rate. Should be a value between 0 and 6")
parser.add_argument("-lp", "--load-profile", nargs='*', help="Load Profile Request Option. If selected, you can specify the date in the format YY-MM-DD,HH:MM, for start and end date respectively.")
parser.add_argument("-ts", "--threshold-set", nargs='?', const='', help="Threshold Set Request Option. If selected, you can specify the threshold value in the format XXX (must be 3 characters)")
parser.add_argument("-ds", "--datetime-set", action="store_true", help="Datetime Set Request Option. Sets date and time to the current date and time.")
parser.add_argument("-tg", "--threshold-get", nargs='*', help="Threshold Get Records Request Option. If selected, you can specify the date in the format YY-MM-DD,HH:MM:SS, for start and end date respectively.")
parser.add_argument("-tp", "--threshold-pin", action="store_true", help="Reset Threshold Pin Option. If threshold pin is set, it will be reset.")
parser.add_argument("-ac", "--amplitude-change", nargs='*', help="Get Sudden Amplitude Change Records")
parser.add_argument("-rt", "--read-time", action="store_true", help="read current time of device")
parser.add_argument("-rd", "--read-date", action="store_true", help="read current date of device")
parser.add_argument("-rs", "--read-serialnumber", action="store_true", help="read serial number of device")
parser.add_argument("-rvmax", "--read-vrms-max", action="store_true", help="read last max vrms value of device")
parser.add_argument("-rvmin", "--read-vrms-min", action="store_true", help="read last min vrms value of device")
parser.add_argument("-rvmean", "--read-vrms-mean", action="store_true", help="read last mean vrms value of device")
parser.add_argument("-rrd", "--read-reset-date", action="store_true", help="read reset dates of this device")
parser.add_argument("-p", "--production", action="store_true", help="Production Info Request Option. Prints production information.")

args = parser.parse_args()

print("serial number: ", args.serial_number)
print("baud rate: ", args.baud_rate)
print("load profile args: ", args.load_profile)
print("threshold set args: ", args.threshold_set)
print("datetime set args: ", args.datetime_set)
print("threshold get args: ", args.threshold_get)
print("threshold pin args: ", args.threshold_pin)
print("production args: ", args.production)
print("amplitude change args: ", args.amplitude_change)

# ----------------------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------------------------------------------------

def checkSubstrings(main_string):
    return '<2>' in main_string or '<1>' in main_string

# ---------------------------------------------------------------------------------------------------------------------

def sendEndConnectionMessage():
    end_connection_bytes = bytearray(b'\x01\x42\x30\x03\x71')
    seri.write(end_connection_bytes)

def checkResponseMessage(response_msg):
    response_max_b_rate = int(chr(response_msg[4]))
    print("response max baud rate: ", response_max_b_rate)

    if len(response_msg) < 8:
        print("Invalid response message length!")
        return False
    
    if not checkSubstrings(response_msg.decode("utf-8")):
        print("Meter Generation cannot found!")
        return False

    if(response_max_b_rate > 6 or response_max_b_rate < 0):
        print("Invalid baud rate!")
        return False
    
    if response_msg[0] != 47:
        print("Invalid message format!")
        return False

    print("Incoming communication response message is valid!")
    return True

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

def calculateBCC(data, xor):
    for i in data:
        xor ^= i

    return xor

# ---------------------------------------------------------------------------------------------------------------------

def getCurrentTime():
    current_time = datetime.now()
    print("current time: ",current_time)

    current_time = current_time + timedelta(seconds=2)
    print("current time (+3 seconds): ",current_time)

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

    datetimeobj = {
        "year": year_str,
        "month": month_str,
        "day": day_str,
        "hour": hour_str,
        "minute": minute_str,
        "second": second_str
    }

    return datetimeobj

# ---------------------------------------------------------------------------------------------------------------------
def sendMessage(msg):
    msg_bcc = calculateBCC(msg, msg[0])
    msg.append(msg_bcc)

    print("message to send: ", msg)
    seri.write(msg)
# ---------------------------------------------------------------------------------------------------------------------

def prepareLoadProfileRequestWithDate(args,header):
    lp_request_message_tail = bytearray(b"\x29\x03")

    if(len(args) > 2):
        raise ValueError("Too Many Arguments!")


    if(len(args) == 1):
        print("only 1 time value provided, this value is going to set as start value and end date will be current time.")
        current_time_dict = getCurrentTime()

        start_date = args[0]
        end_date = current_time_dict["year"] + "-" + current_time_dict["month"] + "-" + current_time_dict["day"] + ";" + current_time_dict["hour"] + ":" + current_time_dict["minute"] + ":" + current_time_dict["second"]
    else:
        print("both time values are provided.")
        start_date = args[0]
        end_date = args[1]

    date_range = start_date + ";" + end_date
    lp_request_message = header + date_range.encode() + lp_request_message_tail

    print("lp_request_message: ", lp_request_message)

    return lp_request_message


# ---------------------------------------------------------------------------------------------------------------------
def sendLoadProfileRequest(msg):
    # load profile request without date
    msg_buffer = bytearray()

    msg_bcc = calculateBCC(msg, msg[0])
    msg.append(msg_bcc)
    seri.write(msg)

    while True:
        record = bytearray(seri.readline())
        
        if(len(record) == 0):
            print("Incoming data is empty")
            break

        if(record[0] == 0x15):
            print("Tariff Device sent NACK.")
            print("No records in this device.")
            break

        msg_buffer.extend(record)
        print(record)

        if(len(record) == 3 and record[0] == 0x0D and record[1] == 0x03):
            break

    if(len(msg_buffer) == 0):
        print("No data received")
        return

    # print(msg_buffer)
    msg_buffer_bcc = msg_buffer.pop()
    message_buffer_bcc_calculated = calculateBCC(msg_buffer, msg_buffer[0])

    print("incoming buffer bcc: ", msg_buffer_bcc)
    print("calculated bcc: ", message_buffer_bcc_calculated)

    if(msg_buffer_bcc != message_buffer_bcc_calculated):
        print("BCC check failed!")
    else:
        print("BCC check passed!")

    time.sleep(0.25)

# ---------------------------------------------------------------------------------------------------------------------

def sendDatetimeSetRequest():
    current_time_obj = getCurrentTime()
    
    passwordMessage = bytearray(b'\x01\x50\x31\x02\x2812345678\x29\x03')
    # ------------------------------------------------------------------------ //
    sendMessage(passwordMessage)
    print("password request sent!")
    time.sleep(0.25)

    print(seri.readline())
    # ------------------------------------------------------------------------ //
    time_str = current_time_obj["hour"] + ":" + current_time_obj["minute"] + ":" + current_time_obj["second"]
    time_str = time_str.encode()
            
    timeset_head = bytearray(b'\x01\x57\x32\x02\x30\x2E\x39\x2E\x31\x28')
    timeset_date = time_str
    timeset_tail = bytearray(b'\x29\x03')
            
    timeset = timeset_head + timeset_date + timeset_tail
    
    sendMessage(timeset)
    print("timeset request sent!")
    time.sleep(0.25)

    print(seri.readline())
    # ------------------------------------------------------------------------ //
    passwordMessage2 = bytearray(b'\x01\x50\x31\x02\x2812345678\x29\x03')
    sendMessage(passwordMessage2)
    print("password request sent!")
    time.sleep(0.25)

    print(seri.readline())
    # ------------------------------------------------------------------------ //
    date_str = current_time_obj["year"] + "-" + current_time_obj["month"] + "-" + current_time_obj["day"]
    date_str = date_str.encode()

    dateset_head = bytearray(b'\x01\x57\x32\x02\x30\x2E\x39\x2E\x32\x28')
    dateset_date = date_str
    dateset_tail = bytearray(b'\x29\x03')
            
    dateset = dateset_head + dateset_date + dateset_tail
    
    sendMessage(dateset)
    print("dateset request sent!")

    time.sleep(0.25)

    print(seri.readline())

# ---------------------------------------------------------------------------------------------------------------------

def sendThresholdSetRequest():
    threshold_head = bytearray(b'\x01\x57\x32\x02T.V.1(')
    threshold_val = args.threshold_set
    threshold_tail = bytearray(b')\x03')

    threshold_set_msg = threshold_head + threshold_val.encode("utf-8") + threshold_tail
    
    sendMessage(threshold_set_msg)
    print("threshold set request sent!")
    print(seri.readline())

    time.sleep(0.25)

# ---------------------------------------------------------------------------------------------------------------------

def sendThresholdPINResetRequest():
    threshold_pin_msg = bytearray(b'\x01\x57\x32\x02T.P.1()\x03')

    sendMessage(threshold_pin_msg)

    print("threshold pin reset request sent!")
    print(seri.readline())

    time.sleep(0.25)

# ----------------------------------------------------------------------------------------

def split_and_trim_bytearray(data, chunk_size=4099, trim_size=3):
    # Split the bytearray into chunks
    chunks = [data[i:i + chunk_size] for i in range(0, len(data), chunk_size)]
    
    # Trim the last 3 elements from each chunk if the chunk is larger than the trim size
    trimmed_chunks = [chunk[:-trim_size] if len(chunk) > trim_size else chunk for chunk in chunks]
    
    return trimmed_chunks

def displayChunks(chunks):

    # Displaying the chunks
    for i, chunk in enumerate(chunks):
        print(len(chunk))

        date_values = chunk[:12]
        data_values = chunk[12:4012]
        vrms_values = chunk[4012:4052]
        variance_value = chunk[4052:4054]
        padding = chunk[4054:]

        # Convert date values (ASCII) to a human-readable string format
        date_str = date_values.decode('ascii')
        formatted_date = f"20{date_str[:2]}-{date_str[2:4]}-{date_str[4:6]} {date_str[6:8]}:{date_str[8:10]}:{date_str[10:12]}"

        # Convert VRMS values (assuming float format in C, so 4 bytes per float)
        vrms_values = struct.unpack('10f', vrms_values)  # Convert 40 bytes to 10 float values

        # Convert variance value (assuming it's a 2-byte unsigned integer)
        variance_value = struct.unpack('H', variance_value)[0]  # Convert to unsigned short

        # Convert data values (assuming they represent some measurable quantity)
        data_values = list(data_values)  # Assuming each byte is a data point

        # Plotting the data
        plt.figure(figsize=(15, 6))
        plt.plot(data_values, label='Data Values')

        plt.axvline(x=2000, color='red', linestyle='--', label='Second Dividor')

        # Add labels and title
        plt.xlabel('Sample Number')
        plt.ylabel('Data Value')
        plt.title(f"Date: {formatted_date},\nVRMS Values: {vrms_values},\nVariance Value: {variance_value}")
        plt.legend()
        
        # Show the plot
        plt.show()

# ----------------------------------------------------------------------------------------

def sendAmplitudeChangeRequest(msg):
    msg_bcc = calculateBCC(msg, msg[0])
    msg.append(msg_bcc)
    seri.write(msg)
    print("amplitude change request sent!")

    time.sleep(0.25)

    alldata = bytearray()

    while True:
        char = seri.read()

        if(char == b''):
            print("Incoming data is empty")
            break

        alldata.extend(char)

    bcc = alldata.pop()
    etx = alldata.pop()
    crlast = alldata.pop()
    alldata = alldata[1:]

    print(alldata)

    chunks = split_and_trim_bytearray(alldata)
    return chunks


# ----------------------------------------------------------------------------------------

def sendVRMSReadRequest(msg):
    sendMessage(msg)

    time.sleep(0.25)

    result = bytearray(seri.readline())
    print(result)

    inc_bcc = result.pop()
    calculated_bcc = calculateBCC(result, result[0])

    print("incoming bcc: ",inc_bcc,"calculated bcc: ", calculated_bcc)

    if(inc_bcc == calculated_bcc):
        print("message is correct!")
    else:
        print("message is NOT correct!")

# ----------------------------------------------------------------------------------------

def readTime():
    read_time_msg = bytearray(b'\x01\x52\x32\x020.9.1()\x03')
    sendMessage(read_time_msg)

    time.sleep(0.25)

    result = bytearray(seri.readline())
    print(result)

    result_bcc_inc = result.pop()
    result_bcc_calculated = calculateBCC(result, result[0])

    print("incoming bcc:",result_bcc_inc,"calculated bcc:",result_bcc_calculated)

    if(result_bcc_inc == result_bcc_calculated):
        print("message is correct!")
    else:
        print("message is NOT correct!")

# ----------------------------------------------------------------------------------------

def readDate():
    read_date_msg = bytearray(b'\x01\x52\x32\x020.9.2()\x03')
    sendMessage(read_date_msg)

    time.sleep(0.25)

    result = bytearray(seri.readline())
    print(result)

    result_bcc_inc = result.pop()
    result_bcc_calculated = calculateBCC(result, result[0])

    print("incoming bcc:",result_bcc_inc,"calculated bcc:",result_bcc_calculated)

    if(result_bcc_inc == result_bcc_calculated):
        print("message is correct!")
    else:
        print("message is NOT correct!")

# ----------------------------------------------------------------------------------------

def readSerialNumber():
    read_serial_number = bytearray(b'\x01\x52\x32\x020.0.0()\x03')
    sendMessage(read_serial_number)

    time.sleep(0.25)

    result = bytearray(seri.readline())
    print(result)

    result_bcc_inc = result.pop()
    result_bcc_calculated = calculateBCC(result, result[0])

    print("incoming bcc:",result_bcc_inc,"calculated bcc:",result_bcc_calculated)

    if(result_bcc_inc == result_bcc_calculated):
        print("message is correct!")
    else:
        print("message is NOT correct!")

# ----------------------------------------------------------------------------------------

def sendProductionMessageRequest():
    production = bytearray(b"\x01R2\x0296.1.3()\x03")

    sendMessage(production)

    print("production message request sent!")
    print(seri.readline())

    time.sleep(0.25)


# ----------------------------------------------------------------------------------------

# variables
max_baud_rate = b"\x36"
max_baud_rate_integer = int(max_baud_rate.decode("utf-8"))
baud_rates = [300, 600, 1200, 2400, 4800, 9600, 19200]
information_message = bytearray(b"\x0601\r\n")
communication_req_msg_base = bytearray(b"/?!\r\n")

# serial number is optional
serial_number = args.serial_number

if(args.baud_rate):
    b_rate_ascii = ord(args.baud_rate)
    max_baud_rate = bytes([b_rate_ascii])
    max_baud_rate_integer = int(max_baud_rate.decode("utf-8"))

try:
    seri = serial.Serial("/dev/ttyUSB0", baudrate=300, bytesize=7, parity="E", stopbits=1, timeout=1)
except FileNotFoundError:
    print("File not found, please check connection and try again!")
    exit(1)
except SerialException as e:
    print("Serial Exception: ", e)
    print('Please check connection and try again!')
    exit(1)

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

# check response, if not valid, exit
if(not checkResponseMessage(communication_request_message_response)):
    print("Invalid response message!")
    exit(1)

# put baudrate to information message
information_message[2:2] = max_baud_rate
seri.write(information_message)
time.sleep(0.25)

seri.close()
seri = serial.Serial(
    "/dev/ttyUSB0",
    baudrate=baud_rates[max_baud_rate_integer],
    bytesize=7,
    parity="E",
    stopbits=1,
    timeout=1,
)

# read information response message
information_response = bytearray(seri.readline())
print("information response: ", information_response)

# extract bcc of incoming message and calculate bcc
bcc_inf_res_received = information_response.pop()
bcc_inf_res_calculated = calculateBCC(information_response, information_response[0])

# check if information response is valid

# check bcc
if(bcc_inf_res_received != bcc_inf_res_calculated):
    print("Information response bcc check failed!")
    exit(1)

if not checkInformationResponse(information_response):
    print("Invalid information response message!")
    exit(1)

if args.load_profile is not None:
    if len(args.load_profile) == 0:
        print("Load Profile Request message will send without date!")
        lp_request_message = bytearray(b"\x01\x52\x32\x02\x50\x2E\x30\x31\x28;\x29\x03")
    else:
        print("Load Profile Request message will send date!")
        lp_request_message_head = bytearray(b"\x01\x52\x32\x02\x50\x2E\x30\x31\x28")
        lp_request_message = prepareLoadProfileRequestWithDate(args.load_profile,lp_request_message_head)

    sendLoadProfileRequest(lp_request_message)


if args.datetime_set:
    sendDatetimeSetRequest()

if args.threshold_set:
    sendThresholdSetRequest()

if args.threshold_get is not None:
    if len(args.threshold_get) == 0:
        print("Threshold Get Request message will send without date!")
        threshold_get_msg = bytearray(b'\x01\x52\x32\x02T.R.1(;)\x03')
    else:
        print("Threshold Get Request message will send date!")
        threshold_request_message_head = bytearray(b'\x01\x52\x32\x02T.R.1(')
        threshold_get_msg = prepareLoadProfileRequestWithDate(args.threshold_get,threshold_request_message_head)

    sendLoadProfileRequest(threshold_get_msg)

if args.threshold_pin:
    sendThresholdPINResetRequest()

if args.amplitude_change is not None:
    if len(args.amplitude_change) == 0:
        print("Amplitude Change Request message will send without date!")
        amplitude_change_msg = bytearray(b'\x01\x52\x32\x029.9.0(;)\x03')
    else:
        print("Amplitude Change Request message will send date!")
        amplitude_change_request_message_head = bytearray(b'\x01\x52\x32\x029.9.0(')
        amplitude_change_msg = prepareLoadProfileRequestWithDate(args.amplitude_change,amplitude_change_request_message_head)
    
    chunks = sendAmplitudeChangeRequest(amplitude_change_msg)
    sendEndConnectionMessage()
    displayChunks(chunks)

if(args.read_time):
    readTime()

if(args.read_date):
    readDate()

if(args.read_serialnumber):
    readSerialNumber()

if args.production:
    sendProductionMessageRequest()

if args.read_vrms_max:
    max_read_msg = bytearray(b'\x01\x52\x32\x02\x33\x32\x2E\x37\x2E\x30\x28\x29\x03')
    sendVRMSReadRequest(max_read_msg)

if args.read_vrms_min:
    min_read_msg = bytearray(b'\x01\x52\x32\x02\x35\x32\x2E\x37\x2E\x30\x28\x29\x03')
    sendVRMSReadRequest(min_read_msg)

if args.read_vrms_mean:
    mean_read_msg = bytearray(b'\x01\x52\x32\x02\x37\x32\x2E\x37\x2E\x30\x28\x29\x03')
    sendVRMSReadRequest(mean_read_msg)

if args.read_reset_date:
    reset_date_request_msg = bytearray(b'\x01\x52\x32\x02\x52\x2E\x44\x2E\x30\x28\x29\x03')
    sendLoadProfileRequest(reset_date_request_msg)

sendEndConnectionMessage()

seri.close()
    
