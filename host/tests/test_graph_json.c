#include "dspgraph_json.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int test_rejects(const char *json, size_t len)
{
    dspg_parse_issue_t issue;
    CHECK(dspg_parse_bounded_json(json, len, &issue) == NULL);
    CHECK(strcmp(issue.code, "E_JSON") == 0);
    return 0;
}

int main(void)
{
    static const char valid[] = "{\"text\":\"[[[[[[[[[\",\"n\":1}  \t\r\n";
    static const char malformed[] = "{\"n\":]";
    static const char trailing[] = "{} trailing";
    static const char too_deep[] = "[[[[[[[[[0]]]]]]]]]";
    char oversized[DSPG_JSON_MAX_BYTES + 1U];
    dspg_parse_issue_t issue;
    cJSON *root = dspg_parse_bounded_json(valid, sizeof(valid) - 1U, &issue);
    CHECK(root != NULL);
    cJSON_Delete(root);
    CHECK(test_rejects(malformed, sizeof(malformed) - 1U) == 0);
    CHECK(test_rejects(trailing, sizeof(trailing) - 1U) == 0);
    CHECK(test_rejects(too_deep, sizeof(too_deep) - 1U) == 0);
    CHECK(test_rejects(NULL, 0U) == 0);
    memset(oversized, ' ', sizeof(oversized));
    CHECK(test_rejects(oversized, sizeof(oversized)) == 0);
    puts("PASS: bounded JSON syntax parser");
    return 0;
}
