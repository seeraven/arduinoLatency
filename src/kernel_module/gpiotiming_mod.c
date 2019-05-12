#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>

MODULE_LICENSE("GPL");

#if defined(ON_RASPBERRY_PI)
  #define GPIO_PIN_INTTEST_IN 26
  #define GPIO_PIN_ARDUINO_IN 4
#elif defined(ON_ODROID_XU4)
  #define GPIO_PIN_INTTEST_IN 22
  #define GPIO_PIN_ARDUINO_IN 33
#else
  #error "No platform specified via define ON_RASPBERRY_PI or ON_ODROID_XU4"
#endif


static volatile u32 inttest_counter_ui = 0;
static volatile u64 inttest_timestamp_ui = 0;

static volatile u32 arduino_counter_ui = 0;
static volatile u64 arduino_timestamp_ui = 0;


static void update_inttest_timestamp(void) {
  inttest_timestamp_ui = ktime_get_raw_ns();
}

static void update_arduino_timestamp(void) {
  arduino_timestamp_ui = ktime_get_raw_ns();
}


static irqreturn_t gpiotiming_inttest_irq_handler(int f_irq_ui,
						  void* f_dev_id_p) {
  inttest_timestamp_ui = ktime_get_raw_ns();
  ++inttest_counter_ui;
  return IRQ_HANDLED;
}

static irqreturn_t gpiotiming_arduino_irq_handler(int f_irq_ui,
						  void* f_dev_id_p) {
  arduino_timestamp_ui = ktime_get_raw_ns();
  ++arduino_counter_ui;
  return IRQ_HANDLED;
}


static const int gpio_inttest_pin_i = GPIO_PIN_INTTEST_IN;
static int irq_on_inttest_gpio_i = -1;

static const int gpio_arduino_pin_i = GPIO_PIN_ARDUINO_IN;
static int irq_on_arduino_gpio_i = -1;


void gpiotiming_gpio_init(void) {
  int rc;
  
  printk(KERN_INFO "GPIOTiming: Initializing GPIO interface...\n");
  rc = gpio_request(gpio_inttest_pin_i, "gpiotiming inttest pin");
  if (rc < 0) {
    printk(KERN_ERR "GPIOTiming: gpio_request of GPIO %d for inttest failed with error %d\n",
	   gpio_inttest_pin_i, rc);
    return;
  }

  rc = gpio_direction_input(gpio_inttest_pin_i);
  if (rc < 0) {
    printk(KERN_ERR "GPIOTiming: gpio_direction_input of GPIO %d for inttest failed with error %d\n",
	   gpio_inttest_pin_i, rc);
    gpio_free(gpio_inttest_pin_i);
    return;
  }

  irq_on_inttest_gpio_i = gpio_to_irq(gpio_inttest_pin_i);
  if (irq_on_inttest_gpio_i < 0) {
    printk(KERN_ERR "GPIOTiming: gpio_to_irq of GPIO %d for inttest failed with error %d\n",
	   gpio_inttest_pin_i, rc);
    gpio_free(gpio_inttest_pin_i);
    return;
  }

  rc = request_irq(irq_on_inttest_gpio_i, &gpiotiming_inttest_irq_handler, IRQF_TRIGGER_FALLING, "gpiotiming", NULL);
  if (rc < 0) {
    printk(KERN_ERR "GPIOTiming: request_irq of GPIO %d for inttest failed with error %d\n",
	   gpio_inttest_pin_i, rc);
    gpio_free(gpio_inttest_pin_i);
    return;
  }
  
  printk(KERN_INFO "GPIOTiming: Installed GPIO interrupt %d for GPIO %d for inttest...\n", irq_on_inttest_gpio_i, gpio_inttest_pin_i);

  
  rc = gpio_request(gpio_arduino_pin_i, "gpiotiming arduino pin");
  if (rc < 0) {
    printk(KERN_ERR "GPIOTiming: gpio_request of GPIO %d for arduino failed with error %d\n",
	   gpio_arduino_pin_i, rc);
    return;
  }

  rc = gpio_direction_input(gpio_arduino_pin_i);
  if (rc < 0) {
    printk(KERN_ERR "GPIOTiming: gpio_direction_input of GPIO %d for arduino failed with error %d\n",
	   gpio_arduino_pin_i, rc);
    gpio_free(gpio_arduino_pin_i);
    return;
  }

  irq_on_arduino_gpio_i = gpio_to_irq(gpio_arduino_pin_i);
  if (irq_on_arduino_gpio_i < 0) {
    printk(KERN_ERR "GPIOTiming: gpio_to_irq of GPIO %d for arduino failed with error %d\n",
	   gpio_arduino_pin_i, rc);
    gpio_free(gpio_arduino_pin_i);
    return;
  }

  rc = request_irq(irq_on_arduino_gpio_i, &gpiotiming_arduino_irq_handler, IRQF_TRIGGER_FALLING, "gpiotiming", NULL);
  if (rc < 0) {
    printk(KERN_ERR "GPIOTiming: request_irq of GPIO %d for arduino failed with error %d\n",
	   gpio_arduino_pin_i, rc);
    gpio_free(gpio_arduino_pin_i);
    return;
  }
  
  printk(KERN_INFO "GPIOTiming: Installed GPIO interrupt %d for GPIO %d for arduino...\n", irq_on_arduino_gpio_i, gpio_arduino_pin_i);
}

void gpiotiming_gpio_exit(void) {
  printk(KERN_INFO "GPIOTiming: Removing GPIO interrupt %d for GPIO %d...\n", irq_on_inttest_gpio_i, gpio_inttest_pin_i);
  if (irq_on_inttest_gpio_i >= 0)
    free_irq(irq_on_inttest_gpio_i, NULL);
  if (irq_on_arduino_gpio_i >= 0)
    free_irq(irq_on_arduino_gpio_i, NULL);
  gpio_free(gpio_inttest_pin_i);
  gpio_free(gpio_arduino_pin_i);
}


static ssize_t inttest_counter_show(struct kobject* f_kobj_p,
				    struct kobj_attribute* f_attribute_p,
				    char* f_buffer_p) {
  return sprintf(f_buffer_p, "%u", inttest_counter_ui);
}

static ssize_t inttest_timestamp_show(struct kobject* f_kobj_p,
				      struct kobj_attribute* f_attribute_p,
				      char* f_buffer_p) {
  return sprintf(f_buffer_p, "%llu", inttest_timestamp_ui);
}

static ssize_t arduino_counter_show(struct kobject* f_kobj_p,
				    struct kobj_attribute* f_attribute_p,
				    char* f_buffer_p) {
  return sprintf(f_buffer_p, "%u", arduino_counter_ui);
}

static ssize_t arduino_timestamp_show(struct kobject* f_kobj_p,
				      struct kobj_attribute* f_attribute_p,
				      char* f_buffer_p) {
  return sprintf(f_buffer_p, "%llu", arduino_timestamp_ui);
}

static struct kobject* gpiotiming_kobject;
static struct kobj_attribute sysfs_inttest_counter_attr = __ATTR(inttest_counter, 0444, inttest_counter_show, NULL);
static struct kobj_attribute sysfs_inttest_timestamp_attr = __ATTR(inttest_timestamp_ns, 0444, inttest_timestamp_show, NULL);
static struct kobj_attribute sysfs_arduino_counter_attr = __ATTR(arduino_counter, 0444, arduino_counter_show, NULL);
static struct kobj_attribute sysfs_arduino_timestamp_attr = __ATTR(arduino_timestamp_ns, 0444, arduino_timestamp_show, NULL);

void gpiotiming_sysfs_init(void) {
  printk(KERN_INFO "GPIOTiming: Initializing SysFS entries...\n");
  // Create a directory /sys/gpiotiming...
  gpiotiming_kobject = kobject_create_and_add("gpiotiming", NULL);

  // Create the inttest_counter file
  if (sysfs_create_file(gpiotiming_kobject, &sysfs_inttest_counter_attr.attr)) {
    pr_debug("Failed to create gpiotiming sysfs file /sys/gpiotiming/inttest_counter!\n");
  }

  // Create the inttest_timestamp_ns file
  if (sysfs_create_file(gpiotiming_kobject, &sysfs_inttest_timestamp_attr.attr)) {
    pr_debug("Failed to create gpiotiming sysfs file /sys/gpiotiming/inttest_timestamp_ns!\n");
  }
  
  // Create the arduino_counter file
  if (sysfs_create_file(gpiotiming_kobject, &sysfs_arduino_counter_attr.attr)) {
    pr_debug("Failed to create gpiotiming sysfs file /sys/gpiotiming/arduino_counter!\n");
  }

  // Create the arduino_timestamp_ns file
  if (sysfs_create_file(gpiotiming_kobject, &sysfs_arduino_timestamp_attr.attr)) {
    pr_debug("Failed to create gpiotiming sysfs file /sys/gpiotiming/arduino_timestamp_ns!\n");
  }
  
  printk(KERN_INFO "GPIOTiming: SysFS entries initialized under /sys/gpiotiming...\n");
}

void gpiotiming_sysfs_exit(void) {
  kobject_put(gpiotiming_kobject);
}


static int __init gpiotiming_init(void){
  printk(KERN_INFO "GPIOTiming: starting...\n");
  gpiotiming_sysfs_init();
  inttest_counter_ui = 0;
  update_inttest_timestamp();
  arduino_counter_ui = 0;
  update_arduino_timestamp();
  gpiotiming_gpio_init();
  printk(KERN_INFO "GPIOTiming: started.\n");
  return 0;
}


static void __exit gpiotiming_exit(void){
  printk(KERN_INFO "GPIOTiming: stopping...\n");
  gpiotiming_sysfs_exit();
  gpiotiming_gpio_exit();
  printk(KERN_INFO "GPIOTiming: stopped.\n");
}

module_init(gpiotiming_init);
module_exit(gpiotiming_exit);
