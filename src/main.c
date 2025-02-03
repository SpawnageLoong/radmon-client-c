/*
 * Copyright (C) 2025  Richard Loong
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

void print_menu();

int main()
{
    int can_active;
    int ret;
    int s, nbytes;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    memset(&frame, 0, sizeof(struct can_frame));
    cout << "Copyright (C) 2025  Richard Loong\nThis program comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it under certain conditions.";
    
    // Check if can0 interface is active, activate it if not
    FILE *fp = popen("ip link show | grep \"can0\"", "r");
    if (fp == NULL) {
        can_active = 0;
        system("sudo ip link set can0 type can bitrate 100000");
        system("sudo ifconfig can0 up");
    } else {
        can_active = 1;
    }
    
    //printf("this is a can receive demo\r\n");
    
    //1.Create socket
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) {
        perror("socket PF_CAN failed");
        return 1;
    }
    
    //2.Specify can0 device
    strcpy(ifr.ifr_name, "can0");
    ret = ioctl(s, SIOCGIFINDEX, &ifr);
    if (ret < 0) {
        perror("ioctl failed");
        return 1;
    }

    //3.Bind the socket to can0
    addr.can_family = PF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    ret = bind(s, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        perror("bind failed");
        return 1;
    }
    
    //4.Define receive rules
    struct can_filter rfilter[1];
    rfilter[0].can_id = 0x123;
    rfilter[0].can_mask = CAN_SFF_MASK;
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));

    //5.Receive data and exit
    while(1) {
        nbytes = read(s, &frame, sizeof(frame));
        if(nbytes > 0) {
            printf("can_id = 0x%X\r\ncan_dlc = %d \r\n", frame.can_id, frame.can_dlc);
            int i = 0;
            for(i = 0; i < 8; i++)
                printf("data[%d] = %d\r\n", i, frame.data[i]);
            break;
        }
    }
    
    //6.Close the socket and can0
    close(s);
    if (can_active == 1) {
        system("sudo ifconfig can0 down");
    }
    
    return 0;
}

void print_menu() {
    cout << "\n██████   █████  ██████  ███    ███  ██████  ███    ██      ██████  █████  ███    ██ ";
    cout << "\n██   ██ ██   ██ ██   ██ ████  ████ ██    ██ ████   ██     ██      ██   ██ ████   ██ ";
    cout << "\n██████  ███████ ██   ██ ██ ████ ██ ██    ██ ██ ██  ██     ██      ███████ ██ ██  ██ ";
    cout << "\n██   ██ ██   ██ ██   ██ ██  ██  ██ ██    ██ ██  ██ ██     ██      ██   ██ ██  ██ ██ ";
    cout << "\n██   ██ ██   ██ ██████  ██      ██  ██████  ██   ████      ██████ ██   ██ ██   ████ ";
    cout << "\n";
    cout << "\n____________________________________________________________________________________";
    cout << "\n";
}