#include "ui.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static bool g_ansi_enabled = true;

static const char *color_reset(void) {
    return g_ansi_enabled ? "\x1b[0m" : "";
}

static const char *color_cyan(void) {
    return g_ansi_enabled ? "\x1b[36m" : "";
}

static const char *color_green(void) {
    return g_ansi_enabled ? "\x1b[32m" : "";
}

static const char *color_yellow(void) {
    return g_ansi_enabled ? "\x1b[33m" : "";
}

static const char *color_red(void) {
    return g_ansi_enabled ? "\x1b[31m" : "";
}

static const char *color_white(void) {
    return g_ansi_enabled ? "\x1b[97m" : "";
}

static const char *color_dim(void) {
    return g_ansi_enabled ? "\x1b[2m" : "";
}

static size_t max_size(size_t left, size_t right) {
    return left > right ? left : right;
}

static size_t min_size(size_t left, size_t right) {
    return left < right ? left : right;
}

static void print_repeated(char ch, size_t count) {
    size_t index;

    for (index = 0; index < count; ++index) {
        putchar(ch);
    }
}

static void print_rule(char ch, size_t width) {
    printf("%s", color_dim());
    print_repeated(ch, width);
    printf("%s\n", color_reset());
}

static void print_section_title(const char *title) {
    printf("\n%s[%s]%s\n", color_cyan(), title, color_reset());
}

static void print_panel_border(size_t width) {
    putchar('+');
    print_repeated('-', width - 2);
    putchar('+');
    putchar('\n');
}

static void print_panel_line(size_t width, const char *label, const char *value) {
    int content_width = (int)(width - 4);
    char buffer[160];

    if (label == NULL) {
        label = "";
    }
    if (value == NULL) {
        value = "";
    }

    if (*label != '\0') {
        snprintf(buffer, sizeof(buffer), "%-14s %s", label, value);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", value);
    }

    printf("| %-*.*s |\n", content_width, content_width, buffer);
}

static void print_table_separator(const size_t widths[4]) {
    size_t index;

    putchar('+');
    for (index = 0; index < 4; ++index) {
        print_repeated('-', widths[index] + 2);
        putchar('+');
    }
    putchar('\n');
}

static void format_bytes(size_t bytes, char *buffer, size_t buffer_size) {
    static const char *units[] = {"B", "KB", "MB", "GB"};
    double value = (double)bytes;
    size_t unit = 0;

    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        unit += 1;
    }

    if (unit == 0) {
        snprintf(buffer, buffer_size, "%zu %s", bytes, units[unit]);
    } else {
        snprintf(buffer, buffer_size, "%.2f %s", value, units[unit]);
    }
}

static void fit_text(const char *source, char *buffer, size_t buffer_size, size_t max_width) {
    size_t length;
    size_t copy_length;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    if (source == NULL) {
        buffer[0] = '\0';
        return;
    }

    length = strlen(source);
    if (length <= max_width) {
        safe_copy(buffer, buffer_size, source);
        return;
    }

    if (max_width <= 3) {
        copy_length = min_size(length, min_size(max_width, buffer_size - 1));
        memcpy(buffer, source, copy_length);
        buffer[copy_length] = '\0';
        return;
    }

    if (buffer_size <= 4) {
        copy_length = min_size(length, buffer_size - 1);
        memcpy(buffer, source, copy_length);
        buffer[copy_length] = '\0';
        return;
    }

    copy_length = min_size(length, min_size(max_width - 3, buffer_size - 4));
    memcpy(buffer, source, copy_length);
    memcpy(buffer + copy_length, "...", 4);
}

static void build_record_display(
    const Record *record,
    char *id_buffer,
    size_t id_capacity,
    char *name_buffer,
    size_t name_capacity,
    char *age_buffer,
    size_t age_capacity,
    char *department_buffer,
    size_t department_capacity
) {
    snprintf(id_buffer, id_capacity, "%d", record->id);
    fit_text(record->name, name_buffer, name_capacity, 18);
    snprintf(age_buffer, age_capacity, "%d", record->age);
    fit_text(record->department, department_buffer, department_capacity, 20);
}

void ui_enable_ansi_support(void) {
#ifdef _WIN32
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    if (output == INVALID_HANDLE_VALUE || !GetConsoleMode(output, &mode)) {
        g_ansi_enabled = false;
        return;
    }

    if (!SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        g_ansi_enabled = false;
    }
#endif
}

void ui_print_banner(void) {
    printf("%s==============================================%s\n", color_cyan(), color_reset());
    printf("%s MiniDB Storage Engine v2.0%s\n", color_white(), color_reset());
    printf("%s B+ Tree | WAL | Optimizer | Bench%s\n", color_dim(), color_reset());
    printf("%s==============================================%s\n", color_cyan(), color_reset());
    printf("%s Snapshot: data/db.csv | WAL: data/db.log | HELP for commands%s\n\n", color_dim(), color_reset());
}

void ui_print_prompt(void) {
    printf("%sdb > %s", color_cyan(), color_reset());
    fflush(stdout);
}

void ui_print_success(const char *message) {
    printf("%s[ok]%s %s\n", color_green(), color_reset(), message);
}

void ui_print_info(const char *message) {
    printf("%s[info]%s %s\n", color_cyan(), color_reset(), message);
}

void ui_print_warning(const char *message) {
    printf("%s[warn]%s %s\n", color_yellow(), color_reset(), message);
}

void ui_print_error(const char *message) {
    printf("%s[error]%s %s\n", color_red(), color_reset(), message);
}

void ui_print_records(const QueryResult *result, const QuerySpec *query, const CommandExecutionSummary *summary) {
    static const char *headers[4] = {"ID", "Name", "Age", "Department"};
    static const size_t caps[4] = {10, 18, 5, 20};
    size_t widths[4];
    size_t index;

    if (result == NULL) {
        return;
    }

    print_section_title("Result Set");

    widths[0] = strlen(headers[0]);
    widths[1] = strlen(headers[1]);
    widths[2] = strlen(headers[2]);
    widths[3] = strlen(headers[3]);

    for (index = 0; index < result->count; ++index) {
        char id_buffer[32];
        char name_buffer[32];
        char age_buffer[32];
        char department_buffer[32];

        build_record_display(
            result->rows[index],
            id_buffer,
            sizeof(id_buffer),
            name_buffer,
            sizeof(name_buffer),
            age_buffer,
            sizeof(age_buffer),
            department_buffer,
            sizeof(department_buffer)
        );

        widths[0] = min_size(caps[0], max_size(widths[0], strlen(id_buffer)));
        widths[1] = min_size(caps[1], max_size(widths[1], strlen(name_buffer)));
        widths[2] = min_size(caps[2], max_size(widths[2], strlen(age_buffer)));
        widths[3] = min_size(caps[3], max_size(widths[3], strlen(department_buffer)));
    }

    print_table_separator(widths);
    printf("| %-*s | %-*s | %-*s | %-*s |\n",
           (int)widths[0], headers[0],
           (int)widths[1], headers[1],
           (int)widths[2], headers[2],
           (int)widths[3], headers[3]);
    print_table_separator(widths);

    if (result->count == 0) {
        printf("| %-*s |\n", (int)(widths[0] + widths[1] + widths[2] + widths[3] + 6), "No rows matched the query.");
    } else {
        for (index = 0; index < result->count; ++index) {
            char id_buffer[32];
            char name_buffer[32];
            char age_buffer[32];
            char department_buffer[32];

            build_record_display(
                result->rows[index],
                id_buffer,
                sizeof(id_buffer),
                name_buffer,
                sizeof(name_buffer),
                age_buffer,
                sizeof(age_buffer),
                department_buffer,
                sizeof(department_buffer)
            );

            printf("| %-*s | %-*s | %-*s | %-*s |\n",
                   (int)widths[0], id_buffer,
                   (int)widths[1], name_buffer,
                   (int)widths[2], age_buffer,
                   (int)widths[3], department_buffer);
        }
    }

    print_table_separator(widths);
    printf("%sRows Displayed:%s %zu", color_dim(), color_reset(), result->count);
    if (summary != NULL && summary->stats.rows_returned > result->count) {
        printf("%s | Rows Matched:%s %zu", color_dim(), color_reset(), summary->stats.rows_returned);
    }
    if (query != NULL && query->order_by.active) {
        printf("%s | Ordered By:%s %s", color_dim(), color_reset(), field_to_string(query->order_by.field));
    }
    if (query != NULL && query->has_limit) {
        printf("%s | Limit:%s %zu", color_dim(), color_reset(), query->limit);
    }
    putchar('\n');
}

void ui_print_count(size_t count, const CommandExecutionSummary *summary) {
    char value[64];

    print_section_title("Aggregate");
    snprintf(value, sizeof(value), "%zu matching row(s)", count);
    printf("%sCOUNT%s = %s%s%s\n",
           color_cyan(),
           color_reset(),
           color_white(),
           value,
           color_reset());

    if (summary != NULL && summary->stats.rows_scanned > 0) {
        printf("%sScanned %zu candidate row(s).%s\n",
               color_dim(),
               summary->stats.rows_scanned,
               color_reset());
    }
}

void ui_print_command_summary(const CommandExecutionSummary *summary, double elapsed_ms) {
    char memory_buffer[32];
    char time_buffer[32];
    char scanned_buffer[32];
    char returned_buffer[32];

    if (summary == NULL || !summary->show_query_panel) {
        return;
    }

    format_bytes(summary->database_memory_bytes, memory_buffer, sizeof(memory_buffer));
    snprintf(time_buffer, sizeof(time_buffer), "%.3f ms", elapsed_ms);
    snprintf(scanned_buffer, sizeof(scanned_buffer), "%zu", summary->stats.rows_scanned);
    snprintf(returned_buffer, sizeof(returned_buffer), "%zu", summary->stats.rows_returned);

    print_section_title("Execution Panel");
    print_panel_border(72);
    print_panel_line(72, "Execution Plan", summary->plan.description[0] != '\0' ? summary->plan.description : "N/A");
    print_panel_line(72, "Optimizer Path", summary->optimizer_path[0] != '\0' ? summary->optimizer_path : "N/A");
    print_panel_line(72, "Index Used", summary->index_used[0] != '\0' ? summary->index_used : "None");
    print_panel_line(72, "Rows Scanned", scanned_buffer);
    print_panel_line(72, "Rows Matched", returned_buffer);
    print_panel_line(72, "Query Time", time_buffer);
    print_panel_line(72, "Memory Usage", memory_buffer);
    print_panel_border(72);
}

void ui_print_status_bar(const DashboardStatus *status) {
    char memory_buffer[32];

    if (status == NULL) {
        return;
    }

    format_bytes(status->memory_usage_bytes, memory_buffer, sizeof(memory_buffer));
    print_rule('=', 78);
    printf("%sRecords:%s %zu  %sSession Queries:%s %zu  %sMemory:%s %s\n",
           color_cyan(),
           color_reset(),
           status->record_count,
           color_cyan(),
           color_reset(),
           status->session_queries,
           color_cyan(),
           color_reset(),
           memory_buffer);
    printf("%sIndexes:%s Hash(id), Hash(department), B+Tree(id), B+Tree(age)\n",
           color_cyan(),
           color_reset());
    printf("%sStorage:%s CSV + WAL\n\n", color_cyan(), color_reset());
}

void ui_print_history(const History *history) {
    size_t start;
    size_t index;

    if (history == NULL || history->count == 0) {
        ui_print_info("Command history is empty.");
        return;
    }

    start = history->count > 12 ? history->count - 12 : 0;

    print_section_title("Recent History");
    print_panel_border(72);
    for (index = history->count; index > start; --index) {
        size_t current = index - 1;
        char preview[80];
        char line[120];

        fit_text(history->items[current], preview, sizeof(preview), 58);
        snprintf(line, sizeof(line), "%3zu. %s", current + 1, preview);
        print_panel_line(72, "", line);
    }
    if (start > 0) {
        char more_buffer[64];
        snprintf(more_buffer, sizeof(more_buffer), "... %zu older command(s) not shown", start);
        print_panel_line(72, "", more_buffer);
    }
    print_panel_border(72);
}

void ui_print_benchmark_report(const BenchmarkReport *report) {
    char throughput_buffer[64];
    char lookup_buffer[64];
    char range_buffer[64];
    char memory_buffer[32];
    char count_buffer[32];

    if (report == NULL) {
        return;
    }

    snprintf(count_buffer, sizeof(count_buffer), "%zu generated record(s)", report->record_count);
    snprintf(throughput_buffer, sizeof(throughput_buffer), "%.2f rows/sec", report->insert_throughput_rows_per_sec);
    snprintf(lookup_buffer, sizeof(lookup_buffer), "%.6f ms/query across %zu lookups", report->exact_lookup_latency_ms, report->exact_lookup_count);
    snprintf(range_buffer, sizeof(range_buffer), "%.6f ms/query across %zu scans", report->range_scan_latency_ms, report->range_query_count);
    format_bytes(report->memory_usage_bytes, memory_buffer, sizeof(memory_buffer));

    print_section_title("Benchmark Dashboard");
    print_panel_border(72);
    print_panel_line(72, "Dataset", count_buffer);
    print_panel_line(72, "Insert Speed", throughput_buffer);
    print_panel_line(72, "Lookup Latency", lookup_buffer);
    print_panel_line(72, "Range Latency", range_buffer);
    print_panel_line(72, "Memory Usage", memory_buffer);
    print_panel_line(72, "Exact Path", report->exact_lookup_plan[0] != '\0' ? report->exact_lookup_plan : "N/A");
    print_panel_line(72, "Range Path", report->range_scan_plan[0] != '\0' ? report->range_scan_plan : "N/A");
    print_panel_border(72);
}

void ui_print_help(void) {
    print_section_title("Command Reference");
    printf("  INSERT <id> <name> <age> <department>\n");
    printf("  SELECT *\n");
    printf("  SELECT WHERE <field> <op> <value> [AND|OR ...] [ORDER BY <field>] [LIMIT n]\n");
    printf("  SELECT ORDER BY age LIMIT 10\n");
    printf("  UPDATE id=<target> field=value [field=value ...]\n");
    printf("  DELETE id=<target>\n");
    printf("  COUNT\n");
    printf("  COUNT WHERE <field> <op> <value> [AND|OR ...]\n");
    printf("  SAVE [path]\n");
    printf("  LOAD [path]\n");
    printf("  HISTORY\n");
    printf("  BENCHMARK [record_count]\n");
    printf("  HELP\n");
    printf("  EXIT\n");
}
