/*
 * linux/drivers/input/keyboard/rk28_ad_button.c
 *
 * Driver for the rk28 matrix keyboard controller.
 *
 * Created: 2009-5-4
 * Author:	LZJ <lzj@rockchip.com>
 *
 * This driver program support to AD key which use for rk28 chip
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <asm/arch/typedef.h>
#include <asm/arch/gpio.h>
#include <asm/arch/hardware.h>
/*
 * Keypad Controller registers
 */
 //#define RK28_PRINT
#include <asm/arch/rk28_debug.h>
#include <asm/arch/adc.h>


/* Debug */
#if 0
#define DBG(x...)	printk(KERN_INFO x)
#else
#define DBG(x...)
#endif
#define WAKEUP_KEY_PORT         GPIOPortE_Pin2  /* 2808SDK BOARD */
//ROCKCHIP AD KEY CODE ,for demo board
//      key		--->	EV	
#if 1
#define AD1KEY1		103//DPAD_UP	----------UP
#define AD1KEY2 		106//DPAD_RIGHT-------FFD
#define AD1KEY3 		105//DPAD_LEFT--------FFW	
#define AD1KEY4 		108//DPAD_DOWN------DOWN		
#define AD1KEY5		62   //ENDCALL           WAKE_DROPPED
#define AD1KEY6		28    //ENTER             

#define AD2KEY1		    114   //VOLUME_DOWN  59	//MENU			//115   //VOLUME_UP
#define AD2KEY2 		115   //VOLUME_UP  102   //HOME			//114   //VOLUME_DOWN
#define AD2KEY3 		59	  //MENU   158	//BACK----ESC	//59	//MENU	
#define AD2KEY4 		102   //HOME   114   //VOLUME_DOWN	//62   //ENDCALL  		
#define AD2KEY5 		158	  //BACK----ESC 115   //VOLUME_UP	//158	//BACK------------ESC
#define AD2KEY6 		28    //ENTER  116	//POWER
/*
#define KEY_VOLUMEDOWN	114
#define KEY_VOLUMEUP		115
#define KEY_HOME			102
#define KEY_UP				103
#define KEY_LEFT				105
#define KEY_RIGHT			106
#define KEY_DOWN			108
#define KEY_KPENTER			96
#define KEY_POWER			116
#define KEY_ESC				158
#define KEY_BACK			1
#define KEY_MENU			59
#define KEY_Confirm			28
*/
#define KEY_VOLUMEDOWN	114
#define KEY_VOLUMEUP		115
#define KEY_HOME			102
#define KEY_UP				103
#define KEY_LEFT				105
#define KEY_RIGHT			106
#define KEY_DOWN			108
#define KEY_KPENTER			96
#define KEY_POWER			116
#define KEY_ESC				158
#define KEY_BACK			1
#define KEY_MENU			59
#define KEY_Confirm			28
/*
#define KEY_CAL				46	//C
#define KEY_PAIHANG			25	//P
#define KEY_SHAOMIAO		31	//S
#define KEY_QMENU			50	//M
#define KEY_TONGXUN		20	//T
#define KEY_QHOME			35	//H
*/
#else 
#define AD1KEY1 		103//DPAD_UP	----------UP
#define AD1KEY2 		106//DPAD_RIGHT-------FFD
#define AD1KEY3 		105//DPAD_LEFT--------FFW	
#define AD1KEY4 		108//DPAD_DOWN------DOWN		
#define AD1KEY5 		28    //ENTER
#define AD1KEY6 		28    //ENTER

#define AD2KEY1 		103//DPAD_UP	----------UP
#define AD2KEY2 		106//DPAD_RIGHT-------FFD
#define AD2KEY3 		108//DPAD_DOWN------DOWN
#define AD2KEY4 		59	//MENU		
#define AD2KEY5 		158 //BACK------------ESC
#define AD2KEY6 		116 //POWER

#endif
#define Valuedrift		50
#define EmptyADValue			950  //1000
#define InvalidADValue	10
#define ADKEYNum		12

/*power event button*/
#define  POWER				116
#define  ENDCALL				62
#define  ONESEC_TIMES		100
#define  SEC_NUM			1
#define  SLEEP_TIME			2	/*per 40ms*/
static unsigned  int	pwrscantimes = 0;
static unsigned  int valuecount = 0;
static unsigned  int g_code = 0;

extern int pm_sleep_status ;
volatile int g_enable_sleep=1;

int g_wake_press = 0;
struct timer_list g_wakeup_key_timer;
volatile int g_wakeup_key_enable = 1;



#define KEY_PHYS_NAME	"rk28_AD_button/input0"
//ADC Registers
typedef  struct tagADC_keyst
{
	unsigned int adc_value;
	unsigned int adc_keycode;
}ADC_keyst,*pADC_keyst;

//	adc	 ---> key	
static  ADC_keyst ad1valuetab[] = {
	{ 95, AD1KEY1},
	{247,AD1KEY2},
	{403,AD1KEY3},
	{561,AD1KEY4},
	{723,AD1KEY5},
	{899,AD1KEY6},

	{EmptyADValue,0}
};
/*
94   250
*/
static  ADC_keyst ad2valuetab[] = {
	{94, KEY_HOME},

	{172, KEY_MENU},
/*	{815, KEY_PAGEUP},
	{765, KEY_UP},
	{709, KEY_LEFT},
	{920, KEY_RIGHT},
	{652, KEY_DOWN},
	{580, KEY_PAGEDOWN},
	{850, KEY_CAL},
	{900, KEY_PAIHANG},
	{505, KEY_SHAOMIAO},
	{410, KEY_ENTER},
	{310, KEY_QMENU},
	{60, KEY_TONGXUN},
	{195, KEY_MENU},
*/
	{EmptyADValue,0}

};



//key code tab
static unsigned char initkey_code[ ] = 
{
	AD1KEY1, AD1KEY2,AD1KEY3,AD1KEY4,AD1KEY5,AD1KEY6,
	AD2KEY1, AD2KEY2,AD2KEY3,AD2KEY4,AD2KEY5,AD2KEY6,ENDCALL	,KEY_WAKEUP
};


struct rk28_AD_button_platform_data {
	int x;
	
};

struct rk28_AD_button {
	struct rk28_AD_button_platform_data *pdata;
	struct timer_list timer;

	struct clk *clk;
	struct input_dev *input_dev;
	void __iomem *mmio_base;
	/* matrix key code map */
	unsigned char keycodes[13];
	/* state row bits of each column scan */
	uint32_t direct_key_state;
	unsigned int direct_key_mask;
	int rotary_rel_code[2];
	int rotary_up_key[2];
	int rotary_down_key[2];
};

 struct rk28_AD_button *prockAD_button;


extern 	void  RockAdcScanning(void);
extern 	void  printADCValue(void);
extern 	int32 ADCInit(void);
extern 	int get_rock_adc1(void);
extern	int get_rock_adc2(void);

unsigned int find_rock_adkeycode(unsigned int advalue,pADC_keyst ptab)
{	
	while(ptab->adc_value!=EmptyADValue)
		{
		//	if((advalue>ptab->adc_value-Valuedrift)&&(advalue<ptab->adc_value+Valuedrift))
			if(abs(advalue-ptab->adc_value)<Valuedrift)
				return ptab->adc_keycode;
			ptab++;
		}
	return 0;
}


static int rk28_AD_button_open(struct input_dev *dev)
{
//	struct rk28_AD_button *AD_button = input_get_drvdata(dev);


	return 0;
}

static void rk28_AD_button_close(struct input_dev *dev)
{
//	struct rk28_AD_button *AD_button = input_get_drvdata(dev);

}

#if 0


#ifdef CONFIG_PM
static int rk28_AD_button_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct rk28_AD_button *AD_button = platform_get_drvdata(pdev);

	clk_disable(AD_button->clk);
	return 0;
}
static void rk28_AD_button_config(struct rk28_AD_button *AD_button)
{
	printk("Enter %s -----%s------%s\n",__FILE__,__FUNCTION__,__LINE__);
/*
	struct rk28_AD_button_platform_data *pdata = AD_button->pdata;
	unsigned int mask = 0, direct_key_num = 0;
	unsigned long kpc = 0;

	// enable matrix keys with automatic scan /
	if (pdata->matrix_key_rows && pdata->matrix_key_cols) {
		kpc |= KPC_ASACT | KPC_MIE | KPC_ME | KPC_MS_ALL;
		kpc |= KPC_MKRN(pdata->matrix_key_rows) |
			   KPC_MKCN(pdata->matrix_key_cols);
	}

	// enable rotary key, debounce interval same as direct keys /
	if (pdata->enable_rotary0) {
		mask |= 0x03;
		direct_key_num = 2;
		kpc |= KPC_REE0;
	}

	if (pdata->enable_rotary1) {
		mask |= 0x0c;
		direct_key_num = 4;
		kpc |= KPC_REE1;
	}

	if (pdata->direct_key_num > direct_key_num)
		direct_key_num = pdata->direct_key_num;

	AD_button->direct_key_mask = ((2 << direct_key_num) - 1) & ~mask;

	// enable direct key 
	if (direct_key_num)
		kpc |= KPC_DE | KPC_DIE | KPC_DKN(direct_key_num);

	AD_button_writel(KPC, kpc | KPC_RE_ZERO_DEB);
	AD_button_writel(KPREC, DEFAULT_KPREC);
	AD_button_writel(KPKDI, pdata->debounce_interval);
*/	
}

static int rk28_AD_button_resume(struct platform_device *pdev)
{
	struct rk28_AD_button *AD_button = platform_get_drvdata(pdev);
	struct input_dev *input_dev = AD_button->input_dev;

	mutex_lock(&input_dev->mutex);

	if (input_dev->users) {
		/* Enable unit clock */
		clk_enable(AD_button->clk);
		rk28_AD_button_config(AD_button);
	}

	mutex_unlock(&input_dev->mutex);

	return 0;
}
#else
#define rk28_AD_button_suspend	NULL
#define rk28_AD_button_resume	NULL
#endif



#endif
#define res_size(res)	((res)->end - (res)->start + 1)

int keydata=58;
static  int ADSampleTimes = 0;
#if 0//test gpioF5 for WIFI power
static bool gpioflag=0;
#include <asm/arch/gpio.h>
#include <asm/arch/iomux.h>
#endif
static void rk28_adkeyscan_timer(unsigned long data)
{
	unsigned int ADKEY1,code = 0,i;
	/*Enable  AD controller to sample */
	prockAD_button->timer.expires  = jiffies+msecs_to_jiffies(10);
	add_timer(&prockAD_button->timer);
	RockAdcScanning();
	if (ADSampleTimes < 4)
	{
		ADSampleTimes ++;		
		goto scan_io_key;  	/* scan gpio button event*/
	}
	ADSampleTimes = 0;	
#if 0
	rockchip_mux_api_set(GPIOF5_APWM3_DPWM3_NAME, IOMUXB_GPIO1_B5);
	GPIOSetPinDirection(GPIOPortF_Pin5, GPIO_OUT);
	if (gpioflag)
	{
		GPIOSetPinLevel(GPIOPortF_Pin5, GPIO_HIGH);
		gpioflag = 0;
	}
	else
	{
		GPIOSetPinLevel(GPIOPortF_Pin5, GPIO_LOW);
		gpioflag = 1;
	}
#endif
	/*Get button value*/
	ADKEY1=get_rock_adc1();
	if(ADKEY1<EmptyADValue)
		printk("\n ADC1 value=%d  \n",ADKEY1);
#if 1	//ffhh
	if((ADKEY1>EmptyADValue)&&(ADKEY1<=InvalidADValue))  
		goto scan_io_key1;
#endif	
	valuecount++;
 	if(valuecount < 2)
		goto scan_code;	
	code=find_rock_adkeycode(ADKEY1,ad2valuetab);
	valuecount = 2;	
	goto scan_code;  ///scan_code;	
scan_io_key1:
	valuecount = 0;	

scan_code:	
	if((g_code == 0) && (code == 0)){
	    goto scan_io_key; 
	}
	///DBG("\n key button PE2 == %d  \n",GPIOGetPinLevel(GPIOPortE_Pin2)); 
	if(code != 0){
		if(valuecount<2)
			goto scan_io_key; 
		if(g_code == 0){
			g_code = code;
			DBG("\n %s::%d rock adc1 key scan ,find press down a key=%d  \n",__func__,__LINE__,g_code);
			input_report_key(prockAD_button->input_dev,g_code,1);
	        	input_sync(prockAD_button->input_dev);
	        	goto scan_io_key; 
		}else{
			if(g_code != code){
				DBG("\n %s::%d rock adc1 key scan ,find press up a key=%d  \n",__func__,__LINE__,g_code);
				input_report_key(prockAD_button->input_dev,g_code,0);
	            		input_sync(prockAD_button->input_dev);
	            		DBG("\n %s::%d rock adc1 key scan ,find press down a key=%d  \n",__func__,__LINE__,code);
	            		input_report_key(prockAD_button->input_dev,code,1);
	            		input_sync(prockAD_button->input_dev);
	            		g_code = code;
	           		goto scan_io_key; 
			}
		}
    
    	}
	if((g_code != 0)&&(code == 0)&&(ADSampleTimes == 0)){
		DBG("\n %s::%d rock adc1 key scan ,find press up a key=%d  \n",__func__,__LINE__,g_code);
	    	input_report_key(prockAD_button->input_dev,g_code,0);
	    	input_sync(prockAD_button->input_dev);
		valuecount = 0;
		g_code = 0;
		goto scan_io_key;
	}
scan_io_key :
//	if(!GPIOGetPinLevel(GPIOPortF_Pin0))
    if(g_wake_press)
    {
        pwrscantimes = 0;
        g_wake_press = !GPIOGetPinLevel(WAKEUP_KEY_PORT);
        if(g_wake_press) return;
    }
	if(!GPIOGetPinLevel(GPIOPortE_Pin2))
	{
		pwrscantimes += 1;
		if(pwrscantimes == (SEC_NUM * ONESEC_TIMES))
		{
			input_report_key(prockAD_button->input_dev,ENDCALL,1);
			input_sync(prockAD_button->input_dev);
			DBG("the kernel come to power down!!!\n");
		
		}
		if(pwrscantimes ==( (SEC_NUM + 1)* ONESEC_TIMES))
		{
			pwrscantimes = 0;
			input_report_key(prockAD_button->input_dev,ENDCALL,0);
			input_sync(prockAD_button->input_dev);
			DBG("the kernel come to power up!!!\n");
		
		}
		return ;
	}
	
	if( pwrscantimes > SLEEP_TIME)
	{
		pwrscantimes = 0;
        printk("========== %s: g_enable_sleep=%d\n", __FUNCTION__, g_enable_sleep);
		if(g_enable_sleep)
		{
			input_report_key(prockAD_button->input_dev,AD1KEY5,1);
			input_sync(prockAD_button->input_dev);
			input_report_key(prockAD_button->input_dev,AD1KEY5,0);
			input_sync(prockAD_button->input_dev);
			printk("\n%s^^level 1^^Wake or sleep ^^^^^!!\n",__FUNCTION__);
	}
}
}

/*
 * 20100309,HSL@RK,send a key to light the screen.
 * (like when insert use to wake up system from deep sleep)
 * the key value should matain unchange!!
 */
void rk28_send_wakeup_key( void ) 
{
        input_report_key(prockAD_button->input_dev,62,1);
        input_sync(prockAD_button->input_dev);
        input_report_key(prockAD_button->input_dev,62,0);
        input_sync(prockAD_button->input_dev);
}

static void rk28_enable_wakeup_key(void *dev_id)
{
    g_wakeup_key_enable = 1;
    del_timer(&g_wakeup_key_timer);
}

//a@nzy
static irqreturn_t rk28_AD_irq_handler(s32 irq, void *dev_id)
{
	rk28printk("\n =================================rk28_AD_irq_handler===================================\n");

    rk28printk("========= pm_sleep_status=%d   PA0=%d\n", pm_sleep_status, GPIOGetPinLevel(WAKEUP_KEY_PORT));
    
   if( pm_sleep_status == 1)
    {
        g_wake_press = !GPIOGetPinLevel(WAKEUP_KEY_PORT);
        rk28printk("========== in sleep: g_wakeup_key_enable=%d  g_wake_press=%d\n", g_wakeup_key_enable, g_wake_press);
        if(g_wakeup_key_enable)
        {
        	input_report_key(prockAD_button->input_dev,AD1KEY5,1);
        	input_sync(prockAD_button->input_dev);
        	input_report_key(prockAD_button->input_dev,AD1KEY5,0);
        	input_sync(prockAD_button->input_dev);
        	
        	printk("\n%s^^level 2^^Wake Up ^^^^^!!\n",__FUNCTION__);

            g_wakeup_key_enable = 0;

            // cmy@20100326: 只有在5S后，或者系统被唤醒后(pm_sleep_status == 1)，才能再处理唤醒IRQ
        	setup_timer(&g_wakeup_key_timer, rk28_enable_wakeup_key, (unsigned long)0);
        	g_wakeup_key_timer.expires  = jiffies + (5*HZ);
        	add_timer(&g_wakeup_key_timer);
        }
    }
    return IRQ_HANDLED;
}

static int __devinit rk28_AD_button_probe(struct platform_device *pdev)
{
	struct rk28_AD_button *AD_button;
	struct input_dev *input_dev;
	int  error,i;
	AD_button = kzalloc(sizeof(struct rk28_AD_button), GFP_KERNEL);
	/* Create and register the input driver. */
	input_dev = input_allocate_device();
	if (!input_dev || !AD_button) {
		dev_err(&pdev->dev, "failed to allocate input device\n");
		error = -ENOMEM;
		goto failed1;
	}

    //a@nzy
	int ret = request_irq(IRQ_NR_ADC, rk28_AD_irq_handler, 0, "ADC", NULL);
	if (ret < 0) {
		printk(KERN_CRIT "Can't register IRQ for ADC\n");
		return ret;
	}
	
	memcpy(AD_button->keycodes, initkey_code, sizeof(AD_button->keycodes));
	input_dev->name = pdev->name;
	input_dev->open = rk28_AD_button_open;
	input_dev->close = rk28_AD_button_close;
	input_dev->dev.parent = &pdev->dev;
	input_dev->phys = KEY_PHYS_NAME;
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0001;
	input_dev->id.version = 0x0100;
	input_dev->keycode = AD_button->keycodes;
	input_dev->keycodesize = sizeof(unsigned char);
	input_dev->keycodemax = ARRAY_SIZE(initkey_code);
	for (i = 0; i < ARRAY_SIZE(initkey_code); i++)
		set_bit(initkey_code[i], input_dev->keybit);
	clear_bit(0, input_dev->keybit);

	AD_button->input_dev = input_dev;
	input_set_drvdata(input_dev, AD_button);

	input_dev->evbit[0] = BIT_MASK(EV_KEY) ;

	platform_set_drvdata(pdev, AD_button);

	ADCInit();
	prockAD_button=AD_button;

	/* Register the input device */
	error = input_register_device(input_dev);
	if (error) {
		dev_err(&pdev->dev, "failed to register input device\n");
		goto failed2;
	}

	setup_timer(&AD_button->timer, rk28_adkeyscan_timer, (unsigned long)AD_button);
	//mod_timer(&AD_button->timer, 100);
	AD_button->timer.expires  = jiffies + 3;
	add_timer(&AD_button->timer);
#if 0//def CONFIG_MACH_RK2808SDK
        error = request_gpio_irq(WAKEUP_KEY_PORT,rk28_AD_irq_handler,GPIOEdgelFalling,NULL);
	if(error)
	{
		printk("unable to request recover key IRQ\n");
		goto failed2;
	}	
#endif        

#if 1//def CONFIG_MACH_PWS700AA
    error = request_gpio_irq(GPIOPortE_Pin2,rk28_AD_irq_handler,GPIOEdgelRising,NULL);
    GPIOPullUpDown(GPIOPortE_Pin2,GPIOPullDown);    
	if(error)
	{
		printk("unable to request recover key IRQ\n");
		goto failed2;
	}	
#endif        

	return 0;

failed2:
	input_unregister_device(AD_button->input_dev);
	platform_set_drvdata(pdev, NULL);	
failed1:
	input_free_device(input_dev);
	kfree(AD_button);
	return error;
}

static int __devexit rk28_AD_button_remove(struct platform_device *pdev)
{
	struct rk28_AD_button *AD_button = platform_get_drvdata(pdev);

	input_unregister_device(AD_button->input_dev);
	input_free_device(AD_button->input_dev);
	kfree(AD_button);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver rk28_AD_button_driver = {
	.probe		= rk28_AD_button_probe,
	.remove 	= __devexit_p(rk28_AD_button_remove),
	.driver 	= {
		.name	= "rk28_AD_button",
		.owner	= THIS_MODULE,
	},
 //   .suspend    = rk28_AD_button_suspend,
  //  .resume     = rk28_AD_button_resume,
};

 int __init rk28_AD_button_init(void)
{
	return platform_driver_register(&rk28_AD_button_driver);
}

static void __exit rk28_AD_button_exit(void)
{
	platform_driver_unregister(&rk28_AD_button_driver);
}

fs_initcall(rk28_AD_button_init);
module_exit(rk28_AD_button_exit);

MODULE_DESCRIPTION("rk28 AD button Controller Driver");
MODULE_LICENSE("GPL");
