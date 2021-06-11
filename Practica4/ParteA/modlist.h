#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist -> Kernel Module -> LIN -> FDI-UCM");
MODULE_AUTHOR("Badr Guaitoune Akdi");

#define BUFFER_LENGTH 256
#define COMMAND_LENGTH 10

#define add 1
#define remove 2
#define cleanup 3
#define COMMAND_ERROR -1

typedef struct{ 
   char *command; 
   int value; 
   int parseValue;
}t_sysCommand;

static struct proc_dir_entry *proc_entry;  
static struct proc_dir_entry  *proc_dir_multilist;

static t_sysCommand table[] = {
    { "add", add , 2}, { "remove", remove , 2 }, { "cleanup", cleanup , 1 }
};
#define N_COMMANDS (sizeof(table)/sizeof(t_sysCommand))

struct list_item {
    void *data;
    struct list_head links;
};

typedef struct list_head mylist;

typedef struct modlist{
    void *mylist;
    struct spinlock_t sp;
    
};

