//
// Created by Omer Militscher on 19/11/2021.
//

#include "message_slot.h"

#include <fcntl.h>        /* open */
#include <unistd.h>       /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){
    int fd;
    unsigned long channel_id, msg_size;
    /* validate that the correct number of command line arguments is passed */
    if (argc != 4) {
        fprintf(stderr, "You should pass exactly 3 arguments");
        exit(1);
    }

    /* open the specified message slot device file */
    if ((fd = open(argv[1], O_WRONLY)) == -1) {
        fprintf(stderr, "failed opening file");
        exit(1);
    }

    /* Set the channel id to the id specified on the command line */
    channel_id = atoi(argv[2]);
    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1) {
        fprintf(stderr, "failed setting channel ID");
        exit(1);
    }

    /* Write the specified message to the message slot file */
    msg_size = strlen(argv[3]);
    if (write(fd, argv[3], msg_size) != msg_size) {
        fprintf(stderr, "failed writing");
        exit(1);
    }

    /* Close the device */
    if (close(fd) == -1) {
        fprintf(stderr, "failed closing file");
        exit(1);
    }

    /* Exit the program with exit value 0 */
    exit(0);
}
