import sys
import argparse
from scapy.all import srp, Ether, Raw

def main():
    parser = argparse.ArgumentParser(description = 'Generate PFC pause frames')
    parser.add_argument('-p', '--priority', type = int, dest = 'prio', help = 'Priority to pause')
    parser.add_argument('-i', '--interface', dest = 'intf', help = 'Interface')
    parser.add_argument('-n', '--number', dest = 'num', type = int, help = 'Number of PFC pause frames to send')
    
    args = parser.parse_args()
    
    valid_args = True 
    error_info = '\n'
    if not args.prio:
        error_info = error_info + 'No priority\n'
        valid_args = False 
    
    elif args.prio < 0 or args.prio > 7:
        error_info = error_info + 'Priority should be in [0, 7]\n'
        valid_args = False 
    
    if not args.intf:
        error_info = error_info + 'No interface\n'
        valid_args = False 
    
    if not args.num:
        error_info = error_info + 'No number of PFC pause frames\n'
        valid_args = False 
    
    elif args.num <= 0:
        error_info = error_info + 'The number of PFC pause frames should be positive\n'
        valid_args = False 
    
    if valid_args == False:
        parser.error(error_info)
    
    eth_hdr = Ether(dst = '01:80:c2:00:00:01', src = '00:11:22:33:44:55', type = 0x8808)  
    opcode = b'\x01\x01'

    classvector = b'\x00' + (1 << args.prio).to_bytes(1, byteorder = 'big')
    
    classtime = b''
    for prio in range(0, 8):
        if prio == args.prio:
            classtime = classtime + b'\xff\xff'
        else:
            classtime = classtime + b'\x00\x00'
    
    pad = b'\x00' * 26
    paylaod = opcode + classvector + classtime + pad 
    
    for i in range(args.num):
        print(i + 1)
        srp(eth_hdr / Raw(load = paylaod), timeout = 2, iface = args.intf)
    
if __name__ == "__main__":
    main()
    