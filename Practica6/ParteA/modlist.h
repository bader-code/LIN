#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/errno.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist -> Kernel Module -> LIN -> FDI-UCM");
MODULE_AUTHOR("Badr Guaitoune Akdi");

#define BUFFER_LENGTH 256                       /**DEFINE**/
#define COMMAND_LENGTH 10                       /**DEFINE**/


#define add 1
#define remove 2
#define cleanup 3

#define new 4                                   /**DEFINE**/
#define delete 5                                /**DEFINE**/

#define COMMAND_ERROR -1                        /**DEFINE**/

#define TRUE  1
#define FALSE 0

typedef struct{                                            /****/
   char *command; 
   int value; 
   int parseValue;
}t_sysCommand;

static t_sysCommand table[] = {                            /****/
    { "new", new , 3}, { "delete", delete , 2 }, { "add", add , 2}, { "remove", remove , 2 }, { "cleanup", cleanup , 1 }
};

#define N_COMMANDS (sizeof(table)/sizeof(t_sysCommand))    /**DEFINE**/


static  int max_entries = 7;    /***/
static  int max_size = 10;      /***/

unsigned int FLAG_REMOVE_PROC = FALSE;

module_param(max_entries, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);  /****/
MODULE_PARM_DESC(max_entries, "An integer");                            /****/
module_param(max_size, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);     /****/
MODULE_PARM_DESC(max_size, "An integer");                               /****/

static struct proc_dir_entry  *proc_entry;           /****/
static struct proc_dir_entry  *proc_dir_multilist;   /****/

struct list_item {
    void *data;
	struct list_head links;
};                                      /**Nodos de la lista**/

typedef enum {e_int, e_string, e_modlist, e_error} t_type;  /**Enumerado de tipos de datos que se pueden guardar en cada modlist**/

typedef struct{
    
    struct list_head mylist;            /**Lista enlazada**/
    unsigned int length;                /**Longitud de la Lista**/
    t_type type;                        /**Tipo de datos en la Lista**/
    struct semaphore sem;               /**                         **/
    char* name;                         /**Nombre del modlist**/

}t_modlist;                             /**Estructura de datos privada para cada modlist creada(test..)**/

typedef struct{
    struct list_head procS;             /**Lista enlazada**/
    struct semaphore sem;               /**                         **/
    unsigned int length;                /**Longitud de la Lista**/
}t_FIFO;


//struct work_struct work;                     /**Tarea (struct work_struct) que se encolará en una workqueue privada del módulo del kernel**/

//t_FIFO FIFO_removeproc;                 /**FIFO para encolar la eliminacion de /proc's**/
t_modlist adminlist;                    /**Estructura de datos  para llevar la cuenta de las entradas /proc y estructuras privadas creadas dinámicamente**/

static ssize_t admin_write(struct file *filp, const char __user *buf, size_t len, loff_t *off);     /****/
static ssize_t admin_read(struct file *filp, char __user *buf, size_t len, loff_t *off);             /***/


static ssize_t list_read(struct file *filp, char __user *buf, size_t len, loff_t *off);             /****/
static ssize_t list_write(struct file *filp, const char __user *buf, size_t len, loff_t *off);      /****/



static int proc_entry_exist(char *kname);                           /****/
static int create_list(char *kname, char type, t_modlist *list);    /****/
static void delete_list(char *kname);                               /****/


struct file_operations fops_admin = {

    .owner = THIS_MODULE, //owner
    .read  = admin_read, //write()
	.write = admin_write, //write()

};
struct file_operations fops_multilist = {

    .owner = THIS_MODULE,//owner
	.read =  list_read,  //read()
	.write = list_write, //write()


};


