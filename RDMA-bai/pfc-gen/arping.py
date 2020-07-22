import sys
import argparse
from scapy.all import srp, Ether, ARP, conf

def main():
    parser = argparse.ArgumentParser(prog = sys.argv[0], description = 'ARP Ping')
    
    parser.add_argument('-d', '--destination', dest = 'dest', help = 'Destination IP')
    parser.add_argument('-i', '--interface', dest = 'intf', help = 'Interface')
    
    args = parser.parse_args()
    
    valid_args = True 
    if not args.dest:
        parser.error('No destination IP')
        valid_args = False 
    
    if not args.intf:
        parser.error('No interface')
        valid_args = False 
    
    if valid_args == False:
        sys.exit(1)
        
    conf.verb = 0
    ans,unans = srp(Ether(dst = "ff:ff:ff:ff:ff:ff") / ARP(pdst = args.dest), timeout = 2, iface = args.intf)

    for send, rcv in ans:
        print(rcv.sprintf(r"%Ether.src% & %ARP.psrc%"))
        #print(rcv.sprintf(r"%Ether.src & %ARP.psrc"))

if __name__ == "__main__":
    main()
    