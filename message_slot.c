//
// Created by Omer Militscher on 19/11/2021.
//

#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/init.h>     /* included for __init and __exit macros */
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/slab.h>
#include "message_slot.h"

MODULE_LICENSE("GPL");


typedef struct channel {
    unsigned long id;
    int msg_size;
    char msg[BUF_LEN];
    struct channel* next;
} channel;

typedef struct msg_slot {
    unsigned long active_channel;
    unsigned int minor_num;
    channel* channels_head;
} msg_slot;


static msg_slot* all_slots[256];
static int slots_count = 0;

//================== HELPER FUNCTIONS ===========================

/* create new message slot with given minor number */
int newSlot(struct file* file, unsigned int minor) {
    msg_slot* new_slot;

    if (all_slots[minor] == NULL) {
        if(!(new_slot = (msg_slot *) kmalloc(sizeof(msg_slot), GFP_KERNEL))) {
            return -ENOMEM;                     /* kmalloc failed */
        }
        all_slots[minor] = new_slot;            /* assign new slot to all slots array */
        new_slot->channels_head = NULL;         /* initiate pointer to channel */
        new_slot->minor_num = minor;            /* set slot minor number */
        new_slot->active_channel = 0;           /* set no channel as active channel of slot */
        slots_count += 1;                       /* increase count of slots */
        file->private_data = (void*) new_slot;  /* associate the new slot with the file descriptor it was invoked on */
    }
    return SUCCESS;
}

/* iterate channels' linked list of a given slot to find if given channel id exists */
channel* findChannel(struct msg_slot* slot, unsigned long channel_id) {
    channel* channel = slot->channels_head;
    while (channel) {
        if (channel->id == channel_id) {
            return channel; /* channel exits, return channel */
        }
        channel = channel->next;
    }
    return NULL;
}

/* create new channel and insert it as new head of channels' linked list in message slot */
int newChannel(struct msg_slot* slot, unsigned long channel_id) {
    channel* new_channel;

    if ((findChannel(slot, channel_id)) == NULL) {  /* if channel does not exist */

        if(!(new_channel = (channel*) kmalloc(sizeof(channel), GFP_KERNEL))) {
            return -ENOMEM;                         /* kmalloc failed */
        }
        new_channel->id = channel_id;               /* set new channel's id */
        new_channel->msg_size = 0;                  /* initiate new channel's size */
        new_channel->msg = NULL;
        new_channel->next = slot->channels_head;    /* assign current head as new channel's next */
        slot->channels_head = new_channel;          /* assign new channel as slot's new head */
        slot->active_channel = new_channel->id;     /* set slot's active channel to new channel */
    }
    return SUCCESS;
}

void freeSlotChannels(struct msg_slot* slot) {
    channel* next;
    channel* head = slot->channels_head;

    while (head) {
        next = head->next;  /* set next channel in linked list */
        kfree(head);        /* free current channel */
        head = next;        /* move head of linked list to next channel */
    }
}

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode* inode, struct file* file) {

    unsigned int minor_num = iminor(inode);         /* get file's minor number */
    int new_slot = newSlot(file, minor_num);        /* create new message slot */
    printk("Invoking device_open(%p)\n", file);
    return new_slot;
}

//---------------------------------------------------------------
static int device_release(struct inode* inode, struct file* file) {
    printk("Invoking device_release(%p,%p)\n", inode, file);
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from it
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
    channel* channel;
    msg_slot* slot = (msg_slot *) file->private_data;

    /* check if call is invalid (buffer or slot non-exist, no channel has been set on file descriptor) */
    if (buffer == NULL || slot == NULL) { return -EINVAL; }
    if (slot->active_channel == 0) { return -EINVAL; }

    /* reach slot's active channel by its id, and set pointer to message in channel */
    channel = findChannel(slot->active_channel);

    /* check if message not exits or longer than buffer */
    if (channel->msg_size == 0) { return -EWOULDBLOCK; }
    if (channel->msg_size > length) { return -ENOSPC; }

    /* perform read */
    printk("Invoking device_read(%p,%ld), file, length");
    if (put_user(channel->msg, buffer) != 0) { return -EFAULT; }

    return (channel->msg_size);
}

//---------------------------------------------------------------
// a process which has already opened the device file attempts to write to it
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
    int i;
    channel* channel;
    msg_slot* slot = (msg_slot *)file->private_data;

    /* check if call is invalid (buffer or slot non-exist, no channel has been set on file descriptor,
     * message non-exist or too long) */
    if (buffer == NULL || slot == NULL) { return -EINVAL; }
    if (slot->active_channel == 0) { return -EINVAL; }
    if (length == 0 || length > BUF_LEN) { return -EMSGSIZE; }

    channel = findChannel(slot->active_channel);

    /* perform write */
    printk("Invoking device_write(%p,%ld)\n", file, length);
    for(i = 0; i < length && i < BUF_LEN; ++i) {
        if (get_user(channel->msg[i], &buffer[i]) != 0) { return -EFAULT; }
    }

    /* update msg size according to result of writing and set channel's msg pointer back to beginning of msg */
    channel->msg -= i;
    if (i != length) {
        channel->msg_size = 0;
        return -EFAULT;     /* failed to write the whole msg, set msg size to 0 and return error */
    }

    (channel->msg_size) = i;

    /* return the number of input characters used */
    return i;
}

//----------------------------------------------------------------
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
    int channel;
    msg_slot* slot;

    /* check if ioctl call is valid */
    if (ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0) {
        return -EINVAL;
    }

    /* create new channel or find an existing one with given channel id */
    slot = (msg_slot*) file->private_data
    channel = newChannel(slot, ioctl_param);

    if (channel != -1) {
        /* set given channel's id according to ioctl parameter */
        (file->private_data)->active_channel = (void*) ioctl_param;
    }

    /* return -1 if tried to set new channel and failed, 0 otherwise */
    return channel;
}

//==================== DEVICE SETUP =============================

// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops = {
    .owner	  = THIS_MODULE,
    .read           = device_read,
    .write          = device_write,
    .open           = device_open,
    .unlocked_ioctl = device_ioctl,
    .release        = device_release,
        };

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void) {
    int rc = -1;

    // Register driver capabilities. Obtain major num
    rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);

    // Negative values signify an error
    if(rc < 0) {
        printk(KERN_ERR "%s registration failed for  %d\n", DEVICE_FILE_NAME, MAJOR_NUM);
        return rc;
    }

    return SUCCESS;
}

//---------------------------------------------------------------
static void __exit simple_cleanup(void) {
    // Unregister the device
    // Should always succeed
    int i;

    for (i = 0; i < slots_count; i++) {
        if (all_slots[i]) {
            freeSlotChannels(all_slots[i]); /* free all channels in slot */
            kfree(all_slots[i]);            /* free slot itself */
        }
    }

    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//=================== END OF FILE ========================
