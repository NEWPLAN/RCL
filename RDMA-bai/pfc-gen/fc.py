import sys
import argparse
from scapy.all import srp, Ether, Raw

def main():
    parser = argparse.ArgumentParser(description = 'Generate Ethernet flow control pause frames')
    parser.add_argument('-i', '--interface', dest = 'intf', help = 'Interface')
    parser.add_argument('-n', '--number', dest = 'num', type = int, help = 'Number of flow control pause frames to send')
    
    args = parser.parse_args()
    
    valid_args = True 
    error_info = '\n'
    
    if not args.intf:
        error_info = error_info + 'No interface\n'
        valid_args = False 
    
    if not args.num:
        error_info = error_info + 'No number of flow control pause frames\n'
        valid_args = False 
    
    elif args.num <= 0:
        error_info = error_info + 'The number of flow control pause frames should be positive\n'
        valid_args = False 
    
    if valid_args == False:
        parser.error(error_info)
    
    eth_hdr = Ether(dst = '01:80:c2:00:00:01', src = '00:11:22:33:44:55', type = 0x8808)  
    
    opcode = b'\x00\x01'
    time = b'\xff\xff'
    pad = b'\x00' * 42
    
    paylaod = opcode + time + pad 
    
    for i in range(args.num):
        print(i + 1)
        srp(eth_hdr / Raw(load = paylaod), timeout = 2, iface = args.intf)
    
if __name__ == "__main__":
    main()