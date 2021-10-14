#include <application.h>

#define SENSOR_DATA_STREAM_SAMPLES 8
#define SENSOR_UPDATE_INTERVAL (1 * 60 * 1000)
#define SIGFOX_REPORT_INTERVAL (15 * 60 * 1000)

twr_data_stream_t sm_thermometer;
TWR_DATA_STREAM_FLOAT_BUFFER(sm_thermometer_buffer, SENSOR_DATA_STREAM_SAMPLES)

twr_data_stream_t sm_voltage;
TWR_DATA_STREAM_FLOAT_BUFFER(sm_voltage_buffer, SENSOR_DATA_STREAM_SAMPLES)

twr_led_t led;
twr_button_t button;
twr_tmp112_t tmp112;
twr_module_pir_t pir;
twr_module_sigfox_t sigfox;

uint16_t motion_count;

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_log_debug("APP: Button event");

        twr_scheduler_plan_now(0);
    }
}

void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        twr_log_debug("APP: Thermometer update event");

        float temperature;

        if (twr_tmp112_get_temperature_celsius(&tmp112, &temperature))
        {
            twr_data_stream_feed(&sm_thermometer, &temperature);
        }
        else
        {
            twr_data_stream_reset(&sm_thermometer);
        }
    }
    else if (event == TWR_TMP112_EVENT_ERROR)
    {
        twr_log_error("APP: Thermometer error event");

        twr_data_stream_reset(&sm_thermometer);
    }
}

void pir_event_handler(twr_module_pir_t *self, twr_module_pir_event_t event, void *event_param)
{
    if (event == TWR_MODULE_PIR_EVENT_MOTION)
    {
        twr_log_debug("APP: Motion event");

        static twr_tick_t timeout;

        twr_tick_t now = twr_tick_get();

        if (now < timeout)
        {
            return;
        }

        timeout = now + 2000;

        if (motion_count < 0xffff)
        {
            motion_count++;
        }
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        twr_log_debug("APP: Battery update event");

        float voltage;

        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_data_stream_feed(&sm_voltage, &voltage);
        }
    }
    else if (event == TWR_MODULE_BATTERY_EVENT_ERROR)
    {
        twr_log_error("APP: Battery error event");

        twr_data_stream_reset(&sm_voltage);
    }
}

void sigfox_event_handler(twr_module_sigfox_t *self, twr_module_sigfox_event_t event, void *param)
{
    if (event == TWR_MODULE_SIGFOX_EVENT_SEND_RF_FRAME_START)
    {
        twr_log_info("APP: Sigfox transmission started event");
    }
    else if (event == TWR_MODULE_SIGFOX_EVENT_SEND_RF_FRAME_DONE)
    {
        twr_log_info("APP: Sigfox transmission finished event");
    }
    if (event == TWR_MODULE_SIGFOX_EVENT_ERROR)
    {
        twr_log_error("APP: Sigfox error event");
    }
}

void application_init(void)
{
    twr_log_init(TWR_LOG_LEVEL_ERROR, TWR_LOG_TIMESTAMP_ABS);

    twr_data_stream_init(&sm_thermometer, SENSOR_DATA_STREAM_SAMPLES, &sm_thermometer_buffer);
    twr_data_stream_init(&sm_voltage, SENSOR_DATA_STREAM_SAMPLES, &sm_voltage_buffer);

    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_pulse(&led, 1000);

    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, NULL);

    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, NULL);
    twr_tmp112_set_update_interval(&tmp112, SENSOR_UPDATE_INTERVAL);

    twr_module_pir_init(&pir);
    twr_module_pir_set_sensitivity(&pir, TWR_MODULE_PIR_SENSITIVITY_MEDIUM);
    twr_module_pir_set_event_handler(&pir, pir_event_handler, NULL);

    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(SENSOR_UPDATE_INTERVAL);

    twr_module_sigfox_init(&sigfox, TWR_MODULE_SIGFOX_REVISION_R2);
    twr_module_sigfox_set_event_handler(&sigfox, sigfox_event_handler, NULL);

    twr_scheduler_plan_absolute(0, 10 * 1000);

    twr_log_info("APP: Initialization finished");
}

void application_task(void)
{
    twr_log_info("APP: Periodic task started");

    static uint8_t buffer[5];

    float average;

    if (twr_data_stream_get_average(&sm_voltage, &average))
    {
        buffer[0] = ceil(average * 10);
    }
    else
    {
        buffer[0] = 0xff;
    }

    if (twr_data_stream_get_average(&sm_thermometer, &average))
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

    if (twr_module_sigfox_send_rf_frame(&sigfox, buffer, sizeof(buffer)))
    {
        motion_count = 0;

        twr_led_pulse(&led, 5000);

        twr_scheduler_plan_current_relative(SIGFOX_REPORT_INTERVAL);
    }
    else
    {
        twr_log_warning("APP: Sigfox transmission deferred");

        twr_scheduler_plan_current_relative(1000);
    }
}
