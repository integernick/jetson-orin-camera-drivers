// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick");
MODULE_DESCRIPTION("Nunchuck I2C driver for Jetson Orin Nano");

static int nunchuk_read_registers(struct i2c_client *client, u8 *buf)
{
    u8 reg = 0x00;
    int ret;

    usleep_range(10000, 20000);

    ret = i2c_master_send(client, &reg, 1);
    if (ret != 1) {
        dev_err(&client->dev, "i2c send failed: %d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    usleep_range(10000, 20000);

    ret = i2c_master_recv(client, buf, 6);
    if (ret != 6) {
        dev_err(&client->dev, "i2c recv failed: %d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    return 0;
}

static int nunchuk_init(struct i2c_client *client)
{
    u8 buf[2];
    int ret;

    buf[0] = 0xf0;
    buf[1] = 0x55;
    ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        dev_err(&client->dev, "init send 1 failed: %d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    usleep_range(1000, 2000);

    buf[0] = 0xfb;
    buf[1] = 0x00;
    ret = i2c_master_send(client, buf, 2);
    if (ret != 2) {
        dev_err(&client->dev, "init send 2 failed: %d\n", ret);
        return ret < 0 ? ret : -EIO;
    }

    return 0;
}

static int nunchuk_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    u8 buf[6];
    int ret;
    int z_pressed, c_pressed;

    ret = nunchuk_init(client);
    if (ret) {
        dev_err(&client->dev, "failed to init nunchuk\n");
        return ret;
    }

    ret = nunchuk_read_registers(client, buf);
    if (ret) {
        dev_err(&client->dev, "failed to read nunchuk\n");
        return ret;
    }

    z_pressed = !(buf[5] & 0x01);
    c_pressed = !(buf[5] & 0x02);

    dev_info(&client->dev,
             "joystick: x=%d y=%d | buttons: z=%d c=%d\n",
             buf[0], buf[1], z_pressed, c_pressed);

    return 0;
}

static int nunchuk_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "nunchuk removed\n");
    return 0;
}

static const struct of_device_id nunchuk_of_match[] = {
    { .compatible = "nintendo,nunchuk" },
    { },
};
MODULE_DEVICE_TABLE(of, nunchuk_of_match);

static struct i2c_driver nunchuk_driver = {
    .driver = {
        .name = "nunchuk",
        .of_match_table = nunchuk_of_match,
    },
    .probe = nunchuk_probe,
    .remove = nunchuk_remove,
};
module_i2c_driver(nunchuk_driver);
