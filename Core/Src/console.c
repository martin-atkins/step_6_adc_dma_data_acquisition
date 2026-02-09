#include "adc_app.h"
#include "console.h"
#include "usart.h"
#include "dma.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define UART_RX_DMA_BUF_SIZE 128
#define LINE_BUF_SIZE 64

typedef void (*console_cmd_fn_t)(int argc, char *argv[]);

typedef struct {
	const char *name;
	console_cmd_fn_t fn;
	const char *help;
} console_cmd_t;

static void cmd_led(int argc, char *argv[]);
static void cmd_help(int argc, char *argv[]);
static void cmd_uptime(int argc, char *argv[]);
static void cmd_status(int argc, char *argv[]);
static void cmd_adc(int argc, char **argv);

static const console_cmd_t cmd_table[] =
{
	{ "help",   cmd_help, "   - show this help" },
	{ "status", cmd_status, " - system status" },
	{ "uptime", cmd_uptime, " - system uptime" },
	{ "led",    cmd_led, "    - led off|slow|fast" },
	{ "adc",    cmd_adc, "    - adc start|stop|volts|latest|avg|temp" },
};

#define CMD_COUNT (sizeof(cmd_table) / sizeof(cmd_table[0]))

static uint8_t uart_rx_dma_buf[UART_RX_DMA_BUF_SIZE];
static uint16_t dma_last_pos = 0;
volatile uint8_t rx_pending = 0;

static char line_buf[LINE_BUF_SIZE];
static uint8_t line_len = 0;

static void console_process_bytes(uint8_t *data, uint16_t len);

static void console_write(const char *s) {
	HAL_UART_Transmit(&huart2, (uint8_t*) s, strlen(s),
	HAL_MAX_DELAY);
}

void console_printf(const char *fmt, ...)
{
    char buf[128];   // keep this reasonable
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    console_write(buf);
}

static void console_prompt(void) {
	HAL_UART_Transmit(&huart2, (uint8_t*) "> ", 2,
	HAL_MAX_DELAY);
}

static void console_handle_command(char *cmd) {
	char *argv[8];
	int argc = 0;

	char *token = strtok(cmd, " ");
	while (token && argc < 8) {
		argv[argc++] = token;
		token = strtok(NULL, " ");
	}

	if (argc == 0)
		return;

	for (size_t i = 0; i < CMD_COUNT; i++) {
		if (strcmp(argv[0], cmd_table[i].name) == 0) {
			cmd_table[i].fn(argc, argv);
			return;
		}
	}

	console_write("unknown command\r\n");
	console_prompt();
}

void console_init(void) {
	HAL_UART_Receive_DMA(&huart2, uart_rx_dma_buf,
	UART_RX_DMA_BUF_SIZE);

	__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);

	console_write("ok\r\n");
	console_prompt();
}

void task_console(void) {
	if (!rx_pending)
		return;

	rx_pending = 0;

	uint16_t dma_pos =
	UART_RX_DMA_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart2.hdmarx);

	if (dma_pos != dma_last_pos) {
		if (dma_pos > dma_last_pos) {
			console_process_bytes(&uart_rx_dma_buf[dma_last_pos],
					dma_pos - dma_last_pos);
		} else {
			console_process_bytes(&uart_rx_dma_buf[dma_last_pos],
			UART_RX_DMA_BUF_SIZE - dma_last_pos);
			if (dma_pos > 0) {
				console_process_bytes(&uart_rx_dma_buf[0], dma_pos);
			}
		}
		dma_last_pos = dma_pos;
	}
}

static void console_process_bytes(uint8_t *data, uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		char c = data[i];

		/* ENTER */
		if (c == '\r' || c == '\n') {
			HAL_UART_Transmit(&huart2, (uint8_t*) "\r\n", 2,
			HAL_MAX_DELAY);

			if (line_len > 0) {
				line_buf[line_len] = '\0';
				console_handle_command(line_buf);
				line_len = 0;
			}
		}

		/* BACKSPACE */
		else if (c == 0x08 || c == 0x7F) {
			if (line_len > 0) {
				line_len--;

				/* erase character on terminal */
				HAL_UART_Transmit(&huart2, (uint8_t*) "\b \b", 3,
				HAL_MAX_DELAY);
			}
		}

		/* PRINTABLE CHARACTER */
		else if (c >= 0x20 && c <= 0x7E) {
			if (line_len < LINE_BUF_SIZE - 1) {
				line_buf[line_len++] = c;

				/* echo */
				HAL_UART_Transmit(&huart2, (uint8_t*) &c, 1,
				HAL_MAX_DELAY);
			} else {
				/* optional: bell or ignore */
			}
		}
	}
}

static void cmd_status(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    char buf[64];

    snprintf(
        buf,
        sizeof(buf),
        "led=%s uptime=%lu ms\r\n",
        led_mode_str(led_get_mode()),
        system_uptime_ms()
    );

    console_write(buf);

	console_write("ok\r\n");
	console_prompt();
}

static void cmd_adc(int argc, char **argv)
{
	if (argc < 2) {
        console_write("usage: adc start|stop|volts|latest|avg|temp\r\n");
        console_prompt();
		return;
	}

    if (!strcmp(argv[1], "start"))
    {
        adc_app_start();
        console_write("adc started\r\n");
    }
    else if (!strcmp(argv[1], "stop"))
    {
        adc_app_stop();
        console_write("adc stopped\r\n");
    }
    else if (!strcmp(argv[1], "volts"))
    {
        uint16_t raw = adc_app_average();
        uint32_t mv = adc_to_voltage(raw);

		console_printf("ADC volts=%lu mV\r\n", mv);
    }
    else if (!strcmp(argv[1], "latest"))
    {
		uint16_t raw = adc_read_avg(16);
		uint32_t mv = (raw * 3300UL) / 4095;

		console_printf("ADC latest=%u (raw) [%lu mV]\r\n", raw, mv);
    }
    else if (!strcmp(argv[1], "avg"))
    {
        uint16_t raw = adc_app_average();
        uint32_t mv = adc_to_voltage(raw);

		console_printf("ADC avg=%u  [%lu mV]\r\n", raw, mv);
    }
    else if (!strcmp(argv[1], "temp"))
    {
		uint16_t raw = adc_read_avg(16);

        float temp_c = thermistor_beta_to_celsius(raw);

        if (!isfinite(temp_c)) {
            console_printf("TEMP ERROR (NaN/Inf)\r\n");
            return;
        }

        float r_ntc = adc_to_ntc_resistance(raw, 10000.0f);
        console_printf("ADC=%u  Rntc=%.0f ohm  Temp=%.2f C\r\n",
                       raw, r_ntc, temp_c);
    }
    else
    {
        console_write("unknown adc command\r\n");
    }

	console_write("ok\r\n");
    console_prompt();
}

static void cmd_led(int argc, char *argv[]) {
	if (argc < 2) {
		console_write("usage: led off|slow|fast\r\n");
		console_prompt();
		return;
	}

	if (strcmp(argv[1], "off") == 0) {
		led_set_mode(LED_MODE_OFF);
	} else if (strcmp(argv[1], "slow") == 0) {
		led_set_mode(LED_MODE_SLOW);
	} else if (strcmp(argv[1], "fast") == 0) {
		led_set_mode(LED_MODE_FAST);
	} else {
		console_write("invalid mode\r\n");
		return;
	}

	console_write("ok\r\n");
	console_prompt();
}

static void cmd_help(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	for (size_t i = 0; i < CMD_COUNT; i++) {
		console_write(cmd_table[i].name);
		console_write(cmd_table[i].help);
		console_write("\r\n");
	}

	console_prompt();
}

static void cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uint32_t ms = system_uptime_ms();

    char buf[32];

    uint32_t sec = ms / 1000;
    uint32_t min = sec / 60;
    uint32_t hr  = min / 60;

    snprintf(
        buf,
        sizeof(buf),
        "%lu:%02lu:%02lu\r\n",
        hr,
        min % 60,
        sec % 60
    );

    console_write(buf);

	console_prompt();
}
