//
// Created by Omer Militscher on 19/11/2021.
//

#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <sys/ioctl.h>

#define MAJOR_NUM 240
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define DEVICE_RANGE_NAME "msg_slot"
#define BUF_LEN 128
#define DEVICE_FILE_NAME "simple_msg_slot"
#define SUCCESS 0

#endif
