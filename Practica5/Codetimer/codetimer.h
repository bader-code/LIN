#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/semaphore.h>
#include <linux/proc_fs.h>
#include <linux/kfifo.h>
#include <linux/errno.h>


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("codetimer - codeconfig -> Kernel Module -> LIN -> FDI-UCM");
MODULE_AUTHOR("Badr Guaitoune Akdi - Gonzalo Fernandez Megia");

#define MAX_CHARACTERS 8                        /**DEFINE**/
#define BUFFER_LENGTH 256                       /**DEFINE**/
#define BUFFER_KFIFO_LENGTH 32                  /**DEFINE**/
#define TRUE 1                                  /**DEFINE**/
#define FALSE 0                                 /**DEFINE**/
#define MAX_PROCESS 1                           /**DEFINE**/

int  timer_period_ms = 500;                           /**Temporizador se activa cada timer_period_ms milisegundos**/
char code_format[MAX_CHARACTERS + 1] = "0000AAAA\0";  /**Cadena de hasta MAX_CHARACTERS caracteres formadas por combinaciones de {0, a, A}:**/
int  emergency_threshold = 50;                        /**Parámetro emergency_threshold indica el porcentaje de ocupación que provoca la activación de dicha tarea**/

unsigned int flags_irq;                         /**FLAG para emplear spin_lock_irqsave() y spin_unlock_irqrestore()**/
unsigned int flags_work_empty;                  /**FLAG para marcar si se ha completado la ejecución de la última tarea planificada**/
unsigned int flags_processes;                   /**FLAG contador que registra número de procesos**/
unsigned int flags_wait_list;                   /**FLAG contador que registra número de procesos esperando (0 ó 1)(Para emular cada variable condición)**/
unsigned int index_rand;                        /**FLAG index aleatorio para la creacion de los codigos**/

static struct proc_dir_entry *proc_config_entry; /**entrada a proc -> /proc/codeconfig**/
static struct proc_dir_entry *proc_timer_entry;  /**entrada a proc -> /proc/codetimer**/


static struct workqueue_struct *queue_works; /**Workqueue privada del driver (creadas explícitamente)**/
struct work_struct work;                     /**Tarea (struct work_struct) que se encolará en una workqueue privada del módulo del kernel**/

struct kfifo cbuffer;          /**Buffer circular**/

DEFINE_SPINLOCK(sp_cbuffer);   /**1 - El buffer debe protegerse mediante un spin lock**/
DEFINE_SEMAPHORE(sp_list);     /**2 - La lista enlazada puede protegerse usando spin lock o semáforo -> Inicializado a 1 para emular el mutex**/
DEFINE_SEMAPHORE(sp_empty);    /**3 - Se hará uso de un semáforo (cola de espera) para bloquear al programa de usuario mientras la lista esté vacía -> Inicializado a 0 (cola de espera)**/

static char lowercase[] = "abcdefghijklmnopqrstuvwxyz";
static char uppercase[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static char numbers[]   = "0123456789";

struct list_item {
    char data[MAX_CHARACTERS + 1];
	struct list_head links;
};                            /**Nodos de la lista**/
struct list_head mylist;      /**Lista enlazada**/

struct timer_list mytimer;    /**Cada temporizador se describe mediante estructura timer_list**/

static ssize_t codeconfig_read (struct file *filp, char __user *buf, size_t len, loff_t *off);          /**Permite consultar valor de parámetros de configuración timer_period_ms, emergency_threshold y code_format**/
static ssize_t codeconfig_write(struct file *filp, const char __user *buf, size_t len, loff_t *off);    /**Permite cambiar valor de parámetros de configuración timer_period_ms, emergency_threshold y code_format**/


static ssize_t codetimer_read   (struct file *filp, char __user *buf, size_t len, loff_t *off);         /**Permite (leer de esta entrada (cat)) consumir elementos de la lista enlazada de códigos**/
static int     codetimer_open   (struct inode *inode, struct file *file);                               /**Abre /proc/codetimer   ->Restricciones: Incrementar CR ⇒ try_module_get(THIS_MODULE);**/
static int     codetimer_release(struct inode *inode, struct file *file);                               /**Cierra /proc/codetimer ->Restricciones: Decrementar CR ⇒ module_put(THIS_MODULE);**/

static void work_handler( struct work_struct *work );
static void timer_handler(unsigned long data);

static struct file_operations codeconfig_fops = {

    .owner = THIS_MODULE,
	.read  = codeconfig_read,  //read();
    .write = codeconfig_write, //write();


};

static struct file_operations codetimer_fops = {

    .owner   = THIS_MODULE,
	.read    = codetimer_read,    //read();
    .open    = codetimer_open,    //oepn();
    .release = codetimer_release, //release();


};