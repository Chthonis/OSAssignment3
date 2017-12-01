#include <linux/module.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");

static int N;
module_param(N, int, S_IRUGO);
static int start = -1;
static int end = -1;
char ** fifoBuffer;
static char fullmsg[256] = {0};
static DEFINE_SEMAPHORE(empty);
static DEFINE_SEMAPHORE(full);
static DEFINE_MUTEX(mutex);
static short msgSize;


ssize_t my_read(struct file *filep, char *buf, size_t count, loff_t *fpos);
ssize_t my_write( struct file *filep, const char *buf, size_t count, loff_t *fpos);

//========================================================_my_fops_========================================================
static struct file_operations my_fops = {
  .owner = THIS_MODULE,
  .read = my_read,
  .write = my_write
};

//========================================================_numpipe_========================================================
static struct miscdevice numpipe = {
  .minor = MISC_DYNAMIC_MINOR,
  .name = "numpipe",
  .fops = &my_fops
};

//========================================================___init_========================================================
int __init numpipe_init(void)
{
  int i;
  int reg = misc_register(&numpipe);
  if(reg < 0)
  {
    printk(KERN_ALERT "Error registering misc device 'numpipe' \n");
  }
  else
  {
    sema_init(&empty, N);
    sema_init(&full, 0);
    mutex_init(&mutex);
  
  }
  fifoBuffer = kmalloc(sizeof(char*)*10,GFP_KERNEL);
  if(!fifoBuffer)
  {
    printk(KERN_ALERT "Error kmallocing buf\n");
  }
  for(i=0; i<N; i++)
  {
    fifoBuffer[i] = kmalloc(sizeof(char)*N, GFP_KERNEL);
    if(!fifoBuffer)
    {
      printk(KERN_ALERT "Error kmallocing buf\n");
    }
  }
  return reg;
}

//========================================================_Read_========================================================
ssize_t my_read(struct file *filep, char *buf, size_t count, loff_t *fpos)
{
  int ctu = 0;
  int readingVal;
  int retFull;
  int retEmpty;
  int retMutex;

  retFull = down_interruptible(&full);
  if(retFull != 0)
  {
    return retFull;
  }
  retMutex = mutex_lock_interruptible(&mutex);
  if(retMutex != 0)
  { 
    up(&full);
    return retMutex;
  }
  
  readingVal = strlen(fifoBuffer[start]);
  ctu = copy_to_user(buf, fifoBuffer[start], readingVal+1);

  if (ctu == 0){
    printk(KERN_INFO "Successfully copy to user\n");

    if(start == end)
    {
      start = -1;
      end = -1;
    }
    else
    {
      if(start == N-1){ start = 0;}
      else{ start+=1;}
    }
  }
  else {
    printk(KERN_ALERT "Failed to copy to user\n");
    //mutex_unlock(&mutex);
    //up(&empty);
    return -EFAULT;
  }

  mutex_unlock(&mutex);
  up(&empty);
  //consume item
  return readingVal;
}

//========================================================_Write_========================================================
ssize_t my_write( struct file *filep, const char *buf, size_t count, loff_t *fpos)
{
  int cfu = 0;
  int writingVal;
  int retFull;
  int retEmpty;
  int retMutex;
  //produce item

  cfu = copy_from_user(fullmsg, buf, count);
  
  if (cfu == 0)
  {
    printk(KERN_INFO "Successfully copy to user\n");
    msgSize = strlen(fullmsg);
  }
  else {
    printk(KERN_ALERT "Failed to copy to user\n");
    return -EFAULT;
  }

  retEmpty = down_interruptible(&empty);
  if(retEmpty != 0){ return retEmpty; }
  retMutex = mutex_lock_interruptible(&mutex);
  if(retMutex != 0)
  {
    up(&full);
    return retMutex;
  }

  if(start == -1 && end == -1)
  {
    start = 0;
    end = 0;
  }
  else
  {	
	  if(end == N-1){ end = 0;}
	  else{ end += 1;}
  }
  strcpy(fifoBuffer[end],fullmsg);	
  printk("QUEUE element %s",fifoBuffer[end]);


  //insert item
  printk(KERN_INFO "device_write(%zu)", count);
  mutex_unlock(&mutex);
  up(&full);

  //  printk(KERN_ALERT "Write Successful size: %i\n", buffSize);
  return count;
}


//========================================================___exit_========================================================
void __exit numpipe_exit(void)
{
  int i;
  for(i=0; i<N; i++)
  {
    kfree(fifoBuffer[i]);
    fifoBuffer[i]=NULL;
  }
  kfree(fifoBuffer);
  misc_deregister(&numpipe);
}


module_init(numpipe_init);
module_exit(numpipe_exit);
