//
// Created by Omer Militscher on 19/11/2021.
//

#include "message_slot.h"

#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
    int fd;
    char buffer[BUF_LEN];
    unsigned long channel_id, msg_size;
    /* Validate that the correct number of command line arguments is passed */
    if (argc != 3) {
        fprintf(stderr, "You should pass exactly 2 arguments");
        exit(1);
    }

    /* Open the specified message slot device file */
    if ((fd = open(argv[1], O_RDONLY)) == -1) {
        fprintf(stderr, "failed opening file");
        exit(1);
    }

    /* Set the channel id to the id specified on the command line */
    channel_id = atoi(argv[2]);
    if (ioctl(fd, MSG_SLOT_CHANNEL, channel_id) == -1) {
        fprintf(stderr, "failed setting channel ID");
        exit(1);
    }

    /* Read a message from the message slot file to a buffer */
    if ((msg_size = read(fd, buffer, BUF_LEN)) == -1) {
        fprintf(stderr, "failed reading");
        exit(1);
    }

    /* Close the device */
    if (close(fd) == -1) {
        fprintf(stderr, "failed closing file");
        exit(1);
    }

    /* Print the message to standard output */
    if (write(STDOUT_FILENO, buffer, msg_size) == -1) {
        fprintf(stderr, "failed printing the message");
    }

    /* Exit the program with exit value 0 */
    exit(0);
}
