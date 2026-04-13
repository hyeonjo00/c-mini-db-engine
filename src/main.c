#include <stdio.h>
#include <string.h>

#include "executor.h"
#include "ui.h"

int main(void) {
    AppContext context;
    DashboardStatus status;
    char input_buffer[DB_MAX_COMMAND_LEN];
    char error[256];

    ui_enable_ansi_support();
    if (!app_context_init(&context, DB_DEFAULT_DATA_PATH, DB_DEFAULT_LOG_PATH, DB_DEFAULT_WAL_PATH, error, sizeof(error))) {
        ui_print_error(error);
        return 1;
    }

    ui_print_banner();

    while (context.running) {
        char *command_text;
        CommandExecutionSummary summary;
        double elapsed_ms;
        bool success;

        ui_print_prompt();
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
            putchar('\n');
            break;
        }

        command_text = trim_whitespace(input_buffer);
        if (command_text == NULL || *command_text == '\0') {
            continue;
        }

        memset(&summary, 0, sizeof(summary));
        success = app_context_run_input(&context, command_text, &summary, &status, &elapsed_ms, error, sizeof(error));
        if (!success) {
            ui_print_error(error);
        } else {
            switch (summary.message_level) {
                case SUMMARY_MESSAGE_SUCCESS:
                    ui_print_success(summary.message);
                    break;
                case SUMMARY_MESSAGE_WARNING:
                    ui_print_warning(summary.message);
                    break;
                case SUMMARY_MESSAGE_INFO:
                    ui_print_info(summary.message);
                    break;
                case SUMMARY_MESSAGE_NONE:
                default:
                    break;
            }

            switch (summary.type) {
                case CMD_SELECT:
                    ui_print_records(&summary.result, summary.has_query ? &summary.query : NULL, &summary);
                    break;
                case CMD_COUNT:
                    ui_print_count(summary.count_value, &summary);
                    break;
                case CMD_HISTORY:
                    ui_print_history(&context.history);
                    break;
                case CMD_BENCHMARK:
                    ui_print_benchmark_report(&summary.benchmark);
                    break;
                case CMD_HELP:
                    ui_print_help();
                    break;
                default:
                    break;
            }

            ui_print_command_summary(&summary, elapsed_ms);
        }
        ui_print_status_bar(&status);
        command_execution_summary_destroy(&summary);
    }

    app_context_destroy(&context);
    return 0;
}
