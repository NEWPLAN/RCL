import sys
import argparse
from scapy.all import sr1, IP, ICMP

def main():
    parser = argparse.ArgumentParser(prog = sys.argv[0], description = 'ICMP Ping')
    
    parser.add_argument('-d', '--destination', dest = 'dest', help = 'Destination IP')
    args = parser.parse_args()
    
    if not args.dest:
        parser.error('No destination IP')
        sys.exit(1)
    
    # Send the packet in layer 3 and return only the first answer 
    p = sr1(IP(dst = args.dest)/ICMP())
    
    if p:
        p.show()

if __name__ == "__main__":
    main()
    