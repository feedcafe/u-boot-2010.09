#ifndef _LINUX_COMPAT_H_
#define _LINUX_COMPAT_H_

#define __user
#define __iomem

#define ndelay(x)	udelay(1)

#define printk	printf

#define WARN_ON(x) if (x) {printf("WARNING in %s line %d\n" \
				  , __FILE__, __LINE__); }

#define KERN_EMERG
#define KERN_ALERT
#define KERN_CRIT
#define KERN_ERR
#define KERN_WARNING
#define KERN_NOTICE
#define KERN_INFO
#define KERN_DEBUG

#define IRQ_HANDLED	1

#define GFP_ATOMIC ((gfp_t) 0)
#define GFP_KERNEL ((gfp_t) 0)

#define kmalloc(size, flags)	malloc(size)
#define kzalloc(size, flags)	calloc(size, 1)
#define vmalloc(size)		malloc(size)
#define kfree(ptr)		free(ptr)
#define vfree(ptr)		free(ptr)

#define spin_lock_init(...)
#define spin_lock(...)
#define spin_lock_irqsave(lock, flags) do { debug("%lu\n", flags); } while (0)
#define spin_unlock(...)
#define spin_unlock_irqrestore(lock, flags) do {flags = 0; } while (0)

#define DECLARE_WAITQUEUE(...)	do { } while (0)
#define add_wait_queue(...)	do { } while (0)
#define remove_wait_queue(...)	do { } while (0)

#define KERNEL_VERSION(a,b,c)	(((a) << 16) + ((b) << 8) + (c))

/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max at all, of course.
 */
#define min_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define max_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })

#ifndef BUG
#define BUG() do { \
	printf("U-Boot BUG at %s:%d!\n", __FILE__, __LINE__); \
} while (0)

#define BUG_ON(condition) do { if (condition) BUG(); } while(0)
#endif /* BUG */

#define PAGE_SIZE	4096
#endif
