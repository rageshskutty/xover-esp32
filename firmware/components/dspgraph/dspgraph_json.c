#include "dspgraph_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static void set_json_error(dspg_parse_issue_t *issue, const char *message)
{
    if (issue != NULL) {
        (void)snprintf(issue->code, sizeof(issue->code), "%s", "E_JSON");
        issue->path[0] = '\0';
        (void)snprintf(issue->msg, sizeof(issue->msg), "%s", message);
    }
}

static bool depth_is_bounded(const char *json, size_t len)
{
    size_t depth = 0U;
    bool in_string = false;
    bool escaped = false;
    for (size_t i = 0U; i < len; i++) {
        char ch = json[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
        } else if (ch == '"') {
            in_string = true;
        } else if (ch == '{' || ch == '[') {
            depth++;
            if (depth > DSPG_JSON_MAX_DEPTH) {
                return false;
            }
        } else if (ch == '}' || ch == ']') {
            if (depth == 0U) {
                return false;
            }
            depth--;
        }
    }
    return !in_string && depth == 0U;
}

cJSON *dspg_parse_bounded_json(const char *json, size_t len, dspg_parse_issue_t *issue)
{
    const char *parse_end = NULL;
    const char *input_end;
    cJSON *root;

    if (issue != NULL) {
        memset(issue, 0, sizeof(*issue));
    }
    if (json == NULL || len == 0U || len > DSPG_JSON_MAX_BYTES) {
        set_json_error(issue, "invalid body size");
        return NULL;
    }
    if (!depth_is_bounded(json, len)) {
        set_json_error(issue, "invalid or excessive nesting");
        return NULL;
    }
    root = cJSON_ParseWithLengthOpts(json, len, &parse_end, 0);
    if (root == NULL || parse_end == NULL) {
        if (root != NULL) {
            cJSON_Delete(root);
        }
        set_json_error(issue, "invalid JSON");
        return NULL;
    }
    input_end = json + len;
    while (parse_end < input_end && isspace((unsigned char)*parse_end)) {
        parse_end++;
    }
    if (parse_end != input_end) {
        cJSON_Delete(root);
        set_json_error(issue, "trailing data");
        return NULL;
    }
    return root;
}
