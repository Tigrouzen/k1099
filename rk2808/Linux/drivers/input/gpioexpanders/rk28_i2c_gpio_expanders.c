/****************************************************************************************
 * driver/input/touchscreen/rk28_i2c_tsPCA955Xs.c
 *Copyright 	:ROCKCHIP  Inc
 *Author	: 	wqq
 *Date		: 2009-07-15
 *This driver use for rk28 chip extern touchscreen. Use i2c IF ,the chip is PCA955Xs
 ********************************************************************************************/

#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/delay.h>
#include <linux/device.h>


#include <asm/types.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <asm/arch/api_intc.h>
#include <asm/arch/typedef.h>

#include <asm/arch/api_intc.h>
#include <asm/arch/hw_define.h>
#include <asm/arch/hardware.h>

#include <asm/arch/iomux.h>
#include <linux/ioport.h>
#include <asm/arch/gpio.h>

#include <linux/i2c.h>

#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
#endif

#include <asm/arch/api_i2c.h>

/******************************************
		DEBUG
*** ***************************************/
//#define RK28_PRINT
#include <asm/arch/rk28_debug.h>


#define PCA953X_INPUT          0
#define PCA953X_OUTPUT         2
#define PCA953X_INVERT         2
#define PCA953X_DIRECTION      6



#define PCA955X_I2C_ADDR   		0x42

#define PCA955X_IIC_SPEED 		200





struct pca955x_chip {
	u16 reg_output;//1:high,0:low
	u16 reg_direction;//0:out ,1:in
};

struct PCA955X_CHIP_EVENT{	
	struct i2c_client *client;
	struct pca955x_chip *chip;
	spinlock_t	lock;
	char	phys[32];
};

struct PCA955X_CHIP_EVENT *g_pca955x_dev;
static struct pca955x_chip pca955xchip=
{
	.reg_output=0,
	.reg_direction=0,
};
/*tochscreen private data*/
static u8 i2c_buf[16];

static int rk28_pca955x_probe(struct i2c_adapter *bus, int address, int kind);





/*read the pca955x register ,used i2c bus*/
static int pca955x_read_regs(struct i2c_client *client, u8 reg, u8 buf[])
{
	int ret;
	struct i2c_msg msgs[1] = {
		{ client->addr, 1, 2, buf },
	};
	client->mode = PCA955XMode;
	client->Channel = I2C_CH1;
	client->speed = PCA955X_IIC_SPEED;
	buf[0] = reg;
	//printk("%s the slave i2c device mode == %d\n",__FUNCTION__,client->adapter->mode);
	ret = i2c_transfer(client->adapter, msgs, 1);
	
	if (ret > 0)
		ret = 0;	
	return ret;
}


/* set the pca955x registe,used i2c bus*/
static int pca955x_write_reg(struct i2c_client *client, u8 reg, u16*  p)
{
	int ret;
	struct i2c_msg msgs[1] = {
		{ client->addr, 0, 3, i2c_buf }
	};
	u8 tempbu[2];
	client->mode = PCA955XMode;
	client->Channel = I2C_CH1;
	client->speed = PCA955X_IIC_SPEED;
	i2c_buf[0] = reg;
	tempbu[0]=*p&0x00ff;
	tempbu[1]=(*p>>8)&0x00ff;
	memcpy(&i2c_buf[1], &tempbu[0], 2);
	
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret > 0)
		ret = 0;
	return ret;
}


static int rk28_pca955x_detach_client(struct i2c_client *client)
{
	rk28printk("************>%s.....%s.....\n",__FILE__,__FUNCTION__);
	return 0;
}

static void rk28_pca955x_shutdown(struct i2c_client *client)
{
	rk28printk("************>%s.....%s.....\n",__FILE__,__FUNCTION__);
}


static unsigned short pca955x_normal_i2c[] = {PCA955X_I2C_ADDR>>1,I2C_CLIENT_END};
static unsigned short pca955x_ignore = I2C_CLIENT_END;

static struct i2c_client_address_data pca955x_addr_data={
	.normal_i2c = pca955x_normal_i2c,
	.probe = &pca955x_ignore,
	.ignore =& pca955x_ignore,
};
static int rk28_pca955x_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap,&pca955x_addr_data,rk28_pca955x_probe);
}

static struct i2c_driver pca955x_driver  = {
	.driver = {
		.name = "pca955x",
		.owner = THIS_MODULE,
	},
	.id = PCA955X_I2C_ADDR,
	.attach_adapter = &rk28_pca955x_attach_adapter,
	.detach_client 	=  &rk28_pca955x_detach_client,
	.shutdown     	=  &rk28_pca955x_shutdown,
};
static struct  i2c_client pca955x_client = {
		.driver = &pca955x_driver,
		.name	= "pca955x",
	};



int pca955x_gpio_direction_input(unsigned off)
{
	struct pca955x_chip *chip=&pca955xchip;
	uint16_t reg_val;
	int ret;

	reg_val = chip->reg_direction | (1u << off);
	ret = pca955x_write_reg(g_pca955x_dev->client, PCA953X_DIRECTION, &reg_val);
	if (ret)
		{
		printk("pca955x_write error=%d\n",reg_val);
		return ret;
		}

	chip->reg_direction = reg_val;
	return 0;
}

int pca955x_gpio_direction_output( int off,int val)
{
	struct pca955x_chip *chip=&pca955xchip;
	uint16_t reg_val;
	int ret;

#if 1
	 if(!g_pca955x_dev ){
		printk("%s:g_pca955x_dev is NULL\n" , __func__ );
		return -1;
	}
#endif
	/* set output level */
	if (val)
		reg_val = chip->reg_output | (1u << off);
	else
		reg_val = chip->reg_output & ~(1u << off);

	ret = pca955x_write_reg(g_pca955x_dev->client, PCA953X_OUTPUT, &reg_val);
	if (ret)
		{
		printk("pca955x_write error=%d\n",reg_val);
		return ret;
		}
	
	chip->reg_output = reg_val;

	/* then direction */
	reg_val = chip->reg_direction & ~(1u << off);//0 as output
	ret = pca955x_write_reg(g_pca955x_dev->client, PCA953X_DIRECTION, &reg_val);
	if (ret)
		{
		printk("pca955x_write error=%d\n",reg_val);
		return ret;
		}
	chip->reg_direction = reg_val;
	return 0;
}

int pca955x_gpio_get_value(unsigned off)
{

	uint16_t reg_val;
	int ret;



	ret = pca955x_read_regs(g_pca955x_dev->client, PCA953X_INPUT, (u8 *)&reg_val);
	if (ret < 0) {
		{
		printk("pca955x_read error=%d\n",reg_val);
		return ret;
		}
	}

	return (reg_val & (1u << off)) ? 1 : 0;
}

int  pca955x_gpio_set_value( unsigned off, int val)
{
	struct pca955x_chip *chip=&pca955xchip;
	uint16_t reg_val;
	int ret;



	if (val)
		reg_val = chip->reg_output | (1u << off);
	else
		reg_val = chip->reg_output & ~(1u << off);

	ret = pca955x_write_reg(g_pca955x_dev->client, PCA953X_OUTPUT, &reg_val);
	if (ret)
	{
		printk("pca955x_write error=%d\n",reg_val);
		return ret;
	}
	chip->reg_output = reg_val;
	return ret;
}

extern void rk28_add_usb_devices(void);

//int pca955x_io_ready = 0;
static int rk28_pca955x_probe(struct i2c_adapter *bus, int address, int kind)

{
	struct PCA955X_CHIP_EVENT *pca955x_dev;
//	struct pca955x_chip *chip;
	unsigned int err = 0;

	pca955xchip.reg_output=0xFF;
	pca955xchip.reg_direction=0xFF;
    printk("%s:g_pca955x_dev is NULL\n" , __func__ );
	pca955x_dev=kzalloc(sizeof(struct PCA955X_CHIP_EVENT), GFP_KERNEL);
	if(!pca955x_dev)

{

		printk("failed to allocate memory!!\n");
		err = -ENOMEM;
		goto nomem;

	}

	pca955x_client.adapter = bus;
	pca955x_client.addr= PCA955X_I2C_ADDR>>1;//address;
	pca955x_client.mode = PCA955XMode;
	pca955x_client.Channel = I2C_CH1;
	pca955x_client.addressBit=I2C_7BIT_ADDRESS_8BIT_REG;
	pca955x_dev->client=&pca955x_client; 
	
      g_pca955x_dev=pca955x_dev;
	  
	err = i2c_attach_client(&pca955x_client);
	if (err < 0)
	{
		printk("gpio expanders i2c attach error\n");
		err = -100;
		 goto attach_err;	
	}
	
#if(defined(CONFIG_BOARD_NX7005))
	pca955x_gpio_direction_output(PCA955X_Pin0,GPIO_HIGH);
	pca955x_gpio_direction_output(PCA955X_Pin1,GPIO_HIGH);
	pca955x_gpio_direction_output(PCA955X_Pin2,GPIO_LOW);
	pca955x_gpio_direction_output(PCA955X_Pin3,GPIO_LOW);

// select Host or internal evdo module
#ifdef CONFIG_DWC_OTG_HOST_PREFERENCE// HOST
	pca955x_gpio_direction_output(PCA955X_Pin4,GPIO_HIGH);
	pca955x_gpio_direction_output(PCA955X_Pin5,GPIO_LOW);
#else
    #ifdef CONFIG_DWC_OTG_EVDO_PREFERENCE// Internal EVDO
    	pca955x_gpio_direction_output(PCA955X_Pin4,GPIO_LOW);
    	pca955x_gpio_direction_output(PCA955X_Pin5,GPIO_HIGH);
    #else// DEVICE
    	pca955x_gpio_direction_output(PCA955X_Pin6,GPIO_LOW);
	mdelay(500);
    	pca955x_gpio_direction_output(PCA955X_Pin6,GPIO_HIGH);
    #endif
#endif



       pca955x_gpio_direction_output(PCA955X_Pin10,GPIO_HIGH);//GPIO_LOW
       pca955x_gpio_direction_output(PCA955X_Pin14,GPIO_LOW);	
       pca955x_gpio_direction_output(PCA955X_Pin15,GPIO_LOW);
       pca955x_gpio_direction_output(PCA955X_Pin16,GPIO_LOW);
       pca955x_gpio_direction_output(PCA955X_Pin17,GPIO_LOW);
	pca955x_gpio_direction_output(PCA955X_Pin12,GPIO_HIGH);	 
	mdelay(5000);
	pca955x_gpio_direction_output(PCA955X_Pin12,GPIO_LOW);	
	pca955x_gpio_direction_output(PCA955X_Pin13,GPIO_HIGH);	
#else
#if(defined(CONFIG_BOARD_ZTX))
	pca955x_gpio_direction_output(PCA955X_Pin0,GPIO_HIGH);
#else
	pca955x_gpio_direction_output(PCA955X_Pin0,GPIO_LOW);
#endif
	pca955x_gpio_direction_output(PCA955X_Pin1,GPIO_HIGH);
	pca955x_gpio_direction_output(PCA955X_Pin2,GPIO_LOW);
	pca955x_gpio_direction_output(PCA955X_Pin3,GPIO_LOW);

// select Host or internal evdo module
#ifdef CONFIG_DWC_OTG_HOST_PREFERENCE// HOST
	pca955x_gpio_direction_output(PCA955X_Pin4,GPIO_HIGH);
	pca955x_gpio_direction_output(PCA955X_Pin5,GPIO_LOW);
#else
    #ifdef CONFIG_DWC_OTG_EVDO_PREFERENCE// Internal EVDO
    	pca955x_gpio_direction_output(PCA955X_Pin4,GPIO_LOW);
    	pca955x_gpio_direction_output(PCA955X_Pin5,GPIO_HIGH);
    #else// DEVICE
    	pca955x_gpio_direction_output(PCA955X_Pin6,GPIO_LOW);
	mdelay(500);
    	pca955x_gpio_direction_output(PCA955X_Pin6,GPIO_HIGH);
    #endif
#endif
#if(defined(CONFIG_BOARD_ZTX))
    	pca955x_gpio_direction_output(PCA955X_Pin6,GPIO_LOW);
#else

#endif

       pca955x_gpio_direction_output(PCA955X_Pin10,GPIO_HIGH);	
       pca955x_gpio_direction_output(PCA955X_Pin14,GPIO_LOW);	
       pca955x_gpio_direction_output(PCA955X_Pin15,GPIO_LOW);
       pca955x_gpio_direction_output(PCA955X_Pin16,GPIO_LOW);
       pca955x_gpio_direction_output(PCA955X_Pin17,GPIO_LOW);
	pca955x_gpio_direction_output(PCA955X_Pin12,GPIO_HIGH);	 
	mdelay(5000);
	pca955x_gpio_direction_output(PCA955X_Pin12,GPIO_LOW);	
	pca955x_gpio_direction_output(PCA955X_Pin13,GPIO_LOW);	
#endif

	
	printk("gpio expanders ok\n");
	
	//For USB HOST temp
	//pca955x_gpio_direction_output(PCA955X_Pin4, GPIO_HIGH);

	//For USB EVDO temp
	//pca955x_gpio_direction_output(PCA955X_Pin5, GPIO_HIGH);
	//pca955x_gpio_direction_output(PCA955X_Pin4, GPIO_LOW);
	//mdelay(100);
	//pca955x_io_ready = 1;

	goto nomem;

attach_err:
	kfree(pca955x_dev);

nomem:
	//rk28_add_usb_devices();

	return err;
}


static int __init rk28_pca955x_init(void)

{
	//printk("\n gpio expanders in I2C\n\n");
	
	return i2c_add_driver(&pca955x_driver);

}


//late_initcall(rk28_pca955x_init);
//fs_initcall(rk28_pca955x_init);
//fs_initcall(rk28_pca955x_init);
module_init(rk28_pca955x_init);

//late_initcall(rk28_pca955x_init);


static void __exit rk28_pca955x_exit(void)

{

	i2c_del_driver(&pca955x_driver);

}

module_exit(rk28_pca955x_exit);


MODULE_DESCRIPTION ("GTT touchscreen driver");

MODULE_AUTHOR("ty<ty@rockchip.com.cn>");

MODULE_LICENSE("GPL");



