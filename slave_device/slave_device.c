#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/net.h>
#include <net/sock.h>
#include <asm/processor.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/mm.h>
#include <asm/page.h>


#ifndef VM_RESERVED
#define VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

#define slave_IOCTL_CREATESOCK 0x12345677
#define slave_IOCTL_MMAP 0x12345678
#define slave_IOCTL_EXIT 0x12345679

#define slave_PRINT_DESCRIPTOR 0x12345680
#define num_page 50
#define BUF_SIZE 512

//#define ASYN

struct dentry  *file1;//debug file

typedef struct socket * ksocket_t;

//functions about kscoket are exported,and thus we use extern here
extern ksocket_t ksocket(int domain, int type, int protocol);
extern int kconnect(ksocket_t socket, struct sockaddr *address, int address_len);
extern ssize_t krecv(ksocket_t socket, void *buffer, size_t length, int flags);
extern int kclose(ksocket_t socket);
extern unsigned int inet_addr(char* ip);
extern char *inet_ntoa(struct in_addr *in); //DO NOT forget to kfree the return pointer

static int __init slave_init(void);
static void __exit slave_exit(void);

int slave_close(struct inode *inode, struct file *filp);
int slave_open(struct inode *inode, struct file *filp);
static long slave_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param);
ssize_t receive_msg(struct file *filp, char *buf, size_t count, loff_t *offp );

static mm_segment_t old_fs;
static ksocket_t sockfd_cli;//socket to the master server
static struct sockaddr_in addr_srv; //address of the master server

#ifdef ASYN
static struct workqueue_struct *wq_fcntl;
DECLARE_WORK(work_fcntl, (void *)receive_msg);
static struct workqueue_struct *wq_mmap;
DECLARE_WORK(work_mmap, (void *)slave_ioctl);
#endif

static int my_mmap(struct file *flip, struct vm_area_struct *vma);

void mmap_open(struct vm_area_struct *vma) {
	return;
}
void mmap_close(struct vm_area_struct *vma) {
	return;
}
//file operations
static struct file_operations slave_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = slave_ioctl,
	.open = slave_open,
	.read = receive_msg,
	.mmap = my_mmap,
	.release = slave_close
};
//device info
static struct miscdevice slave_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "slave_device",
	.fops = &slave_fops
};

struct vm_operations_struct mmap_vm_ops = {
	.open = mmap_open,
	.close = mmap_close
};

static int my_mmap(struct file *flip, struct vm_area_struct *vma) {
	vma->vm_flags |= VM_RESERVED;
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_private_data = flip->private_data;

	unsigned long size = vma->vm_end - vma->vm_start;
	if(remap_pfn_range(vma, vma->vm_start, virt_to_phys(vma->vm_private_data)>>PAGE_SHIFT, size, vma->vm_page_prot))
		return -EAGAIN;
	mmap_open(vma);

	#ifdef ASYN
	wq_mmap = create_workqueue("slave_wq_mmap");
	queue_work(wq_mmap, &work_mmap);
	printk("ASYN workqueue created_slave\n");
	#endif

	return 0;
}

static int __init slave_init(void)
{
	int ret;
	file1 = debugfs_create_file("slave_debug", 0644, NULL, NULL, &slave_fops);

	//register the device
	if( (ret = misc_register(&slave_dev)) < 0){
		printk(KERN_ERR "misc_register failed!\n");
		return ret;
	}

	printk(KERN_INFO "slave has been registered!\n");

	return 0;
}

static void __exit slave_exit(void)
{
	misc_deregister(&slave_dev);

	#ifdef ASYN
	if(wq_fcntl) destroy_workqueue(wq_fcntl);
	if(wq_mmap) destroy_workqueue(wq_mmap);
	#endif
	
	printk(KERN_INFO "slave exited!\n");
	debugfs_remove(file1);
}


int slave_close(struct inode *inode, struct file *filp)
{
	kfree(filp->private_data);
	return 0;
}

int slave_open(struct inode *inode, struct file *filp)
{
	filp->private_data = kmalloc(PAGE_SIZE * num_page, GFP_KERNEL);
	return 0;
}
static long slave_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	long ret = -EINVAL;

	int addr_len ;
	unsigned int i;
	size_t len, data_size = 0;
	char *tmp, ip[20], buf[BUF_SIZE];
	struct page *p_print;
	unsigned char *px;

    pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
    pte_t *ptep, pte;
	old_fs = get_fs();
	set_fs(KERNEL_DS);

    //printk("slave device ioctl");

	switch(ioctl_num){
		case slave_IOCTL_CREATESOCK:// create socket and connect to master
            printk("slave device ioctl create socket");

			if(copy_from_user(ip, (char*)ioctl_param, sizeof(ip)))
				return -ENOMEM;

			sprintf(current->comm, "ksktcli");

			memset(&addr_srv, 0, sizeof(addr_srv));
			addr_srv.sin_family = AF_INET;
			addr_srv.sin_port = htons(2325);
			addr_srv.sin_addr.s_addr = inet_addr(ip);
			addr_len = sizeof(struct sockaddr_in);

			sockfd_cli = ksocket(AF_INET, SOCK_STREAM, 0);
			printk("sockfd_cli = 0x%p  socket is created\n", sockfd_cli);
			if (sockfd_cli == NULL)
			{
				printk("socket failed\n");
				return -1;
			}
			if (kconnect(sockfd_cli, (struct sockaddr*)&addr_srv, addr_len) < 0)
			{
				printk("connect failed\n");
				return -1;
			}
			tmp = inet_ntoa(&addr_srv.sin_addr);
			printk("connected to : %s %d\n", tmp, ntohs(addr_srv.sin_port));
			kfree(tmp);
			//printk("kfree(tmp)");

			#ifdef ASYN
			wq_fcntl = create_workqueue("slave_wq_fcntl");
			#endif

			ret = 0;
			break;
		case slave_IOCTL_MMAP:
			ret = krecv(sockfd_cli, file->private_data, PAGE_SIZE, MSG_WAITALL);
			break;

		case slave_IOCTL_EXIT:
			if(kclose(sockfd_cli) == -1)
			{
				printk("kclose cli error\n");
				return -1;
			}
			ret = 0;
			break;
		case slave_PRINT_DESCRIPTOR:
			pgd = pgd_offset(current->mm, ioctl_param);
			p4d = p4d_offset(pgd, ioctl_param);
			pud = pud_offset(p4d, ioctl_param);
			pmd = pmd_offset(pud, ioctl_param);
			ptep = pte_offset_kernel(pmd , ioctl_param);
			pte = *ptep;
			if(pte_none(pte)){
				printk("slave: pte fault\n");
				break;
			}
			printk("slave: %lX\n", pte);
			ret = 0;
			break;
		default:
            		printk("unknown ioctl parameter");
			break;
	}
	set_fs(old_fs);

	return ret;
}

ssize_t receive_msg(struct file *filp, char *buf, size_t count, loff_t *offp )
{
//call when user is reading from this device
	char msg[BUF_SIZE];
	size_t len;
	len = krecv(sockfd_cli, msg, count, 0);
	if(copy_to_user(buf, msg, len))
		return -ENOMEM;
	return len;
}


module_init(slave_init);
module_exit(slave_exit);
MODULE_LICENSE("GPL");
