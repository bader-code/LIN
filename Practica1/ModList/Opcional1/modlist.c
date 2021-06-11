#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist -> Kernel Module -> LIN -> FDI-UCM");
MODULE_AUTHOR("Badr Guaitoune Akdi - Gonzalo Fernandez Megia");

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

static struct proc_dir_entry *proc_entry;//entrada a proc -> /proc

static t_sysCommand table[] = {
    { "add",add,2}, { "remove",remove,2}, { "cleanup",cleanup,1}
};

#define N_COMMANDS (sizeof(table)/sizeof(t_sysCommand))

struct list_item {
  #ifdef PARTE_OPCIONAL
	 char *data;
  #else
   int data;
  #endif
	 struct list_head links;
};

struct list_head mylist;



int Command(char *command, int num){

    int i;
    for (i = 0; i < N_COMMANDS; i++) {
        t_sysCommand sys = table[i];
        if (strcmp(sys.command, command) == 0){
            if(sys.parseValue == num){
              return sys.value;
            }
            else{
              return COMMAND_ERROR;
            }
        }
    }
    return COMMAND_ERROR;


}


static ssize_t my_modlist_write(struct file *filp, const char __user *buf, size_t len, loff_t *off){
	   
     struct list_item *item = NULL;
	   struct list_head *cur_node = NULL;
     struct list_head *aux = NULL;
     
     char kbuf[BUFFER_LENGTH] = "", kcommand[COMMAND_LENGTH] = "";

     int n_values = 0;

     #ifdef PARTE_OPCIONAL 
	     char *num;
     #else
       int num = 0;
     #endif 
     
     if((*off) > 0){return 0;}// Sacado de Clipboard - Solo se puede escribir una vez hablar con el profesor.
     
     if(len > (BUFFER_LENGTH - 1)) {printk(KERN_INFO "modlist: not enough space!!\n");return -ENOSPC;}// Sacado de Clipboard maximo se puede escribir  el tamaño del buffer seguro - hablar con el profesor.
     
	   if(copy_from_user(kbuf, buf, len)){return -ENOSPC;}

     kbuf[len] = '\0';
     #ifdef PARTE_OPCIONAL 
	      n_values = sscanf(kbuf, "%s %s",kcommand,&num);
     #else
        n_values = sscanf(kbuf, "%s %d",kcommand,&num);
     #endif 
     
     switch (Command(kcommand,n_values)) {
       case add: item = (struct list_item*)vmalloc(sizeof(struct list_item)); 
         #ifdef PARTE_OPCIONAL
           num = kbuf + 4;//'add '; = 4
           item->data = (char*)vmalloc(sizeof(strlen(num) + 1));
           strcpy(item->data, num);
         #else
           item->data = num;
         #endif
           list_add_tail(&item->links, &mylist);break;
       case remove: item = NULL; cur_node = NULL; aux = NULL;
                          #ifdef PARTE_OPCIONAL
                           num = kbuf + 7;//'remove '; = 7
                          #endif
                          list_for_each_safe(cur_node, aux, &mylist){
		                         item = list_entry(cur_node, struct list_item, links);
                             #ifdef PARTE_OPCIONAL
                               if(strcmp(item->data, num) == 0){
				                         list_del(cur_node);
			                      	   vfree(item);
                               }
                             #else
			                         if(item->data == num){
				                         list_del(cur_node);
			                      	   vfree(item);
                               }
                             #endif
			                    }; break;
       case cleanup: item = NULL; cur_node = NULL; aux = NULL;
                          list_for_each_safe(cur_node, aux, &mylist){
			                        item = list_entry(cur_node, struct list_item, links);
			                        list_del(cur_node);
			                        vfree(item);
                           }; break;
       case COMMAND_ERROR:return -EPERM;break;
     }

      (*off) += len;  /* Update the file pointer */

      return len; 


}



static ssize_t my_modlist_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

     struct list_item *item = NULL; 
     struct list_head *cur_node = NULL; 
  
     char kbuf[BUFFER_LENGTH] = "", kbuf_aux[BUFFER_LENGTH] = "";
     char *ptr_list = kbuf, *ptr_aux = kbuf_aux;

     int nr_bytes = 0;
 
     if ((*off) > 0){return 0;} /* Tell the application that there is nothing left to read */

     /*#define list_for_each(pos, head) 
           for(pos = (head)->next; pos != (head); pos = pos->next)*/
     list_for_each(cur_node, &mylist) {  
       item = list_entry(cur_node, struct list_item, links);
       #ifdef PARTE_OPCIONAL
         ptr_aux += sprintf(ptr_aux, "%s", item->data);
         if((((ptr_aux - kbuf_aux) + ptr_list) - kbuf) < BUFFER_LENGTH){
             ptr_list += sprintf(ptr_list, "%s", item->data);
             ptr_aux = kbuf_aux;                               
         }
         else{
           return -ENOSPC;
         }	
       #else
         ptr_aux += sprintf(ptr_aux, "%d\n", item->data);
         if((((ptr_aux - kbuf_aux) + ptr_list) - kbuf)  < BUFFER_LENGTH){
             ptr_list += sprintf(ptr_list, "%d\n", item->data);  
             ptr_aux = kbuf_aux;                               
         }
         else{
           return -ENOSPC;
         }
       #endif
     }

     nr_bytes = ptr_list - kbuf;
    
     if (len < nr_bytes){return -ENOSPC;}
  
     if (copy_to_user(buf, kbuf,nr_bytes)){return -EINVAL;}
    
     (*off) += nr_bytes;  /* Update the file pointer */

     return nr_bytes; 

  
}




struct file_operations fops = {

   .owner = THIS_MODULE,//owner
	 .read = my_modlist_read, //read()
	 .write = my_modlist_write, //write()


};

int module_modlist_init(void){

  //Crea la lista enlazada
  INIT_LIST_HEAD(&mylist); //INIT_LIST_HEAD(plist1)

  // struct proc_dir_entry *proc_create(const char *name,umode_t mode,struct proc_dir_entry *parent,const struct file_operations *ops);
  if((proc_entry = proc_create("modlist", 0666, NULL, &fops)) == NULL){
    printk(KERN_INFO "modlist -> proc_create() creation failed\n");
    return -ENOMEM;
  }

  return 0;


}

void module_modlist_clean(void){

	remove_proc_entry("modlist", NULL);
	if(!list_empty(&mylist)){

		struct list_item *item = NULL;
		struct list_head *cur_node = NULL;
		struct list_head *aux = NULL;

		list_for_each_safe(cur_node, aux, &mylist){
			item = list_entry(cur_node, struct list_item, links);
			list_del(cur_node);
			vfree(item);
		}	
  }


}

module_init(module_modlist_init);
module_exit(module_modlist_clean);