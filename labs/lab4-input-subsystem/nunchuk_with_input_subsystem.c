// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/input.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nick");
MODULE_DESCRIPTION("nunchuk I2C input driver for Jetson Orin Nano");

/* TODO 1: Define a private structure to link the I2C client
 * and the input device. The polling function receives an
 * input_dev, but needs to reach the i2c_client to read
 * registers. This struct bridges that gap. */
struct nunchuk_dev {
    struct i2c_client *client;
};

/* Reuse nunchuk_read_registers and nunchuk_init from Lab 3 unchanged */
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

/* TODO 2: Implement the polling function.
 * Prototype hint: look at input_setup_polling() to find
 * what signature this function needs.
 *
 * Inside this function:
 * - Retrieve your nunchuk_dev from the input_dev (hint: input_get_drvdata)
 * - Read the nunchuk registers
 * - Extract Z and C button states from buf[5]
 * - Report them (hint: input_report_key with BTN_Z, BTN_C)
 * - Don't forget to call input_sync() at the end
 */
void nunchuk_poll(struct input_dev *dev)
{
    u8 buf[6];
    int ret;
    int z_pressed, c_pressed;
    struct nunchuk_dev *nunchuk = input_get_drvdata(dev);
    struct i2c_client *client = nunchuk->client;

    ret = nunchuk_read_registers(client, buf);
    if (ret) {
        dev_err(&client->dev, "failed to read nunchuk\n");
        return;
    }

    z_pressed = !(buf[5] & 0x01);
    c_pressed = !(buf[5] & 0x02);

    input_report_key(dev, BTN_Z, z_pressed);
    input_report_key(dev, BTN_C, c_pressed);

    /* BONUS TODO 1: Report joystick X and Y axes.
     * The data is already in buf — check the Nunchuck data format
     * to find which bytes hold X and Y.
     * Hint: the function name is similar to input_report_key
     * but for absolute axes. Use ABS_X and ABS_Y as codes. */
    input_report_abs(dev, ABS_X, buf[0]);
    input_report_abs(dev, ABS_Y, buf[1]);

    input_sync(dev);
}

static int nunchuk_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    struct nunchuk_dev *nunchuk;
    struct input_dev *input;
    int ret;

    ret = nunchuk_init(client);
    if (ret)
        return ret;

    /* TODO 3: Allocate the private structure with devm_kzalloc
     * and store the i2c_client pointer in it */
    nunchuk = devm_kzalloc(&client->dev, sizeof(*nunchuk), GFP_KERNEL);
    if (!nunchuk)
        return -ENOMEM;
    nunchuk->client = client;

    /* TODO 4: Allocate an input device with devm_input_allocate_device */
    input = devm_input_allocate_device(&client->dev);
    if (!input)
        return -ENOMEM;

    /* TODO 5: Set the input device name, bus type,
     * and declare which event types and key codes
     * this device can generate (EV_KEY, BTN_Z, BTN_C) */
    input->name = "nunchuk";
    input->id.bustype = BUS_I2C;
    set_bit(EV_KEY, input->evbit);
    set_bit(BTN_Z, input->keybit);
    set_bit(BTN_C, input->keybit);

    /* BONUS TODO 2: Declare that this device also generates
     * absolute axis events.
     * - Add EV_ABS to evbit
     * - Add ABS_X and ABS_Y to absbit
     * - Configure axis ranges with input_set_abs_params()
     *   Look up the prototype:
     *     rg -F "input_set_abs_params" include/linux/input.h
     *   Nunchuck X range: 30–220, Y range: 40–200
     *   Use fuzz=4, flat=8 */
    set_bit(EV_ABS, input->evbit);
    input_set_abs_params(input, ABS_X, 0, 255, 4, 8);
    input_set_abs_params(input, ABS_Y, 0, 255, 4, 8);

    /* BONUS TODO 3: For the nunchuk to be recognized as a joystick
     * by applications, declare classic gamepad buttons.
     * These don't need to actually do anything — they just tell
     * the input subsystem this is a joystick-class device.
     * Hint: BTN_TL, BTN_SELECT, BTN_MODE, BTN_START, BTN_TR,
     *        BTN_TL2, BTN_B, BTN_Y, BTN_A, BTN_X, BTN_TR2
     * Also check: zcat /proc/config.gz | grep JOYDEV */
    set_bit(BTN_TL, input->keybit);
    set_bit(BTN_SELECT, input->keybit);
    set_bit(BTN_MODE, input->keybit);
    set_bit(BTN_START, input->keybit);
    set_bit(BTN_TR, input->keybit);
    set_bit(BTN_TL2, input->keybit);
    set_bit(BTN_B, input->keybit);
    set_bit(BTN_Y, input->keybit);
    set_bit(BTN_A, input->keybit);
    set_bit(BTN_X, input->keybit);
    set_bit(BTN_TR2, input->keybit);

    /* TODO 6: Link the private structure to the input device
     * so the polling function can find it (hint: input_set_drvdata) */
    input_set_drvdata(input, nunchuk);

    /* TODO 7: Set up polling with your poll function,
     * set the polling interval to 50ms */
    input_setup_polling(input, nunchuk_poll);
    input_set_poll_interval(input, 50);

    /* TODO 8: Register the input device */
    ret = input_register_device(input);
    if (ret) {
            dev_err(&client->dev, "failed to register input device\n");
            return ret;
    }

    return 0;
}

/* The rest (remove, of_match, i2c_driver, module_i2c_driver)
 * stays the same as Lab 3 */
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