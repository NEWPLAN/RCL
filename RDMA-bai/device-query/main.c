#include <stdio.h>
#include <infiniband/verbs.h>

int main(int argc, char *argv[])
{       
        int num_devices, i;
        struct ibv_device **dev_list;

        dev_list = ibv_get_device_list(&num_devices);
        if (!dev_list) {
                printf("No IB devices\n");
                return 0;
        }

        // for each IB device
        for (i = 0; i < num_devices; i++) {
                printf("Device %d: %s\n", i + 1, ibv_get_device_name(dev_list[i]));
        }

        // free the list of IB devices
        ibv_free_device_list(dev_list);

        return 0;
}