#include <math.h>
#include <string.h>

#include "dspgraph_build.h"

static const char *str_of(const cJSON *n, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(n, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}

static double num_of(const cJSON *n, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(n, key);
    return cJSON_IsNumber(v) ? v->valuedouble : (double)NAN;
}

static double freq_of(const cJSON *vars, const cJSON *n)
{
    const cJSON *f = cJSON_GetObjectItemCaseSensitive(n, "frequency_hz");
    if (cJSON_IsString(f)) {
        f = cJSON_GetObjectItemCaseSensitive(vars, &f->valuestring[1]);
    }
    return cJSON_IsNumber(f) ? f->valuedouble : (double)NAN;
}

static const char *const FAMILIES[] = {"Butterworth", "Linkwitz-Riley", "Bessel"};
static const char *const MODES[] = {"lowpass", "highpass"};
static const char *const SHAPES[] = {"low", "high"};

/* Index of s in names (matching the dsp_* enum order), or -1 when unknown. */
static int lookup(const char *s, const char *const *names, int n)
{
    for (int i = 0; i < n; i++) {
        if (strcmp(s, names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static dsp_err_t design_biquad(const cJSON *jn, dsp_sos_t *s)
{
    const cJSON *b = cJSON_GetObjectItemCaseSensitive(jn, "b");
    const cJSON *a = cJSON_GetObjectItemCaseSensitive(jn, "a");
    if (cJSON_GetArraySize(b) != 3 || cJSON_GetArraySize(a) != 2) {
        return DSP_E_PARAM;
    }
    memset(s, 0, sizeof(*s));
    s->b0 = (float)cJSON_GetArrayItem(b, 0)->valuedouble;
    s->b1 = (float)cJSON_GetArrayItem(b, 1)->valuedouble;
    s->b2 = (float)cJSON_GetArrayItem(b, 2)->valuedouble;
    s->a1 = (float)cJSON_GetArrayItem(a, 0)->valuedouble;
    s->a2 = (float)cJSON_GetArrayItem(a, 1)->valuedouble;
    return dsp_check_stable(s);
}

static dsp_err_t design_eq(const char *t, const cJSON *jn, double f, dsp_sos_t *s)
{
    double q = num_of(jn, "q");
    double db = num_of(jn, "db");
    if (strcmp(t, "peq") == 0) {
        return dsp_design_peq(f, q, db, B_FS_HZ, s);
    }
    if (strcmp(t, "shelf") == 0) {
        int shape = lookup(str_of(jn, "shape"), SHAPES, 2);
        return (shape < 0) ? DSP_E_PARAM : dsp_design_shelf((dsp_shelf_t)shape, f, db, B_FS_HZ, s);
    }
    if (strcmp(t, "allpass") == 0) {
        return dsp_design_allpass(f, q, B_FS_HZ, s);
    }
    return (strcmp(t, "notch") == 0) ? dsp_design_notch(f, q, B_FS_HZ, s) : DSP_E_PARAM;
}

static dsp_err_t design_mix(const cJSON *jn, bnode_t *b)
{
    const cJSON *gains = cJSON_GetObjectItemCaseSensitive(jn, "gains");
    int n = cJSON_GetArraySize(gains);
    if (n < 2 || (unsigned)n > B_MAX_IN) {
        return DSP_E_PARAM;
    }
    for (int k = 0; k < n; k++) {
        b->gains[k] = (float)cJSON_GetArrayItem(gains, k)->valuedouble;
    }
    b->kind = K_MIX;
    return DSP_OK;
}

dsp_err_t dspg_compile_node(const cJSON *vars, const cJSON *jn, bnode_t *b)
{
    const char *t = str_of(jn, "type");
    double f = freq_of(vars, jn);
    dsp_err_t rc = DSP_OK;

    if (strlen(str_of(jn, "id")) >= sizeof(b->id)) {
        return DSP_E_PARAM;
    }
    (void)memset(b, 0, sizeof(*b));
    (void)memcpy(b->id, str_of(jn, "id"), strlen(str_of(jn, "id")) + 1U);

    if (cJSON_GetObjectItemCaseSensitive(jn, "frequency_hz") != NULL && !isfinite(f)) {
        return DSP_E_PARAM;
    }
    b->kind = K_CASCADE;
    b->casc.n = 1U;
    if (strcmp(t, "input") == 0) {
        b->kind = K_INPUT;
    } else if (strcmp(t, "output") == 0) {
        b->kind = K_OUTPUT;
    } else if (strcmp(t, "crossover") == 0) {
        int fam = lookup(str_of(jn, "family"), FAMILIES, 3);
        int mode = lookup(str_of(jn, "mode"), MODES, 2);
        rc = (fam < 0 || mode < 0)
                 ? DSP_E_PARAM
                 : dsp_design_xo((dsp_family_t)fam, (dsp_mode_t)mode,
                                 (unsigned)num_of(jn, "order"), f, B_FS_HZ, &b->casc);
    } else if (strcmp(t, "biquad") == 0) {
        rc = design_biquad(jn, &b->casc.sos[0]);
    } else if (strcmp(t, "gain") == 0) {
        b->kind = K_GAIN;
        b->gain = dsp_db_to_linear((float)num_of(jn, "db"));
    } else if (strcmp(t, "delay") == 0) {
        b->kind = K_DELAY;
        b->delay = dsp_delay_us_to_samples(num_of(jn, "delay_us"), B_FS_HZ);
    } else if (strcmp(t, "polarity") == 0) {
        b->kind = K_POLARITY;
        b->invert = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(jn, "invert"));
    } else if (strcmp(t, "mix") == 0) {
        rc = design_mix(jn, b);
    } else {
        rc = design_eq(t, jn, f, &b->casc.sos[0]);
    }
    return rc;
}
