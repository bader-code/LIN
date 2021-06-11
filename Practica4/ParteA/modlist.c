#include "modlist.h"

int Command(char *command, int n_values){

    int i;
    for (i = 0; i < N_COMMANDS; i++) {
        t_sysCommand sys = table[i];
        if (strcmp(sys.command, command) == 0){
            if(sys.parseValue == n_values){
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

     int num = 0,n_values = 0;

     if((*off) > 0){return 0;}// Sacado de Clipboard - Solo se puede escribir una vez hablar con el profesor.
     
     if(len > (BUFFER_LENGTH - 1)) {printk(KERN_INFO "modlist: not enough space!!\n");return -ENOSPC;}// Sacado de Clipboard maximo se puede escribir  el tamaño del buffer seguro - hablar con el profesor.
     
	   if(copy_from_user(kbuf, buf, len)){return -ENOSPC;}

     n_values = sscanf(kbuf, "%s %d",kcommand,&num);

     switch (Command(kcommand,n_values)) {
       case add: 
         item = (struct list_item*)vmalloc(sizeof(struct list_item)); 
         spin_lock(&sp);
         item->data = num;  
         list_add_tail(&item->links, &mylist);
         spin_unlock(&sp);
         break;
       case remove: item = 
       	 NULL; cur_node = NULL; aux = NULL;
       	 spin_lock(&sp);
         list_for_each_safe(cur_node, aux, &mylist){
         item = list_entry(cur_node, struct list_item, links);
            if(item->data == num){
                 list_del(cur_node);
          	   vfree(item);
     		}
	     } 
	     spin_unlock(&sp);
	     break;
       case cleanup: 
       	item = NULL; cur_node = NULL; aux = NULL;
       		spin_lock(&sp);
            list_for_each_safe(cur_node, aux, &mylist){
                item = list_entry(cur_node, struct list_item, links);
                list_del(cur_node);
                vfree(item);
            }
            spin_unlock(&sp);
        break;
       case COMMAND_ERROR: return -EPERM;break;
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
     spin_lock(&sp);
     list_for_each(cur_node, &mylist) {  
       item = list_entry(cur_node, struct list_item, links);
         ptr_aux += sprintf(ptr_aux, "%d\n", item->data);
         if((((ptr_aux - kbuf_aux) + ptr_list) - kbuf)  < BUFFER_LENGTH){
             ptr_list += sprintf(ptr_list, "%d\n", item->data);  
             ptr_aux = kbuf_aux;                               
         }
         else{
           return -ENOSPC;
         }
     }
     spin_unlock(&sp);

     nr_bytes = ptr_list - kbuf;
    
     if (len < nr_bytes){return -ENOSPC;}
  
     if (copy_to_user(buf, kbuf,nr_bytes)){return -EINVAL;}
    
     (*off) += nr_bytes;  /* Update the file pointer */

     return nr_bytes; 

  
}




struct file_operations fops_multilist = {

   .owner = THIS_MODULE,//owner
	 .read = my_modlist_read, //read()
	 .write = my_modlist_write, //write()


};
struct file_operations fops_admin = {

   .owner = THIS_MODULE,//owner
	 .write = my_modlist_write, //write()


};

int module_modlist_init(void){



  
  //Crea la lista enlazada
  INIT_LIST_HEAD(&mylist); 

  /* Create proc directory */
  if(!(proc_dir_multilist = proc_mkdir("multilist",NULL))){
    printk(KERN_INFO "modlist -> proc_mkdir() creation failed\n");
    return -ENOMEM;
  }
  // struct proc_dir_entry *proc_create(const char *name,umode_t mode,struct proc_dir_entry *parent,const struct file_operations *ops);
  if((proc_entry = proc_create("test", 0666, proc_dir_multilist, &fops_multilist)) == NULL){
    printk(KERN_INFO "test -> proc_create() creation failed\n");
    return -ENOMEM;
  }
  // struct proc_dir_entry *proc_create(const char *name,umode_t mode,struct proc_dir_entry *parent,const struct file_operations *ops);
  if((proc_entry = proc_create("admin", 0666, proc_dir_multilist, &fops_admin)) == NULL){
    printk(KERN_INFO "test -> proc_create() creation failed\n");
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