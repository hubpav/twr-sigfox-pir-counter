#include <application.h>

#define SENSOR_DATA_STREAM_SAMPLES 8
#define SENSOR_UPDATE_INTERVAL (1 * 60 * 1000)
#define SIGFOX_REPORT_INTERVAL (15 * 60 * 1000)

bc_data_stream_t sm_thermometer;
BC_DATA_STREAM_FLOAT_BUFFER(sm_thermometer_buffer, SENSOR_DATA_STREAM_SAMPLES)

bc_data_stream_t sm_voltage;
BC_DATA_STREAM_FLOAT_BUFFER(sm_voltage_buffer, SENSOR_DATA_STREAM_SAMPLES)

bc_led_t led;
bc_button_t button;
bc_tmp112_t tmp112;
bc_module_pir_t pir;
bc_module_sigfox_t sigfox;

uint16_t motion_count;

void button_event_handler(bc_button_t *self, bc_button_event_t event, void *event_param)
{
    if (event == BC_BUTTON_EVENT_PRESS)
    {
        bc_log_debug("APP: Button event");

        bc_scheduler_plan_now(0);
    }
}

void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    if (event == BC_TMP112_EVENT_UPDATE)
    {
        bc_log_debug("APP: Thermometer update event");

        float temperature;

        if (bc_tmp112_get_temperature_celsius(&tmp112, &temperature))
        {
            bc_data_stream_feed(&sm_thermometer, &temperature);
        }
        else
        {
            bc_data_stream_reset(&sm_thermometer);
        }
    }
    else if (event == BC_TMP112_EVENT_ERROR)
    {
        bc_log_error("APP: Thermometer error event");

        bc_data_stream_reset(&sm_thermometer);
    }
}

void pir_event_handler(bc_module_pir_t *self, bc_module_pir_event_t event, void *event_param)
{
    if (event == BC_MODULE_PIR_EVENT_MOTION)
    {
        bc_log_debug("APP: Motion event");

        if (motion_count < 0xffff)
        {
            motion_count++;
        }
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        bc_log_debug("APP: Battery update event");

        float voltage;

        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_data_stream_feed(&sm_voltage, &voltage);
        }
    }
    else if (event == BC_MODULE_BATTERY_EVENT_ERROR)
    {
        bc_log_error("APP: Battery error event");

        bc_data_stream_reset(&sm_voltage);
    }
}

void sigfox_event_handler(bc_module_sigfox_t *self, bc_module_sigfox_event_t event, void *param)
{
    if (event == BC_MODULE_SIGFOX_EVENT_SEND_RF_FRAME_START)
    {
        bc_log_info("APP: Sigfox transmission started event");
    }
    else if (event == BC_MODULE_SIGFOX_EVENT_SEND_RF_FRAME_DONE)
    {
        bc_log_info("APP: Sigfox transmission finished event");
    }
    if (event == BC_MODULE_SIGFOX_EVENT_ERROR)
    {
        bc_log_error("APP: Sigfox error event");
    }
}

void application_init(void)
{
    bc_log_init(BC_LOG_LEVEL_DEBUG, BC_LOG_TIMESTAMP_ABS);

    bc_data_stream_init(&sm_thermometer, SENSOR_DATA_STREAM_SAMPLES, &sm_thermometer_buffer);
    bc_data_stream_init(&sm_voltage, SENSOR_DATA_STREAM_SAMPLES, &sm_voltage_buffer);

    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_pulse(&led, 1000);

    bc_button_init(&button, BC_GPIO_BUTTON, BC_GPIO_PULL_DOWN, false);
    bc_button_set_event_handler(&button, button_event_handler, NULL);

    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    bc_tmp112_set_update_interval(&tmp112, SENSOR_UPDATE_INTERVAL);

    bc_module_pir_init(&pir);
    bc_module_pir_set_sensitivity(&pir, BC_MODULE_PIR_SENSITIVITY_MEDIUM);
    bc_module_pir_set_event_handler(&pir, pir_event_handler, NULL);

    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(SENSOR_UPDATE_INTERVAL);

    bc_module_sigfox_init(&sigfox, BC_MODULE_SIGFOX_REVISION_R2);
    bc_module_sigfox_set_event_handler(&sigfox, sigfox_event_handler, NULL);

    bc_scheduler_plan_absolute(0, 10 * 1000);

    bc_log_info("APP: Initialization finished");
}

void application_task(void)
{
    bc_log_info("APP: Periodic task started");

    static uint8_t buffer[5];

    float average;

    if (bc_data_stream_get_average(&sm_voltage, &average))
    {
        buffer[0] = ceil(average * 10);
    }
    else
    {
        buffer[0] = 0xff;
    }

    if (bc_data_stream_get_average(&sm_thermometer, &average))
    {
        int16_t a = average * 10;

        buffer[1] = a >> 8;
        buffer[2] = a;
    }
    else
    {
        buffer[1] = 0x7f;
        buffer[2] = 0xff;
    }

    buffer[3] = motion_count >> 8;
    buffer[4] = motion_count;

    if (bc_module_sigfox_send_rf_frame(&sigfox, buffer, sizeof(buffer)))
    {
        motion_count = 0;

        bc_led_pulse(&led, 5000);

        bc_scheduler_plan_current_relative(SIGFOX_REPORT_INTERVAL);
    }
    else
    {
        bc_log_warning("APP: Sigfox transmission deferred");

        bc_scheduler_plan_current_relative(1000);
    }
}
