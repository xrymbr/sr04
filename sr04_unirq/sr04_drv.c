#include <linux/module.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/ktime.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <asm/current.h>
#include <linux/kthread.h>



#define GPIO_COUNT 10

static int major = 0;
static struct class *sr04_class;
static DECLARE_WAIT_QUEUE_HEAD(sr04wq);
static u64 sr04_data = 0;  



struct gpio_pin{
    int gpio;
    int minor;
    int flag;
    int irq;
    struct gpio_desc *gpiod;
};
struct gpio_pin gpio_sr04[GPIO_COUNT];





static ssize_t sr04_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
#if 0
    int us = 0;
    int val;
    int ret;
    int timeout = 1000000;
	unsigned long flags;


    local_irq_save(flags);
    gpiod_set_value(gpio_sr04[0].gpiod,1);
    udelay(15);
    gpiod_set_value(gpio_sr04[0].gpiod,0);
    
    while(!gpiod_get_value(gpio_sr04[1].gpiod)&&timeout--)
    {
        udelay(1);
    }
    if(!timeout)
    {
        local_irq_restore(flags);
        return EAGAIN;
    }
    
    timeout = 1000000;
    while(gpiod_get_value(gpio_sr04[1].gpiod)&&timeout--)
    {
        udelay(1);
        us++;
    }

    
    if(!timeout)
    {
        local_irq_restore(flags);
        return EAGAIN;
    }
    
    local_irq_restore(flags);
    if(copy_to_user(buf,&us,4))
        ret = -EFAULT;
    return 4;
#else
    int ret;

    gpiod_set_value(gpio_sr04[0].gpiod,1);
    udelay(15);
    gpiod_set_value(gpio_sr04[0].gpiod,0);

    wait_event_interruptible(sr04wq, sr04_data != 0); 
    if(copy_to_user(buf,&sr04_data,4))
        ret = -EFAULT;
    sr04_data = 0;
    return 4;


#endif
}



static const struct file_operations sr04_opr = {
	.owner		= THIS_MODULE,
	.read		= sr04_read,
};
    

static irqreturn_t sr04_isr(int irq, void *dev_id)
{
    int val = gpiod_get_value(gpio_sr04[1].gpiod);

    if(val)
    {
       sr04_data =  ktime_get_ns();  
    }
    else
    {
        sr04_data =  ktime_get_ns() - sr04_data;        
        //wake_up_interruptible(&sr04wq);        
        wake_up(&sr04wq);        
    }

	return IRQ_HANDLED; // IRQ_WAKE_THREAD;
}




static int sr04_probe(struct platform_device *pdev)
{
    static int count;
    int i;
    int ret;    
    count = gpiod_count(&pdev->dev,"sr04");
    printk("count = %d \n",count);

    for(i = 0;i < count;i++)
        gpio_sr04[i].minor = i;            
    
    gpio_sr04[0].gpiod = devm_gpiod_get_index(&pdev->dev,"sr04",0,GPIOD_OUT_LOW);
    if (IS_ERR(gpio_sr04[0].gpiod)) {
        ret = PTR_ERR(gpio_sr04[0].gpiod);
        printk("devm_gpiod_get_index is err\n");
        return ret;
    }      
    
    gpio_sr04[1].gpiod = devm_gpiod_get_index(&pdev->dev,"sr04",1,GPIOD_IN);
    if (IS_ERR(gpio_sr04[1].gpiod)) {
        ret = PTR_ERR(gpio_sr04[1].gpiod);
        printk("devm_gpiod_get_index is err\n");
        return ret;
    }      
    gpio_sr04[1].irq = gpiod_to_irq(gpio_sr04[1].gpiod);

    
    ret = request_irq(gpio_sr04[1].irq,sr04_isr, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "sr04", NULL);
    
	if (ret < 0) {
		printk(KERN_WARNING "inia100: unable to get irq %d\n",
				gpio_sr04[1].irq);
        free_irq(gpio_sr04[1].irq,&gpio_sr04[1]);
        
	}
   return 0;
}

static int sr04_remove(struct platform_device *pdev)
{
    static int count;
    int i;
    int err;    

    
    count = gpiod_count(&pdev->dev,"sr04");
    
    for(i = 0;i < count;i++)
    {
        gpiod_put(gpio_sr04[i].gpiod);
    }
    return 0;
}


static const struct of_device_id lc_sr04[] = {
    { .compatible = "sr04test" },
    { },
};

/* 1. 定义platform_driver */
static struct platform_driver sr04s_driver = {
    .probe      = sr04_probe,
    .remove     = sr04_remove,
    .driver     = {
        .name   = "sr04test",
        .of_match_table = lc_sr04,
    },
};






static int __init sr04_init(void)
{
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    
    major = register_chrdev(0,"sr04opr",&sr04_opr);
    sr04_class = class_create(THIS_MODULE,"sr04_class");
	if (IS_ERR(sr04_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "sr04opr");
		return PTR_ERR(sr04_class);
	}
    
	device_create(sr04_class, NULL, MKDEV(major, 0), NULL, "lc_sr04");
    

	return platform_driver_register(&sr04s_driver);
} 

static void __exit sr04_exit(void)
{
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
    platform_driver_unregister(&sr04s_driver);
    device_destroy(sr04_class,MKDEV(major, 0));
    class_destroy(sr04_class);
    unregister_chrdev(major, "sr04opr");
    free_irq(gpio_sr04[1].irq,NULL);

} 



module_init(sr04_init);
module_exit(sr04_exit);
MODULE_LICENSE("GPL");



