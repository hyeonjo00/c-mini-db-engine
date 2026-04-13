#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stddef.h>

#include "benchmark.h"
#include "database.h"
#include "executor.h"
#include "history.h"

void ui_enable_ansi_support(void);
void ui_print_banner(void);
void ui_print_prompt(void);
void ui_print_success(const char *message);
void ui_print_info(const char *message);
void ui_print_warning(const char *message);
void ui_print_error(const char *message);
void ui_print_records(const QueryResult *result, const QuerySpec *query, const CommandExecutionSummary *summary);
void ui_print_count(size_t count, const CommandExecutionSummary *summary);
void ui_print_command_summary(const CommandExecutionSummary *summary, double elapsed_ms);
void ui_print_status_bar(const DashboardStatus *status);
void ui_print_history(const History *history);
void ui_print_benchmark_report(const BenchmarkReport *report);
void ui_print_help(void);

#endif
