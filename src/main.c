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
#include <time.h>
#include <linux/limits.h>

using namespace std;

#define CMD_FRAME_ID 0x010;
#define RSP_FRAME_ID 0x011;

void print_logo();
void main_menu(int* user_input);
void command_can_dump_full();
void command_can_dump_part();
void save_can_dump_full();

int s;
struct can_frame frame;

int main()
{
    int user_input;
    int is_exit = 0;

    int ret;
    struct sockaddr_can addr;
    struct ifreq ifr;
    
    memset(&frame, 0, sizeof(struct can_frame));
    system("clear");
    cout << "\nCopyright (C) 2025  Richard Loong\nThis program comes with ABSOLUTELY NO WARRANTY.\nThis is free software, and you are welcome to redistribute it under certain conditions.\n\n";
    
    // Check if can0 interface is active, activate it if not
    FILE *fp = popen("ip link show | grep \"can0\" | grep \"UP\"", "r");
    if (fp == NULL) {
        cout << "can0 interface not detected, enabling now...\n";
        system("sudo ip link set can0 up type can bitrate 1000000");
        system("sudo ifconfig can0 txqueuelen 65536");
        cout << "can0 interface enabled\n\n";
    } else {
        cout << "can0 interface detected\n\n";
    }
    system("sudo ifconfig can0 up");
    
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
    rfilter[0].can_id = RSP_FRAME_ID;
    rfilter[0].can_mask = CAN_SFF_MASK;
    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter));
    

    //4.Disable filtering rules, do not receive packets, only send
    //setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

    print_logo();
    while (!is_exit) {
        main_menu(&user_input);
        switch (user_input) {
            case 1:
                command_can_dump_full();
                break;
            case 2:
                command_can_dump_part();
                break;
            case 3:
                save_can_dump_full();
                break;
            case 9:
                is_exit = 1;
                break;
            default:
                cout << "\nUnknown command received\nEnter a number 1-9: ";
                break;
        }
    }
    

    

    //5.Receive data and exit
    /*
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
    */
    
    // Close the socket and can0
    cout << "Exiting...\n\n";
    close(s);
    //system("sudo ifconfig can0 down");
    return 0;
}

void print_logo() {
    cout << "\n██████   █████  ██████  ███    ███  ██████  ███    ██      ██████  █████  ███    ██ ";
    cout << "\n██   ██ ██   ██ ██   ██ ████  ████ ██    ██ ████   ██     ██      ██   ██ ████   ██ ";
    cout << "\n██████  ███████ ██   ██ ██ ████ ██ ██    ██ ██ ██  ██     ██      ███████ ██ ██  ██ ";
    cout << "\n██   ██ ██   ██ ██   ██ ██  ██  ██ ██    ██ ██  ██ ██     ██      ██   ██ ██  ██ ██ ";
    cout << "\n██   ██ ██   ██ ██████  ██      ██  ██████  ██   ████      ██████ ██   ██ ██   ████ ";
    cout << "\n";
    return;
}

void main_menu(int* user_input) {
    cout << "\n____________________________________________________________________________________";
    cout << "\n";
    cout << "\nAvailable Options:";
    cout << "\n  1) Dump FRAM (32kB)";
    cout << "\n  2) Dump FRAM (Select a sector)";
    cout << "\n  3) Save FRAM to dump file (32kB)";
    cout << "\n";
    cout << "\n  9) Exit";
    cout << "\n";
    cout << "\nEnter a number 1-9: ";

    scanf("%d", user_input);
    cout << "\n";

    return;
}

void command_can_dump_full() {
    // Set send data
    frame.can_id = CMD_FRAME_ID;
    frame.can_dlc = 8; // data length
    frame.data[0] = 0x01;
    frame.data[1] = 0x00;
    frame.data[2] = 0x00;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;
    
    int nbytes;
    
    // Send message
    nbytes = write(s, &frame, sizeof(frame)); 
    cout << "\nDump command sent\n";
    if(nbytes != sizeof(frame)) {
        cout << "Send Error frame[0]!\r\n";
    }
    return;
}

void command_can_dump_part() {
    int is_input_valid;
    char user_hex_input[4];
    char address_H[2];
    char address_L[2];
    char length_H[2];
    char length_L[2];

    // Set send data
    frame.can_id = CMD_FRAME_ID;
    frame.can_dlc = 8; // data length
    frame.data[0] = 0x02;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    // Get param A (address)
    is_input_valid = 0;
    while (is_input_valid == 0) {
        is_input_valid = 1;
        cout << "\nEnter the starting address (4 hex-digits): ";
        
        scanf("%s", user_hex_input);
        cout << "\n";

        for (int i=0; i<4; i++) {
            if ( !user_hex_input[i] || 'f' < user_hex_input[i] || user_hex_input[i] < '0') {
                cout << "\nInvalid input. Please enter a 4 digit hex value (0 - F).\n";
                is_input_valid = 0;
                break;
            }
        }
        address_H[0] = user_hex_input[0];
        address_H[1] = user_hex_input[1];
        address_L[0] = user_hex_input[2];
        address_L[1] = user_hex_input[3];
    }

    // Get param B (length)
    is_input_valid = 0;
    while (is_input_valid == 0) {
        is_input_valid = 1;
        cout << "\nEnter the length to read (4 hex-digits): ";
        
        scanf("%s", user_hex_input);
        cout << "\n";

        for (int i=0; i<4; i++) {
            if ( !user_hex_input[i] || 'f' < user_hex_input[i] || user_hex_input[i] < '0') {
                cout << "\nInvalid input. Please enter a 4 digit hex value (0 - F).\n";
                is_input_valid = 0;
                break;
            }
        }
        length_H[0] = user_hex_input[0];
        length_H[1] = user_hex_input[1];
        length_L[0] = user_hex_input[2];
        length_L[1] = user_hex_input[3];
    }
    
    frame.data[1] = (unsigned char)strtol(address_H, NULL, 16);
    frame.data[2] = (unsigned char)strtol(address_L, NULL, 16);
    frame.data[3] = (unsigned char)strtol(length_H, NULL, 16);
    frame.data[4] = (unsigned char)strtol(length_L, NULL, 16);
    
    int nbytes;
    printf("can_id  = 0x%X\r\n", frame.can_id);
    printf("can_dlc = %d\r\n", frame.can_dlc);
    for(int i = 0; i < 8; i++)
        printf("data[%d] = %d\r\n", i, frame.data[i]);
    
    // Send message
    nbytes = write(s, &frame, sizeof(frame)); 
    cout << "\nmessage sent\n";
    if(nbytes != sizeof(frame)) {
        printf("Send Error frame[0]!\r\n");
    }
    return;
}

void save_can_dump_full() {
    FILE *fptr;
    time_t epoch_time;
    epoch_time = time(NULL);

    char filename[23 + sizeof(char)];
    sprintf(filename, "radmon-dumps/%ld.dump", epoch_time);

    char path[PATH_MAX + 1];
    ssize_t length = readlink("/proc/self/exe", path, PATH_MAX);
    path[length] = '\0';
    char cwd[sizeof(path)];
    strncpy(cwd, path, strlen(path) - 14);
    cwd[strlen(path) - 7] = '\0';

    char file_path[PATH_MAX + 25];
    sprintf(file_path, "%s/%s", cwd, filename);
    char touch[PATH_MAX + 32];
    sprintf(touch, "touch %s\n", file_path);

    // Create the dump file
    system(touch);
    fptr = fopen(file_path, "w");
    if (fptr == NULL) {
        cout << "Error opening file\n";
        return;
    }
    //cout << "filename: " << filename << "\n\n";

    // Set send data
    frame.can_id = CMD_FRAME_ID;
    frame.can_dlc = 8; // data length
    frame.data[0] = 0x01;
    frame.data[1] = 0x00;
    frame.data[2] = 0x00;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    // Send message
    int nbytes = write(s, &frame, sizeof(frame)); 
    cout << "\nDump command sent\n";
    if(nbytes != sizeof(frame)) {
        cout << "Send Error frame[0]!\r\n";
        return;
    }
    
    cout << "\nReceiving data...\n";
    for (int i = 0; i < 8191; i++) {
        nbytes = read(s, &frame, sizeof(frame));
        if(nbytes > 0) {
            //printf("can_id = 0x%X\r\ncan_dlc = %d \r\n", frame.can_id, frame.can_dlc);
            //fprintf(fptr, "test data\n");
            for(int j = 0; j < 8; j++) {
                fprintf(fptr, "%2X ", frame.data[j]);
            }
            fprintf(fptr, "\n");
        }
    }
    fclose(fptr);

    cout << "\nData saved in " << file_path << "\n";
    return;
}
