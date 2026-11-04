
#include "axibram.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/sizes.h>

// THe number of axibram probe
static int probe_device_num = 0;

static int axibram_remove(struct platform_device *pdev);
static int axibram_probe(struct platform_device *pdev)
{
    int rc = 0;
    unsigned int dev_id = 0;
    struct axibram_device *dev = kzalloc(sizeof(struct axibram_device), GFP_KERNEL);

    if ( NULL == dev )
    {
		axibram_log_err("Unable to allocate the AXI bram device structure.\r\n");
		return -ENOMEM;
	}

    dev->pdev = pdev;
    // 强制告诉内核，硬件支持并在 1G 内存（32位地址）内寻址
    rc = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
    if ( rc )
    {
        axibram_log_err("Failed to set DMA mask 32 (%d).\n", rc);
    }

    dev_id = (unsigned int)of_device_get_match_data(&pdev->dev);
    axibram_log_info("insert axibram device id 0x%x.\n", dev_id - 0x100);

    // Assign the character device name
    sprintf(dev->chrdev_name, "%s%d", AXIBRAM_DEV_NAME, dev_id - 0x100);
    rc = axibram_chdrv_init(dev, probe_device_num);
    if ( rc < 0 )
    {
        kfree(dev);
        axibram_log_err("AXI bram probe is error (%d).\n", rc);
		return rc;
    } 

	++probe_device_num;
	dev_set_drvdata(&pdev->dev, dev);
	axibram_log_info("AXI bram probe is OK, name = %s.\n", dev->chrdev_name);

    axibram_chdrv_load_resouce(dev);
	if ( 0 == dev->resource.memCount )
	{
        axibram_log_info("AXI bram %s load memory resource failed.\n", dev->chrdev_name);
        axibram_remove(pdev);
		return -ENODEV;
	}
	
    return 0;
}

static int axibram_remove(struct platform_device *pdev)
{
    struct axibram_device *dev = dev_get_drvdata(&pdev->dev);
    probe_device_num = probe_device_num - 1;

    platform_set_drvdata(pdev, NULL);

    if ( dev )
    {
        axibram_log_info("Axibram %d Remove is OK\n", dev->dev_cnt);
        axibram_chdrv_release_resource(dev);
        axibram_chdrv_exit(dev, probe_device_num);
        kfree(dev);
    }

    return 0;
}

static void axibram_shutdown(struct platform_device *pdev)
{
}
static int axibram_suspend(struct device *dev)
{
	return 0;
}


static int axibram_resume(struct device *dev)
{
	return 0;
}

static struct dev_pm_ops axibram_pm_ops = {
    .suspend = axibram_suspend,
    .resume  = axibram_resume,
};

static struct of_device_id axibram_of_device_id_matchs[] =
{
	{
		.compatible = "xlnx,axibram-chrdev0",
		.data = (void*)0x100
	},
	{
		.compatible = "xlnx,axibram-chrdev1",
		.data = (void*)0x101
	},
	{
		.compatible = "xlnx,axibram-chrdev2",
		.data = (void*)0x102
	},
	{
		.compatible = "xlnx,axibram-chrdev3",
		.data = (void*)0x103
	},
	{
		.compatible = "xlnx,axibram-chrdev4",
		.data = (void*)0x104
	},
	{
		.compatible = "xlnx,axibram-chrdev5",
		.data = (void*)0x105
	},
	{
		.compatible = "xlnx,axibram-chrdev6",
		.data = (void*)0x106
	},
	{
		.compatible = "xlnx,axibram-chrdev7",
		.data = (void*)0x107
	},
	{
		.compatible = "xlnx,axibram-chrdev8",
		.data = (void*)0x108
	},
	{
		.compatible = "xlnx,axibram-chrdev9",
		.data = (void*)0x109
	},
	{
		.compatible = "xlnx,axibram-chrdev10",
		.data = (void*)0x10A
	},
	{
		.compatible = "xlnx,axibram-chrdev11",
		.data = (void*)0x10B
	},
	{
		.compatible = "xlnx,axibram-chrdev12",
		.data = (void*)0x10C
	},
	{
		.compatible = "xlnx,axibram-chrdev13",
		.data = (void*)0x10D
	},
	{
		.compatible = "xlnx,axibram-chrdev14",
		.data = (void*)0x10E
	},
	{
		.compatible = "xlnx,axibram-chrdev15",
		.data = (void*)0x10F
	},
	{
		.compatible = "xlnx,axibram-chrdev16",
		.data = (void*)0x110
	},
	{}
};

static struct platform_driver axibram_driver =
    {
        .driver =
            {
                .name = AXIBRAM_DEV_NAME,
                .owner = THIS_MODULE,
                .of_match_table = axibram_of_device_id_matchs,
                .pm = &axibram_pm_ops,
            },
        .probe = axibram_probe,
        .remove = axibram_remove,
        .shutdown = axibram_shutdown,
};

static int __init axibram_module_init(void)
{
    return platform_driver_register(&axibram_driver);
}

static void __exit axibram_module_cleanup(void)
{
    platform_driver_unregister(&axibram_driver);
}

// module_platform_driver(Axi4Lite_driver);
module_init(axibram_module_init);
module_exit(axibram_module_cleanup);

MODULE_VERSION("1.0");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("huafeng");
MODULE_DESCRIPTION("Module to provide a userspace interface for transferring "
				   "data from the processor to the logic fabric via AXI");
MODULE_ALIAS("IRQ linux driver");
// 模块所支持的设备
MODULE_DEVICE_TABLE(of, axibram_of_device_id_matchs);
