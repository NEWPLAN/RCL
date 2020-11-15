import os
import time


def get_local_IP():
    import socket
    hostname = socket.gethostname()
    ip = socket.gethostbyname(hostname)
    return ip


def get_cluster():
    cluster = ["12.12.12."+str(x) for x in range(116, 100, -1)]
    return cluster


def check_cluster(IPlist, ip):
    if ip not in IPlist:
        print(ip, " not in ", IPlist)
    else:
        print(ip, " are cached in ", IPlist)


def build_msg_size():
    msg = [" --size 100000", " --size 400000",
           " --size 1000000", " --size 4000000",
           " --size 10000000"]
    return msg


def filter_cluster(cluster):
    filter_cluster = []
    for x in range(len(cluster), 1, -1):
        filter_cluster.append(cluster[:x])
        # print(cluster[:x])
    return filter_cluster


def build_cmd(cluster, myip, msg):
    #os.system("ls ./")
    cluster_for_current = filter_cluster(cluster)
    for each in cluster_for_current:
        if myip in each:
            cluster_config = " --control " + " ".join(each)
            for msg_config in msg:
                cmd_args = "../RDMABenchmark " + cluster_config + msg_config
                for loops in range(5):
                    print(cmd_args)
                    time.sleep(5)
                    os.system(cmd_args)

        else:
            print(myip, "is not in: ", each)


def test():
    local_ip_addr = get_local_IP()
    cluster_ip_addr = get_cluster()
    msg = build_msg_size()

    print("local IP: " + local_ip_addr)
    print("Cluster: ", get_cluster())
    print("msg: ", msg)

    #check_cluster(cluster_ip_addr, local_ip_addr)

    build_cmd(cluster_ip_addr, local_ip_addr, msg)


if __name__ == "__main__":
    test()
