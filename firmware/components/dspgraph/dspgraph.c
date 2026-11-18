#include "dspgraph.h"

#include <string.h>

#include "dspgraph_graph.h"
#include "dspgraph_json.h"
#include "dspgraph_validate.h"

int dspg_validate(const char *json, size_t len, dspg_report_t *report)
{
    dspg_parse_issue_t parse_issue;
    cJSON *root;
    int rc;

    if (report == NULL) {
        return -1;
    }
    memset(report, 0, sizeof(*report));
    root = dspg_parse_bounded_json(json, len, &parse_issue);
    if (root == NULL) {
        dspg_report_add(report, parse_issue.code, parse_issue.path, parse_issue.msg);
        return -1;
    }
    rc = dspg_validate_tree(root, report);
    if (rc == 0) {
        rc = dspg_validate_graph(root, report);
    }
    cJSON_Delete(root);
    return rc;
}
