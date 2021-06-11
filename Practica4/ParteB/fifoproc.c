#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>		
#include <linux/semaphore.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Modlist -> Kernel Module -> LIN -> FDI-UCM");
MODULE_AUTHOR("Badr Guaitoune Akdi - Gonzalo Fernandez Megia");

#define MAX_CBUFFER_LEN 256
#define MAX_KBUF 256

static struct proc_dir_entry *proc_entry;//entrada a proc -> /proc

struct kfifo cbuffer; /* Buffer circular */
int prod_count = 0; /* Número de procesos que abrieron la entrada /proc para
escritura (productores) */
int cons_count = 0; /* Número de procesos que abrieron la entrada /proc para
lectura (consumidores) */
struct semaphore mtx; /* para garantizar Exclusión Mutua */
struct semaphore sem_prod; /* cola de espera para productor(es) */
struct semaphore sem_cons; /* cola de espera para consumidor(es) */
int nr_prod_waiting=0; /* Número de procesos productores esperando */
int nr_cons_waiting=0; /* Número de procesos consumidores esperando */




static ssize_t fifoproc_write(struct file *, const char __user *buf, size_t len, loff_t * off) {
	char kbuffer[MAX_KBUF];

	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) { return Error;}
	if (copy_from_user(kbuffer,buff,len)) { return Error;}
	
	if(down_interruptible(&mtx))
		return -EINTR;
	/* Esperar hasta que haya hueco para insertar (debe haber consumidores) */
	while (kfifo_avail(&cbuffer)<len && cons_count>0){
		nr_prod_waiting++;
		up(&mtx);//Libera el mutex
		//Se bloquea en la cola
		if(down_interruptible(&sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}
		//Adquiere el mutex
		if(down_interruptible(&mtx))
			return -EINTR;
			//nr_prod_waiting--
	}
	/* Detectar fin de comunicación por error (consumidor cierra FIFO antes) */
	if (cons_count == 0) {up(&mtx); return -EPIPE;}
	kfifo_in(&cbuffer,kbuffer,len);
	/* Despertar a posible consumidor bloqueado */

	if(nr_cons_waiting > 0){
		up(&sem_cons);
		nr_cons_waiting--;
	}
	up(&mtx);
	return len;
}


static int fifoproc_open(struct inode *, struct file *){
	//lock(mtx);
	if(down_interruptible(&mtx))
		return -EINTR;

	if(file->f_mode & FMODE_READ){//Abre para lectura(consumidor)
		cons_count++;
		//cond_signal(prod,mtx);
		if(nr_prod_waiting > 0){
			up(&sem_prod);
			nr_prod_waiting--;
		}
		
		while(prod_count < 1){
			nr_cons_waiting++;
			up(&mtx);//Libera el mutex
			//Se bloquea en la cola
			if(down_interruptible(&sem_cons)){
				down(&mtx);
				nr_cons_waiting--;
				up(&mtx);
				return -EINTR;
			}
			//Adquiere el mutex
			if(down_interruptible(&mtx))
				return -EINTR;
				//nr_prod_waiting--
		}
		
	}
	else{//Abre para escritura(productor)
		prod_count++;
		//cond_signal(prod,mtx);
		if(nr_cons_waiting > 0){
			up(&sem_cons);
			nr_cons_waiting--;
		}
		
		while(cons_count < 1){
			nr_prod_waiting++;
			up(&mtx);//Libera el mutex
			//Se bloquea en la cola
			if(down_interruptible(&sem_prod)){
				down(&mtx);
				nr_prod_waiting--;
				up(&mtx);
				return -EINTR;
			}
			//Adquiere el mutex
			if(down_interruptible(&mtx))
				return -EINTR;
				//nr_prod_waiting--
		}
	}
	
	//unlock(mtx);
	up(&mtx);
	return 0;
}

int fifoproc_read(struct file *, char __user *buf, size_t len, loff_t * off){
	char kbuffer[MAX_KBUF];
	if (len> MAX_CBUFFER_LEN || len> MAX_KBUF) { return Error;}
	if (copy_from_user(kbuffer,buf,len)) { return Error;}
	
	if(down_interruptible(&mtx))
		return -EINTR;
	while(prod_count > 0 && kfifo_len(&cbuffer)<len)
		//cond_wait(cons,mtx);
		nr_cons_waiting++;
		up(&mtx);//Libera el mutex
		//Se bloquea en la cola
		if(down_interruptible(&sem_cons)){
			down(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EINTR;
		}
		//Adquiere el mutex
		if(down_interruptible(&mtx))
			return -EINTR;
			//nr_cons_waiting--
	/* Detectar fin de comunicación por error (No hay productores y el buffer esta vacio ) */
	if (prod_count == 0 && kfifo_isempty(&cbuffer)) {up(&mtx); return 0;}
	kfifo_out(&cbuffer, kbuffer,len);

	//cond_signal(prod);
	if(nr_prod_waiting > 0){
		up(&sem_prod);
		nr_prod_waiting--;
	}
	up(&mtx);
	if(copy_to_user(buf,kbuf,len))
		return -EINVAL;
	return len;
}
//-----------TRADUCIR-------------------
static int  fifoproc_release(struct inode *, struct file *){
	if(down_interruptible(&mtx))
		return -EINTR;
	if(file->f_mode & FMODE_READ){//se elimina un escritor(productor)
		cons_count--;
		//cond_signal(prod,mtx);
		if(nr_prod_waiting > 0){
			up(&sem_prod);
			nr_prod_waiting--;
		}
	}

	else{//se elimina un lector(consumidor)
		prod_count--;
		//cond_signal(cons,mtx);
		if(nr_cons_waiting > 0){
			up(&sem_cons);
			nr_cons_waiting--;
		}
		
	}
	//vaciar buffer kfiforeset
	if(prod_count == 0 && cons_count == 0){
		kfifo_reset(&cbuffer);
	}
		//kfiforeset
	up(&mtx);
}

//-----------TRADUCIR-------------------

struct file_operations fops = {

   .owner = THIS_MODULE,//owner
   .read = fifoproc_read, //read()
   .write = fifoproc_write, //write()
   .open = fifoproc_open,
   .release = fifoproc_release,

};



int module_modlist_init(void){

	int ret;
	//Inicializamos la fifo
	ret = kfifo_alloc(&cbuffer, MAX_KBUF*sizeof(int), GFP_KERNEL);
	if (ret) {
		printk(KERN_ERR "error kfifo_alloc\n");
		return ret;
	} 
	//Inicializamos los semáforos
	sema_init(&mtx, 0);//Semaforo que actua como mutex
	sema_init(&sem_prod,0);//Semaforos que actuan como variables de condicion
	sema_init(&sem_cons,0);
	

	// struct proc_dir_entry *proc_create(const char *name,umode_t mode,struct proc_dir_entry *parent,const struct file_operations *ops);
	if((proc_entry = proc_create("fifoproc", 0666, NULL, &fops)) == NULL){
		printk(KERN_INFO "fifoproc -> proc_create() creation failed\n");
		kfifo_reset(&cbuffer);//No se si es necesario
		kfifo_free(&cbuffer);
		return -ENOMEM;
	}
	
	return 0;


}

void module_modlist_clean(void){
	kfifo_reset(&cbuffer);//Elimina todos los elementos
	kfifo_free(&cbuffer);
	remove_proc_entry("fifoproc", NULL);
}

module_init(module_modlist_init);
module_exit(module_modlist_clean);