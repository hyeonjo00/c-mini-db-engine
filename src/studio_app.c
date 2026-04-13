#include "studio_app.h"

#include <raylib.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bptree.h"
#include "executor.h"
#include "index.h"

#define STUDIO_WINDOW_WIDTH 1200
#define STUDIO_WINDOW_HEIGHT 800
#define STUDIO_MIN_WIDTH 1040
#define STUDIO_MIN_HEIGHT 700

#define STUDIO_EDITOR_CAPACITY 4096
#define STUDIO_METRIC_CAPACITY 48
#define STUDIO_MAX_SNIPPETS 4
#define STUDIO_SNIPPET_PATH "data/snippets.txt"
#define STUDIO_SNIPPET_LINE_CAPACITY (STUDIO_EDITOR_CAPACITY * 2 + 256)
#define STUDIO_PANEL_RADIUS 0.18f
#define STUDIO_PANEL_PADDING 20.0f
#define STUDIO_BORDER_WIDTH 1.0f
#define STUDIO_TEXT_APP_TITLE 30
#define STUDIO_TEXT_SECTION 17
#define STUDIO_TEXT_LABEL 11
#define STUDIO_TEXT_VALUE 13
#define STUDIO_TEXT_BODY 14
#define STUDIO_GAP 16.0f

typedef enum StudioBannerTone {
    STUDIO_BANNER_NONE = 0,
    STUDIO_BANNER_INFO,
    STUDIO_BANNER_SUCCESS,
    STUDIO_BANNER_WARNING,
    STUDIO_BANNER_ERROR
} StudioBannerTone;

typedef struct MetricSeries {
    float values[STUDIO_METRIC_CAPACITY];
    size_t count;
    size_t next;
} MetricSeries;

typedef struct StudioSnippet {
    char title[64];
    char query[STUDIO_EDITOR_CAPACITY];
} StudioSnippet;

typedef struct StudioTheme {
    Color background;
    Color background_alt;
    Color panel;
    Color panel_alt;
    Color panel_shadow;
    Color border;
    Color border_soft;
    Color text;
    Color text_muted;
    Color text_soft;
    Color accent;
    Color accent_hover;
    Color accent_soft;
    Color success;
    Color warning;
    Color error;
    Color row_alt;
    Color row_hover;
    Color selection;
    Color status_bar;
    Color editor_gutter;
    Color syntax_keyword;
    Color syntax_number;
    Color syntax_string;
    Color syntax_operator;
} StudioTheme;

typedef struct StudioState {
    AppContext context;
    DashboardStatus status;
    CommandExecutionSummary summary;
    BenchmarkReport last_benchmark;
    double last_elapsed_ms;
    char editor_text[STUDIO_EDITOR_CAPACITY];
    bool editor_focused;
    StudioBannerTone banner_tone;
    char banner_text[256];
    int history_scroll;
    int grid_offset;
    bool has_selected_record;
    int selected_record_id;
    Field grid_sort_field;
    bool grid_sort_desc;
    MetricSeries latency_series;
    MetricSeries scanned_series;
    MetricSeries memory_series;
    MetricSeries cache_series;
    size_t index_usage[5];
    StudioSnippet snippets[STUDIO_MAX_SNIPPETS];
    size_t snippet_count;
    bool has_benchmark_summary;
} StudioState;

static const StudioTheme g_theme = {
    {17, 24, 39, 255},
    {17, 24, 39, 255},
    {31, 41, 55, 255},
    {31, 41, 55, 255},
    {4, 8, 20, 72},
    {55, 65, 81, 255},
    {55, 65, 81, 255},
    {249, 250, 251, 255},
    {156, 163, 175, 255},
    {156, 163, 175, 255},
    {96, 165, 250, 255},
    {96, 165, 250, 255},
    {96, 165, 250, 255},
    {96, 165, 250, 255},
    {96, 165, 250, 255},
    {96, 165, 250, 255},
    {36, 46, 60, 255},
    {44, 57, 75, 255},
    {46, 64, 90, 255},
    {31, 41, 55, 255},
    {24, 33, 46, 255},
    {96, 165, 250, 255},
    {249, 250, 251, 255},
    {249, 250, 251, 255},
    {156, 163, 175, 255}
};

static const char *g_snippet_titles[] = {
    "SELECT *",
    "Age > 30",
    "Order By Age"
};

static const char *g_snippet_queries[] = {
    "SELECT *",
    "SELECT WHERE age > 30",
    "SELECT ORDER BY age LIMIT 10"
};

static Field g_grid_sort_field = FIELD_NONE;
static bool g_grid_sort_desc = false;

static int clamp_int(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static size_t min_size(size_t left, size_t right) {
    return left < right ? left : right;
}

static float max_float(float left, float right) {
    return left > right ? left : right;
}

static float clamp_float(float value, float minimum, float maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static Color mix_color(Color left, Color right, float amount) {
    Color mixed;
    float t = clamp_float(amount, 0.0f, 1.0f);

    mixed.r = (unsigned char)((float)left.r + ((float)right.r - (float)left.r) * t);
    mixed.g = (unsigned char)((float)left.g + ((float)right.g - (float)left.g) * t);
    mixed.b = (unsigned char)((float)left.b + ((float)right.b - (float)left.b) * t);
    mixed.a = (unsigned char)((float)left.a + ((float)right.a - (float)left.a) * t);
    return mixed;
}

static float measure_text_width(const char *text, int font_size) {
    Vector2 measured = MeasureTextEx(GetFontDefault(), text, (float)font_size, 0.0f);
    return measured.x;
}

static void fit_text_to_width(const char *source, char *buffer, size_t buffer_size, int font_size, float max_width) {
    size_t length;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (source == NULL) {
        return;
    }

    safe_copy(buffer, buffer_size, source);
    if (measure_text_width(buffer, font_size) <= max_width) {
        return;
    }

    length = strlen(buffer);
    while (length > 3) {
        length -= 1;
        buffer[length] = '\0';
        if (length >= 3) {
            buffer[length - 3] = '.';
            buffer[length - 2] = '.';
            buffer[length - 1] = '.';
        }

        if (measure_text_width(buffer, font_size) <= max_width) {
            return;
        }
    }

    safe_copy(buffer, buffer_size, "...");
}

static bool append_text(char *buffer, size_t capacity, const char *text) {
    size_t current_length;
    size_t append_length;

    if (buffer == NULL || text == NULL) {
        return false;
    }

    current_length = strlen(buffer);
    append_length = strlen(text);
    if (current_length + append_length >= capacity) {
        return false;
    }

    memcpy(buffer + current_length, text, append_length + 1);
    return true;
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

static void metric_series_push(MetricSeries *series, float value) {
    if (series == NULL) {
        return;
    }

    series->values[series->next] = value;
    if (series->count < STUDIO_METRIC_CAPACITY) {
        series->count += 1;
    }
    series->next = (series->next + 1) % STUDIO_METRIC_CAPACITY;
}

static float metric_series_value(const MetricSeries *series, size_t index) {
    size_t start;

    if (series == NULL || series->count == 0 || index >= series->count) {
        return 0.0f;
    }

    start = (series->next + STUDIO_METRIC_CAPACITY - series->count) % STUDIO_METRIC_CAPACITY;
    return series->values[(start + index) % STUDIO_METRIC_CAPACITY];
}

static size_t file_size_or_zero(const char *path) {
    struct stat file_info;

    if (path == NULL || *path == '\0') {
        return 0;
    }

    if (stat(path, &file_info) != 0) {
        return 0;
    }

    return (size_t)file_info.st_size;
}

static bool file_exists(const char *path) {
    struct stat file_info;

    if (path == NULL || *path == '\0') {
        return false;
    }

    return stat(path, &file_info) == 0;
}

static void draw_card(Rectangle rect, Color fill, Color border) {
    Rectangle shadow = {rect.x, rect.y + 10.0f, rect.width, rect.height};

    DrawRectangleRounded(shadow, STUDIO_PANEL_RADIUS, 14, g_theme.panel_shadow);
    DrawRectangleRounded(rect, STUDIO_PANEL_RADIUS, 12, fill);
    DrawRectangleLinesEx(rect, STUDIO_BORDER_WIDTH, border);
}

static void draw_surface(Rectangle rect, Color fill, Color border) {
    DrawRectangleRounded(rect, STUDIO_PANEL_RADIUS, 12, fill);
    DrawRectangleLinesEx(rect, STUDIO_BORDER_WIDTH, border);
}

static void draw_divider(Rectangle rect, float y) {
    DrawLineEx((Vector2){rect.x + STUDIO_PANEL_PADDING, y}, (Vector2){rect.x + rect.width - STUDIO_PANEL_PADDING, y}, 1.0f, Fade(g_theme.border, 0.45f));
}

static void draw_section_title(Rectangle rect, const char *title, const char *subtitle) {
    DrawText(title, (int)rect.x, (int)rect.y, STUDIO_TEXT_SECTION, g_theme.text);
    if (subtitle != NULL && subtitle[0] != '\0') {
        DrawText(subtitle, (int)rect.x, (int)(rect.y + 22.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
    }
}

static bool draw_button(Rectangle rect, const char *label, bool primary) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, rect);
    bool clicked = hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    Color fill = primary
        ? mix_color(g_theme.accent, g_theme.text, hovered ? 0.08f : 0.0f)
        : mix_color(g_theme.panel, g_theme.border, hovered ? 0.22f : 0.10f);
    Color border = primary ? g_theme.accent : g_theme.border;
    Color text_color = primary ? g_theme.background : g_theme.text;
    int label_width = MeasureText(label, STUDIO_TEXT_BODY);

    DrawRectangleRounded(rect, STUDIO_PANEL_RADIUS, 12, fill);
    DrawRectangleLinesEx(rect, STUDIO_BORDER_WIDTH, border);
    DrawText(label, (int)(rect.x + (rect.width - (float)label_width) * 0.5f), (int)(rect.y + rect.height * 0.5f - 7.0f), STUDIO_TEXT_BODY, text_color);
    return clicked;
}

static void draw_badge(Rectangle rect, const char *label, Color fill, Color text_color) {
    char fitted[96];
    int label_width;

    fit_text_to_width(label, fitted, sizeof(fitted), 13, rect.width - 12.0f);
    label_width = MeasureText(fitted, 13);
    DrawRectangleRounded(rect, STUDIO_PANEL_RADIUS, 12, fill);
    DrawText(fitted, (int)(rect.x + (rect.width - (float)label_width) * 0.5f), (int)(rect.y + rect.height * 0.5f - 7.0f), 13, text_color);
}

static void draw_metric_row(Rectangle rect, const char *label, const char *value) {
    char fitted[128];

    DrawText(label, (int)rect.x, (int)(rect.y + 2.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
    fit_text_to_width(value, fitted, sizeof(fitted), STUDIO_TEXT_VALUE, rect.width * 0.56f);
    DrawTextEx(GetFontDefault(),
               fitted,
               (Vector2){rect.x + rect.width - measure_text_width(fitted, STUDIO_TEXT_VALUE), rect.y + 1.0f},
               (float)STUDIO_TEXT_VALUE,
               0.0f,
               g_theme.text);
}

static void draw_sidebar_label(Rectangle rect, const char *label) {
    char upper[64];
    size_t index;

    safe_copy(upper, sizeof(upper), label == NULL ? "" : label);
    for (index = 0; upper[index] != '\0'; ++index) {
        upper[index] = (char)toupper((unsigned char)upper[index]);
    }
    DrawText(upper, (int)rect.x, (int)rect.y, STUDIO_TEXT_LABEL, g_theme.text_muted);
}

static void draw_sidebar_item(Rectangle rect, const char *label, bool hovered, bool selected) {
    Color fill = selected ? Fade(g_theme.accent, 0.18f) : (hovered ? Fade(g_theme.accent, 0.10f) : BLANK);
    Color bullet = selected ? g_theme.accent : (hovered ? mix_color(g_theme.text_muted, g_theme.accent, 0.45f) : g_theme.text_muted);

    if (fill.a > 0) {
        DrawRectangleRounded(rect, STUDIO_PANEL_RADIUS, 12, fill);
    }
    DrawCircle((int)(rect.x + 8.0f), (int)(rect.y + rect.height * 0.5f), 2.0f, bullet);
    DrawText(label, (int)(rect.x + 18.0f), (int)(rect.y + rect.height * 0.5f - 7.0f), STUDIO_TEXT_BODY - 1, selected ? g_theme.text : g_theme.text_muted);
}

static void draw_sparkline(Rectangle rect, const MetricSeries *series, Color color) {
    size_t index;

    if (series == NULL || series->count < 2) {
        DrawLineEx((Vector2){rect.x, rect.y + rect.height * 0.5f}, (Vector2){rect.x + rect.width, rect.y + rect.height * 0.5f}, 1.0f, g_theme.border_soft);
        return;
    }

    {
        float min_value = metric_series_value(series, 0);
        float max_value = min_value;
        for (index = 1; index < series->count; ++index) {
            float value = metric_series_value(series, index);
            if (value < min_value) {
                min_value = value;
            }
            if (value > max_value) {
                max_value = value;
            }
        }

        if (max_value - min_value < 0.001f) {
            max_value = min_value + 1.0f;
        }

        for (index = 0; index + 1 < series->count; ++index) {
            float value_a = metric_series_value(series, index);
            float value_b = metric_series_value(series, index + 1);
            float normalized_a = (value_a - min_value) / (max_value - min_value);
            float normalized_b = (value_b - min_value) / (max_value - min_value);
            float x_a = rect.x + ((float)index / (float)(series->count - 1)) * rect.width;
            float x_b = rect.x + ((float)(index + 1) / (float)(series->count - 1)) * rect.width;
            float y_a = rect.y + rect.height - normalized_a * rect.height;
            float y_b = rect.y + rect.height - normalized_b * rect.height;
            DrawLineEx((Vector2){x_a, y_a}, (Vector2){x_b, y_b}, 2.0f, color);
        }
    }
}

static void studio_set_banner(StudioState *state, StudioBannerTone tone, const char *message) {
    if (state == NULL) {
        return;
    }

    state->banner_tone = tone;
    if (message == NULL) {
        state->banner_text[0] = '\0';
        return;
    }

    safe_copy(state->banner_text, sizeof(state->banner_text), message);
}

static StudioBannerTone banner_tone_from_summary(const CommandExecutionSummary *summary) {
    if (summary == NULL) {
        return STUDIO_BANNER_INFO;
    }

    switch (summary->message_level) {
        case SUMMARY_MESSAGE_SUCCESS:
            return STUDIO_BANNER_SUCCESS;
        case SUMMARY_MESSAGE_WARNING:
            return STUDIO_BANNER_WARNING;
        case SUMMARY_MESSAGE_INFO:
        case SUMMARY_MESSAGE_NONE:
        default:
            return STUDIO_BANNER_INFO;
    }
}

static void draw_banner(Rectangle rect, StudioBannerTone tone, const char *message) {
    Color fill = mix_color(g_theme.panel, g_theme.accent, 0.10f);
    Color border = mix_color(g_theme.border, g_theme.accent, 0.35f);
    Color text_color = g_theme.text;
    char fitted[192];
    const char *prefix = "READY";

    if (message == NULL || message[0] == '\0' || tone == STUDIO_BANNER_NONE) {
        return;
    }

    switch (tone) {
        case STUDIO_BANNER_SUCCESS:
            prefix = "DONE";
            break;
        case STUDIO_BANNER_WARNING:
            prefix = "NOTE";
            break;
        case STUDIO_BANNER_ERROR:
            prefix = "ALERT";
            break;
        case STUDIO_BANNER_INFO:
        default:
            prefix = "READY";
            break;
    }

    fit_text_to_width(message, fitted, sizeof(fitted), STUDIO_TEXT_BODY, rect.width - 94.0f);
    DrawRectangleRounded(rect, STUDIO_PANEL_RADIUS, 12, fill);
    DrawRectangleLinesEx(rect, STUDIO_BORDER_WIDTH, border);
    DrawCircle((int)(rect.x + 16.0f), (int)(rect.y + rect.height * 0.5f), 3.0f, g_theme.accent);
    DrawText(prefix, (int)(rect.x + 28.0f), (int)(rect.y + rect.height * 0.5f - 7.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
    DrawText(fitted, (int)(rect.x + 72.0f), (int)(rect.y + rect.height * 0.5f - 7.0f), STUDIO_TEXT_BODY, text_color);
}

static bool is_keyword_token(const char *token) {
    static const char *keywords[] = {
        "SELECT", "WHERE", "INSERT", "UPDATE", "DELETE", "COUNT", "SAVE", "LOAD",
        "ORDER", "BY", "LIMIT", "AND", "OR", "BENCHMARK", "EXIT", "HELP", "HISTORY"
    };
    size_t index;

    if (token == NULL) {
        return false;
    }

    for (index = 0; index < sizeof(keywords) / sizeof(keywords[0]); ++index) {
        if (string_ieq(token, keywords[index])) {
            return true;
        }
    }

    return false;
}

static void uppercase_token(const char *source, char *buffer, size_t capacity) {
    size_t index;

    if (buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';
    if (source == NULL) {
        return;
    }

    for (index = 0; source[index] != '\0' && index + 1 < capacity; ++index) {
        buffer[index] = (char)toupper((unsigned char)source[index]);
    }
    buffer[index] = '\0';
}

static Color token_color(const char *token) {
    size_t index;
    bool numeric = true;
    bool quoted = false;

    if (token == NULL || token[0] == '\0') {
        return g_theme.text;
    }

    if (is_keyword_token(token)) {
        return g_theme.syntax_keyword;
    }

    if ((strcmp(token, "=") == 0) || (strcmp(token, "!=") == 0) || (strcmp(token, ">") == 0) ||
        (strcmp(token, ">=") == 0) || (strcmp(token, "<") == 0) || (strcmp(token, "<=") == 0)) {
        return g_theme.syntax_operator;
    }

    quoted = token[0] == '"' || token[strlen(token) - 1] == '"';
    if (quoted) {
        return g_theme.syntax_string;
    }

    for (index = 0; token[index] != '\0'; ++index) {
        if (!isdigit((unsigned char)token[index]) && !(index == 0 && token[index] == '-')) {
            numeric = false;
            break;
        }
    }

    return numeric ? g_theme.syntax_number : g_theme.text;
}

static bool tokenize_query(const char *input, char tokens[][96], size_t *count) {
    char current[96];
    size_t current_length = 0;
    bool in_quote = false;
    size_t token_count = 0;
    const char *cursor;

    if (count == NULL) {
        return false;
    }

    *count = 0;
    if (input == NULL) {
        return true;
    }

    memset(current, 0, sizeof(current));
    for (cursor = input; ; ++cursor) {
        char ch = *cursor;
        bool at_end = ch == '\0';
        bool separator = !in_quote && (at_end || isspace((unsigned char)ch));

        if (ch == '"') {
            in_quote = !in_quote;
        }

        if (separator) {
            if (current_length > 0) {
                current[current_length] = '\0';
                if (token_count < 96) {
                    safe_copy(tokens[token_count], 96, current);
                    token_count += 1;
                }
                current_length = 0;
                current[0] = '\0';
            }

            if (at_end) {
                break;
            }
            continue;
        }

        if ((ch == '=' || ch == '!' || ch == '>' || ch == '<') && !in_quote) {
            if (current_length > 0) {
                current[current_length] = '\0';
                if (token_count < 96) {
                    safe_copy(tokens[token_count], 96, current);
                    token_count += 1;
                }
                current_length = 0;
                current[0] = '\0';
            }

            if (token_count < 96) {
                char op[3];
                op[0] = ch;
                op[1] = '\0';
                op[2] = '\0';
                if ((ch == '!' || ch == '>' || ch == '<') && cursor[1] == '=') {
                    op[1] = '=';
                    op[2] = '\0';
                    ++cursor;
                }
                safe_copy(tokens[token_count], 96, op);
                token_count += 1;
            }
            continue;
        }

        if (current_length + 1 < sizeof(current)) {
            current[current_length++] = ch;
        }
    }

    *count = token_count;
    return true;
}

static void studio_format_query(char *buffer, size_t capacity) {
    char tokens[96][96];
    char formatted[STUDIO_EDITOR_CAPACITY];
    size_t count = 0;
    size_t index = 0;

    if (buffer == NULL || capacity == 0) {
        return;
    }

    tokenize_query(buffer, tokens, &count);
    memset(formatted, 0, sizeof(formatted));

    while (index < count) {
        char upper[96];
        uppercase_token(tokens[index], upper, sizeof(upper));

        if (index == 0) {
            append_text(formatted, sizeof(formatted), upper);
            index += 1;
            continue;
        }

        if (string_ieq(tokens[index], "WHERE") || string_ieq(tokens[index], "LIMIT")) {
            append_text(formatted, sizeof(formatted), "\n");
            append_text(formatted, sizeof(formatted), upper);
        } else if (string_ieq(tokens[index], "ORDER") && index + 1 < count && string_ieq(tokens[index + 1], "BY")) {
            append_text(formatted, sizeof(formatted), "\nORDER BY");
            index += 1;
        } else if (string_ieq(tokens[index], "AND") || string_ieq(tokens[index], "OR")) {
            append_text(formatted, sizeof(formatted), "\n  ");
            append_text(formatted, sizeof(formatted), upper);
        } else if (strcmp(tokens[index], "=") == 0 || strcmp(tokens[index], "!=") == 0 ||
                   strcmp(tokens[index], ">") == 0 || strcmp(tokens[index], ">=") == 0 ||
                   strcmp(tokens[index], "<") == 0 || strcmp(tokens[index], "<=") == 0) {
            append_text(formatted, sizeof(formatted), " ");
            append_text(formatted, sizeof(formatted), tokens[index]);
        } else {
            append_text(formatted, sizeof(formatted), " ");
            append_text(formatted, sizeof(formatted), is_keyword_token(tokens[index]) ? upper : tokens[index]);
        }

        index += 1;
    }

    safe_copy(buffer, capacity, formatted);
}

static void draw_sql_editor_text(Rectangle rect, const char *text, bool focused) {
    float x = rect.x + 46.0f;
    float y = rect.y + 16.0f;
    int line_number = 1;
    size_t index = 0;
    char token[128];
    size_t token_length = 0;
    Font font = GetFontDefault();

    DrawRectangleRounded((Rectangle){rect.x + 8.0f, rect.y + 8.0f, 30.0f, rect.height - 16.0f}, STUDIO_PANEL_RADIUS, 12, mix_color(g_theme.background, g_theme.panel, 0.35f));
    BeginScissorMode((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height);
    DrawText(TextFormat("%d", line_number), (int)(rect.x + 17.0f), (int)(y + 1.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);

    while (text != NULL && text[index] != '\0') {
        char ch = text[index];

        if (ch == '\n') {
            if (token_length > 0) {
                token[token_length] = '\0';
                DrawTextEx(font, token, (Vector2){x, y}, 18.0f, 0.0f, token_color(token));
                x += measure_text_width(token, 18);
                token_length = 0;
            }

            line_number += 1;
            x = rect.x + 46.0f;
            y += 24.0f;
            DrawText(TextFormat("%d", line_number), (int)(rect.x + 17.0f), (int)(y + 1.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
            index += 1;
            continue;
        }

        if (isspace((unsigned char)ch)) {
            if (token_length > 0) {
                token[token_length] = '\0';
                DrawTextEx(font, token, (Vector2){x, y}, 16.0f, 0.0f, token_color(token));
                x += measure_text_width(token, 16);
                token_length = 0;
            }

            x += ch == '\t' ? 18.0f : 8.0f;
            index += 1;
            continue;
        }

        if ((ch == '=' || ch == '!' || ch == '>' || ch == '<') && token_length == 0) {
            char op[3];
            op[0] = ch;
            op[1] = '\0';
            op[2] = '\0';
            if ((ch == '!' || ch == '<' || ch == '>') && text[index + 1] == '=') {
                op[1] = '=';
                op[2] = '\0';
                index += 1;
            }
            DrawTextEx(font, op, (Vector2){x, y}, 16.0f, 0.0f, token_color(op));
            x += measure_text_width(op, 16);
            index += 1;
            continue;
        }

        if (token_length + 1 < sizeof(token)) {
            token[token_length++] = ch;
        }
        index += 1;
    }

    if (token_length > 0) {
        token[token_length] = '\0';
        DrawTextEx(font, token, (Vector2){x, y}, 16.0f, 0.0f, token_color(token));
        x += measure_text_width(token, 16);
    }

    if (focused && ((int)(GetTime() * 1.75) % 2 == 0)) {
        DrawRectangle((int)x, (int)y, 2, 18, g_theme.accent);
    }
    EndScissorMode();
}

static void studio_set_editor_text(StudioState *state, const char *text) {
    if (state == NULL) {
        return;
    }

    if (text == NULL) {
        state->editor_text[0] = '\0';
        return;
    }

    safe_copy(state->editor_text, sizeof(state->editor_text), text);
}

static bool copy_trimmed_text(const char *source, char *buffer, size_t capacity) {
    char working[STUDIO_EDITOR_CAPACITY];
    char *trimmed;

    if (buffer == NULL || capacity == 0) {
        return false;
    }

    buffer[0] = '\0';
    if (source == NULL) {
        return false;
    }

    if (!safe_copy(working, sizeof(working), source)) {
        return false;
    }

    trimmed = trim_whitespace(working);
    if (trimmed == NULL) {
        return false;
    }

    return safe_copy(buffer, capacity, trimmed);
}

static bool editor_has_content(const StudioState *state) {
    char trimmed[STUDIO_EDITOR_CAPACITY];

    if (state == NULL) {
        return false;
    }

    return copy_trimmed_text(state->editor_text, trimmed, sizeof(trimmed)) && trimmed[0] != '\0';
}

static size_t studio_default_snippet_count(void) {
    return sizeof(g_snippet_titles) / sizeof(g_snippet_titles[0]);
}

static void studio_snippet_title_from_query(const char *query, char *title, size_t title_capacity) {
    size_t index = 0;
    size_t length = 0;

    if (title == NULL || title_capacity == 0) {
        return;
    }

    title[0] = '\0';
    if (query == NULL) {
        return;
    }

    while (query[index] != '\0' && query[index] != '\n' && length + 1 < title_capacity) {
        char ch = query[index++];
        title[length++] = (ch == '\t') ? ' ' : ch;
    }
    title[length] = '\0';

    {
        char *trimmed = trim_whitespace(title);
        if (trimmed != title && trimmed != NULL) {
            memmove(title, trimmed, strlen(trimmed) + 1);
        }
    }

    if (strlen(title) > 34) {
        title[31] = '.';
        title[32] = '.';
        title[33] = '.';
        title[34] = '\0';
    }
}

static bool snippet_escape_text(const char *source, char *buffer, size_t capacity) {
    size_t index;
    size_t out = 0;

    if (buffer == NULL || capacity == 0) {
        return false;
    }

    buffer[0] = '\0';
    if (source == NULL) {
        return true;
    }

    for (index = 0; source[index] != '\0'; ++index) {
        char ch = source[index];
        const char *replacement = NULL;
        size_t replacement_length = 0;

        switch (ch) {
            case '\\':
                replacement = "\\\\";
                replacement_length = 2;
                break;
            case '\n':
                replacement = "\\n";
                replacement_length = 2;
                break;
            case '\r':
                replacement = "\\r";
                replacement_length = 2;
                break;
            case '|':
                replacement = "\\p";
                replacement_length = 2;
                break;
            default:
                replacement = NULL;
                replacement_length = 0;
                break;
        }

        if (replacement != NULL) {
            if (out + replacement_length >= capacity) {
                return false;
            }
            memcpy(buffer + out, replacement, replacement_length);
            out += replacement_length;
            continue;
        }

        if (out + 1 >= capacity) {
            return false;
        }
        buffer[out++] = ch;
    }

    buffer[out] = '\0';
    return true;
}

static bool snippet_unescape_text(const char *source, char *buffer, size_t capacity) {
    size_t index;
    size_t out = 0;

    if (buffer == NULL || capacity == 0) {
        return false;
    }

    buffer[0] = '\0';
    if (source == NULL) {
        return true;
    }

    for (index = 0; source[index] != '\0'; ++index) {
        char ch = source[index];
        char decoded = '\0';

        if (ch == '\\' && source[index + 1] != '\0') {
            switch (source[index + 1]) {
                case 'n':
                    decoded = '\n';
                    break;
                case 'r':
                    decoded = '\r';
                    break;
                case 'p':
                    decoded = '|';
                    break;
                case '\\':
                    decoded = '\\';
                    break;
                default:
                    decoded = source[index + 1];
                    break;
            }
            index += 1;
        } else {
            decoded = ch;
        }

        if (out + 1 >= capacity) {
            return false;
        }
        buffer[out++] = decoded;
    }

    buffer[out] = '\0';
    return true;
}

static bool studio_save_snippet_library(const StudioState *state, char *error, size_t error_capacity) {
    FILE *stream;
    size_t index;
    size_t custom_start = studio_default_snippet_count();

    if (state == NULL) {
        snprintf(error, error_capacity, "Snippet save received a null state.");
        return false;
    }

    if (!ensure_parent_directory(STUDIO_SNIPPET_PATH)) {
        snprintf(error, error_capacity, "Failed to prepare snippet storage at %s.", STUDIO_SNIPPET_PATH);
        return false;
    }

    stream = fopen(STUDIO_SNIPPET_PATH, "w");
    if (stream == NULL) {
        snprintf(error, error_capacity, "Failed to open %s for snippet persistence.", STUDIO_SNIPPET_PATH);
        return false;
    }

    fprintf(stream, "# MiniDB Studio snippets\n");
    for (index = custom_start; index < state->snippet_count; ++index) {
        char encoded_title[128];
        char encoded_query[STUDIO_EDITOR_CAPACITY * 2];

        if (!snippet_escape_text(state->snippets[index].title, encoded_title, sizeof(encoded_title)) ||
            !snippet_escape_text(state->snippets[index].query, encoded_query, sizeof(encoded_query))) {
            fclose(stream);
            snprintf(error, error_capacity, "A saved snippet exceeded the persistence format limits.");
            return false;
        }

        fprintf(stream, "%s|%s\n", encoded_title, encoded_query);
    }

    fclose(stream);
    return true;
}

static void studio_load_snippet_library(StudioState *state) {
    FILE *stream;
    char line[STUDIO_SNIPPET_LINE_CAPACITY];
    size_t custom_start = studio_default_snippet_count();

    if (state == NULL) {
        return;
    }

    stream = fopen(STUDIO_SNIPPET_PATH, "r");
    if (stream == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), stream) != NULL && state->snippet_count < STUDIO_MAX_SNIPPETS) {
        char *separator;
        char *newline;
        char decoded_title[64];
        char decoded_query[STUDIO_EDITOR_CAPACITY];
        size_t index;
        bool duplicate = false;

        newline = strpbrk(line, "\r\n");
        if (newline != NULL) {
            *newline = '\0';
        }

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        separator = strchr(line, '|');
        if (separator == NULL) {
            continue;
        }

        *separator = '\0';
        if (!snippet_unescape_text(line, decoded_title, sizeof(decoded_title)) ||
            !snippet_unescape_text(separator + 1, decoded_query, sizeof(decoded_query))) {
            continue;
        }

        if (decoded_query[0] == '\0') {
            continue;
        }

        for (index = custom_start; index < state->snippet_count; ++index) {
            if (strcmp(state->snippets[index].query, decoded_query) == 0) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            continue;
        }

        safe_copy(state->snippets[state->snippet_count].title, sizeof(state->snippets[state->snippet_count].title), decoded_title);
        safe_copy(state->snippets[state->snippet_count].query, sizeof(state->snippets[state->snippet_count].query), decoded_query);
        state->snippet_count += 1;
    }

    fclose(stream);
}

static void studio_seed_snippets(StudioState *state) {
    size_t index;

    if (state == NULL) {
        return;
    }

    state->snippet_count = 0;
    for (index = 0; index < studio_default_snippet_count() && index < STUDIO_MAX_SNIPPETS; ++index) {
        safe_copy(state->snippets[index].title, sizeof(state->snippets[index].title), g_snippet_titles[index]);
        safe_copy(state->snippets[index].query, sizeof(state->snippets[index].query), g_snippet_queries[index]);
        state->snippet_count += 1;
    }
}

static void studio_save_current_snippet(StudioState *state) {
    char trimmed[STUDIO_EDITOR_CAPACITY];
    char error[256];
    size_t index;
    size_t target_index;

    if (state == NULL) {
        return;
    }

    if (!copy_trimmed_text(state->editor_text, trimmed, sizeof(trimmed)) || trimmed[0] == '\0') {
        studio_set_banner(state, STUDIO_BANNER_WARNING, "Write a query before saving a snippet.");
        return;
    }

    for (index = 0; index < state->snippet_count; ++index) {
        if (strcmp(state->snippets[index].query, trimmed) == 0) {
            studio_set_banner(state, STUDIO_BANNER_INFO, "That query is already available in saved snippets.");
            return;
        }
    }

    if (state->snippet_count < STUDIO_MAX_SNIPPETS) {
        target_index = state->snippet_count++;
    } else {
        size_t custom_start = studio_default_snippet_count();
        if (custom_start >= STUDIO_MAX_SNIPPETS) {
            target_index = STUDIO_MAX_SNIPPETS - 1;
        } else {
            for (index = custom_start; index + 1 < STUDIO_MAX_SNIPPETS; ++index) {
                state->snippets[index] = state->snippets[index + 1];
            }
            target_index = STUDIO_MAX_SNIPPETS - 1;
        }
    }

    studio_snippet_title_from_query(trimmed, state->snippets[target_index].title, sizeof(state->snippets[target_index].title));
    safe_copy(state->snippets[target_index].query, sizeof(state->snippets[target_index].query), trimmed);
    if (!studio_save_snippet_library(state, error, sizeof(error))) {
        studio_set_banner(state, STUDIO_BANNER_WARNING, "Saved snippet for this session, but failed to persist it on disk.");
        return;
    }

    studio_set_banner(state, STUDIO_BANNER_SUCCESS, "Saved the current editor contents as a reusable snippet.");
}

static const Record *studio_selected_record(const StudioState *state) {
    size_t index;

    if (state == NULL || !state->summary.has_result_rows || !state->has_selected_record) {
        return NULL;
    }

    for (index = 0; index < state->summary.result.count; ++index) {
        const Record *record = state->summary.result.rows[index];
        if (record->id == state->selected_record_id) {
            return record;
        }
    }

    return NULL;
}

static void studio_build_assistant_content(
    const StudioState *state,
    char *hint,
    size_t hint_capacity,
    char suggestions[3][DB_MAX_COMMAND_LEN]
) {
    const Record *selected;
    size_t index;

    if (hint == NULL || suggestions == NULL) {
        return;
    }

    hint[0] = '\0';
    for (index = 0; index < 3; ++index) {
        suggestions[index][0] = '\0';
    }

    selected = studio_selected_record(state);
    if (selected != NULL) {
        safe_copy(hint, hint_capacity, "Context-aware suggestions based on the selected row.");
        snprintf(suggestions[0], DB_MAX_COMMAND_LEN, "SELECT WHERE id = %d", selected->id);
        snprintf(suggestions[1], DB_MAX_COMMAND_LEN, "SELECT WHERE department = %s ORDER BY age LIMIT 10", selected->department);
        snprintf(suggestions[2], DB_MAX_COMMAND_LEN, "SELECT WHERE age > %d ORDER BY age LIMIT 10", selected->age);
        return;
    }

    if (state != NULL && state->summary.show_query_panel && state->summary.plan.type == PLAN_FULL_SCAN) {
        safe_copy(hint, hint_capacity, "The last query fell back to a scan. Try a more index-friendly path.");
        safe_copy(suggestions[0], DB_MAX_COMMAND_LEN, "SELECT WHERE id = 1");
        safe_copy(suggestions[1], DB_MAX_COMMAND_LEN, "SELECT WHERE age > 30 ORDER BY age LIMIT 10");
        safe_copy(suggestions[2], DB_MAX_COMMAND_LEN, "SELECT WHERE department = Oncology");
        return;
    }

    safe_copy(hint, hint_capacity, "Quick demo-friendly queries for the current dataset.");
    safe_copy(suggestions[0], DB_MAX_COMMAND_LEN, "SELECT *");
    safe_copy(suggestions[1], DB_MAX_COMMAND_LEN, "SELECT WHERE age > 30");
    safe_copy(suggestions[2], DB_MAX_COMMAND_LEN, "SELECT ORDER BY age LIMIT 10");
}

static void compute_result_grid_widths(const QueryResult *result, size_t start, size_t end, float available_width, float widths[4]) {
    size_t index;
    float total_width;
    float flexible_width;
    float desired_name;
    float desired_department;

    widths[0] = max_float(72.0f, measure_text_width("ID", 14) + 24.0f);
    widths[1] = max_float(148.0f, measure_text_width("Name", 14) + 24.0f);
    widths[2] = max_float(72.0f, measure_text_width("Age", 14) + 24.0f);
    widths[3] = max_float(180.0f, measure_text_width("Department", 14) + 24.0f);

    if (result == NULL) {
        return;
    }

    end = min_size(end, start + 24);
    for (index = start; index < end && index < result->count; ++index) {
        const Record *record = result->rows[index];
        char id_text[24];
        char age_text[24];

        snprintf(id_text, sizeof(id_text), "%d", record->id);
        snprintf(age_text, sizeof(age_text), "%d", record->age);

        widths[0] = max_float(widths[0], measure_text_width(id_text, 14) + 24.0f);
        widths[1] = max_float(widths[1], measure_text_width(record->name, 14) + 24.0f);
        widths[2] = max_float(widths[2], measure_text_width(age_text, 14) + 24.0f);
        widths[3] = max_float(widths[3], measure_text_width(record->department, 14) + 24.0f);
    }

    widths[0] = clamp_float(widths[0], 72.0f, 96.0f);
    widths[2] = clamp_float(widths[2], 72.0f, 90.0f);
    desired_name = clamp_float(widths[1], 148.0f, 240.0f);
    desired_department = clamp_float(widths[3], 180.0f, 280.0f);

    total_width = widths[0] + widths[2] + desired_name + desired_department;
    if (total_width <= available_width) {
        widths[1] = desired_name;
        widths[3] = available_width - widths[0] - widths[1] - widths[2];
        return;
    }

    flexible_width = available_width - widths[0] - widths[2];
    widths[1] = flexible_width * (desired_name / (desired_name + desired_department));
    widths[1] = clamp_float(widths[1], 140.0f, flexible_width - 170.0f);
    widths[3] = available_width - widths[0] - widths[1] - widths[2];
    if (widths[3] < 170.0f) {
        widths[3] = 170.0f;
        widths[1] = available_width - widths[0] - widths[2] - widths[3];
    }
}

static void draw_metric_tile(Rectangle rect, const char *label, const char *value, Color accent) {
    char fitted[96];

    draw_card(rect, g_theme.panel_alt, g_theme.border_soft);
    DrawRectangleRounded((Rectangle){rect.x + 12.0f, rect.y + 14.0f, 10.0f, rect.height - 28.0f}, 0.35f, 8, Fade(accent, 0.85f));
    DrawText(label, (int)(rect.x + 30.0f), (int)(rect.y + 14.0f), 12, g_theme.text_soft);
    fit_text_to_width(value, fitted, sizeof(fitted), 18, rect.width - 42.0f);
    DrawText(fitted, (int)(rect.x + 30.0f), (int)(rect.y + rect.height * 0.5f - 7.0f), 18, g_theme.text);
}

static void draw_benchmark_panel(const StudioState *state, Rectangle rect) {
    const BenchmarkReport *report = state->summary.show_benchmark_panel ? &state->summary.benchmark : &state->last_benchmark;
    Rectangle summary_rect = {rect.x + 4.0f, rect.y + 44.0f, rect.width - 8.0f, 104.0f};
    Rectangle phases_rect = {rect.x + 4.0f, summary_rect.y + summary_rect.height + 14.0f, rect.width - 8.0f, rect.height - 166.0f};
    const char *phase_labels[] = {"Insert", "Exact Lookup", "Range Scan"};
    double phase_values[3];
    char memory_text[32];
    double max_phase;
    size_t index;

    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Benchmark Summary", "Synthetic engine timings captured from the last run.");

    if (state == NULL || !state->has_benchmark_summary) {
        DrawText("Run BENCHMARK 50000 to populate throughput and latency telemetry.", (int)rect.x, (int)(rect.y + 56.0f), 15, g_theme.text_muted);
        return;
    }

    phase_values[0] = report->insert_ms;
    phase_values[1] = report->exact_lookup_ms;
    phase_values[2] = report->range_scan_ms;
    max_phase = max_float((float)phase_values[0], max_float((float)phase_values[1], (float)phase_values[2]));
    if (max_phase < 0.001) {
        max_phase = 1.0;
    }

    format_bytes(report->memory_usage_bytes, memory_text, sizeof(memory_text));
    draw_surface(summary_rect, g_theme.panel_alt, g_theme.border_soft);
    draw_metric_row((Rectangle){summary_rect.x + 14.0f, summary_rect.y + 14.0f, summary_rect.width - 28.0f, 18.0f}, "Dataset", TextFormat("%i records", (int)report->record_count));
    draw_metric_row((Rectangle){summary_rect.x + 14.0f, summary_rect.y + 34.0f, summary_rect.width - 28.0f, 18.0f}, "Insert Throughput", TextFormat("%.0f rows/sec", report->insert_throughput_rows_per_sec));
    draw_metric_row((Rectangle){summary_rect.x + 14.0f, summary_rect.y + 54.0f, summary_rect.width - 28.0f, 18.0f}, "Lookup Latency", TextFormat("%.4f ms/query", report->exact_lookup_latency_ms));
    draw_metric_row((Rectangle){summary_rect.x + 14.0f, summary_rect.y + 74.0f, summary_rect.width - 28.0f, 18.0f}, "Memory", memory_text);

    draw_surface(phases_rect, g_theme.background_alt, g_theme.border_soft);
    draw_badge((Rectangle){phases_rect.x + phases_rect.width - 178.0f, phases_rect.y + 12.0f, 160.0f, 24.0f}, report->range_scan_plan, Fade(g_theme.accent, 0.18f), g_theme.accent_hover);
    for (index = 0; index < 3; ++index) {
        float row_y = phases_rect.y + 52.0f + (float)index * 28.0f;
        float track_width = phases_rect.width - 166.0f;
        float bar_width = (float)(phase_values[index] / max_phase) * track_width;

        DrawText(phase_labels[index], (int)(phases_rect.x + 16.0f), (int)row_y, 13, g_theme.text_soft);
        DrawRectangleRounded((Rectangle){phases_rect.x + 114.0f, row_y + 3.0f, track_width, 12.0f}, 0.35f, 8, g_theme.panel);
        DrawRectangleRounded((Rectangle){phases_rect.x + 114.0f, row_y + 3.0f, bar_width, 12.0f}, 0.35f, 8, index == 0 ? g_theme.accent_soft : g_theme.accent);
        DrawText(TextFormat("%.2f ms", phase_values[index]), (int)(phases_rect.x + phases_rect.width - 74.0f), (int)row_y, 13, g_theme.text_muted);
    }
}

static int compare_grid_rows(const void *left, const void *right) {
    const Record *lhs = *(const Record *const *)left;
    const Record *rhs = *(const Record *const *)right;
    int comparison = 0;

    switch (g_grid_sort_field) {
        case FIELD_ID:
            comparison = (lhs->id > rhs->id) - (lhs->id < rhs->id);
            break;
        case FIELD_NAME:
            comparison = strcmp(lhs->name, rhs->name);
            break;
        case FIELD_AGE:
            comparison = (lhs->age > rhs->age) - (lhs->age < rhs->age);
            break;
        case FIELD_DEPARTMENT:
            comparison = strcmp(lhs->department, rhs->department);
            break;
        case FIELD_NONE:
        default:
            comparison = 0;
            break;
    }

    if (comparison == 0) {
        comparison = (lhs->id > rhs->id) - (lhs->id < rhs->id);
    }

    return g_grid_sort_desc ? -comparison : comparison;
}

static void result_grid_sort(StudioState *state, Field field, bool descending) {
    if (state == NULL || !state->summary.has_result_rows || state->summary.result.count < 2 || field == FIELD_NONE) {
        return;
    }

    g_grid_sort_field = field;
    g_grid_sort_desc = descending;
    qsort(state->summary.result.rows, state->summary.result.count, sizeof(Record *), compare_grid_rows);
}

static size_t plan_usage_slot(QueryPlanType type) {
    switch (type) {
        case PLAN_HASH_ID_EXACT:
        case PLAN_HASH_DEPARTMENT_EXACT:
            return 0;
        case PLAN_BPTREE_ID_EXACT:
        case PLAN_BPTREE_ID_RANGE:
        case PLAN_BPTREE_ID_ORDERED:
            return 1;
        case PLAN_BPTREE_AGE_EXACT:
        case PLAN_BPTREE_AGE_RANGE:
        case PLAN_BPTREE_AGE_ORDERED:
            return 2;
        case PLAN_FULL_SCAN:
        default:
            return 3;
    }
}

static void studio_push_metrics(StudioState *state) {
    float cache_ratio = 0.0f;

    if (state == NULL) {
        return;
    }

    if (state->context.query_cache_lookups > 0) {
        cache_ratio = ((float)state->context.query_cache_hits * 100.0f) / (float)state->context.query_cache_lookups;
    }

    metric_series_push(&state->latency_series, (float)state->last_elapsed_ms);
    metric_series_push(&state->scanned_series, state->summary.show_query_panel ? (float)state->summary.stats.rows_scanned : 0.0f);
    metric_series_push(&state->memory_series, (float)state->status.memory_usage_bytes / (1024.0f * 1024.0f));
    metric_series_push(&state->cache_series, cache_ratio);

    if (state->summary.show_query_panel) {
        state->index_usage[plan_usage_slot(state->summary.plan.type)] += 1;
    } else if (state->summary.show_benchmark_panel) {
        state->index_usage[0] += 1;
        state->index_usage[2] += 1;
    }
}

static void studio_execute_text(StudioState *state, const char *command_text) {
    char error[256];
    bool success;

    if (state == NULL || command_text == NULL) {
        return;
    }

    success = app_context_run_input(
        &state->context,
        command_text,
        &state->summary,
        &state->status,
        &state->last_elapsed_ms,
        error,
        sizeof(error)
    );

    if (!success) {
        studio_set_banner(state, STUDIO_BANNER_ERROR, error);
        return;
    }

    state->history_scroll = 0;
    state->grid_offset = 0;
    state->has_selected_record = false;
    state->selected_record_id = 0;
    state->grid_sort_field = FIELD_NONE;
    state->grid_sort_desc = false;
    if (state->summary.show_benchmark_panel) {
        state->last_benchmark = state->summary.benchmark;
        state->has_benchmark_summary = true;
    }
    studio_push_metrics(state);
    studio_set_banner(state, banner_tone_from_summary(&state->summary), state->summary.message);
}

static void handle_editor_input(StudioState *state) {
    int pressed;
    bool control_down;

    if (state == NULL || !state->editor_focused) {
        return;
    }

    control_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (control_down && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))) {
        if (editor_has_content(state)) {
            studio_execute_text(state, state->editor_text);
        }
        return;
    }

    if (control_down && IsKeyPressed(KEY_V)) {
        const char *clipboard = GetClipboardText();
        append_text(state->editor_text, sizeof(state->editor_text), clipboard);
    }

    if (control_down && IsKeyPressed(KEY_L)) {
        state->editor_text[0] = '\0';
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        size_t length = strlen(state->editor_text);
        if (length > 0) {
            state->editor_text[length - 1] = '\0';
        }
    }

    if (IsKeyPressed(KEY_TAB)) {
        append_text(state->editor_text, sizeof(state->editor_text), "    ");
    } else if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        append_text(state->editor_text, sizeof(state->editor_text), "\n");
    }

    pressed = GetCharPressed();
    while (pressed > 0) {
        if (pressed >= 32 && pressed <= 126) {
            char next[2];
            next[0] = (char)pressed;
            next[1] = '\0';
            append_text(state->editor_text, sizeof(state->editor_text), next);
        }
        pressed = GetCharPressed();
    }
}

static bool export_current_result_csv(const StudioState *state, const char *path, char *error, size_t error_capacity) {
    FILE *stream;
    size_t index;

    if (state == NULL || !state->summary.has_result_rows || path == NULL) {
        snprintf(error, error_capacity, "No result grid is available for export.");
        return false;
    }

    if (!ensure_parent_directory(path)) {
        snprintf(error, error_capacity, "Failed to prepare export directory.");
        return false;
    }

    stream = fopen(path, "w");
    if (stream == NULL) {
        snprintf(error, error_capacity, "Failed to open export file.");
        return false;
    }

    fprintf(stream, "id,name,age,department\n");
    for (index = 0; index < state->summary.result.count; ++index) {
        const Record *record = state->summary.result.rows[index];
        fprintf(stream, "%d,\"%s\",%d,\"%s\"\n", record->id, record->name, record->age, record->department);
    }

    fclose(stream);
    return true;
}

static void storage_health(const StudioState *state, char *status, size_t status_capacity, Color *color) {
    size_t wal_size;
    bool has_snapshot;

    if (status == NULL || status_capacity == 0) {
        return;
    }

    wal_size = file_size_or_zero(state->context.wal_path);
    has_snapshot = file_exists(state->context.active_csv_path);

    if (!has_snapshot) {
        safe_copy(status, status_capacity, "Snapshot Missing");
        if (color != NULL) {
            *color = g_theme.error;
        }
    } else if (wal_size > 2 * 1024 * 1024) {
        safe_copy(status, status_capacity, "Checkpoint Recommended");
        if (color != NULL) {
            *color = g_theme.warning;
        }
    } else {
        safe_copy(status, status_capacity, "Healthy");
        if (color != NULL) {
            *color = g_theme.success;
        }
    }
}

static void tree_traversal_description(const StudioState *state, char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    if (state == NULL || !state->summary.show_query_panel) {
        safe_copy(buffer, buffer_size, "Awaiting query traversal.");
        return;
    }

    switch (state->summary.plan.type) {
        case PLAN_HASH_ID_EXACT:
            safe_copy(buffer, buffer_size, "bucket -> chain -> record");
            break;
        case PLAN_HASH_DEPARTMENT_EXACT:
            safe_copy(buffer, buffer_size, "bucket -> department chain -> matching records");
            break;
        case PLAN_BPTREE_AGE_RANGE:
            safe_copy(buffer, buffer_size, "root -> internal nodes -> first matching leaf -> leaf chain");
            break;
        case PLAN_BPTREE_AGE_ORDERED:
            safe_copy(buffer, buffer_size, "root -> leftmost age leaf -> sequential leaf traversal");
            break;
        case PLAN_BPTREE_AGE_EXACT:
            safe_copy(buffer, buffer_size, "root -> age leaf -> duplicate value list");
            break;
        case PLAN_BPTREE_ID_EXACT:
        case PLAN_BPTREE_ID_RANGE:
        case PLAN_BPTREE_ID_ORDERED:
            safe_copy(buffer, buffer_size, "root -> id leaf path -> matching records");
            break;
        case PLAN_FULL_SCAN:
        default:
            safe_copy(buffer, buffer_size, "record array -> predicate evaluation -> optional sort");
            break;
    }
}

static void draw_line_chart(Rectangle rect, const char *title, const MetricSeries *series, Color color, const char *suffix) {
    float min_value = 0.0f;
    float max_value = 1.0f;
    size_t index;

    draw_card(rect, g_theme.panel, g_theme.border);
    DrawText(title, (int)(rect.x + 12.0f), (int)(rect.y + 12.0f), 15, g_theme.text);

    if (series == NULL || series->count == 0) {
        DrawText("No samples yet", (int)(rect.x + 12.0f), (int)(rect.y + 42.0f), 14, g_theme.text_soft);
        return;
    }

    for (index = 0; index < series->count; ++index) {
        float value = metric_series_value(series, index);
        if (index == 0 || value < min_value) {
            min_value = value;
        }
        if (index == 0 || value > max_value) {
            max_value = value;
        }
    }

    if (max_value - min_value < 0.001f) {
        max_value = min_value + 1.0f;
    }

    for (index = 0; index + 1 < series->count; ++index) {
        float value_a = metric_series_value(series, index);
        float value_b = metric_series_value(series, index + 1);
        float normalized_a = (value_a - min_value) / (max_value - min_value);
        float normalized_b = (value_b - min_value) / (max_value - min_value);
        float x_a = rect.x + 12.0f + ((float)index / (float)(series->count - 1)) * (rect.width - 24.0f);
        float x_b = rect.x + 12.0f + ((float)(index + 1) / (float)(series->count - 1)) * (rect.width - 24.0f);
        float y_a = rect.y + rect.height - 18.0f - normalized_a * (rect.height - 52.0f);
        float y_b = rect.y + rect.height - 18.0f - normalized_b * (rect.height - 52.0f);
        DrawLineEx((Vector2){x_a, y_a}, (Vector2){x_b, y_b}, 2.0f, color);
    }

    {
        char value_buffer[64];
        snprintf(value_buffer, sizeof(value_buffer), "%.2f%s", metric_series_value(series, series->count - 1), suffix == NULL ? "" : suffix);
        DrawText(value_buffer, (int)(rect.x + 12.0f), (int)(rect.y + rect.height - 34.0f), 14, g_theme.text_muted);
    }
}

static void draw_index_usage_chart(Rectangle rect, const size_t counts[5]) {
    static const char *labels[] = {"Hash", "B+ Id", "B+ Age", "Scan", "Other"};
    size_t max_count = 1;
    size_t index;

    draw_card(rect, g_theme.panel, g_theme.border);
    DrawText("Index Usage Frequency", (int)(rect.x + 12.0f), (int)(rect.y + 12.0f), 15, g_theme.text);

    for (index = 0; index < 5; ++index) {
        if (counts[index] > max_count) {
            max_count = counts[index];
        }
    }

    for (index = 0; index < 5; ++index) {
        float bar_width = ((float)counts[index] / (float)max_count) * (rect.width - 100.0f);
        float y = rect.y + 42.0f + index * 22.0f;
        DrawText(labels[index], (int)(rect.x + 12.0f), (int)y, 13, g_theme.text_soft);
        DrawRectangleRounded((Rectangle){rect.x + 70.0f, y + 4.0f, bar_width, 12.0f}, 0.4f, 8, index == 2 ? g_theme.accent : g_theme.accent_soft);
        DrawText(TextFormat("%i", (int)counts[index]), (int)(rect.x + rect.width - 24.0f), (int)y, 13, g_theme.text_muted);
    }
}

static void draw_sql_workspace(StudioState *state, Rectangle rect) {
    Rectangle execute_button = {rect.x + rect.width - 320.0f, rect.y, 96.0f, 30.0f};
    Rectangle format_button = {execute_button.x + execute_button.width + 8.0f, execute_button.y, 84.0f, 30.0f};
    Rectangle clear_button = {format_button.x + format_button.width + 8.0f, execute_button.y, 70.0f, 30.0f};
    Rectangle save_button = {clear_button.x + clear_button.width + 8.0f, clear_button.y, 54.0f, 30.0f};
    Rectangle editor_rect = {rect.x, rect.y + 44.0f, rect.width, rect.height - 44.0f};
    Vector2 mouse = GetMousePosition();

    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "SQL Editor", "Multi-line command editor with formatting, snippets, and Ctrl+Enter execution.");

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        state->editor_focused = CheckCollisionPointRec(mouse, editor_rect);
    }

    if (draw_button(execute_button, "Execute", true)) {
        if (editor_has_content(state)) {
            studio_execute_text(state, state->editor_text);
        }
    }
    if (draw_button(format_button, "Format", false)) {
        studio_format_query(state->editor_text, sizeof(state->editor_text));
    }
    if (draw_button(clear_button, "Clear", false)) {
        state->editor_text[0] = '\0';
    }
    if (draw_button(save_button, "Save", false)) {
        studio_save_current_snippet(state);
    }

    draw_surface(editor_rect, mix_color(g_theme.panel, g_theme.background, 0.25f), state->editor_focused ? g_theme.accent : g_theme.border);
    DrawLineEx((Vector2){editor_rect.x + 12.0f, editor_rect.y + 10.0f}, (Vector2){editor_rect.x + editor_rect.width - 12.0f, editor_rect.y + 10.0f}, 1.0f, Fade(g_theme.border, 0.45f));
    draw_sql_editor_text(editor_rect, state->editor_text, state->editor_focused);
}

static void draw_storage_browser(StudioState *state, Rectangle rect) {
    char wal_bytes[32];
    char health[64];
    char path_display[128];
    Color health_color = g_theme.success;

    fit_text_to_width(state->context.active_csv_path, path_display, sizeof(path_display), 14, rect.width - 36.0f);
    format_bytes(file_size_or_zero(state->context.wal_path), wal_bytes, sizeof(wal_bytes));
    storage_health(state, health, sizeof(health), &health_color);

    draw_sidebar_label((Rectangle){rect.x, rect.y, rect.width, 14.0f}, "Storage Browser");
    DrawText(path_display, (int)rect.x, (int)(rect.y + 22.0f), 12, g_theme.text);
    draw_metric_row((Rectangle){rect.x, rect.y + 42.0f, rect.width, 16.0f}, "WAL Size", wal_bytes);
    draw_metric_row((Rectangle){rect.x, rect.y + 60.0f, rect.width, 16.0f}, "Records", TextFormat("%i", (int)state->status.record_count));
    draw_metric_row((Rectangle){rect.x, rect.y + 78.0f, rect.width, 16.0f}, "Last Save", state->context.last_save_timestamp);
    draw_badge((Rectangle){rect.x, rect.y + 102.0f, 96.0f, 18.0f}, health, Fade(health_color, 0.16f), g_theme.accent);
}

static void draw_result_grid(StudioState *state, Rectangle rect) {
    static const Field headers[] = {FIELD_ID, FIELD_NAME, FIELD_AGE, FIELD_DEPARTMENT};
    static const char *labels[] = {"ID", "Name", "Age", "Department"};
    Rectangle title_rect = {rect.x, rect.y, rect.width, 24.0f};
    Rectangle table_rect = {rect.x, rect.y + 42.0f, rect.width, rect.height - 42.0f};
    Rectangle header_rect = {table_rect.x + 12.0f, table_rect.y + 12.0f, table_rect.width - 24.0f, 40.0f};
    Rectangle body_rect = {table_rect.x + 12.0f, header_rect.y + header_rect.height + 4.0f, table_rect.width - 24.0f, table_rect.height - 106.0f};
    Rectangle footer_rect = {table_rect.x + 12.0f, table_rect.y + table_rect.height - 30.0f, table_rect.width - 24.0f, 20.0f};
    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();
    float widths[4];
    int visible_rows;
    int max_offset;
    size_t page_end;
    int current_page;
    int total_pages;
    int header_index;
    char summary_text[96];
    float row_height = 32.0f;

    draw_section_title(title_rect, "Results", "Sortable result grid with sticky headers, pagination, and CSV export.");
    if (draw_button((Rectangle){rect.x + rect.width - 104.0f, rect.y, 104.0f, 32.0f}, "Export CSV", false)) {
        char error[256];
        if (export_current_result_csv(state, "data/export_result.csv", error, sizeof(error))) {
            studio_set_banner(state, STUDIO_BANNER_SUCCESS, "Exported current grid to data/export_result.csv");
        } else {
            studio_set_banner(state, STUDIO_BANNER_ERROR, error);
        }
    }

    draw_surface(table_rect, mix_color(g_theme.panel, g_theme.background, 0.12f), g_theme.border);

    visible_rows = (int)(body_rect.height / row_height);
    if (visible_rows < 1) {
        visible_rows = 1;
    }
    max_offset = (int)state->summary.result.count - visible_rows;
    if (max_offset < 0) {
        max_offset = 0;
    }
    compute_result_grid_widths(&state->summary.result, (size_t)state->grid_offset, (size_t)(state->grid_offset + visible_rows), header_rect.width, widths);

    if (CheckCollisionPointRec(mouse, body_rect) && wheel != 0.0f) {
        state->grid_offset = clamp_int(state->grid_offset - (int)(wheel * 2.0f), 0, max_offset);
    }

    draw_surface(header_rect, mix_color(g_theme.panel, g_theme.text, 0.06f), g_theme.border);
    for (header_index = 0; header_index < 4; ++header_index) {
        Rectangle cell = {header_rect.x, header_rect.y, widths[header_index], header_rect.height};
        int offset;
        char label[32];
        for (offset = 0; offset < header_index; ++offset) {
            cell.x += widths[offset];
        }

        snprintf(label, sizeof(label), "%s%s", labels[header_index],
                 state->grid_sort_field == headers[header_index] ? (state->grid_sort_desc ? " v" : " ^") : "");
        DrawText(label, (int)(cell.x + 14.0f), (int)(cell.y + 12.0f), 13, state->grid_sort_field == headers[header_index] ? g_theme.text : g_theme.text_muted);

        if (CheckCollisionPointRec(mouse, cell) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            if (state->grid_sort_field == headers[header_index]) {
                state->grid_sort_desc = !state->grid_sort_desc;
            } else {
                state->grid_sort_field = headers[header_index];
                state->grid_sort_desc = false;
            }
            result_grid_sort(state, state->grid_sort_field, state->grid_sort_desc);
            state->grid_offset = 0;
        }
    }

    BeginScissorMode((int)body_rect.x, (int)body_rect.y, (int)body_rect.width, (int)body_rect.height);
    page_end = min_size(state->summary.result.count, (size_t)(state->grid_offset + visible_rows));
    for (header_index = state->grid_offset; (size_t)header_index < page_end; ++header_index) {
        const Record *record = state->summary.result.rows[header_index];
        float row_y = body_rect.y + (header_index - state->grid_offset) * row_height;
        Rectangle row = {body_rect.x, row_y, body_rect.width, row_height};
        Rectangle id_cell = {row.x, row.y, widths[0], row.height};
        Rectangle name_cell = {id_cell.x + widths[0], row.y, widths[1], row.height};
        Rectangle age_cell = {name_cell.x + widths[1], row.y, widths[2], row.height};
        Rectangle department_cell = {age_cell.x + widths[2], row.y, widths[3], row.height};
        char number_text[64];
        char fitted[64];
        float text_width;

        DrawRectangleRec(row, header_index % 2 == 0 ? mix_color(g_theme.panel, g_theme.background, 0.20f) : mix_color(g_theme.panel, g_theme.background, 0.08f));
        if (state->has_selected_record && record->id == state->selected_record_id) {
            DrawRectangleRec(row, Fade(g_theme.accent, 0.18f));
            DrawRectangle((int)row.x, (int)row.y + 4, 3, (int)row.height - 8, g_theme.accent);
        } else if (CheckCollisionPointRec(mouse, row)) {
            DrawRectangleRec(row, Fade(g_theme.accent, 0.10f));
        }
        DrawLineEx((Vector2){row.x, row.y + row.height}, (Vector2){row.x + row.width, row.y + row.height}, 1.0f, Fade(g_theme.border, 0.18f));

        snprintf(number_text, sizeof(number_text), "%d", record->id);
        text_width = measure_text_width(number_text, 14);
        DrawTextEx(GetFontDefault(), number_text, (Vector2){id_cell.x + id_cell.width - text_width - 16.0f, row.y + 8.0f}, 14.0f, 0.0f, g_theme.text);

        fit_text_to_width(record->name, fitted, sizeof(fitted), 14, name_cell.width - 26.0f);
        DrawText(fitted, (int)(name_cell.x + 14.0f), (int)(row.y + 8.0f), 14, g_theme.text);

        snprintf(number_text, sizeof(number_text), "%d", record->age);
        text_width = measure_text_width(number_text, 14);
        DrawTextEx(GetFontDefault(), number_text, (Vector2){age_cell.x + age_cell.width - text_width - 16.0f, row.y + 8.0f}, 14.0f, 0.0f, g_theme.text);

        fit_text_to_width(record->department, fitted, sizeof(fitted), 14, department_cell.width - 26.0f);
        DrawText(fitted, (int)(department_cell.x + 14.0f), (int)(row.y + 8.0f), 14, g_theme.text);

        if (CheckCollisionPointRec(mouse, row) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            state->has_selected_record = true;
            state->selected_record_id = record->id;
        }
    }
    EndScissorMode();

    current_page = visible_rows > 0 ? (state->grid_offset / visible_rows) + 1 : 1;
    total_pages = visible_rows > 0 ? ((int)state->summary.result.count + visible_rows - 1) / visible_rows : 1;
    if (total_pages < 1) {
        total_pages = 1;
    }
    snprintf(summary_text, sizeof(summary_text), "Page %i/%i   Showing %i-%i of %i",
             current_page,
             total_pages,
             state->summary.result.count == 0 ? 0 : state->grid_offset + 1,
             (int)page_end,
             (int)state->summary.result.count);
    DrawText(summary_text, (int)footer_rect.x, (int)footer_rect.y, 12, g_theme.text_muted);
    if (state->has_selected_record) {
        DrawText(TextFormat("Selected Row %i", state->selected_record_id), (int)(footer_rect.x + 216.0f), (int)footer_rect.y, 12, g_theme.text);
    }

    if (draw_button((Rectangle){footer_rect.x + footer_rect.width - 158.0f, footer_rect.y - 4.0f, 72.0f, 26.0f}, "Prev", false)) {
        state->grid_offset = clamp_int(state->grid_offset - visible_rows, 0, max_offset);
    }
    if (draw_button((Rectangle){footer_rect.x + footer_rect.width - 78.0f, footer_rect.y - 4.0f, 72.0f, 26.0f}, "Next", false)) {
        state->grid_offset = clamp_int(state->grid_offset + visible_rows, 0, max_offset);
    }
}

static void draw_count_panel(const StudioState *state, Rectangle rect) {
    Rectangle surface = {rect.x + rect.width * 0.16f, rect.y + 70.0f, rect.width * 0.68f, 188.0f};

    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Aggregate Result", "COUNT output using the same optimizer path as SELECT.");
    draw_surface(surface, mix_color(g_theme.panel, g_theme.background, 0.16f), g_theme.border);
    DrawText("COUNT", (int)(surface.x + 24.0f), (int)(surface.y + 22.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
    DrawText(TextFormat("%i", (int)state->summary.count_value), (int)(surface.x + 24.0f), (int)(surface.y + 54.0f), 54, g_theme.text);
    draw_metric_row((Rectangle){surface.x + 24.0f, surface.y + 132.0f, surface.width - 48.0f, 16.0f}, "Rows Scanned", TextFormat("%i", (int)state->summary.stats.rows_scanned));
    draw_metric_row((Rectangle){surface.x + 24.0f, surface.y + 152.0f, surface.width - 48.0f, 16.0f}, "Optimizer Path", state->summary.optimizer_path);
}

static void draw_help_panel(Rectangle rect) {
    static const char *lines[] = {
        "Ctrl+Enter executes the editor contents",
        "Format rewrites the current command with SQL-style spacing",
        "Save Current Query stores the editor contents as a reusable snippet",
        "Save and Load buttons checkpoint and restore the active CSV + WAL session",
        "Export writes the current result set to data/export_result.csv",
        "History items can be clicked back into the editor"
    };
    size_t index;
    Rectangle surface = {rect.x + rect.width * 0.14f, rect.y + 54.0f, rect.width * 0.72f, 210.0f};

    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Studio Help", "A lightweight C storage engine with a cleaner desktop shell.");
    draw_surface(surface, mix_color(g_theme.panel, g_theme.background, 0.16f), g_theme.border);
    for (index = 0; index < sizeof(lines) / sizeof(lines[0]); ++index) {
        char fitted_line[160];
        fit_text_to_width(lines[index], fitted_line, sizeof(fitted_line), 14, surface.width - 46.0f);
        DrawCircle((int)(surface.x + 18.0f), (int)(surface.y + 29.0f + index * 28.0f), 2.0f, index == 0 ? g_theme.accent : g_theme.text_muted);
        DrawText(fitted_line, (int)(surface.x + 30.0f), (int)(surface.y + 22.0f + index * 28.0f), 14, index == 0 ? g_theme.text : g_theme.text_muted);
    }
}

static void draw_empty_panel(Rectangle rect) {
    static const char *hints[] = {
        "SELECT *",
        "SELECT WHERE age > 30",
        "SELECT ORDER BY age LIMIT 10"
    };
    Rectangle surface = {rect.x + rect.width * 0.18f, rect.y + 70.0f, rect.width * 0.64f, 190.0f};
    char title_text[96];
    char body_text[160];
    int title_width;
    int body_width;
    size_t index;

    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Results", "Run a query, benchmark, or COUNT statement to populate the main workspace.");
    draw_surface(surface, mix_color(g_theme.panel, g_theme.background, 0.16f), g_theme.border);
    fit_text_to_width("No active data view", title_text, sizeof(title_text), 28, surface.width - 40.0f);
    fit_text_to_width("Start with a quick query to inspect the in-memory table.", body_text, sizeof(body_text), 15, surface.width - 40.0f);
    title_width = MeasureText(title_text, 28);
    body_width = MeasureText(body_text, 15);
    DrawText(title_text, (int)(surface.x + (surface.width - title_width) * 0.5f), (int)(surface.y + 34.0f), 28, g_theme.text);
    DrawText(body_text, (int)(surface.x + (surface.width - body_width) * 0.5f), (int)(surface.y + 76.0f), 15, g_theme.text_muted);
    for (index = 0; index < 3; ++index) {
        Rectangle badge = {surface.x + 28.0f, surface.y + 118.0f + (float)index * 26.0f, surface.width - 56.0f, 20.0f};
        draw_badge(badge, hints[index], Fade(g_theme.accent, 0.12f), index == 0 ? g_theme.text : g_theme.text_muted);
    }
}

static void draw_execution_panel(const StudioState *state, Rectangle rect) {
    char memory_text[32];
    char traversal[160];
    char fitted_plan[128];
    Rectangle plan_rect = {rect.x, rect.y + 28.0f, rect.width, 58.0f};
    Rectangle metrics_rect = {rect.x, rect.y + 96.0f, rect.width, rect.height - 108.0f};

    format_bytes(state->summary.database_memory_bytes, memory_text, sizeof(memory_text));
    tree_traversal_description(state, traversal, sizeof(traversal));
    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Execution", "Planner choice and runtime path.");

    if (state->summary.show_query_panel) {
        fit_text_to_width(state->summary.plan.description, fitted_plan, sizeof(fitted_plan), 15, plan_rect.width - 28.0f);
        draw_surface(plan_rect, mix_color(g_theme.panel, g_theme.background, 0.20f), g_theme.border);
        DrawText("Execution Plan", (int)(plan_rect.x + 14.0f), (int)(plan_rect.y + 10.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
        DrawText(fitted_plan, (int)(plan_rect.x + 14.0f), (int)(plan_rect.y + 28.0f), 15, g_theme.text);
        draw_badge((Rectangle){plan_rect.x + plan_rect.width - 126.0f, plan_rect.y + 10.0f, 112.0f, 20.0f}, state->summary.optimizer_path, Fade(g_theme.accent, 0.14f), g_theme.accent);
        draw_surface(metrics_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
        draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 14.0f, metrics_rect.width - 28.0f, 16.0f}, "Latency", TextFormat("%.3f ms", state->last_elapsed_ms));
        if (metrics_rect.height >= 52.0f) {
            draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 34.0f, metrics_rect.width - 28.0f, 16.0f}, "Rows Scanned", TextFormat("%i", (int)state->summary.stats.rows_scanned));
        }
        if (metrics_rect.height >= 72.0f) {
            draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 54.0f, metrics_rect.width - 28.0f, 16.0f}, "Memory", memory_text);
        }
        fit_text_to_width(traversal, fitted_plan, sizeof(fitted_plan), 13, rect.width - 32.0f);
        if (metrics_rect.height >= 48.0f) {
            DrawText(fitted_plan, (int)(metrics_rect.x + 14.0f), (int)(metrics_rect.y + metrics_rect.height - 20.0f), 12, g_theme.text_muted);
        }
    } else if (state->summary.show_benchmark_panel) {
        format_bytes(state->summary.benchmark.memory_usage_bytes, memory_text, sizeof(memory_text));
        draw_surface(plan_rect, mix_color(g_theme.panel, g_theme.background, 0.20f), g_theme.border);
        DrawText("Benchmark Session", (int)(plan_rect.x + 14.0f), (int)(plan_rect.y + 10.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
        DrawText("Synthetic engine benchmark complete.", (int)(plan_rect.x + 14.0f), (int)(plan_rect.y + 28.0f), 15, g_theme.text);
        draw_surface(metrics_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
        draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 14.0f, metrics_rect.width - 28.0f, 16.0f}, "Runtime", TextFormat("%.3f ms", state->last_elapsed_ms));
        if (metrics_rect.height >= 52.0f) {
            draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 34.0f, metrics_rect.width - 28.0f, 16.0f}, "Dataset", TextFormat("%i rows", (int)state->summary.benchmark.record_count));
        }
        if (metrics_rect.height >= 72.0f) {
            draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 54.0f, metrics_rect.width - 28.0f, 16.0f}, "Memory", memory_text);
        }
    } else {
        draw_surface(plan_rect, mix_color(g_theme.panel, g_theme.background, 0.20f), g_theme.border);
        DrawText("Awaiting query", (int)(plan_rect.x + 14.0f), (int)(plan_rect.y + 24.0f), 17, g_theme.text);
        DrawText("Run SELECT, COUNT, or BENCHMARK to populate this panel.", (int)(plan_rect.x + 14.0f), (int)(plan_rect.y + 42.0f), 12, g_theme.text_muted);
        draw_surface(metrics_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
        draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 14.0f, metrics_rect.width - 28.0f, 16.0f}, "Current Memory", memory_text);
    }
}

static void draw_leaf_chain(Rectangle rect, size_t count, Color fill) {
    size_t nodes = min_size(count, 7);
    size_t index;
    float x = rect.x;

    for (index = 0; index < nodes; ++index) {
        DrawRectangleRounded((Rectangle){x, rect.y, 14.0f, rect.height}, 0.25f, 8, fill);
        if (index + 1 < nodes) {
            DrawLineEx((Vector2){x + 14.0f, rect.y + rect.height * 0.5f}, (Vector2){x + 20.0f, rect.y + rect.height * 0.5f}, 1.5f, Fade(g_theme.border, 0.55f));
        }
        x += 20.0f;
    }
}

static void draw_index_explorer(const StudioState *state, Rectangle rect) {
    IdIndexStats id_hash_stats;
    DepartmentIndexStats department_stats;
    BPlusTreeStats id_tree_stats;
    BPlusTreeStats age_tree_stats;
    char traversal[160];
    char fitted_traversal[160];
    Rectangle stats_rect = {rect.x, rect.y + 28.0f, rect.width, 78.0f};
    Rectangle chain_rect = {rect.x, rect.y + 116.0f, rect.width, rect.height - 116.0f};

    id_index_collect_stats(&state->context.database.id_index, &id_hash_stats);
    department_index_collect_stats(&state->context.database.department_index, &department_stats);
    bptree_collect_stats(&state->context.database.id_tree, &id_tree_stats);
    bptree_collect_stats(&state->context.database.age_tree, &age_tree_stats);
    tree_traversal_description(state, traversal, sizeof(traversal));

    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Index Explorer", "Hash and B+ Tree topology at a glance.");
    draw_surface(stats_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
    draw_metric_row((Rectangle){stats_rect.x + 14.0f, stats_rect.y + 12.0f, stats_rect.width - 28.0f, 16.0f}, "Hash(id)", TextFormat("%i entries | chain %i", (int)id_hash_stats.entry_count, (int)id_hash_stats.max_chain_length));
    draw_metric_row((Rectangle){stats_rect.x + 14.0f, stats_rect.y + 30.0f, stats_rect.width - 28.0f, 16.0f}, "Hash(department)", TextFormat("%i keys | %i rows", (int)department_stats.key_count, (int)department_stats.record_count));
    draw_metric_row((Rectangle){stats_rect.x + 14.0f, stats_rect.y + 48.0f, stats_rect.width - 28.0f, 16.0f}, "B+Tree(age)", TextFormat("%i nodes | depth %i", (int)age_tree_stats.node_count, (int)age_tree_stats.depth));
    draw_surface(chain_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
    DrawText(TextFormat("Leaf Chain %i", (int)age_tree_stats.leaf_chain_length), (int)(chain_rect.x + 14.0f), (int)(chain_rect.y + 12.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
    draw_leaf_chain((Rectangle){chain_rect.x + 14.0f, chain_rect.y + 34.0f, chain_rect.width - 28.0f, 6.0f}, age_tree_stats.leaf_chain_length, g_theme.accent);
    if (rect.height >= 164.0f) {
        fit_text_to_width(traversal, fitted_traversal, sizeof(fitted_traversal), 12, chain_rect.width - 28.0f);
        DrawText(fitted_traversal, (int)(chain_rect.x + 14.0f), (int)(chain_rect.y + chain_rect.height - 18.0f), 12, g_theme.text_muted);
    }
}

static void draw_performance_dashboard(StudioState *state, Rectangle rect) {
    char memory_text[32];
    size_t total_index_usage = 0;
    size_t index;
    float segment_x;
    float segment_width_total;
    float actions_y = rect.y + rect.height - 24.0f;
    float metrics_height = rect.height >= 170.0f ? 84.0f : 64.0f;
    Rectangle metrics_rect = {rect.x, rect.y + 30.0f, rect.width, metrics_height};
    Rectangle spark_rect = {rect.x, metrics_rect.y + metrics_rect.height + 8.0f, rect.width, 28.0f};
    Rectangle benchmark_rect = {rect.x, spark_rect.y + spark_rect.height + 8.0f, rect.width, clamp_float(actions_y - (spark_rect.y + spark_rect.height + 14.0f), 0.0f, rect.height)};

    for (index = 0; index < 5; ++index) {
        total_index_usage += state->index_usage[index];
    }

    format_bytes(state->status.memory_usage_bytes, memory_text, sizeof(memory_text));
    draw_section_title((Rectangle){rect.x, rect.y, rect.width, 24.0f}, "Performance", "Compact session telemetry and actions.");
    draw_surface(metrics_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
    draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 12.0f, metrics_rect.width - 28.0f, 16.0f}, "Latency", state->latency_series.count > 0 ? TextFormat("%.2f ms", metric_series_value(&state->latency_series, state->latency_series.count - 1)) : "No samples");
    draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 32.0f, metrics_rect.width - 28.0f, 16.0f}, "Rows Scanned", state->scanned_series.count > 0 ? TextFormat("%.0f", metric_series_value(&state->scanned_series, state->scanned_series.count - 1)) : "0");
    if (metrics_rect.height >= 58.0f) {
        draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 52.0f, metrics_rect.width - 28.0f, 16.0f}, "Memory", memory_text);
    }
    if (metrics_rect.height >= 78.0f) {
        draw_metric_row((Rectangle){metrics_rect.x + 14.0f, metrics_rect.y + 72.0f, metrics_rect.width - 28.0f, 16.0f}, "Cache Hit Ratio", state->cache_series.count > 0 ? TextFormat("%.1f%%", metric_series_value(&state->cache_series, state->cache_series.count - 1)) : "0.0%");
    }

    draw_surface(spark_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
    DrawText("Latency Trend", (int)(spark_rect.x + 14.0f), (int)(spark_rect.y + 10.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
    draw_sparkline((Rectangle){spark_rect.x + 14.0f, spark_rect.y + 24.0f, spark_rect.width - 28.0f, 10.0f}, &state->latency_series, g_theme.accent);

    if (benchmark_rect.height >= 44.0f) {
        draw_surface(benchmark_rect, mix_color(g_theme.panel, g_theme.background, 0.14f), g_theme.border);
        DrawText("Benchmark", (int)(benchmark_rect.x + 14.0f), (int)(benchmark_rect.y + 10.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
        if (state->has_benchmark_summary) {
            draw_metric_row((Rectangle){benchmark_rect.x + 14.0f, benchmark_rect.y + 28.0f, benchmark_rect.width - 28.0f, 16.0f}, "Last Run", TextFormat("%i records", (int)state->last_benchmark.record_count));
            if (benchmark_rect.height >= 64.0f) {
                draw_metric_row((Rectangle){benchmark_rect.x + 14.0f, benchmark_rect.y + 46.0f, benchmark_rect.width - 28.0f, 16.0f}, "Range Path", state->last_benchmark.range_scan_plan);
            }
        } else {
            DrawText("No benchmark captured yet.", (int)(benchmark_rect.x + 14.0f), (int)(benchmark_rect.y + 30.0f), 12, g_theme.text_muted);
        }
        if (benchmark_rect.height >= 82.0f) {
            DrawText("Index Mix", (int)(benchmark_rect.x + 14.0f), (int)(benchmark_rect.y + benchmark_rect.height - 28.0f), STUDIO_TEXT_LABEL, g_theme.text_muted);
            draw_surface((Rectangle){benchmark_rect.x + 14.0f, benchmark_rect.y + benchmark_rect.height - 16.0f, benchmark_rect.width - 28.0f, 6.0f}, mix_color(g_theme.panel, g_theme.background, 0.18f), g_theme.border);
            segment_x = benchmark_rect.x + 15.0f;
            segment_width_total = benchmark_rect.width - 30.0f;
            for (index = 0; index < 4; ++index) {
                float share = total_index_usage > 0 ? (float)state->index_usage[index] / (float)total_index_usage : 0.0f;
                float segment_width = segment_width_total * share;

                if (segment_width > 0.0f) {
                    DrawRectangleRounded((Rectangle){segment_x, benchmark_rect.y + benchmark_rect.height - 15.0f, segment_width, 4.0f}, STUDIO_PANEL_RADIUS, 10, g_theme.accent);
                }
                segment_x += segment_width;
            }
        }
    }

    if (draw_button((Rectangle){rect.x, actions_y, (rect.width - 10.0f) * 0.5f, 24.0f}, "Restore Snapshot", false)) {
        studio_execute_text(state, "LOAD");
    }
    if (draw_button((Rectangle){rect.x + (rect.width + 10.0f) * 0.5f, actions_y, (rect.width - 10.0f) * 0.5f, 24.0f}, "Bench 50k", false)) {
        studio_set_editor_text(state, "BENCHMARK 50000");
        studio_execute_text(state, "BENCHMARK 50000");
    }
}

static void draw_status_bar(const StudioState *state, Rectangle rect) {
    char memory_text[32];
    float meta_x = rect.x + 20.0f;
    float meta_y = rect.y + rect.height - 28.0f;

    format_bytes(state->status.memory_usage_bytes, memory_text, sizeof(memory_text));
    draw_card(rect, g_theme.panel, g_theme.border);
    DrawText("MiniDB Studio", (int)(rect.x + 20.0f), (int)(rect.y + 16.0f), STUDIO_TEXT_APP_TITLE, g_theme.text);
    DrawText("Storage engine studio for parser, optimizer, B+ Tree, WAL, and persistence demos.", (int)(rect.x + 22.0f), (int)(rect.y + 50.0f), 13, g_theme.text_muted);
    draw_banner((Rectangle){rect.x + rect.width - 322.0f, rect.y + 18.0f, 294.0f, 30.0f}, state->banner_tone, state->banner_text);
    draw_divider(rect, rect.y + rect.height - 34.0f);
    draw_metric_row((Rectangle){meta_x, meta_y, 120.0f, 16.0f}, "Records", TextFormat("%i", (int)state->status.record_count));
    draw_metric_row((Rectangle){meta_x + 136.0f, meta_y, 140.0f, 16.0f}, "Indexes", "4 loaded");
    draw_metric_row((Rectangle){meta_x + 292.0f, meta_y, 134.0f, 16.0f}, "Storage", "CSV + WAL");
    draw_metric_row((Rectangle){rect.x + rect.width - 260.0f, meta_y, 110.0f, 16.0f}, "Queries", TextFormat("%i", (int)state->status.session_queries));
    draw_metric_row((Rectangle){rect.x + rect.width - 134.0f, meta_y, 114.0f, 16.0f}, "Memory", memory_text);
}

static void draw_left_sidebar(StudioState *state, Rectangle rect) {
    char assistant_hint[160];
    char assistant_queries[3][DB_MAX_COMMAND_LEN];
    Rectangle content = {rect.x + STUDIO_PANEL_PADDING, rect.y + STUDIO_PANEL_PADDING, rect.width - STUDIO_PANEL_PADDING * 2.0f, rect.height - STUDIO_PANEL_PADDING * 2.0f};
    Rectangle assistant_rect = {content.x, content.y, content.width, 132.0f};
    Rectangle history_rect = {content.x, assistant_rect.y + assistant_rect.height + 20.0f, content.width, clamp_float(rect.height * 0.14f, 74.0f, 98.0f)};
    Rectangle snippets_rect = {content.x, history_rect.y + history_rect.height + 20.0f, content.width, 94.0f};
    Rectangle storage_rect = {content.x, snippets_rect.y + snippets_rect.height + 20.0f, content.width, content.y + content.height - (snippets_rect.y + snippets_rect.height + 20.0f)};
    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();
    int visible_history;
    int max_scroll;
    size_t index;

    draw_card(rect, g_theme.panel, g_theme.border);
    studio_build_assistant_content(state, assistant_hint, sizeof(assistant_hint), assistant_queries);

    draw_sidebar_label((Rectangle){assistant_rect.x, assistant_rect.y, assistant_rect.width, 14.0f}, "AI Query Assistant");
    DrawText(assistant_hint, (int)assistant_rect.x, (int)(assistant_rect.y + 18.0f), 12, g_theme.text_muted);
    for (index = 0; index < 3; ++index) {
        if (assistant_queries[index][0] == '\0') {
            continue;
        }
        {
            char fitted_query[DB_MAX_COMMAND_LEN];
            bool hovered;
            bool selected;
            Rectangle item_rect = {assistant_rect.x, assistant_rect.y + 44.0f + (float)index * 22.0f, assistant_rect.width, 18.0f};

            fit_text_to_width(assistant_queries[index], fitted_query, sizeof(fitted_query), 15, assistant_rect.width - 18.0f);
            hovered = CheckCollisionPointRec(mouse, item_rect);
            selected = strcmp(state->editor_text, assistant_queries[index]) == 0;
            draw_sidebar_item(item_rect, fitted_query, hovered, selected);
            if (hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                studio_set_editor_text(state, assistant_queries[index]);
                state->editor_focused = true;
            }
        }
    }

    draw_sidebar_label((Rectangle){history_rect.x, history_rect.y, history_rect.width, 14.0f}, "Query History");
    visible_history = (int)((history_rect.height - 42.0f) / 24.0f);
    if (visible_history < 1) {
        visible_history = 1;
    }
    max_scroll = (int)state->context.history.count - visible_history;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    if (CheckCollisionPointRec(mouse, history_rect) && wheel != 0.0f) {
        state->history_scroll = clamp_int(state->history_scroll - (int)wheel, 0, max_scroll);
    }
    BeginScissorMode((int)history_rect.x, (int)(history_rect.y + 34.0f), (int)history_rect.width, (int)(history_rect.height - 38.0f));
    if (state->context.history.count == 0) {
        DrawText("No queries yet.", (int)history_rect.x, (int)(history_rect.y + 42.0f), 14, g_theme.text_soft);
    } else {
        int row;
        for (row = 0; row < visible_history; ++row) {
            int history_index = (int)state->context.history.count - 1 - (state->history_scroll + row);
            Rectangle row_rect;
            char label[208];
            char fitted[208];
            bool selected;

            if (history_index < 0) {
                break;
            }

            row_rect = (Rectangle){history_rect.x, history_rect.y + 24.0f + (float)row * 22.0f, history_rect.width, 18.0f};
            snprintf(label, sizeof(label), "%02d  %s", history_index + 1, state->context.history.items[history_index]);
            fit_text_to_width(label, fitted, sizeof(fitted), 13, row_rect.width - 10.0f);
            selected = strcmp(state->editor_text, state->context.history.items[history_index]) == 0;
            draw_sidebar_item(row_rect, fitted, CheckCollisionPointRec(mouse, row_rect), selected);
            if (CheckCollisionPointRec(mouse, row_rect) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                studio_set_editor_text(state, state->context.history.items[history_index]);
                state->editor_focused = true;
            }
        }
    }
    EndScissorMode();

    draw_sidebar_label((Rectangle){snippets_rect.x, snippets_rect.y, snippets_rect.width, 14.0f}, "Saved Snippets");
    for (index = 0; index < state->snippet_count; ++index) {
        Rectangle row_rect = {snippets_rect.x, snippets_rect.y + 22.0f + (float)index * 18.0f, snippets_rect.width, 16.0f};
        bool hovered = CheckCollisionPointRec(mouse, row_rect);
        char fitted_title[64];
        bool selected = strcmp(state->editor_text, state->snippets[index].query) == 0;

        fit_text_to_width(state->snippets[index].title, fitted_title, sizeof(fitted_title), 13, row_rect.width - 12.0f);
        draw_sidebar_item(row_rect, fitted_title, hovered, selected);
        if (hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            studio_set_editor_text(state, state->snippets[index].query);
            state->editor_focused = true;
        }
    }

    draw_storage_browser(state, storage_rect);
}

static void draw_center_main(StudioState *state, Rectangle rect) {
    float editor_height = clamp_float(rect.height * 0.33f, 214.0f, 258.0f);
    Rectangle content = {rect.x + 20.0f, rect.y + 18.0f, rect.width - 40.0f, rect.height - 36.0f};
    Rectangle editor_rect = {content.x, content.y, content.width, editor_height};
    Rectangle results_rect = {content.x, editor_rect.y + editor_rect.height + 28.0f, content.width, content.y + content.height - (editor_rect.y + editor_rect.height + 28.0f)};

    draw_card(rect, g_theme.panel, g_theme.border);
    draw_sql_workspace(state, editor_rect);
    draw_divider(rect, editor_rect.y + editor_rect.height + 12.0f);

    if (state->summary.show_benchmark_panel) {
        draw_benchmark_panel(state, results_rect);
    } else if (state->summary.has_result_rows) {
        draw_result_grid(state, results_rect);
    } else if (state->summary.has_count_value) {
        draw_count_panel(state, results_rect);
    } else if (state->summary.show_help_panel) {
        draw_help_panel(results_rect);
    } else {
        draw_empty_panel(results_rect);
    }
}

static void draw_right_inspector(StudioState *state, Rectangle rect) {
    Rectangle content = {rect.x + 18.0f, rect.y + 18.0f, rect.width - 36.0f, rect.height - 36.0f};
    Rectangle execution_rect = {content.x, content.y, content.width, clamp_float(rect.height * 0.34f, 184.0f, 210.0f)};
    Rectangle index_rect = {content.x, execution_rect.y + execution_rect.height + 22.0f, content.width, clamp_float(rect.height * 0.25f, 146.0f, 170.0f)};
    Rectangle performance_rect = {content.x, index_rect.y + index_rect.height + 22.0f, content.width, content.y + content.height - (index_rect.y + index_rect.height + 22.0f)};

    draw_card(rect, g_theme.panel, g_theme.border);
    draw_surface(execution_rect, mix_color(g_theme.panel, g_theme.background, 0.10f), g_theme.border);
    draw_execution_panel(state, (Rectangle){execution_rect.x + 14.0f, execution_rect.y + 12.0f, execution_rect.width - 28.0f, execution_rect.height - 24.0f});
    draw_surface(index_rect, mix_color(g_theme.panel, g_theme.background, 0.10f), g_theme.border);
    draw_index_explorer(state, (Rectangle){index_rect.x + 14.0f, index_rect.y + 12.0f, index_rect.width - 28.0f, index_rect.height - 24.0f});
    draw_surface(performance_rect, mix_color(g_theme.panel, g_theme.background, 0.10f), g_theme.border);
    draw_performance_dashboard(state, (Rectangle){performance_rect.x + 14.0f, performance_rect.y + 12.0f, performance_rect.width - 28.0f, performance_rect.height - 24.0f});
}

static bool studio_initialize(StudioState *state) {
    char error[256];

    memset(state, 0, sizeof(*state));
    if (!app_context_init(&state->context, DB_DEFAULT_DATA_PATH, DB_DEFAULT_LOG_PATH, DB_DEFAULT_WAL_PATH, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return false;
    }

    app_context_snapshot_status(&state->context, &state->status);
    studio_seed_snippets(state);
    studio_load_snippet_library(state);
    studio_set_editor_text(state, "SELECT *");
    state->editor_focused = true;
    state->has_selected_record = false;
    state->selected_record_id = 0;
    studio_set_banner(state, STUDIO_BANNER_INFO, "Ready. Use Ctrl+Enter to execute the editor contents.");
    return true;
}

static void studio_shutdown(StudioState *state) {
    if (state == NULL) {
        return;
    }

    command_execution_summary_destroy(&state->summary);
    app_context_destroy(&state->context);
}

int studio_run(void) {
    StudioState state;

    if (!studio_initialize(&state)) {
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(STUDIO_WINDOW_WIDTH, STUDIO_WINDOW_HEIGHT, "MiniDB Studio");
    SetWindowMinSize(STUDIO_MIN_WIDTH, STUDIO_MIN_HEIGHT);
    SetTargetFPS(60);

    while (!WindowShouldClose() && state.context.running) {
        float screen_width = (float)GetScreenWidth();
        float screen_height = (float)GetScreenHeight();
        float outer_margin = 18.0f;
        float header_height = 96.0f;
        float left_width = 264.0f;
        float right_width = 300.0f;
        float body_y = outer_margin + header_height + STUDIO_GAP;
        float body_height = screen_height - body_y - outer_margin;
        float center_width = screen_width - outer_margin * 2.0f - left_width - right_width - STUDIO_GAP * 2.0f;
        Rectangle header_rect = {outer_margin, outer_margin, screen_width - outer_margin * 2.0f, header_height};
        Rectangle left_rect = {outer_margin, body_y, left_width, body_height};
        Rectangle center_rect = {left_rect.x + left_rect.width + STUDIO_GAP, body_y, center_width, body_height};
        Rectangle right_rect = {center_rect.x + center_rect.width + STUDIO_GAP, body_y, right_width, body_height};

        handle_editor_input(&state);

        BeginDrawing();
        ClearBackground(g_theme.background);
        draw_status_bar(&state, header_rect);
        draw_left_sidebar(&state, left_rect);
        draw_center_main(&state, center_rect);
        draw_right_inspector(&state, right_rect);
        EndDrawing();
    }

    CloseWindow();
    studio_shutdown(&state);
    return 0;
}
