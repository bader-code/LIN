#include "codetimer.h"


static ssize_t codeconfig_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    char *kbuf = (char*)vmalloc(BUFFER_LENGTH), *ptr_kbuf;
    int retval = 0;

    if(len < BUFFER_LENGTH - 1){retval = -ENOSPC; goto out_error;}
    if ((*off) > 0){retval = 0; goto out_error;}

    ptr_kbuf = kbuf;

    ptr_kbuf += sprintf(ptr_kbuf,"timer_period_ms=%d\n",timer_period_ms);
    ptr_kbuf += sprintf(ptr_kbuf,"emergency_threshold=%d\n",emergency_threshold);
    ptr_kbuf += sprintf(ptr_kbuf,"code_format=%s\n",code_format);
  
    if (copy_to_user(buf, kbuf,(ptr_kbuf - kbuf))){retval = -ENOSPC; goto out_error;}


    vfree(kbuf);
    (*off)+=len;
	return len;

out_error:

	vfree(kbuf);
    return retval;


}
static ssize_t codeconfig_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

    char *kbuf = (char*)vmalloc(BUFFER_LENGTH);
    char *pre_code_format = (char*)vmalloc(strlen(code_format));
    strcpy(pre_code_format,code_format);

    int retval = 0, i = 0, correct_formar = TRUE;
    
    if(len > (BUFFER_LENGTH - 1)) {printk(KERN_INFO "codeconfig: not enough space!!\n");retval = -ENOSPC; goto out_error;}
     
	if(copy_from_user(kbuf, buf, len)){retval = -ENOSPC; goto out_error;}
     
    kbuf[len] = '\0';

    if(strlen(kbuf) <= 2){retval = -EINVAL; goto out_error;}

    if(sscanf(kbuf,"timer_period_ms %d",&timer_period_ms) != 1){
        if(sscanf(kbuf,"code_format %s",code_format) != 1){
            if(sscanf(kbuf,"emergency_threshold %d",&emergency_threshold) != 1){
                retval = -EINVAL; goto out_error;
            }
        }
        else{
            if(MAX_CHARACTERS < (strlen(code_format))){
                strcpy(code_format,pre_code_format);
                retval = -EINVAL; goto out_error;
            }
            while(i < strlen(code_format) && correct_formar){
                if((unsigned int)code_format[i] != (unsigned int)'A'){
                    if((unsigned int)code_format[i] != (unsigned int)'a'){
                        if((unsigned int)code_format[i] != (unsigned int)'0'){
                            strcpy(code_format,pre_code_format);
                            retval = -EINVAL; goto out_error;
                        }
                    }
                }
                i++;
            }
            if(down_interruptible(&sp_list)){return -EINTR;}
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
            up(&sp_list);
        }
    }

    vfree(pre_code_format);
    vfree(kbuf);
    (*off)+=len;
	return len;

out_error:

    vfree(pre_code_format);
	vfree(kbuf);
    return retval;


}




static ssize_t codetimer_read   (struct file *filp, char __user *buf, size_t len, loff_t *off){

    struct list_item *item = NULL;
	struct list_head *cur_node = NULL;
    struct list_head *aux = NULL;

    char *kbuf = (char*)vmalloc(BUFFER_LENGTH), *kbuf_aux = (char*)vmalloc(BUFFER_LENGTH);
    char *ptr_kbuf = kbuf, *ptr_kbuf_aux = kbuf_aux;

    int nr_bytes = 0;


    if(down_interruptible(&sp_list)){return -EINTR;}

    while(list_empty(&mylist)){
        flags_wait_list++;

        up(&sp_list);
        if(down_interruptible(&sp_empty)){
            down(&sp_list);
			flags_wait_list--;
			up(&sp_list);
            return -EINTR;
        }
        if(down_interruptible(&sp_list)){
            flags_wait_list--;
            return -EINTR;
        }
    }


    list_for_each_safe(cur_node, aux, &mylist) {  
        item = list_entry(cur_node, struct list_item, links);
        ptr_kbuf_aux += sprintf(ptr_kbuf_aux, "%s\n", item->data);
        if((((ptr_kbuf_aux - kbuf_aux) + ptr_kbuf) - kbuf)  < BUFFER_LENGTH){
            ptr_kbuf += sprintf(ptr_kbuf, "%s\n", item->data);  
            ptr_kbuf_aux = kbuf_aux;  
            list_del(cur_node);                             
        }
        else{
           return -ENOSPC;
        }
    }
    up(&sp_list);

    nr_bytes = ptr_kbuf - kbuf;
    if (len < nr_bytes){return -ENOSPC;}
  
    if (copy_to_user(buf, kbuf,nr_bytes)){return -EINVAL;}
    
    (*off) += nr_bytes; 
    return nr_bytes; 


}


static int     codetimer_open   (struct inode *inode, struct file *file){

    if(flags_processes >= MAX_PROCESS){return -EINTR;}
    flags_processes++;
    try_module_get(THIS_MODULE);

     
    mytimer.expires = jiffies + ((timer_period_ms*HZ)/1000);        /**temporizador expira en 'delay' ticks**/
    add_timer(&mytimer);

    return 0;
}
static int     codetimer_release(struct inode *inode, struct file *file) {

	del_timer_sync(&mytimer);
	flush_workqueue(queue_works);
	kfifo_reset(&cbuffer);	
    flags_processes--;
    module_put(THIS_MODULE);

    if(down_interruptible(&sp_list)){return -EINTR;}
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
    up(&sp_list);


	return 0;

}



static void work_handler( struct work_struct *work ){


    struct list_item *item = NULL;
	
    char cbuffer_aux[BUFFER_KFIFO_LENGTH*sizeof(char)];
    char *ptr_cbuffer_aux = cbuffer_aux;

    unsigned int i_max = kfifo_len(&cbuffer);

    printk(KERN_INFO "%d codes moved from the buffer to the list\n",(int)((kfifo_len(&cbuffer))/(strlen(code_format))));

    spin_lock_irqsave(&sp_cbuffer, flags_irq);
    kfifo_out(&cbuffer, ptr_cbuffer_aux, (kfifo_len(&cbuffer)));
    spin_unlock_irqrestore(&sp_cbuffer, flags_irq);

    ptr_cbuffer_aux = cbuffer_aux;

    down(&sp_list);
    while((ptr_cbuffer_aux - cbuffer_aux) < i_max){
        item = (struct list_item*)vmalloc(sizeof(struct list_item)); 
        ptr_cbuffer_aux += sprintf(item->data,"%s",ptr_cbuffer_aux) + 1;
        list_add_tail(&item->links, &mylist);
    }
    up(&sp_list);
    if(flags_wait_list > 0){
		flags_wait_list--;
		up(&sp_empty);
	}

    flags_work_empty = TRUE;
    
}
static void timer_handler(unsigned long data) {

    char code[strlen(code_format) + 1];
    unsigned int i = 0, cpu = 0, interval = 0;
    unsigned int rand = (1000000000 + get_random_int()) % 99999999;

   while(i < (strlen(code_format))){
        if(((unsigned int)code_format[i]) == ((unsigned int)'0')){
            index_rand = (index_rand + (rand % 10)) % strlen(numbers);
            code[i] = numbers[index_rand];
        }
        else{
            if(((unsigned int)code_format[i]) == ((unsigned int)'A')){
                index_rand = (index_rand + (rand % 10)) % strlen(uppercase);
                code[i] = uppercase[index_rand];
            }
            else{
                index_rand = (index_rand + (rand % 10)) % strlen(lowercase);
                code[i] = lowercase[index_rand];
            }
        }
        rand = rand / 10;
        i++;
    }

    code[i] = '\0';

    printk(KERN_INFO "Generated code: %s\n",code);

    spin_lock_irqsave(&sp_cbuffer, flags_irq);
    kfifo_in(&cbuffer, code, (strlen(code_format) + 1));
    if(((kfifo_len(&cbuffer) * 100) / kfifo_size(&cbuffer)) >= emergency_threshold && flags_work_empty){

        cpu = smp_processor_id();
        cpu = (cpu == 0)? 1 : 0;
        queue_work_on(cpu,queue_works, &work);
        flags_work_empty = FALSE;
        
    }
    spin_unlock_irqrestore(&sp_cbuffer, flags_irq);

    
    mod_timer(&(mytimer), jiffies + ((timer_period_ms*HZ)/1000)); 


}



int module_code_init(void){

    int retval = 0;

    if((proc_config_entry = proc_create("codeconfig", 0666, NULL, &codeconfig_fops)) == NULL){
        retval =  -ENOMEM;
        printk(KERN_INFO "codeconfig -> proc_config_create() operation failed\n");
        goto out_error;
    }
    if((proc_timer_entry = proc_create("codetimer", 0666, NULL, &codetimer_fops)) == NULL){
       retval = -ENOMEM;
       printk(KERN_INFO "codetimer -> proc_timer_create() operation failed\n");
       goto out_error;
    }
    if((queue_works = create_workqueue("private_workqueue")) == NULL){
        retval = -ENOMEM;
        printk(KERN_INFO "queue_works -> create_workqueue('private_workqueue') operation failed\n");
        goto out_error;
    }
    if((retval = kfifo_alloc(&cbuffer, BUFFER_KFIFO_LENGTH*sizeof(char), GFP_KERNEL)) != 0){
        printk(KERN_INFO "cbuffer -> kfifo_alloc() operation failed\n");
        goto out_error;
    }

    INIT_WORK(&work, work_handler);
    INIT_LIST_HEAD(&mylist);

    
	sema_init(&sp_list, 1);
	sema_init(&sp_empty,0);

    flags_processes = 0;
    index_rand = 0;
    flags_irq = 0;
    flags_work_empty = TRUE;

    init_timer(&mytimer);                                       /**Inicialización básica**/
    mytimer.data = 0;                                           /**Ej: 0 (no usado aquí)**/
    mytimer.function = timer_handler;                           /**Función a ejecutar cuando temporizador expire**/
     

    return retval;

out_error:

    remove_proc_entry("codeconfig", NULL);
    remove_proc_entry("codetimer", NULL);
    destroy_workqueue(queue_works);

   return retval;
}
void module_code_clean(void){

    del_timer_sync(&mytimer);
    remove_proc_entry("codeconfig", NULL);
    remove_proc_entry("codetimer", NULL);
    flush_workqueue(queue_works);
	destroy_workqueue(queue_works);

}

module_init(module_code_init);
module_exit(module_code_clean);