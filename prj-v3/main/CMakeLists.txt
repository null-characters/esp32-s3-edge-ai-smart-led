idf_component_register(
    SRCS "src/main.c"
         "src/drivers/led_pwm.c"
         "src/drivers/uart_driver.c"
         "src/drivers/i2s_driver.c"
         "src/network/wifi_sta.c"
         "src/multimodal/tflm_allocator.c"
    INCLUDE_DIRS "include"
                 "src/drivers"
                 "src/network"
                 "src/multimodal"
)
