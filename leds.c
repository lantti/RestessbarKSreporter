#include <stdlib.h>
#include <stdio.h>
#include "vmtype.h"
#include "vmtimer.h"
#include "vmsystem.h"
#include "vmdcl.h"
#include "vmdcl_gpio.h"
#include "leds.h"

VM_DCL_HANDLE gpio_red_handle = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_green_handle = VM_DCL_HANDLE_INVALID;
VM_DCL_HANDLE gpio_blue_handle = VM_DCL_HANDLE_INVALID;

static void blink_cb(VM_TIMER_ID_NON_PRECISE timer_id, void* user_data)
{
	green_led_off();
	vm_timer_delete_non_precise(timer_id);	
}

void init_leds()
{
	gpio_red_handle = vm_dcl_open(VM_DCL_GPIO, REDLED);
	if (gpio_red_handle != VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
		vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
		vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
	}

	gpio_green_handle = vm_dcl_open(VM_DCL_GPIO, GREENLED);
	if (gpio_green_handle != VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
		vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
		vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
	}

	gpio_blue_handle = vm_dcl_open(VM_DCL_GPIO, BLUELED);
	if (gpio_blue_handle != VM_DCL_HANDLE_INVALID)
	{
		vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_SET_MODE_0, NULL);
		vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_SET_DIRECTION_OUT, NULL);
		vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
	}
}

void free_leds()
{
	vm_dcl_close(gpio_red_handle);
	vm_dcl_close(gpio_green_handle);
	vm_dcl_close(gpio_blue_handle);
}

void red_led_on()
{
	vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
}

void red_led_off()
{
	vm_dcl_control(gpio_red_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
}

void green_led_on()
{
	vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
}

void green_led_off()
{
	vm_dcl_control(gpio_green_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
}

void blue_led_on()
{
	vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_WRITE_LOW, NULL);
}

void blue_led_off()
{
	vm_dcl_control(gpio_blue_handle, VM_DCL_GPIO_COMMAND_WRITE_HIGH, NULL);
}

void blink_green()
{
	green_led_on();
	vm_timer_create_non_precise(1000, blink_cb, NULL);
}
